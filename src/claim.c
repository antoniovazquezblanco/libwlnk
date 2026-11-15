/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * claim.c
 * Exclusive claims on radios.
 *
 * A claim resolves the radio, takes its lock, refuses to touch anything
 * while an earlier claim's record is there, checks rfkill, then takes down
 * every non-monitor link on the radio. Its monitor interfaces come after.
 * Every change is recorded before it is applied, and release and recover
 * are the same undo of the record.
 */

#include <net/if.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "claim.h"
#include "err.h"
#include "iface.h"

#include "err_internal.h"
#include "journal.h"
#include "link.h"
#include "lock.h"
#include "nl.h"
#include "radio.h"
#include "vif.h"

/** Step: a link taken down. Its argument is the interface. */
#define STEP_LINK_DOWN "link-down"
/** Step: a monitor interface created. Its argument is the interface. */
#define STEP_VIF       "vif"
/** Most monitor interfaces one claim creates. */
#define VIF_MAX        8

/** A monitor interface of a claim. */
struct wlnk_vif {
    /** Its name. */
    char ifname[WLNK_IFNAME_MAX];
};

/** A claim. */
struct wlnk_claim {
    /** The radio claimed. */
    wlnk_radio_t radio;
    /** Its lock, held for as long as the claim. */
    int lock_fd;
    /** Its record, open for as long as the claim. */
    wlnk_journal_t journal;
    /** For its monitor interfaces. */
    wlnk_nl_t *nl;
    /** Its monitor interfaces. */
    struct wlnk_vif vifs[VIF_MAX];
    /** How many. */
    size_t n_vifs;
};

/**
 * @brief Undo one recorded step of a claim.
 * @param user The wlnk_nl_t handle, for monitor interfaces.
 * @param kind What the step did.
 * @param arg What it did it to.
 * @return WLNK_OK when undone; WLNK_E_UNSUPPORTED for a step this build
 * does not know, which stays recorded.
 */
static int undo_step(void *user, const char *kind, const char *arg)
{
    if (strcmp(kind, STEP_VIF) == 0)
        return wlnk_vif_del(user, arg);
    if (strcmp(kind, STEP_LINK_DOWN) == 0) {
        int rc = wlnk_if_set_up(arg, true);
        /* An interface gone since has no link left to restore. */
        return rc == WLNK_E_NOT_FOUND ? wlnk_ok() : rc;
    }
    char msg[WLNK_ERR_MSG_MAX];
    snprintf(msg, sizeof(msg), "claim: unknown step %s, left recorded", kind);
    return wlnk_fail(WLNK_E_UNSUPPORTED, 0, msg);
}

/**
 * @brief Take down every non-monitor link that is up on the claimed radio,
 * recording each first.
 *
 * Links already down are left alone, so they stay down at release.
 *
 * @param c The claim.
 * @return WLNK_OK, or the failure.
 */
static int links_down(wlnk_claim_t *c)
{
    wlnk_iface_t *ifaces = NULL;
    size_t n = 0;
    int rc = wlnk_iface_list(&ifaces, &n);
    for (size_t i = 0; rc == WLNK_OK && i < n; i++) {
        const wlnk_iface_t *f = &ifaces[i];
        if (f->wiphy != c->radio.wiphy || f->kind == WLNK_IF_MONITOR)
            continue;
        bool up = false;
        rc = wlnk_if_is_up(f->ifname, &up);
        if (rc == WLNK_E_NOT_FOUND) {
            rc = wlnk_ok(); /* gone since it was listed */
            continue;
        }
        if (rc != WLNK_OK || !up)
            continue;
        rc = wlnk_journal_step(&c->journal, STEP_LINK_DOWN, f->ifname);
        if (rc == WLNK_OK)
            rc = wlnk_if_set_up(f->ifname, false);
    }
    wlnk_iface_free(ifaces);
    return rc;
}

/**
 * @brief Put the radio back after a claim failed part way.
 *
 * What cannot be undone stays recorded, for wlnk_recover(). The claim's own
 * failure is the one reported.
 *
 * @param c The claim.
 * @param rc Its failure.
 * @return @p rc.
 */
static int unwind(wlnk_claim_t *c, int rc)
{
    int e = wlnk_err_syserrno();
    char msg[WLNK_ERR_MSG_MAX];
    snprintf(msg, sizeof(msg), "%s", wlnk_err_msg());
    wlnk_journal_close(&c->journal);
    (void)wlnk_journal_undo(&c->radio, undo_step, c->nl);
    return wlnk_fail(rc, e, msg);
}

