/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * nl_test.c
 * Check the netlink transport: the libnl error mapper (libnl reports
 * negative NLE_* codes of its own, never errno), opening the nl80211
 * socket and running requests on it.
 *
 * Internal test. Needs no radio and no privileges; where the kernel has no
 * nl80211 the socket tests report themselves skipped, unless
 * WLNK_TEST_NL80211 is set (CI's simulated radios), which makes them fail.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

#include <errno.h>
#include <netlink/errno.h>
#include <stdlib.h>
#include <string.h>
#include "err.h"

#include "nl.h"

/** An interface index no kernel hands out. */
#define NO_IFINDEX 0x7fffffffu

/** Each documented libnl failure lands on the status the public contract
 * names for it. */
static void test_failures_map(void **state)
{
    (void)state;

    assert_int_equal(wlnk_nle_status(-NLE_INTR), WLNK_E_INTERRUPTED);
    assert_int_equal(wlnk_nle_status(-NLE_NOMEM), WLNK_E_NO_MEMORY);
    assert_int_equal(wlnk_nle_status(-NLE_PERM), WLNK_E_PERM);
    assert_int_equal(wlnk_nle_status(-NLE_NOACCESS), WLNK_E_PERM);
    assert_int_equal(wlnk_nle_status(-NLE_OBJ_NOTFOUND), WLNK_E_NOT_FOUND);
    assert_int_equal(wlnk_nle_status(-NLE_NODEV), WLNK_E_NOT_FOUND);
    assert_int_equal(wlnk_nle_status(-NLE_OPNOTSUPP), WLNK_E_UNSUPPORTED);
    assert_int_equal(wlnk_nle_status(-NLE_INVAL), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_nle_status(-NLE_RANGE), WLNK_E_INVALID_ARG);
}

/** A libnl "object busy" is not the radio-busy status: that one has claim
 * semantics, so an unmapped code stays a plain I/O failure. NLE_AGAIN is
 * retried at the call and is a failure only once the retries run out. */
static void test_unmapped_failures_are_io(void **state)
{
    (void)state;

    assert_int_equal(wlnk_nle_status(-NLE_AGAIN), WLNK_E_IO);
    assert_int_equal(wlnk_nle_status(-NLE_BUSY), WLNK_E_IO);
    assert_int_equal(wlnk_nle_status(-NLE_FAILURE), WLNK_E_IO);
    assert_int_equal(wlnk_nle_status(-NLE_MSG_TRUNC), WLNK_E_IO);
}

/** 0 and the positive byte counts a send returns are not failures. */
static void test_non_failures(void **state)
{
    (void)state;

    assert_int_equal(wlnk_nle_status(0), WLNK_OK);
    assert_int_equal(wlnk_nle_status(1), WLNK_OK);
    assert_int_equal(wlnk_nle_status(4096), WLNK_OK);
}

/** A missing output handle is refused before any socket is opened. */
static void test_open_needs_handle(void **state)
{
    (void)state;

    assert_int_equal(wlnk_nl_open(NULL), WLNK_E_INVALID_ARG);
}

/** Closing a missing or already closed handle is a no-op. */
static void test_close_without_handle(void **state)
{
    (void)state;
    wlnk_nl_t *h = NULL;

    wlnk_nl_close(NULL);
    wlnk_nl_close(&h);
    assert_null(h);
}

/**
 * @brief Open a handle, or skip the test where the kernel has no nl80211.
 *
 * With WLNK_TEST_NL80211 set nl80211 is required, so a missing one fails.
 *
 * @return The open handle.
 */
static wlnk_nl_t *open_or_skip(void)
{
    wlnk_nl_t *h = NULL;
    int rc = wlnk_nl_open(&h);
    if (rc == WLNK_E_NOT_FOUND) {
        assert_null(h);
        if (getenv("WLNK_TEST_NL80211") != NULL)
            fail_msg("WLNK_TEST_NL80211 is set, but %s", wlnk_err_msg());
        skip();
    }
    assert_int_equal(rc, WLNK_OK);
    return h;
}

/** Opening needs no privileges: it finds nl80211 or reports it missing.
 * Closing clears the handle. */
static void test_open_close(void **state)
{
    (void)state;
    wlnk_nl_t *h = open_or_skip();

    assert_non_null(h);
    assert_string_equal(wlnk_err_msg(), "");

    wlnk_nl_close(&h);
    assert_null(h);
}

/** Without a handle there is no message, and running none fails cleanly. */
static void test_run_without_handle(void **state)
{
    (void)state;

    assert_null(wlnk_nl_msg(NULL, NL80211_CMD_GET_PROTOCOL_FEATURES, false));
    assert_int_equal(wlnk_nl_run(NULL, NULL, NULL, NULL, "nothing"),
                     WLNK_E_NO_MEMORY);
}

/**
 * @brief Count replies that carry the protocol features.
 * @param attrs Parsed reply.
 * @param user Counter to increment.
 * @return WLNK_OK, or WLNK_E_IO for a reply without the features.
 */
static int count_features(wlnk_nl_attrs_t attrs, void *user)
{
    int *n = user;
    (*n)++;
    return attrs[NL80211_ATTR_PROTOCOL_FEATURES] != NULL ? WLNK_OK : WLNK_E_IO;
}

/** Each reply reaches the callback: protocol features is one reply, needing
 * no radio and no privileges. */
static void test_run_delivers_replies(void **state)
{
    (void)state;
    wlnk_nl_t *h = open_or_skip();
    int n = 0;

    struct nl_msg *msg =
        wlnk_nl_msg(h, NL80211_CMD_GET_PROTOCOL_FEATURES, false);
    assert_non_null(msg);
    assert_int_equal(
        wlnk_nl_run(h, msg, count_features, &n, "get protocol features"),
        WLNK_OK);
    assert_int_equal(n, 1);
    assert_string_equal(wlnk_err_msg(), "");
    wlnk_nl_close(&h);
}

/**
 * @brief Stop at the first reply with a status of the caller's own.
 * @param attrs Parsed reply, unused.
 * @param user Unused.
 * @return WLNK_E_BUSY.
 */
static int stop_busy(wlnk_nl_attrs_t attrs, void *user)
{
    (void)attrs;
    (void)user;
    return WLNK_E_BUSY;
}

/** A callback that stops ends the exchange with its status. */
static void test_callback_stops_run(void **state)
{
    (void)state;
    wlnk_nl_t *h = open_or_skip();

    struct nl_msg *msg =
        wlnk_nl_msg(h, NL80211_CMD_GET_PROTOCOL_FEATURES, false);
    assert_non_null(msg);
    assert_int_equal(wlnk_nl_run(h, msg, stop_busy, NULL, "stop"), WLNK_E_BUSY);
    wlnk_nl_close(&h);
}

/** A stopped request leaves nothing behind: the kernel's acknowledgement is
 * read, so the next request on the same handle gets its own reply. */
static void test_reusable_after_stop(void **state)
{
    (void)state;
    wlnk_nl_t *h = open_or_skip();
    int n = 0;

    struct nl_msg *msg =
        wlnk_nl_msg(h, NL80211_CMD_GET_PROTOCOL_FEATURES, false);
    assert_int_equal(wlnk_nl_run(h, msg, stop_busy, NULL, "stop"), WLNK_E_BUSY);

    msg = wlnk_nl_msg(h, NL80211_CMD_GET_PROTOCOL_FEATURES, false);
    assert_int_equal(
        wlnk_nl_run(h, msg, count_features, &n, "get protocol features"),
        WLNK_OK);
    assert_int_equal(n, 1);
    wlnk_nl_close(&h);
}

/**
 * @brief Count every reply.
 * @param attrs Parsed reply, unused.
 * @param user Counter to increment.
 * @return WLNK_OK.
 */
static int count_all(wlnk_nl_attrs_t attrs, void *user)
{
    (void)attrs;
    int *n = user;
    (*n)++;
    return WLNK_OK;
}

/**
 * @brief Dump every radio, counting the replies.
 * @param h Open handle.
 * @param cb Reply callback.
 * @param n Counter for @p cb.
 * @return wlnk_nl_run()'s result.
 */
static int dump_wiphys(wlnk_nl_t *h, wlnk_nl_cb cb, int *n)
{
    struct nl_msg *msg = wlnk_nl_msg(h, NL80211_CMD_GET_WIPHY, true);
    return wlnk_nl_run(h, msg, cb, n, "get wiphy");
}

/** A dump stopped at its first reply drops the rest, so the same handle
 * dumps every radio again. Needs two radios (CI's simulated ones) and skips
 * without them. */
static void test_dump_reusable_after_stop(void **state)
{
    (void)state;
    wlnk_nl_t *h = open_or_skip();
    int all = 0;
    int again = 0;

    assert_int_equal(dump_wiphys(h, count_all, &all), WLNK_OK);
    if (all < 2) {
        wlnk_nl_close(&h);
        skip();
    }
    assert_int_equal(dump_wiphys(h, stop_busy, NULL), WLNK_E_BUSY);
    assert_int_equal(dump_wiphys(h, count_all, &again), WLNK_OK);
    assert_int_equal(again, all);
    wlnk_nl_close(&h);
}

/** A kernel refusal keeps its errno: an interface index that does not
 * exist is ENODEV, reported as not found. */
static void test_kernel_refusal(void **state)
{
    (void)state;
    wlnk_nl_t *h = open_or_skip();

    struct nl_msg *msg = wlnk_nl_msg(h, NL80211_CMD_GET_INTERFACE, false);
    assert_non_null(msg);
    assert_int_equal(nla_put_u32(msg, NL80211_ATTR_IFINDEX, NO_IFINDEX), 0);
    assert_int_equal(wlnk_nl_run(h, msg, NULL, NULL, "get interface"),
                     WLNK_E_NOT_FOUND);
    assert_int_equal(wlnk_err_syserrno(), ENODEV);
    assert_int_equal(strncmp(wlnk_err_msg(), "nl80211 get interface: ", 23), 0);
    wlnk_nl_close(&h);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_failures_map),
        cmocka_unit_test(test_unmapped_failures_are_io),
        cmocka_unit_test(test_non_failures),
        cmocka_unit_test(test_open_needs_handle),
        cmocka_unit_test(test_close_without_handle),
        cmocka_unit_test(test_open_close),
        cmocka_unit_test(test_run_without_handle),
        cmocka_unit_test(test_run_delivers_replies),
        cmocka_unit_test(test_callback_stops_run),
        cmocka_unit_test(test_kernel_refusal),
        cmocka_unit_test(test_reusable_after_stop),
        cmocka_unit_test(test_dump_reusable_after_stop),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
