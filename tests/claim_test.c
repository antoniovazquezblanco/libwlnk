/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * claim_test.c
 * Check claiming a radio, releasing it, and recovering a claim that was
 * never released.
 *
 * Internal test. The run directory is pointed at WLNK_RUNDIR, a directory
 * in the build tree, so the real one is never touched. Only the radio
 * behind WLNK_TEST_IFACE (CI's simulated radio) is ever claimed: without
 * privileges that must change nothing, as root it must work and is undone.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "claim.h"
#include "err.h"
#include "iface.h"

#include "link.h"
#include "nl.h"
#include "radio.h"
#include "vif.h"

#ifndef WLNK_RUNDIR
#error "WLNK_RUNDIR must point the test at its own run directory"
#endif

/* Root of sysfs, as radio.c reads it. */
#ifndef WLNK_SYSFS
#define WLNK_SYSFS "/sys"
#endif

/** Name of the monitor interface the tests create. */
#define MONITOR "wlnkclaim0"

/**
 * @brief The interface whose radio the tests may claim, or skip the test.
 * @return WLNK_TEST_IFACE.
 */
static const char *test_iface_or_skip(void)
{
    const char *ifname = getenv("WLNK_TEST_IFACE");
    if (ifname == NULL || ifname[0] == '\0')
        skip();
    return ifname;
}

/**
 * @brief Whether an interface's link is up.
 * @param ifname The interface.
 * @return True when up.
 */
static bool is_up(const char *ifname)
{
    bool up = false;
    if (wlnk_if_is_up(ifname, &up) != WLNK_OK)
        fail_msg("%s: %s", ifname, wlnk_err_msg());
    return up;
}

/** Missing arguments are refused, and leave no claim. */
static void test_needs_arguments(void **state)
{
    (void)state;
    static int dummy;
    wlnk_claim_t *h = (wlnk_claim_t *)&dummy;

    assert_int_equal(wlnk_claim(NULL, &h), WLNK_E_INVALID_ARG);
    assert_null(h);
    assert_int_equal(wlnk_claim("wlan0", NULL), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_release(NULL), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_release(&h), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_recover(NULL), WLNK_E_INVALID_ARG);
}

/** Only a wireless interface has a radio to claim or recover. */
static void test_not_a_radio(void **state)
{
    (void)state;
    wlnk_claim_t *h = NULL;

    assert_int_equal(wlnk_claim("wlnk-absent0", &h), WLNK_E_NOT_FOUND);
    assert_null(h);
    assert_int_equal(wlnk_recover("wlnk-absent0"), WLNK_E_NOT_FOUND);
}

/** Without privileges a claim that would take a link down is refused and
 * leaves nothing to recover; one with nothing to change needs none. */
static void test_unprivileged_claim(void **state)
{
    (void)state;
    if (geteuid() == 0)
        skip();
    const char *ifname = test_iface_or_skip();
    wlnk_claim_t *h = NULL;
    bool up = is_up(ifname);

    for (int i = 0; i < 2; i++) { /* the second finds no record left */
        int rc = wlnk_claim(ifname, &h);
        if (up) {
            assert_int_equal(rc, WLNK_E_PERM);
            assert_null(h);
            /* The claim's failure, not its unwinding's. */
            assert_non_null(strstr(wlnk_err_msg(), "CAP_NET_ADMIN"));
        } else {
            assert_int_equal(rc, WLNK_OK);
            assert_int_equal(wlnk_release(&h), WLNK_OK);
        }
        assert_int_equal(is_up(ifname), up);
    }
}

/** A claim takes the radio's links down and holds the radio until it is
 * released, which brings them back up. */
static void test_claim_and_release(void **state)
{
    (void)state;
    if (geteuid() != 0)
        skip();
    const char *ifname = test_iface_or_skip();
    bool was_up = is_up(ifname);
    assert_int_equal(wlnk_if_set_up(ifname, true), WLNK_OK);

    wlnk_claim_t *h = NULL;
    assert_int_equal(wlnk_claim(ifname, &h), WLNK_OK);
    assert_non_null(h);
    assert_false(is_up(ifname));

    /* Held: neither a second claim nor a recovery touches it. */
    wlnk_claim_t *other = NULL;
    assert_int_equal(wlnk_claim(ifname, &other), WLNK_E_BUSY);
    assert_null(other);
    assert_int_equal(wlnk_recover(ifname), WLNK_E_BUSY);
    assert_false(is_up(ifname));

    assert_int_equal(wlnk_release(&h), WLNK_OK);
    assert_null(h);
    assert_true(is_up(ifname));
    assert_int_equal(wlnk_recover(ifname), WLNK_OK); /* nothing left */

    assert_int_equal(wlnk_if_set_up(ifname, was_up), WLNK_OK);
}

/**
 * @brief A non-monitor interface on another radio than the test's.
 * @param wiphy The test's radio.
 * @param out Receives its name.
 * @return Whether there is one.
 */
static bool other_radio_iface(int32_t wiphy, char *out)
{
    wlnk_iface_t *ifaces = NULL;
    size_t n = 0;
    assert_int_equal(wlnk_iface_list(&ifaces, &n), WLNK_OK);
    bool found = false;
    for (size_t i = 0; !found && i < n; i++) {
        found = ifaces[i].wiphy != wiphy && ifaces[i].kind != WLNK_IF_MONITOR;
        if (found)
            memcpy(out, ifaces[i].ifname, WLNK_IFNAME_MAX);
    }
    wlnk_iface_free(ifaces);
    return found;
}

/** Links found down, monitor interfaces and other radios are left as they
 * are. */
static void test_claim_leaves_the_rest(void **state)
{
    (void)state;
    if (geteuid() != 0)
        skip();
    const char *ifname = test_iface_or_skip();
    bool was_up = is_up(ifname);
    wlnk_iface_t d;
    assert_int_equal(wlnk_iface_info(ifname, &d), WLNK_OK);
    wlnk_nl_t *nl = NULL;
    assert_int_equal(wlnk_nl_open(&nl), WLNK_OK);
    assert_int_equal(wlnk_vif_add_monitor(nl, d.wiphy, MONITOR), WLNK_OK);
    assert_int_equal(wlnk_if_set_up(ifname, false), WLNK_OK);
    char other[WLNK_IFNAME_MAX] = "";
    bool other_was_up = false;
    if (other_radio_iface(d.wiphy, other)) {
        other_was_up = is_up(other);
        assert_int_equal(wlnk_if_set_up(other, true), WLNK_OK);
    }

    wlnk_claim_t *h = NULL;
    assert_int_equal(wlnk_claim(ifname, &h), WLNK_OK);
    assert_true(is_up(MONITOR));
    if (other[0] != '\0')
        assert_true(is_up(other));
    assert_int_equal(wlnk_release(&h), WLNK_OK);
    assert_false(is_up(ifname));
    assert_true(is_up(MONITOR));

    assert_int_equal(wlnk_vif_del(nl, MONITOR), WLNK_OK);
    wlnk_nl_close(&nl);
    assert_int_equal(wlnk_if_set_up(ifname, was_up), WLNK_OK);
    if (other[0] != '\0')
        assert_int_equal(wlnk_if_set_up(other, other_was_up), WLNK_OK);
}

/** The claim of a process that died is reported, never claimed over, until
 * it is recovered. */
static void test_dead_claim_is_recovered(void **state)
{
    (void)state;
    if (geteuid() != 0)
        skip();
    const char *ifname = test_iface_or_skip();
    bool was_up = is_up(ifname);
    assert_int_equal(wlnk_if_set_up(ifname, true), WLNK_OK);

    pid_t child = fork();
    assert_true(child >= 0);
    if (child == 0) {
        wlnk_claim_t *h = NULL;
        _exit(wlnk_claim(ifname, &h) == WLNK_OK ? 0 : 1); /* never released */
    }
    int status = 0;
    assert_int_equal(waitpid(child, &status, 0), child);
    assert_true(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    assert_false(is_up(ifname));

    wlnk_claim_t *h = NULL;
    assert_int_equal(wlnk_claim(ifname, &h), WLNK_E_STALE_STATE);
    assert_null(h);
    char pid[32];
    snprintf(pid, sizeof(pid), "pid %ld ", (long)child);
    assert_non_null(strstr(wlnk_err_msg(), pid));
    assert_false(is_up(ifname));

    assert_int_equal(wlnk_recover(ifname), WLNK_OK);
    assert_true(is_up(ifname));
    assert_int_equal(wlnk_claim(ifname, &h), WLNK_OK);
    assert_int_equal(wlnk_release(&h), WLNK_OK);

    assert_int_equal(wlnk_if_set_up(ifname, was_up), WLNK_OK);
}

/**
 * @brief Soft block or unblock a radio through its rfkill switch.
 * @param phy The radio's phy.
 * @param block Whether to block it.
 * @return Whether it has a switch to set.
 */
static bool soft_block(const char *phy, bool block)
{
    char dir[256];
    snprintf(dir, sizeof(dir), "%s/class/ieee80211/%s", WLNK_SYSFS, phy);
    DIR *d = opendir(dir);
    if (d == NULL)
        return false;
    char path[512] = "";
    struct dirent *e;
    while (path[0] == '\0' && (e = readdir(d)) != NULL)
        if (strncmp(e->d_name, "rfkill", 6) == 0)
            snprintf(path, sizeof(path), "%s/%.32s/soft", dir, e->d_name);
    closedir(d);
    FILE *fp = path[0] != '\0' ? fopen(path, "w") : NULL;
    if (fp == NULL)
        return false;
    fputs(block ? "1\n" : "0\n", fp);
    fclose(fp);
    return true;
}

/** A radio blocked by rfkill is refused, and left as it is. */
static void test_blocked_radio_is_refused(void **state)
{
    (void)state;
    if (geteuid() != 0)
        skip();
    const char *ifname = test_iface_or_skip();
    bool was_up = is_up(ifname);
    wlnk_radio_t r;
    assert_int_equal(wlnk_radio_resolve(ifname, &r), WLNK_OK);
    if (!soft_block(r.phy, true))
        skip(); /* no rfkill switch */

    wlnk_claim_t *h = NULL;
    int rc = wlnk_claim(ifname, &h);
    soft_block(r.phy, false);
    assert_int_equal(rc, WLNK_E_RF_KILL);
    assert_null(h);
    assert_int_equal(wlnk_recover(ifname), WLNK_OK); /* nothing recorded */

    /* A block takes the radio's links down, and lifting it does not bring
     * them back. */
    assert_int_equal(wlnk_if_set_up(ifname, was_up), WLNK_OK);
}

/**
 * @brief Write the record of the test interface's radio by hand.
 * @param path Receives its path.
 * @param cap Size of @p path.
 * @param text Its contents.
 */
static void put_record(char *path, size_t cap, const char *text)
{
    wlnk_radio_t r;
    assert_int_equal(wlnk_radio_resolve(test_iface_or_skip(), &r), WLNK_OK);
    snprintf(path, cap, "%s/%s.journal", WLNK_RUNDIR, r.id);
    assert_true(mkdir(WLNK_RUNDIR, 0755) == 0 || errno == EEXIST);
    FILE *fp = fopen(path, "w");
    assert_non_null(fp);
    fputs(text, fp);
    fclose(fp);
}

/** Recovery counts an interface gone since as restored, and keeps a step
 * this build does not know recorded, so the radio stays unclaimable. */
static void test_recover_odd_records(void **state)
{
    (void)state;
    const char *ifname = test_iface_or_skip();
    char path[256];

    put_record(path, sizeof(path), "wlnk1 pid=1\nlink-down wlnk-gone0\n");
    assert_int_equal(wlnk_recover(ifname), WLNK_OK);
    assert_int_equal(access(path, F_OK), -1);

    put_record(path, sizeof(path), "wlnk1 pid=1\nfrob x\n");
    assert_int_equal(wlnk_recover(ifname), WLNK_E_UNSUPPORTED);
    wlnk_claim_t *h = NULL;
    assert_int_equal(wlnk_claim(ifname, &h), WLNK_E_STALE_STATE);
    assert_int_equal(unlink(path), 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_needs_arguments),
        cmocka_unit_test(test_not_a_radio),
        cmocka_unit_test(test_unprivileged_claim),
        cmocka_unit_test(test_claim_and_release),
        cmocka_unit_test(test_claim_leaves_the_rest),
        cmocka_unit_test(test_dead_claim_is_recovered),
        cmocka_unit_test(test_blocked_radio_is_refused),
        cmocka_unit_test(test_recover_odd_records),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
