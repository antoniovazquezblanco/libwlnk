/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * claim.c
 * Exclusive claims on radios.
 *
 * A claim resolves the radio, takes its lock, refuses to touch anything
 * while an earlier claim's record is there, checks rfkill, then takes down
 * every non-monitor link on the radio, each recorded before it is applied.
 * Release and recover are the same undo of the record.
 */

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
#include "radio.h"

/** Step: a link taken down. Its argument is the interface. */
#define STEP_LINK_DOWN "link-down"

/** A claim. */
struct wlnk_claim {
    /** The radio claimed. */
    wlnk_radio_t radio;
    /** Its lock, held for as long as the claim. */
    int lock_fd;
    /** Its record, open for as long as the claim. */
    wlnk_journal_t journal;
};

/**
 * @brief Undo one recorded step of a claim.
 * @param user Unused.
 * @param kind What the step did.
 * @param arg What it did it to.
 * @return WLNK_OK when undone; WLNK_E_UNSUPPORTED for a step this build
 * does not know, which stays recorded.
 */
static int undo_step(void *user, const char *kind, const char *arg)
{
    (void)user;
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
    (void)wlnk_journal_undo(&c->radio, undo_step, NULL);
    return wlnk_fail(rc, e, msg);
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
    int rc = wlnk_journal_undo(&c->radio, undo_step, NULL);
    wlnk_lock_drop(&c->lock_fd);
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
    rc = wlnk_journal_undo(&r, undo_step, NULL);
    wlnk_lock_drop(&lock);
    return rc;
}