/**
 * @brief Pick a name for a new monitor interface on the claimed radio.
 *
 * The radio's index keeps two radios' names apart, and a slot two
 * interfaces of one radio; the first slot no interface uses is taken.
 *
 * @param c The claim.
 * @param out Receives the name, WLNK_IFNAME_MAX long.
 * @return WLNK_OK; WLNK_E_VIF_LIMIT when every slot is in use.
 */
static int vif_name(const wlnk_claim_t *c, char *out)
{
    for (unsigned slot = 0; slot < VIF_MAX; slot++) {
        int n =
            snprintf(out, WLNK_IFNAME_MAX, "wlnk%dm%u", c->radio.wiphy, slot);
        if (n < 0 || n >= WLNK_IFNAME_MAX)
            break;
        if (if_nametoindex(out) == 0)
            return wlnk_ok();
    }
    return wlnk_fail(WLNK_E_VIF_LIMIT, 0,
                     "monitor: no free interface name for this radio");
}

int wlnk_claim(const char *ifname, wlnk_claim_t **h)
{
    if (h == NULL)
        return wlnk_fail(WLNK_E_INVALID_ARG, 0, "claim: no output");
    *h = NULL;
    wlnk_claim_t *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return wlnk_fail(WLNK_E_NO_MEMORY, 0, "claim: out of memory");
    c->lock_fd = -1;

    int rc = wlnk_radio_resolve(ifname, &c->radio);
    if (rc == WLNK_OK)
        rc = wlnk_nl_open(&c->nl);
    if (rc == WLNK_OK)
        rc = wlnk_lock_take(&c->radio, &c->lock_fd);
    /* An earlier claim's changes are never undone here: that is the
     * caller's decision, through wlnk_recover(). */
    if (rc == WLNK_OK)
        rc = wlnk_journal_check(&c->radio);
    if (rc == WLNK_OK)
        rc = wlnk_rfkill_check(&c->radio);
    if (rc == WLNK_OK)
        rc = wlnk_journal_start(&c->radio, &c->journal);
    if (rc == WLNK_OK) {
        rc = links_down(c);
        if (rc != WLNK_OK)
            rc = unwind(c, rc);
    }
    if (rc != WLNK_OK) {
        wlnk_lock_drop(&c->lock_fd);
        wlnk_nl_close(&c->nl);
        free(c);
        return rc;
    }
    *h = c;
    return wlnk_ok();
}

int wlnk_release(wlnk_claim_t **h)
{
    if (h == NULL || *h == NULL)
        return wlnk_fail(WLNK_E_INVALID_ARG, 0, "release: no claim");
    wlnk_claim_t *c = *h;
    *h = NULL;
    wlnk_journal_close(&c->journal);
    int rc = wlnk_journal_undo(&c->radio, undo_step, c->nl);
    wlnk_lock_drop(&c->lock_fd);
    wlnk_nl_close(&c->nl);
    free(c);
    return rc;
}

int wlnk_recover(const char *ifname)
{
    wlnk_radio_t r;
    int rc = wlnk_radio_resolve(ifname, &r);
    if (rc != WLNK_OK)
        return rc;
    /* A live claim's radio is never touched. */
    int lock = -1;
    rc = wlnk_lock_take(&r, &lock);
    if (rc != WLNK_OK)
        return rc;
    wlnk_nl_t *nl = NULL;
    rc = wlnk_nl_open(&nl);
    if (rc == WLNK_OK)
        rc = wlnk_journal_undo(&r, undo_step, nl);
    wlnk_nl_close(&nl);
    wlnk_lock_drop(&lock);
    return rc;
}

int wlnk_claim_monitor(wlnk_claim_t *h, wlnk_vif_t **vif)
{
    if (vif != NULL)
        *vif = NULL;
    if (h == NULL || vif == NULL)
        return wlnk_fail(WLNK_E_INVALID_ARG, 0, "monitor: no claim");
    if (h->n_vifs == VIF_MAX)
        return wlnk_fail(WLNK_E_VIF_LIMIT, 0,
                         "monitor: no room for another in this claim");

    struct wlnk_vif *v = &h->vifs[h->n_vifs];
    int rc = vif_name(h, v->ifname);
    /* Recorded first: one never created undoes to nothing. */
    if (rc == WLNK_OK)
        rc = wlnk_journal_step(&h->journal, STEP_VIF, v->ifname);
    if (rc == WLNK_OK)
        rc = wlnk_vif_add_monitor(h->nl, h->radio.wiphy, v->ifname);
    if (rc != WLNK_OK)
        return rc;
    h->n_vifs++;
    *vif = v;
    return wlnk_ok();
}

const char *wlnk_vif_ifname(const wlnk_vif_t *vif)
{
    return vif != NULL ? vif->ifname : NULL;
}
