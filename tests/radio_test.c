/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * radio_test.c
 * Check how interfaces resolve to radios, how radios are named and how
 * their rfkill state reads.
 *
 * Internal test. The library sources are rebuilt with sysfs pointed at
 * WLNK_SYSFS, a tree this test builds in the build directory, so the real
 * one is never read.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "err.h"

#include "radio.h"

#ifndef WLNK_SYSFS
#error "WLNK_SYSFS must point the test at its own sysfs tree"
#endif

/**
 * @brief Create a directory and its parents.
 * @param path Directory, relative to the test's sysfs.
 */
static void mkdirs(const char *path)
{
    char full[512];
    snprintf(full, sizeof(full), "%s/%s", WLNK_SYSFS, path);
    for (char *p = full + 1; *p != '\0'; p++) {
        if (*p != '/')
            continue;
        *p = '\0';
        if (mkdir(full, 0755) != 0 && errno != EEXIST)
            fail_msg("mkdir %s", full);
        *p = '/';
    }
    if (mkdir(full, 0755) != 0 && errno != EEXIST)
        fail_msg("mkdir %s", full);
}

/**
 * @brief Write a sysfs attribute, with the trailing newline sysfs adds.
 * @param path Attribute, relative to the test's sysfs.
 * @param value Its value.
 */
static void put(const char *path, const char *value)
{
    char full[512];
    snprintf(full, sizeof(full), "%s/%s", WLNK_SYSFS, path);
    FILE *fp = fopen(full, "w");
    assert_non_null(fp);
    fprintf(fp, "%s\n", value);
    fclose(fp);
}

/**
 * @brief Point a phy's device link somewhere.
 * @param phy Phy name.
 * @param target Link target.
 */
static void device_link(const char *phy, const char *target)
{
    char full[512];
    snprintf(full, sizeof(full), "%s/class/ieee80211/%s/device", WLNK_SYSFS,
             phy);
    unlink(full);
    assert_int_equal(symlink(target, full), 0);
}

/**
 * @brief Add a wireless interface on a phy.
 * @param ifname Interface name.
 * @param phy Phy name.
 * @param index Wiphy index.
 */
static void wireless(const char *ifname, const char *phy, const char *index)
{
    char path[128];
    snprintf(path, sizeof(path), "class/net/%s/phy80211", ifname);
    mkdirs(path);
    snprintf(path, sizeof(path), "class/net/%s/phy80211/name", ifname);
    put(path, phy);
    snprintf(path, sizeof(path), "class/net/%s/phy80211/index", ifname);
    put(path, index);
    snprintf(path, sizeof(path), "class/ieee80211/%s", phy);
    mkdirs(path);
}

/**
 * @brief Give a phy an rfkill switch.
 * @param phy Phy name.
 * @param node Switch name.
 * @param hard Hard block state, "0" or "1".
 * @param soft Soft block state, "0" or "1".
 */
static void rfkill(const char *phy, const char *node, const char *hard,
                   const char *soft)
{
    char path[128];
    snprintf(path, sizeof(path), "class/ieee80211/%s/%s", phy, node);
    mkdirs(path);
    snprintf(path, sizeof(path), "class/ieee80211/%s/%s/hard", phy, node);
    put(path, hard);
    snprintf(path, sizeof(path), "class/ieee80211/%s/%s/soft", phy, node);
    put(path, soft);
}

/**
 * @brief Build the test's sysfs tree.
 * @param state Unused.
 * @return 0.
 */
static int build_sysfs(void **state)
{
    (void)state;
    mkdirs("class/net/eth0"); /* not wireless: no phy80211 */
    wireless("wlan0", "phy0", "0");
    device_link("phy0", "../../../devices/pci0000:00/0000:02:00.0");
    rfkill("phy0", "rfkill0", "0", "0");
    wireless("wlan1", "phy1", "1"); /* two phys on one USB device */
    device_link("phy1", "../../../devices/usb1/1-1/1-1.4/1-1.4:1.0");
    rfkill("phy1", "rfkill1", "0", "1");
    wireless("wlan2", "phy2", "2");
    device_link("phy2", "../../../devices/usb1/1-1/1-1.4/1-1.4:1.0");
    rfkill("phy2", "rfkill2", "1", "0");
    wireless("wlan3", "phy3", "3"); /* no device link nor rfkill switch */
    wireless("wlan4", "phy4", "4"); /* a bus name a file cannot carry */
    device_link("phy4", "../../../devices/odd/.odd bus*");
    return 0;
}

/** A radio alone on its device is named by its bus path. */
static void test_single_radio(void **state)
{
    (void)state;
    wlnk_radio_t r;

    assert_int_equal(wlnk_radio_resolve("wlan0", &r), WLNK_OK);
    assert_string_equal(r.id, "0000:02:00.0");
    assert_string_equal(r.phy, "phy0");
    assert_string_equal(r.ifname, "wlan0");
    assert_int_equal(r.wiphy, 0);
    assert_string_equal(wlnk_err_msg(), "");
}

