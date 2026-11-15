/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * version_test.c
 * Check the run-time version against the Meson project version.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

#include "version.h"

/* The project version, passed by tests/meson.build. */
#if !defined(WLNK_TEST_VERSION_STRING) || !defined(WLNK_TEST_VERSION_MAJOR) || \
    !defined(WLNK_TEST_VERSION_MINOR) || !defined(WLNK_TEST_VERSION_PATCH)
#error "WLNK_TEST_VERSION_* must be defined by tests/meson.build"
#endif

static void test_string_is_project_version(void **state)
{
    (void)state;
    assert_string_equal(wlnk_version_string(), WLNK_TEST_VERSION_STRING);
}

/** Layout documented in version.h. */
static void test_number_layout(void **state)
{
    (void)state;
    assert_int_equal(wlnk_version(), (WLNK_TEST_VERSION_MAJOR << 16) |
                                         (WLNK_TEST_VERSION_MINOR << 8) |
                                         WLNK_TEST_VERSION_PATCH);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_string_is_project_version),
        cmocka_unit_test(test_number_layout),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
