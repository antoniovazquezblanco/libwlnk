/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * journal_test.c
 * Check how claims record their steps, and how a leftover record is found.
 *
 * Internal test. The run directory is pointed at WLNK_RUNDIR, a directory
 * in the build tree, so the real one is never touched.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "err.h"

#include "journal.h"

#ifndef WLNK_RUNDIR
#error "WLNK_RUNDIR must point the test at its own run directory"
#endif

/** The test radio's record. */
#define RECORD WLNK_RUNDIR "/test-radio.journal"
/** A file a planted link points at, which must never be written. */
#define VICTIM WLNK_RUNDIR "/victim"

/**
 * @brief A radio that exists only here.
 * @return The radio.
 */
static wlnk_radio_t test_radio(void)
{
    wlnk_radio_t r;
    memset(&r, 0, sizeof(r));
    snprintf(r.id, sizeof(r.id), "test-radio");
    snprintf(r.phy, sizeof(r.phy), "phy99");
    snprintf(r.ifname, sizeof(r.ifname), "wlnktest0");
    r.wiphy = 99;
    return r;
}

/**
 * @brief Start from an empty run directory.
 * @param state Unused.
 * @return 0.
 */
static int clean(void **state)
{
    (void)state;
    unlink(RECORD);
    unlink(VICTIM);
    if (mkdir(WLNK_RUNDIR, 0755) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

/**
 * @brief Write a file in full.
 * @param path The file.
 * @param text Its contents.
 */
static void put(const char *path, const char *text)
{
    FILE *fp = fopen(path, "w");
    assert_non_null(fp);
    fputs(text, fp);
    fclose(fp);
}

/**
 * @brief Read a file in full.
 * @param path The file.
 * @param buf Receives its contents; "" when it is missing.
 * @param cap Size of @p buf.
 */
static void slurp(const char *path, char *buf, size_t cap)
{
    buf[0] = '\0';
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        return;
    size_t n = fread(buf, 1, cap - 1, fp);
    buf[n] = '\0';
    fclose(fp);
}

/**
 * @brief The header a record written by this process starts with.
 * @param buf Receives it.
 * @param cap Size of @p buf.
 */
static void own_header(char *buf, size_t cap)
{
    snprintf(buf, cap, "wlnk1 pid=%ld\n", (long)getpid());
}

/** Each step is on disk as soon as it is recorded, and the record stays
 * once closed. */
static void test_steps_are_on_disk(void **state)
{
    (void)state;
    wlnk_radio_t r = test_radio();
    wlnk_journal_t j;
    char want[128];
    char have[128];

    assert_int_equal(wlnk_journal_start(&r, &j), WLNK_OK);
    assert_non_null(j.fp);
    /* A program the claimer runs must not inherit the record. */
    assert_true(fcntl(fileno(j.fp), F_GETFD) & FD_CLOEXEC);
    own_header(want, sizeof(want));
    slurp(RECORD, have, sizeof(have));
    assert_string_equal(have, want);

    assert_int_equal(wlnk_journal_step(&j, "vif", "wlnk0mon"), WLNK_OK);
    assert_int_equal(wlnk_journal_step(&j, "link-down", "wlan0"), WLNK_OK);
    strcat(want, "vif wlnk0mon\nlink-down wlan0\n");
    slurp(RECORD, have, sizeof(have));
    assert_string_equal(have, want);

    wlnk_journal_close(&j);
    assert_null(j.fp);
    slurp(RECORD, have, sizeof(have));
    assert_string_equal(have, want);
}

/** A radio has one record at a time: a second start leaves the first
 * alone. */
static void test_record_is_exclusive(void **state)
{
    (void)state;
    wlnk_radio_t r = test_radio();
    wlnk_journal_t j;
    wlnk_journal_t other;
    char want[128];
    char have[128];

    assert_int_equal(wlnk_journal_start(&r, &j), WLNK_OK);
    assert_int_equal(wlnk_journal_step(&j, "vif", "wlnk0mon"), WLNK_OK);
    assert_int_equal(wlnk_journal_start(&r, &other), WLNK_E_STALE_STATE);
    assert_null(other.fp);
    wlnk_journal_close(&j);

    own_header(want, sizeof(want));
    strcat(want, "vif wlnk0mon\n");
    slurp(RECORD, have, sizeof(have));
    assert_string_equal(have, want);
}

/** A symlink planted at the record path is never followed, but still counts
 * as a leftover. */
static void test_symlink_is_not_followed(void **state)
{
    (void)state;
    wlnk_radio_t r = test_radio();
    wlnk_journal_t j;
    char have[32];

    put(VICTIM, "victim\n");
    assert_int_equal(symlink(VICTIM, RECORD), 0);
    assert_int_equal(wlnk_journal_start(&r, &j), WLNK_E_STALE_STATE);
    assert_null(j.fp);
    assert_int_equal(wlnk_journal_check(&r), WLNK_E_STALE_STATE);
    slurp(VICTIM, have, sizeof(have));
    assert_string_equal(have, "victim\n");
}

/** No record is no leftover, and looking never creates the run
 * directory. */
static void test_check_finds_nothing(void **state)
{
    (void)state;
    wlnk_radio_t r = test_radio();
    struct stat st;

    assert_int_equal(wlnk_journal_check(&r), WLNK_OK);
    assert_int_equal(rmdir(WLNK_RUNDIR), 0);
    assert_int_equal(wlnk_journal_check(&r), WLNK_OK);
    assert_int_equal(stat(WLNK_RUNDIR, &st), -1);
}

/** A leftover record is reported, naming the pid that left it. */
static void test_check_names_the_pid(void **state)
{
    (void)state;
    wlnk_radio_t r = test_radio();
    wlnk_journal_t j;
    char want[96];

    assert_int_equal(wlnk_journal_start(&r, &j), WLNK_OK);
    wlnk_journal_close(&j);
    assert_int_equal(wlnk_journal_check(&r), WLNK_E_STALE_STATE);
    snprintf(want, sizeof(want),
             "phy99: radio claimed by pid %ld and not released",
             (long)getpid());
    assert_string_equal(wlnk_err_msg(), want);

    put(RECORD, "wlnk1 pid=4242\nvif wlnk0mon\n");
    assert_int_equal(wlnk_journal_check(&r), WLNK_E_STALE_STATE);
    assert_string_equal(wlnk_err_msg(),
                        "phy99: radio claimed by pid 4242 and not released");
}

/** Anything at the record path counts, but only a whole header of ours
 * names a pid. */
static void test_check_trusts_only_whole_headers(void **state)
{
    (void)state;
    wlnk_radio_t r = test_radio();
    static const char *const leftovers[] = {
        "",               /* died before the header */
        "wlnk1 pid=42",   /* died mid-header: maybe only part of the pid */
        "hello pid=42\n", /* not a record of ours */
        "wlnk1 pid=x\n",
    };

    for (size_t i = 0; i < sizeof(leftovers) / sizeof(leftovers[0]); i++) {
        put(RECORD, leftovers[i]);
        assert_int_equal(wlnk_journal_check(&r), WLNK_E_STALE_STATE);
        assert_string_equal(wlnk_err_msg(),
                            "phy99: radio claimed and not released");
    }
}

/** A step is two words that read back as written; anything else is
 * refused and leaves the record as it was. */
static void test_step_is_two_words(void **state)
{
    (void)state;
    wlnk_radio_t r = test_radio();
    wlnk_journal_t j;
    char want[512];
    char have[512];
    char longest[256];
    char too_long[257];
    memset(longest, 'a', sizeof(longest) - 1);
    longest[sizeof(longest) - 1] = '\0';
    memset(too_long, 'a', sizeof(too_long) - 1);
    too_long[sizeof(too_long) - 1] = '\0';

    assert_int_equal(wlnk_journal_start(&r, &j), WLNK_OK);
    assert_int_equal(wlnk_journal_step(&j, "vif", NULL), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_journal_step(&j, NULL, "x"), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_journal_step(&j, "", "x"), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_journal_step(&j, "vif", ""), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_journal_step(&j, "v f", "x"), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_journal_step(&j, "vif", "a b"), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_journal_step(&j, "vif", "a\nb"), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_journal_step(&j, "abcdefghijklmnopqrstuvwxy", "x"),
                     WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_journal_step(&j, "vif", too_long),
                     WLNK_E_INVALID_ARG);
    own_header(want, sizeof(want));
    slurp(RECORD, have, sizeof(have));
    assert_string_equal(have, want);

    assert_int_equal(wlnk_journal_step(&j, "abcdefghijklmnopqrstuvw", longest),
                     WLNK_OK);
    wlnk_journal_close(&j);
    snprintf(want + strlen(want), sizeof(want) - strlen(want),
             "abcdefghijklmnopqrstuvw %s\n", longest);
    slurp(RECORD, have, sizeof(have));
    assert_string_equal(have, want);
}

/** Missing arguments, a radio that never resolved, or a closed record are
 * refused. */
static void test_needs_arguments(void **state)
{
    (void)state;
    wlnk_radio_t r = test_radio();
    wlnk_radio_t unresolved;
    memset(&unresolved, 0, sizeof(unresolved));
    wlnk_journal_t j;

    assert_int_equal(wlnk_journal_check(NULL), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_journal_check(&unresolved), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_journal_start(NULL, &j), WLNK_E_INVALID_ARG);
    assert_null(j.fp);
    assert_int_equal(wlnk_journal_start(&r, NULL), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_journal_start(&unresolved, &j), WLNK_E_INVALID_ARG);
    assert_int_equal(access(RECORD, F_OK), -1);

    assert_int_equal(wlnk_journal_step(NULL, "vif", "x"), WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_journal_step(&j, "vif", "x"), WLNK_E_INVALID_ARG);
    wlnk_journal_close(NULL);
    wlnk_journal_close(&j);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_steps_are_on_disk, clean),
        cmocka_unit_test_setup(test_record_is_exclusive, clean),
        cmocka_unit_test_setup(test_symlink_is_not_followed, clean),
        cmocka_unit_test_setup(test_check_finds_nothing, clean),
        cmocka_unit_test_setup(test_check_names_the_pid, clean),
        cmocka_unit_test_setup(test_check_trusts_only_whole_headers, clean),
        cmocka_unit_test_setup(test_step_is_two_words, clean),
        cmocka_unit_test_setup(test_needs_arguments, clean),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