/** Phys sharing a device are told apart by an ordinal, by phy name: the
 * first keeps the bare bus path. */
static void test_phys_sharing_a_device(void **state)
{
    (void)state;
    wlnk_radio_t a, b;

    assert_int_equal(wlnk_radio_resolve("wlan1", &a), WLNK_OK);
    assert_int_equal(wlnk_radio_resolve("wlan2", &b), WLNK_OK);
    assert_string_equal(a.id, "1-1.4:1.0");
    assert_string_equal(b.id, "1-1.4:1.0.1");
    assert_int_equal(b.wiphy, 2);
}

/** A phy with no device link is named after the phy. */
static void test_radio_without_device(void **state)
{
    (void)state;
    wlnk_radio_t r;

    assert_int_equal(wlnk_radio_resolve("wlan3", &r), WLNK_OK);
    assert_string_equal(r.id, "phy3");
}

/** An identity is always safe as a file name. */
static void test_identity_is_a_safe_name(void **state)
{
    (void)state;
    wlnk_radio_t r;

    assert_int_equal(wlnk_radio_resolve("wlan4", &r), WLNK_OK);
    assert_string_equal(r.id, "_odd_bus_");
}

/** An interface that is not wireless, or not there at all, has no radio. */
static void test_not_a_radio(void **state)
{
    (void)state;
    wlnk_radio_t r;

    assert_int_equal(wlnk_radio_resolve("eth0", &r), WLNK_E_NOT_FOUND);
    assert_int_equal(r.wiphy, -1);
    assert_true(strstr(wlnk_err_msg(), "eth0") != NULL);
    assert_int_equal(wlnk_radio_resolve("wlnk-absent0", &r), WLNK_E_NOT_FOUND);
}

/** Missing and malformed arguments are refused. */
static void test_needs_arguments(void **state)
{
    (void)state;
    wlnk_radio_t r;

    assert_int_equal(wlnk_radio_resolve(NULL, &r), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_radio_resolve("", &r), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_radio_resolve("wlan0", NULL), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_radio_resolve("wlnk-0123456789ab", &r),
                     WLNK_E_INVALID_ARG);
}

/** A radio whose rfkill switch is off, that has none, or that sysfs does
 * not show, is usable. */
static void test_rfkill_unblocked(void **state)
{
    (void)state;
    wlnk_radio_t r;

    assert_int_equal(wlnk_radio_resolve("wlan0", &r), WLNK_OK);
    assert_int_equal(wlnk_rfkill_check(&r), WLNK_OK);
    assert_int_equal(wlnk_radio_resolve("wlan3", &r), WLNK_OK);
    assert_int_equal(wlnk_rfkill_check(&r), WLNK_OK);
    snprintf(r.phy, sizeof(r.phy), "wlnk-absent-phy");
    assert_int_equal(wlnk_rfkill_check(&r), WLNK_OK);
    assert_string_equal(wlnk_err_msg(), "");
}

/** A soft or a hard block is reported, and says which. */
static void test_rfkill_blocked(void **state)
{
    (void)state;
    wlnk_radio_t r;

    assert_int_equal(wlnk_radio_resolve("wlan1", &r), WLNK_OK);
    assert_int_equal(wlnk_rfkill_check(&r), WLNK_E_RF_KILL);
    assert_string_equal(wlnk_err_msg(), "phy1: radio soft blocked (rfkill)");
    assert_int_equal(wlnk_radio_resolve("wlan2", &r), WLNK_OK);
    assert_int_equal(wlnk_rfkill_check(&r), WLNK_E_RF_KILL);
    assert_string_equal(wlnk_err_msg(), "phy2: radio hard blocked (rfkill)");
}

/** No radio, or one that never resolved, is refused. */
static void test_rfkill_needs_a_radio(void **state)
{
    (void)state;
    wlnk_radio_t r;

    assert_int_equal(wlnk_rfkill_check(NULL), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_radio_resolve("eth0", &r), WLNK_E_NOT_FOUND);
    assert_int_equal(wlnk_rfkill_check(&r), WLNK_E_INVALID_ARG);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_single_radio),
        cmocka_unit_test(test_phys_sharing_a_device),
        cmocka_unit_test(test_radio_without_device),
        cmocka_unit_test(test_identity_is_a_safe_name),
        cmocka_unit_test(test_not_a_radio),
        cmocka_unit_test(test_needs_arguments),
        cmocka_unit_test(test_rfkill_unblocked),
        cmocka_unit_test(test_rfkill_blocked),
        cmocka_unit_test(test_rfkill_needs_a_radio),
    };
    return cmocka_run_group_tests(tests, build_sysfs, NULL);
}
