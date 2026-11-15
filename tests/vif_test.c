/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * vif_test.c
 * Check monitor interface creation, deletion and tuning.
 *
 * Internal test. Interfaces are only ever created and tuned on the radio
 * behind WLNK_TEST_IFACE (CI's simulated radio): without privileges that
 * must be refused, as root it must work and is undone.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

#include <errno.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include "err.h"
#include "iface.h"

#include "link.h"
#include "nl.h"
#include "vif.h"

/** An interface name no machine has. */
#define ABSENT  "wlnk-absent0"
/** The monitor interface the tests create. */
#define MONITOR "wlnkmon0"

/** Missing and malformed names are refused before anything is asked. */
static void test_needs_a_name(void **state)
{
    (void)state;

    assert_int_equal(wlnk_vif_add_monitor(NULL, 0, NULL), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_vif_add_monitor(NULL, 0, ""), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_vif_add_monitor(NULL, 0, "wlnk-0123456789ab"),
                     WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_vif_del(NULL, NULL), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_vif_del(NULL, ""), WLNK_E_INVALID_ARG);
}

/** Deleting an interface that is not there succeeds, so undo converges. */
static void test_deleting_the_absent_converges(void **state)
{
    (void)state;

    assert_int_equal(wlnk_vif_del(NULL, ABSENT), WLNK_OK);
    assert_string_equal(wlnk_err_msg(), "");
}

/**
 * @brief The radio the tests may create on, or skip the test.
 * @param nl Receives an open handle.
 * @return The radio behind WLNK_TEST_IFACE.
 */
static int32_t test_radio_or_skip(wlnk_nl_t **nl)
{
    const char *ifname = getenv("WLNK_TEST_IFACE");
    if (ifname == NULL || ifname[0] == '\0')
        skip();
    wlnk_iface_t d;
    if (wlnk_iface_info(ifname, &d) != WLNK_OK)
        fail_msg("%s: %s", ifname, wlnk_err_msg());
    assert_int_equal(wlnk_nl_open(nl), WLNK_OK);
    return d.wiphy;
}

/** Without privileges creation is refused and leaves nothing behind. */
static void test_create_needs_privileges(void **state)
{
    (void)state;
    if (geteuid() == 0)
        skip();
    wlnk_nl_t *nl = NULL;
    int32_t wiphy = test_radio_or_skip(&nl);

    assert_int_equal(wlnk_vif_add_monitor(nl, wiphy, MONITOR), WLNK_E_PERM);
    assert_int_equal(if_nametoindex(MONITOR), 0);
    wlnk_nl_close(&nl);
}

/** As root a monitor interface is created on the radio, up, and deleted
 * again; deleting it twice still succeeds. */
static void test_create_and_delete_as_root(void **state)
{
    (void)state;
    if (geteuid() != 0)
        skip();
    wlnk_nl_t *nl = NULL;
    int32_t wiphy = test_radio_or_skip(&nl);
    assert_int_equal(wlnk_vif_del(nl, MONITOR), WLNK_OK); /* earlier runs */

    int rc = wlnk_vif_add_monitor(nl, wiphy, MONITOR);
    if (rc != WLNK_OK)
        fail_msg("%s: %s", wlnk_strerr(rc), wlnk_err_msg());
    bool up = false;
    assert_int_equal(wlnk_if_is_up(MONITOR, &up), WLNK_OK);
    assert_true(up);
    wlnk_iface_t d;
    assert_int_equal(wlnk_iface_info(MONITOR, &d), WLNK_OK);
    assert_int_equal(d.kind, WLNK_IF_MONITOR);
    assert_int_equal(d.wiphy, wiphy);

    assert_int_equal(wlnk_vif_del(nl, MONITOR), WLNK_OK);
    assert_int_equal(if_nametoindex(MONITOR), 0);
    assert_int_equal(wlnk_vif_del(nl, MONITOR), WLNK_OK);
    wlnk_nl_close(&nl);
}

/** Missing arguments, an ambiguous channel and an absent interface are
 * refused before nl80211 is asked. */
static void test_tune_needs_arguments(void **state)
{
    (void)state;
    wlnk_chandef_t cd = {.freq_mhz = 2412};
    wlnk_chandef_t ambiguous = {.freq_mhz = 2437, .width = WLNK_CHW_40};

    assert_int_equal(wlnk_vif_set_channel(NULL, NULL, &cd), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_vif_set_channel(NULL, MONITOR, NULL),
                     WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_vif_set_channel(NULL, MONITOR, &ambiguous),
                     WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_vif_set_channel(NULL, ABSENT, &cd), WLNK_E_NOT_FOUND);
}

/** Without privileges tuning is refused. */
static void test_tune_needs_privileges(void **state)
{
    (void)state;
    if (geteuid() == 0)
        skip();
    wlnk_nl_t *nl = NULL;
    (void)test_radio_or_skip(&nl);
    wlnk_chandef_t cd = {.freq_mhz = 2412};

    assert_int_equal(wlnk_vif_set_channel(nl, getenv("WLNK_TEST_IFACE"), &cd),
                     WLNK_E_PERM);
    wlnk_nl_close(&nl);
}

/**
 * @brief The first frequencies of a radio that may be tuned to freely.
 * @param ifname Interface on the radio.
 * @param out Receives them.
 * @param max Room in @p out.
 * @return How many were found.
 */
static size_t usable_freqs(const char *ifname, uint32_t *out, size_t max)
{
    wlnk_freq_t *freqs = NULL;
    size_t n = 0;
    size_t found = 0;

    assert_int_equal(wlnk_freq_list(ifname, &freqs, &n), WLNK_OK);
    for (size_t i = 0; i < n && found < max; i++)
        if ((freqs[i].flags &
             (WLNK_FREQ_DISABLED | WLNK_FREQ_NO_IR | WLNK_FREQ_RADAR)) == 0)
            out[found++] = freqs[i].freq_mhz;
    wlnk_freq_free(freqs);
    return found;
}

/** As root a monitor interface is tuned and the kernel reports where it
 * landed; a frequency the radio lacks is refused; and a kernel that refuses
 * to tune while another interface on the radio is up says it is busy. */
static void test_tune_as_root(void **state)
{
    (void)state;
    if (geteuid() != 0)
        skip();
    wlnk_nl_t *nl = NULL;
    int32_t wiphy = test_radio_or_skip(&nl);
    const char *other = getenv("WLNK_TEST_IFACE");
    bool was_up = false;

    assert_int_equal(wlnk_if_is_up(other, &was_up), WLNK_OK);
    assert_int_equal(wlnk_if_set_up(other, false), WLNK_OK);
    assert_int_equal(wlnk_vif_del(nl, MONITOR), WLNK_OK); /* earlier runs */
    assert_int_equal(wlnk_vif_add_monitor(nl, wiphy, MONITOR), WLNK_OK);

    uint32_t freqs[2];
    size_t n = usable_freqs(other, freqs, 2);
    assert_true(n > 0);
    for (size_t i = 0; i < n; i++) {
        wlnk_chandef_t cd = {.freq_mhz = freqs[i]};
        int rc = wlnk_vif_set_channel(nl, MONITOR, &cd);
        if (rc != WLNK_OK)
            fail_msg("%u MHz: %s: %s", (unsigned)freqs[i], wlnk_strerr(rc),
                     wlnk_err_msg());
        wlnk_iface_t d;
        assert_int_equal(wlnk_iface_info(MONITOR, &d), WLNK_OK);
        assert_int_equal(d.chan.freq_mhz, freqs[i]);
    }

    wlnk_chandef_t nowhere = {.freq_mhz = 1};
    assert_int_not_equal(wlnk_vif_set_channel(nl, MONITOR, &nowhere), WLNK_OK);

    /* cfg80211 tunes a monitor only while the radio has nothing else up. */
    wlnk_chandef_t cd = {.freq_mhz = freqs[0]};
    assert_int_equal(wlnk_if_set_up(other, true), WLNK_OK);
    int rc = wlnk_vif_set_channel(nl, MONITOR, &cd);
    if (rc != WLNK_OK) {
        assert_int_equal(rc, WLNK_E_BUSY);
        assert_int_equal(wlnk_err_syserrno(), EBUSY);
    }

    assert_int_equal(wlnk_if_set_up(other, was_up), WLNK_OK);
    assert_int_equal(wlnk_vif_del(nl, MONITOR), WLNK_OK);
    wlnk_nl_close(&nl);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_needs_a_name),
        cmocka_unit_test(test_deleting_the_absent_converges),
        cmocka_unit_test(test_create_needs_privileges),
        cmocka_unit_test(test_create_and_delete_as_root),
        cmocka_unit_test(test_tune_needs_arguments),
        cmocka_unit_test(test_tune_needs_privileges),
        cmocka_unit_test(test_tune_as_root),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
