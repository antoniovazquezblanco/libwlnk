/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * nm_test.c
 * Check reading and changing whether NetworkManager manages an interface.
 *
 * Internal test. Only WLNK_TEST_IFACE is ever changed, and only when
 * WLNK_TEST_NM=1 says NetworkManager manages it (CI's NetworkManager job):
 * without privileges its policy must refuse, as root it must work and is
 * undone.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "err.h"

#include "managers/nm.h"

/** An interface name no machine has. */
#define ABSENT "wlnk-absent0"

/**
 * @brief The interface NetworkManager manages for the tests, or skip the
 * test.
 * @return WLNK_TEST_IFACE.
 */
static const char *test_iface_or_skip(void)
{
    const char *nm = getenv("WLNK_TEST_NM");
    const char *ifname = getenv("WLNK_TEST_IFACE");
    if (nm == NULL || strcmp(nm, "1") != 0 || ifname == NULL ||
        ifname[0] == '\0')
        skip();
    bool managed = false;
    assert_int_equal(wlnk_nm_is_managed(ifname, &managed), WLNK_OK);
    if (!managed)
        fail_msg("%s: not managed by NetworkManager", ifname);
    return ifname;
}

/** Missing names and outputs are refused before anything is asked. */
static void test_needs_arguments(void **state)
{
    (void)state;
    bool managed = true;

    assert_int_equal(wlnk_nm_is_managed(NULL, &managed), WLNK_E_INVALID_ARG);
    assert_false(managed);
    assert_int_equal(wlnk_nm_is_managed("", &managed), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_nm_is_managed(ABSENT, NULL), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_nm_set_managed(NULL, false), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_nm_set_managed("", true), WLNK_E_INVALID_ARG);
}

/** An interface NetworkManager does not know, or a NetworkManager not
 * running, is not managed and has nothing to change. */
static void test_unknown_is_not_managed(void **state)
{
    (void)state;
    bool managed = true;

    assert_int_equal(wlnk_nm_is_managed(ABSENT, &managed), WLNK_OK);
    assert_false(managed);
    assert_string_equal(wlnk_err_msg(), "");
    assert_int_equal(wlnk_nm_set_managed(ABSENT, false), WLNK_OK);
    assert_int_equal(wlnk_nm_set_managed(ABSENT, true), WLNK_OK);
}

/** Without privileges NetworkManager's policy refuses, and the interface
 * stays managed. */
static void test_unmanage_needs_privileges(void **state)
{
    (void)state;
    if (geteuid() == 0)
        skip();
    const char *ifname = test_iface_or_skip();

    assert_int_equal(wlnk_nm_set_managed(ifname, false), WLNK_E_PERM);
    assert_non_null(strstr(wlnk_err_msg(), ifname));
    bool managed = false;
    assert_int_equal(wlnk_nm_is_managed(ifname, &managed), WLNK_OK);
    assert_true(managed);
}

/** As root an interface is left alone, then managed again. */
static void test_unmanage_and_manage_as_root(void **state)
{
    (void)state;
    if (geteuid() != 0)
        skip();
    const char *ifname = test_iface_or_skip();

    assert_int_equal(wlnk_nm_set_managed(ifname, false), WLNK_OK);
    bool managed = true;
    int rc = wlnk_nm_is_managed(ifname, &managed);
    /* Put back before checking, so a failure leaves it managed. */
    assert_int_equal(wlnk_nm_set_managed(ifname, true), WLNK_OK);
    assert_int_equal(rc, WLNK_OK);
    assert_false(managed);
    assert_int_equal(wlnk_nm_is_managed(ifname, &managed), WLNK_OK);
    assert_true(managed);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_needs_arguments),
        cmocka_unit_test(test_unknown_is_not_managed),
        cmocka_unit_test(test_unmanage_needs_privileges),
        cmocka_unit_test(test_unmanage_and_manage_as_root),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
