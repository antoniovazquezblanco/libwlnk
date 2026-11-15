/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * journal_test.c
 * Check how claims record their steps, how a leftover record is found, and
 * how a record is undone.
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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "err.h"

#include "err_internal.h"
#include "journal.h"

#ifndef WLNK_RUNDIR
#error "WLNK_RUNDIR must point the test at its own run directory"
#endif

/** The test radio's record. */
#define RECORD WLNK_RUNDIR "/test-radio.journal"
/** Where a partial undo writes the record's replacement. */
#define TEMP   WLNK_RUNDIR "/test-radio.journal.new"
/** A file a planted link points at, which must never be written. */
#define VICTIM WLNK_RUNDIR "/victim"

/** What a test undo was given, and whether it fails the "stuck" steps. */
struct undo_log {
    /** The steps, one "kind arg" line each, in the order given. */
    char seen[512];
    /** Whether "stuck" steps fail to undo. */
    bool stuck;
};

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
    unlink(TEMP);
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

/**
 * @brief Record steps for the test radio, as a claim would.
 * @param steps "kind arg" pairs, oldest first, ending in NULL.
 */
static void record(const char *const *steps)
{
    wlnk_radio_t r = test_radio();
    wlnk_journal_t j;
    assert_int_equal(wlnk_journal_start(&r, &j), WLNK_OK);
    for (size_t i = 0; steps[i] != NULL; i += 2)
        assert_int_equal(wlnk_journal_step(&j, steps[i], steps[i + 1]),
                         WLNK_OK);
    wlnk_journal_close(&j);
}

/**
 * @brief Undo a step for the tests: log it, and fail it when it is stuck.
 *
 * A stuck "busy" step fails with WLNK_E_BUSY, any other with WLNK_E_PERM,
 * so the failure reported can be told apart.
 *
 * @param user The struct undo_log.
 * @param kind What the step did.
 * @param arg What it did it to.
 * @return WLNK_OK, or the step's failure.
 */
static int fake_undo(void *user, const char *kind, const char *arg)
{
    struct undo_log *log = user;
    size_t len = strlen(log->seen);
    snprintf(log->seen + len, sizeof(log->seen) - len, "%s %s\n", kind, arg);
    if (!log->stuck || strcmp(kind, "stuck") != 0)
        return wlnk_ok();
    char msg[32];
    snprintf(msg, sizeof(msg), "stuck %s", arg);
    if (strcmp(arg, "busy") == 0)
        return wlnk_fail(WLNK_E_BUSY, EBUSY, msg);
    return wlnk_fail(WLNK_E_PERM, EPERM, msg);
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

/** Undo goes newest step first, and removes a record fully undone. */
static void test_undo_newest_first(void **state)
{
    (void)state;
    wlnk_radio_t r = test_radio();
    struct undo_log log = {"", true};
    static const char *const steps[] = {
        "vif", "a", "link-down", "b", "vif", "c", NULL,
    };

    record(steps);
    assert_int_equal(wlnk_journal_undo(&r, fake_undo, &log), WLNK_OK);
    assert_string_equal(log.seen, "vif c\nlink-down b\nvif a\n");
    assert_int_equal(access(RECORD, F_OK), -1);
    assert_int_equal(wlnk_journal_check(&r), WLNK_OK);
}

/** Every step is tried. Those that fail stay recorded, in order, for a later
 * undo, and the first failure is the one reported. */
static void test_undo_keeps_what_failed(void **state)
{
    (void)state;
    wlnk_radio_t r = test_radio();
    struct undo_log log = {"", true};
    char want[128];
    char have[128];
    static const char *const steps[] = {
        "vif",   "a",    "stuck", "perm", "vif", "b",
        "stuck", "busy", "vif",   "c",    NULL,
    };

    record(steps);
    assert_int_equal(wlnk_journal_undo(&r, fake_undo, &log), WLNK_E_BUSY);
    assert_string_equal(log.seen,
                        "vif c\nstuck busy\nvif b\nstuck perm\nvif a\n");
    assert_string_equal(wlnk_err_msg(), "stuck busy");
    assert_int_equal(wlnk_err_syserrno(), EBUSY);
    own_header(want, sizeof(want));
    strcat(want, "stuck perm\nstuck busy\n");
    slurp(RECORD, have, sizeof(have));
    assert_string_equal(have, want);
    assert_int_equal(access(TEMP, F_OK), -1);

    /* Once they can be undone, the record goes. */
    log.seen[0] = '\0';
    log.stuck = false;
    assert_int_equal(wlnk_journal_undo(&r, fake_undo, &log), WLNK_OK);
    assert_string_equal(log.seen, "stuck busy\nstuck perm\n");
    assert_int_equal(access(RECORD, F_OK), -1);
}

/** No record, or no run directory, is nothing to undo; undo never creates
 * the run directory. */
static void test_undo_without_record(void **state)
{
    (void)state;
    wlnk_radio_t r = test_radio();
    struct undo_log log = {"", true};
    struct stat st;

    assert_int_equal(wlnk_journal_undo(&r, fake_undo, &log), WLNK_OK);
    assert_int_equal(rmdir(WLNK_RUNDIR), 0);
    assert_int_equal(wlnk_journal_undo(&r, fake_undo, &log), WLNK_OK);
    assert_int_equal(stat(WLNK_RUNDIR, &st), -1);
    assert_string_equal(log.seen, "");
}

/** A last line cut short was never applied, so it is dropped, not undone.
 * So is a header cut short, or one never written. */
static void test_undo_drops_a_cut_line(void **state)
{
    (void)state;
    wlnk_radio_t r = test_radio();
    struct undo_log log = {"", true};
    static const char *const cut[] = {"", "wlnk", "wlnk1 pid=1"};

    put(RECORD, "wlnk1 pid=1\nvif a\nvif b");
    assert_int_equal(wlnk_journal_undo(&r, fake_undo, &log), WLNK_OK);
    assert_string_equal(log.seen, "vif a\n");
    assert_int_equal(access(RECORD, F_OK), -1);

    for (size_t i = 0; i < sizeof(cut) / sizeof(cut[0]); i++) {
        log.seen[0] = '\0';
        put(RECORD, cut[i]);
        assert_int_equal(wlnk_journal_undo(&r, fake_undo, &log), WLNK_OK);
        assert_string_equal(log.seen, "");
        assert_int_equal(access(RECORD, F_OK), -1);
    }
}

/** A record holding anything this build would not have written is refused
 * whole, and left exactly as it is. */
static void test_undo_refuses_what_it_cannot_read(void **state)
{
    (void)state;
    wlnk_radio_t r = test_radio();
    struct undo_log log = {"", true};
    char long_arg[300];
    char long_line[340];
    char have[400];
    snprintf(long_arg, sizeof(long_arg), "wlnk1 pid=1\nvif %0256d\n", 0);
    snprintf(long_line, sizeof(long_line), "wlnk1 pid=1\nvif %0300d\n", 0);
    const char *const bad[] = {
        "hello\nvif a\n",         /* not ours */
        "wlnk\nvif a\n",          /* a whole header, but not ours */
        "hell",                   /* a cut header, not ours */
        "wlnk1 pid=1\nvif\n",     /* one word */
        "wlnk1 pid=1\nvif a b\n", /* three */
        "wlnk1 pid=1\n vif\n",    /* no kind */
        "wlnk1 pid=1\nvif \n",    /* no argument */
        "wlnk1 pid=1\nabcdefghijklmnopqrstuvwxy a\n", /* kind too long */
        long_arg,                                     /* argument too long */
        long_line,                                    /* line too long */
        "wlnk1 pid=1\nvif a b\nvif c\n", /* one bad step spoils all */
    };

    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        put(RECORD, bad[i]);
        assert_int_equal(wlnk_journal_undo(&r, fake_undo, &log),
                         WLNK_E_UNSUPPORTED);
        assert_string_equal(log.seen, "");
        slurp(RECORD, have, sizeof(have));
        assert_string_equal(have, bad[i]);
    }
}

