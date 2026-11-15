/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * err_test.c
 * Check the wlnk_strerr contract.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

#include <string.h>
#include "err.h"

#define FALLBACK "unknown status"

/** Every documented code. They are contiguous from WLNK_OK (0), which
 * test_list_is_complete relies on. */
static const int codes[] = {
    WLNK_OK,
    WLNK_E_PERM,
    WLNK_E_BUSY,
    WLNK_E_UNSUPPORTED,
    WLNK_E_INVALID_ARG,
    WLNK_E_NO_MEMORY,
    WLNK_E_NOT_FOUND,
    WLNK_E_IO,
    WLNK_E_TIMEOUT,
    WLNK_E_RF_KILL,
    WLNK_E_VIF_LIMIT,
    WLNK_E_STALE_STATE,
    WLNK_E_INTERRUPTED,
};
#define N_CODES (sizeof(codes) / sizeof(codes[0]))

/** WLNK_OK is 0, as documented in err.h. */
static void test_ok_is_zero(void **state)
{
    (void)state;
    assert_int_equal(WLNK_OK, 0);
}

/** Every code has a real message, never the fallback. */
static void test_codes_have_messages(void **state)
{
    (void)state;
    for (size_t i = 0; i < N_CODES; i++) {
        const char *s = wlnk_strerr(codes[i]);
        assert_non_null(s);
        assert_true(s[0] != '\0');
        assert_string_not_equal(s, FALLBACK);
    }
}

/** Messages are distinct, so a message identifies its code. */
static void test_messages_are_distinct(void **state)
{
    (void)state;
    for (size_t i = 0; i < N_CODES; i++)
        for (size_t j = i + 1; j < N_CODES; j++)
            assert_string_not_equal(wlnk_strerr(codes[i]),
                                    wlnk_strerr(codes[j]));
}

/** The first value past the list has no message: a new status code fails
 * here until it is added to the list above. */
static void test_list_is_complete(void **state)
{
    (void)state;
    assert_string_equal(wlnk_strerr((int)N_CODES), FALLBACK);
}

/** The messages are part of the contract, so each is pinned: a rewording
 * fails here until it is made on purpose. In the order of codes[]. */
static const char *const texts[] = {
    "success",
    "insufficient privileges",
    "radio is busy",
    "operation not supported",
    "invalid argument",
    "out of memory",
    "not found",
    "I/O error",
    "operation timed out",
    "radio is blocked (rfkill)",
    "no room for another interface on this radio",
    "an earlier claim was left unfinished",
    "interrupted by a signal",
};

static void test_messages_are_pinned(void **state)
{
    (void)state;
    assert_int_equal(sizeof(texts) / sizeof(texts[0]), N_CODES);
    for (size_t i = 0; i < N_CODES; i++)
        assert_string_equal(wlnk_strerr(codes[i]), texts[i]);
}

/** A message is read by the person running the consumer, so it is complete
 * on its own: it never points at a function, a field or "the detail". */
static void test_messages_are_for_people(void **state)
{
    (void)state;
    static const char *const programmer_words[] = {
        "wlnk_", "()", "detail", "syserrno", "holder", "errno", "vif",
    };
    for (size_t i = 0; i < N_CODES; i++)
        for (size_t w = 0; w < sizeof(programmer_words) / sizeof(char *); w++)
            if (strstr(wlnk_strerr(codes[i]), programmer_words[w]) != NULL)
                fail_msg("\"%s\" mentions \"%s\"", wlnk_strerr(codes[i]),
                         programmer_words[w]);
}

/** Unknown values map to the stable fallback; wlnk_strerr never fails. */
static void test_unknown_values(void **state)
{
    (void)state;
    assert_string_equal(wlnk_strerr(-1), FALLBACK);
    assert_string_equal(wlnk_strerr(99999), FALLBACK);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ok_is_zero),
        cmocka_unit_test(test_codes_have_messages),
        cmocka_unit_test(test_messages_are_distinct),
        cmocka_unit_test(test_list_is_complete),
        cmocka_unit_test(test_messages_are_pinned),
        cmocka_unit_test(test_messages_are_for_people),
        cmocka_unit_test(test_unknown_values),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
