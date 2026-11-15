/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * chandef_test.c
 * Check the segment-centre derivation behind wlnk_set_channel().
 *
 * Internal test: wlnk_chandef_resolve() is what turns the chandef a caller
 * may leave half-filled into the one nl80211 is given, so it is checked
 * directly rather than through a radio.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

#include "err.h"
#include "iface.h"

#include "chandef.h"

/** Centres the 802.11 channel plan gives, for control channels across every
 * block a width defines. */
static const struct {
    uint32_t freq, width, center1;
} g_derived[] = {
    /* 5 GHz 40 MHz: both halves of a block resolve to its centre. */
    {5180, WLNK_CHW_40, 5190},
    {5200, WLNK_CHW_40, 5190},
    {5220, WLNK_CHW_40, 5230},
    {5700, WLNK_CHW_40, 5710},
    /* U-NII-3 restarts the grid 5 MHz off the lower one: channel 149 pairs
     * with 153 around 5755, not with the 5740/5760 block below it. */
    {5745, WLNK_CHW_40, 5755},
    {5765, WLNK_CHW_40, 5755},
    {5785, WLNK_CHW_40, 5795},
    {5805, WLNK_CHW_40, 5795},
    {5825, WLNK_CHW_40, 5835},
    {5745, WLNK_CHW_80, 5775},
    {5805, WLNK_CHW_80, 5775},
    {5745, WLNK_CHW_160, 5815},
    /* 5 GHz 80 MHz and 160 MHz. */
    {5180, WLNK_CHW_80, 5210},
    {5240, WLNK_CHW_80, 5210},
    {5260, WLNK_CHW_80, 5290},
    {5180, WLNK_CHW_160, 5250},
    {5320, WLNK_CHW_160, 5250},
    {5500, WLNK_CHW_160, 5570},
    {5640, WLNK_CHW_160, 5570},
    {5660, WLNK_CHW_80, 5690},
    /* 6 GHz, whose grid starts 10 MHz below channel 1 (5955). */
    {5955, WLNK_CHW_40, 5965},
    {5975, WLNK_CHW_40, 5965},
    {5955, WLNK_CHW_80, 5985},
    {5955, WLNK_CHW_160, 6025},
    {6095, WLNK_CHW_160, 6025},
    {6115, WLNK_CHW_160, 6185}, /* the next 160 MHz block up */
    /* 2.4 GHz, where only one 40 MHz placement fits at the band edges. */
    {2412, WLNK_CHW_40, 2422},
    {2427, WLNK_CHW_40, 2437},
    {2457, WLNK_CHW_40, 2447},
    {2472, WLNK_CHW_40, 2462},
};

static void test_centres_are_derived(void **state)
{
    (void)state;
    for (size_t i = 0; i < sizeof(g_derived) / sizeof(g_derived[0]); i++) {
        wlnk_chandef_t in = {.freq_mhz = g_derived[i].freq,
                             .width = g_derived[i].width};
        wlnk_chandef_t out;
        assert_int_equal(wlnk_chandef_resolve(&in, &out), WLNK_OK);
        assert_int_equal(out.freq_mhz, g_derived[i].freq);
        assert_int_equal(out.width, g_derived[i].width);
        assert_int_equal(out.center1_mhz, g_derived[i].center1);
        assert_int_equal(out.center2_mhz, 0);
    }
}

/** Over every 20 MHz channel centre the wide bands define, the derived
 * segment contains its control channel whole. The three ranges are the
 * grids themselves: 5 GHz channels 36 to 144, then U-NII-3 and above, then
 * 6 GHz. Frequencies between them are not channel centres and have no
 * segment to be part of. */
static void test_derived_centre_covers_the_control_channel(void **state)
{
    (void)state;
    static const struct {
        uint32_t width, span;
    } widths[] = {{WLNK_CHW_40, 40}, {WLNK_CHW_80, 80}, {WLNK_CHW_160, 160}};
    static const struct {
        uint32_t first, last;
    } grids[] = {{5180, 5720}, {5745, 5885}, {5955, 7115}};

    int checked = 0;
    for (size_t w = 0; w < sizeof(widths) / sizeof(widths[0]); w++)
        for (size_t g = 0; g < sizeof(grids) / sizeof(grids[0]); g++)
            for (uint32_t freq = grids[g].first; freq <= grids[g].last;
                 freq += 20) {
                wlnk_chandef_t in = {.freq_mhz = freq,
                                     .width = widths[w].width};
                wlnk_chandef_t out;
                assert_int_equal(wlnk_chandef_resolve(&in, &out), WLNK_OK);
                uint32_t half = widths[w].span / 2;
                assert_true(out.center1_mhz >= half);
                /* The control channel's own 20 MHz lies inside the segment. */
                assert_true(freq + 10 <= out.center1_mhz + half);
                assert_true(freq >= out.center1_mhz - half + 10);
                checked++;
            }
    assert_true(checked > 200);
}