/** A record path that is not a plain file is neither read nor written
 * through. */
static void test_undo_refuses_unsafe_record(void **state)
{
    (void)state;
    wlnk_radio_t r = test_radio();
    struct undo_log log = {"", true};
    char have[64];
    struct stat st;

    put(VICTIM, "wlnk1 pid=1\nvif a\n");
    assert_int_equal(symlink(VICTIM, RECORD), 0);
    assert_int_equal(wlnk_journal_undo(&r, fake_undo, &log), WLNK_E_IO);
    assert_int_equal(lstat(RECORD, &st), 0);
    assert_true(S_ISLNK(st.st_mode));

    assert_int_equal(unlink(RECORD), 0);
    assert_int_equal(link(VICTIM, RECORD), 0);
    assert_int_equal(wlnk_journal_undo(&r, fake_undo, &log), WLNK_E_IO);

    assert_string_equal(log.seen, "");
    slurp(VICTIM, have, sizeof(have));
    assert_string_equal(have, "wlnk1 pid=1\nvif a\n");
}

/** A partial undo never writes through what was left at its temporary
 * name. */
static void test_rewrite_ignores_a_planted_temporary(void **state)
{
    (void)state;
    wlnk_radio_t r = test_radio();
    struct undo_log log = {"", true};
    char want[64];
    char have[64];
    static const char *const steps[] = {"stuck", "perm", "vif", "a", NULL};

    put(VICTIM, "victim\n");
    assert_int_equal(symlink(VICTIM, TEMP), 0);
    record(steps);
    assert_int_equal(wlnk_journal_undo(&r, fake_undo, &log), WLNK_E_PERM);
    slurp(VICTIM, have, sizeof(have));
    assert_string_equal(have, "victim\n");
    own_header(want, sizeof(want));
    strcat(want, "stuck perm\n");
    slurp(RECORD, have, sizeof(have));
    assert_string_equal(have, want);
    assert_int_equal(access(TEMP, F_OK), -1);
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

    struct undo_log log = {"", true};
    assert_int_equal(wlnk_journal_undo(NULL, fake_undo, &log),
                     WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_journal_undo(&unresolved, fake_undo, &log),
                     WLNK_E_INVALID_ARG);
    assert_int_equal(wlnk_journal_undo(&r, NULL, &log), WLNK_E_INVALID_ARG);
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
        cmocka_unit_test_setup(test_undo_newest_first, clean),
        cmocka_unit_test_setup(test_undo_keeps_what_failed, clean),
        cmocka_unit_test_setup(test_undo_without_record, clean),
        cmocka_unit_test_setup(test_undo_drops_a_cut_line, clean),
        cmocka_unit_test_setup(test_undo_refuses_what_it_cannot_read, clean),
        cmocka_unit_test_setup(test_undo_refuses_unsafe_record, clean),
        cmocka_unit_test_setup(test_rewrite_ignores_a_planted_temporary, clean),
        cmocka_unit_test_setup(test_needs_arguments, clean),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
