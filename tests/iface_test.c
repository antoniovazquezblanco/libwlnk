/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * iface_test.c
 * The display-name helpers of iface.h.
 *
 * The names are part of the contract, so each one is pinned here: a
 * reworded name fails until this table is updated on purpose. The first
 * value past each table must be "unknown", so a value added to a header
 * without a name fails too.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdbool.h>
#include "iface.h"

#define UNKNOWN "unknown"
#define N(a)    (sizeof(a) / sizeof((a)[0]))

typedef const char *(*namer_t)(uint32_t);

struct named {
    uint32_t value;
    const char *name;
};

/* Each table holds every value its header defines, in order: enum values
 * from 0, or mask bits from bit 0. Either way the next value is the first
 * one a header could add. */
static const struct named kinds[] = {
    {WLNK_IF_STATION, "station"}, {WLNK_IF_AP, "AP"},
    {WLNK_IF_MONITOR, "monitor"}, {WLNK_IF_P2P, "P2P"},
    {WLNK_IF_MESH, "mesh"},       {WLNK_IF_IBSS, "IBSS"},
    {WLNK_IF_OTHER, "other"},
};

static const struct named widths[] = {
    {WLNK_CHW_20_NOHT, "20 MHz (no HT)"},
    {WLNK_CHW_20, "20 MHz"},
    {WLNK_CHW_40, "40 MHz"},
    {WLNK_CHW_80, "80 MHz"},
    {WLNK_CHW_80P80, "80+80 MHz"},
    {WLNK_CHW_160, "160 MHz"},
    {WLNK_CHW_320, "320 MHz"},
    {WLNK_CHW_5, "5 MHz"},
    {WLNK_CHW_10, "10 MHz"},
};

static const struct named caps[] = {
    {WLNK_CAP_MONITOR, "monitor"},   {WLNK_CAP_MONITOR_TX, "monitor TX"},
    {WLNK_CAP_SCAN, "scan"},         {WLNK_CAP_BAND_2GHZ, "2.4 GHz"},
    {WLNK_CAP_BAND_5GHZ, "5 GHz"},   {WLNK_CAP_BAND_6GHZ, "6 GHz"},
    {WLNK_CAP_BAND_60GHZ, "60 GHz"},
};

static const struct named freq_flags[] = {
    {WLNK_FREQ_DISABLED, "disabled"},
    {WLNK_FREQ_NO_IR, "no IR"},
    {WLNK_FREQ_RADAR, "radar"},
};

/* Every value in the table has its pinned name, the names are distinct,
 * and the first value past the table has none. */
static void check_table(namer_t name, const struct named *t, size_t n,
                        bool bits)
{
    for (size_t i = 0; i < n; i++) {
        uint32_t expect = bits ? 1u << i : (uint32_t)i;
        assert_int_equal(t[i].value, expect); /* table in header order */
        assert_string_equal(name(t[i].value), t[i].name);
        assert_string_not_equal(t[i].name, UNKNOWN);
        for (size_t j = 0; j < i; j++)
            assert_string_not_equal(t[i].name, t[j].name);
    }
    assert_string_equal(name(bits ? 1u << n : (uint32_t)n), UNKNOWN);
    assert_string_equal(name(UINT32_MAX), UNKNOWN);
}

static void test_ifkind_names(void **state)
{
    (void)state;
    check_table(wlnk_ifkind_str, kinds, N(kinds), false);
}

static void test_chwidth_names(void **state)
{
    (void)state;
    check_table(wlnk_chwidth_str, widths, N(widths), false);
}

static void test_cap_names(void **state)
{
    (void)state;
    check_table(wlnk_cap_str, caps, N(caps), true);
}

static void test_freq_flag_names(void **state)
{
    (void)state;
    check_table(wlnk_freq_flag_str, freq_flags, N(freq_flags), true);
}

/* The bit helpers name one bit: nothing, or several at once, is not one. */
static void test_bit_helpers_name_one_bit(void **state)
{
    (void)state;
    assert_string_equal(wlnk_cap_str(0), UNKNOWN);
    assert_string_equal(wlnk_cap_str(WLNK_CAP_MONITOR | WLNK_CAP_SCAN),
                        UNKNOWN);
    assert_string_equal(wlnk_freq_flag_str(0), UNKNOWN);
    assert_string_equal(wlnk_freq_flag_str(WLNK_FREQ_NO_IR | WLNK_FREQ_RADAR),
                        UNKNOWN);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ifkind_names),
        cmocka_unit_test(test_chwidth_names),
        cmocka_unit_test(test_cap_names),
        cmocka_unit_test(test_freq_flag_names),
        cmocka_unit_test(test_bit_helpers_name_one_bit),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
