/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * nl_test.c
 * Check the libnl error mapper: libnl reports negative NLE_* codes of its
 * own, never errno.
 *
 * Internal test: the mapper is pure, so this needs no socket and no
 * privileges.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

#include <netlink/errno.h>
#include "err.h"

#include "nl.h"

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
 * semantics, so an unmapped code stays a plain I/O failure. */
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

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_failures_map),
        cmocka_unit_test(test_unmapped_failures_are_io),
        cmocka_unit_test(test_non_failures),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