/** The narrow widths are one segment centred on the control channel. */
static void test_narrow_widths_centre_on_the_channel(void **state)
{
    (void)state;
    static const uint32_t widths[] = {WLNK_CHW_20_NOHT, WLNK_CHW_20, WLNK_CHW_5,
                                      WLNK_CHW_10};
    for (size_t i = 0; i < sizeof(widths) / sizeof(widths[0]); i++) {
        wlnk_chandef_t in = {.freq_mhz = 2437, .width = widths[i]};
        wlnk_chandef_t out;
        assert_int_equal(wlnk_chandef_resolve(&in, &out), WLNK_OK);
        assert_int_equal(out.center1_mhz, 2437);
        assert_int_equal(out.center2_mhz, 0);
    }
}

/** A centre the caller supplied is never second-guessed. */
static void test_given_centre_is_kept(void **state)
{
    (void)state;
    wlnk_chandef_t in = {
        .freq_mhz = 2437, .width = WLNK_CHW_40, .center1_mhz = 2427};
    wlnk_chandef_t out;
    assert_int_equal(wlnk_chandef_resolve(&in, &out), WLNK_OK);
    assert_int_equal(out.center1_mhz, 2427); /* HT40-, not the derived HT40+ */

    wlnk_chandef_t wide = {
        .freq_mhz = 6115, .width = WLNK_CHW_320, .center1_mhz = 6105};
    assert_int_equal(wlnk_chandef_resolve(&wide, &out), WLNK_OK);
    assert_int_equal(out.center1_mhz, 6105);
}

/** 80+80 keeps both segments; every other width clears the second. */
static void test_second_segment_is_80p80_only(void **state)
{
    (void)state;
    wlnk_chandef_t in = {.freq_mhz = 5180,
                         .width = WLNK_CHW_80P80,
                         .center1_mhz = 5210,
                         .center2_mhz = 5530};
    wlnk_chandef_t out;
    assert_int_equal(wlnk_chandef_resolve(&in, &out), WLNK_OK);
    assert_int_equal(out.center1_mhz, 5210);
    assert_int_equal(out.center2_mhz, 5530);

    in.width = WLNK_CHW_80;
    assert_int_equal(wlnk_chandef_resolve(&in, &out), WLNK_OK);
    assert_int_equal(out.center2_mhz, 0);
}

/** Where more than one block fits, the caller has to say which. */
static void test_ambiguous_widths_are_refused(void **state)
{
    (void)state;
    static const wlnk_chandef_t bad[] = {
        /* 2.4 GHz channels 5 to 9: HT40+ and HT40- both fit. */
        {.freq_mhz = 2432, .width = WLNK_CHW_40},
        {.freq_mhz = 2437, .width = WLNK_CHW_40},
        {.freq_mhz = 2442, .width = WLNK_CHW_40},
        /* Channel 14 has no 40 MHz placement at all. */
        {.freq_mhz = 2484, .width = WLNK_CHW_40},
        /* The 320 MHz blocks overlap. */
        {.freq_mhz = 6115, .width = WLNK_CHW_320},
        /* 80+80 needs both segments named. */
        {.freq_mhz = 5180, .width = WLNK_CHW_80P80, .center1_mhz = 5210},
        {.freq_mhz = 5180, .width = WLNK_CHW_80P80, .center2_mhz = 5530},
        /* The 4.9 GHz block and 60 GHz have no wide-channel grid. */
        {.freq_mhz = 4920, .width = WLNK_CHW_40},
        {.freq_mhz = 58320, .width = WLNK_CHW_80},
    };
    wlnk_chandef_t out;
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
        assert_int_equal(wlnk_chandef_resolve(&bad[i], &out),
                         WLNK_E_INVALID_ARG);
}

static void test_invalid_input(void **state)
{
    (void)state;
    wlnk_chandef_t out;
    wlnk_chandef_t no_freq = {.width = WLNK_CHW_20};
    wlnk_chandef_t bad_width = {.freq_mhz = 2437, .width = 99};
    assert_int_equal(wlnk_chandef_resolve(&no_freq, &out), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_chandef_resolve(&bad_width, &out),
                     WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_chandef_resolve(NULL, &out), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_chandef_resolve(&no_freq, NULL), WLNK_E_INVALID_ARG);
}

/** Resolving twice changes nothing: the output is already resolved. */
static void test_resolve_is_idempotent(void **state)
{
    (void)state;
    for (uint32_t freq = 2412; freq <= 7115; freq += 5)
        for (uint32_t width = WLNK_CHW_20_NOHT; width <= WLNK_CHW_10; width++) {
            wlnk_chandef_t in = {.freq_mhz = freq, .width = width};
            wlnk_chandef_t once, twice;
            if (wlnk_chandef_resolve(&in, &once) != WLNK_OK)
                continue;
            assert_int_equal(wlnk_chandef_resolve(&once, &twice), WLNK_OK);
            assert_int_equal(twice.freq_mhz, once.freq_mhz);
            assert_int_equal(twice.width, once.width);
            assert_int_equal(twice.center1_mhz, once.center1_mhz);
            assert_int_equal(twice.center2_mhz, once.center2_mhz);
        }
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_centres_are_derived),
        cmocka_unit_test(test_derived_centre_covers_the_control_channel),
        cmocka_unit_test(test_narrow_widths_centre_on_the_channel),
        cmocka_unit_test(test_given_centre_is_kept),
        cmocka_unit_test(test_second_segment_is_80p80_only),
        cmocka_unit_test(test_ambiguous_widths_are_refused),
        cmocka_unit_test(test_invalid_input),
        cmocka_unit_test(test_resolve_is_idempotent),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
