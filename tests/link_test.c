/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * link_test.c
 * Check interface link state.
 *
 * Internal test. Reading needs no privileges. Changing state is only ever
 * tried on the interface WLNK_TEST_IFACE names (CI's simulated radio),
 * never on one the machine relies on: without privileges it must be
 * refused, as root it must work and is undone.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "err.h"

#include "link.h"

/** An interface name no machine has. */
#define ABSENT "wlnk-absent0"

/** Missing and malformed arguments are refused. */
static void test_needs_arguments(void **state)
{
    (void)state;
    bool up = true;

    assert_int_equal(wlnk_if_is_up(NULL, &up), WLNK_E_INVALID_ARG);
    assert_false(up);
    assert_int_equal(wlnk_if_is_up("lo", NULL), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_if_is_up("", &up), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_if_set_up(NULL, true), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_if_set_up("", true), WLNK_E_INVALID_ARG);
}

/** A name too long for the kernel is refused rather than cut short, which
 * could name another interface. */
static void test_long_name_is_refused(void **state)
{
    (void)state;
    bool up;

    assert_int_equal(wlnk_if_is_up("wlnk-0123456789ab", &up),
                     WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_if_set_up("wlnk-0123456789ab", true),
                     WLNK_E_INVALID_ARG);
}

/** An interface no machine has is not found, with the kernel's errno. */
static void test_unknown_interface(void **state)
{
    (void)state;
    bool up = true;

    assert_int_equal(wlnk_if_is_up(ABSENT, &up), WLNK_E_NOT_FOUND);
    assert_false(up);
    assert_int_equal(wlnk_err_syserrno(), ENODEV);
    assert_int_equal(wlnk_if_set_up(ABSENT, true), WLNK_E_NOT_FOUND);
}

/** The loopback interface is up on any running Linux machine. */
static void test_loopback_is_up(void **state)
{
    (void)state;
    bool up = false;

    assert_int_equal(wlnk_if_is_up("lo", &up), WLNK_OK);
    assert_true(up);
    assert_string_equal(wlnk_err_msg(), "");
}

/**
 * @brief The interface the tests may change, or skip the test.
 * @return Its name.
 */
static const char *test_iface_or_skip(void)
{
    const char *ifname = getenv("WLNK_TEST_IFACE");
    if (ifname == NULL || ifname[0] == '\0')
        skip();
    return ifname;
}

/** Without privileges a change is refused and the state stays as it was. */
static void test_change_needs_privileges(void **state)
{
    (void)state;
    const char *ifname = test_iface_or_skip();
    if (geteuid() == 0)
        skip();
    bool was, now;

    assert_int_equal(wlnk_if_is_up(ifname, &was), WLNK_OK);
    int rc = wlnk_if_set_up(ifname, !was);
    if (rc == WLNK_OK) /* privileged after all: put it back first */
        (void)wlnk_if_set_up(ifname, was);
    assert_int_equal(rc, WLNK_E_PERM);
    assert_true(strstr(wlnk_err_msg(), "CAP_NET_ADMIN") != NULL);
    assert_int_equal(wlnk_if_is_up(ifname, &now), WLNK_OK);
    assert_int_equal(now, was);
}

/** As root the link goes down and up, each change visible at once, and the
 * interface is left as it was found. */
static void test_change_as_root(void **state)
{
    (void)state;
    const char *ifname = test_iface_or_skip();
    if (geteuid() != 0)
        skip();
    bool was, now;

    assert_int_equal(wlnk_if_is_up(ifname, &was), WLNK_OK);
    assert_int_equal(wlnk_if_set_up(ifname, false), WLNK_OK);
    assert_int_equal(wlnk_if_is_up(ifname, &now), WLNK_OK);
    assert_false(now);
    assert_int_equal(wlnk_if_set_up(ifname, false), WLNK_OK); /* no-op */
    assert_int_equal(wlnk_if_set_up(ifname, true), WLNK_OK);
    assert_int_equal(wlnk_if_is_up(ifname, &now), WLNK_OK);
    assert_true(now);
    assert_int_equal(wlnk_if_set_up(ifname, was), WLNK_OK);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_needs_arguments),
        cmocka_unit_test(test_long_name_is_refused),
        cmocka_unit_test(test_unknown_interface),
        cmocka_unit_test(test_loopback_is_up),
        cmocka_unit_test(test_change_needs_privileges),
        cmocka_unit_test(test_change_as_root),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
