/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * lock_test.c
 * Check how radios are locked for a claim.
 *
 * Internal test. The library source is rebuilt with the run directory
 * pointed at WLNK_RUNDIR, a directory in the build tree, so the real one is
 * never touched.
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
#include <sys/wait.h>
#include <unistd.h>
#include "err.h"

#include "lock.h"

#ifndef WLNK_RUNDIR
#error "WLNK_RUNDIR must point the test at its own run directory"
#endif

/** The test radio's lock. */
#define LOCK_FILE WLNK_RUNDIR "/test-radio.lock"
/** A file planted links point at, which must never be written. */
#define VICTIM    WLNK_RUNDIR "/victim"

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
    unlink(LOCK_FILE);
    unlink(VICTIM);
    if (mkdir(WLNK_RUNDIR, 0755) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

/**
 * @brief Create the victim, with known contents.
 */
static void make_victim(void)
{
    FILE *fp = fopen(VICTIM, "w");
    assert_non_null(fp);
    fputs("victim\n", fp);
    fclose(fp);
}

/**
 * @brief Check the victim was neither truncated nor written.
 */
static void assert_victim_intact(void)
{
    char buf[16] = "";
    FILE *fp = fopen(VICTIM, "r");
    assert_non_null(fp);
    assert_non_null(fgets(buf, sizeof(buf), fp));
    fclose(fp);
    assert_string_equal(buf, "victim\n");
}

/** A missing run directory is created. */
static void test_creates_run_directory(void **state)
{
    (void)state;
    wlnk_radio_t r = test_radio();
    struct stat st;

    assert_int_equal(rmdir(WLNK_RUNDIR), 0);
    int fd = -1;
    assert_int_equal(wlnk_lock_take(&r, &fd), WLNK_OK);
    assert_int_equal(stat(WLNK_RUNDIR, &st), 0);
    assert_true(S_ISDIR(st.st_mode));
    wlnk_lock_drop(&fd);
}

/** A held lock refuses a second claim, naming the holder, and is free again
 * once dropped. */
static void test_lock_excludes(void **state)
{
    (void)state;
    wlnk_radio_t r = test_radio();

    int first = -1;
    assert_int_equal(wlnk_lock_take(&r, &first), WLNK_OK);
    assert_true(first >= 0);
    /* A program the claimer runs must not inherit the radio. */
    assert_true(fcntl(first, F_GETFD) & FD_CLOEXEC);

    int second = 99;
    assert_int_equal(wlnk_lock_take(&r, &second), WLNK_E_BUSY);
    assert_int_equal(second, -1);
    char held[64];
    snprintf(held, sizeof(held), "phy99: radio held by pid %ld",
             (long)getpid());
    assert_string_equal(wlnk_err_msg(), held);

    wlnk_lock_drop(&first);
    assert_int_equal(first, -1);
    assert_int_equal(wlnk_lock_take(&r, &second), WLNK_OK);
    wlnk_lock_drop(&second);

    /* Never deleted: unlinking races with a claimer that has it open. */
    assert_int_equal(access(LOCK_FILE, F_OK), 0);
}

/** A lock held by another process names that process, and is free once it
 * exits, even without being dropped. */
static void test_holder_exit_frees_the_lock(void **state)
{
    (void)state;
    wlnk_radio_t r = test_radio();
    int ready[2];
    int done[2];
    assert_int_equal(pipe(ready), 0);
    assert_int_equal(pipe(done), 0);

    pid_t child = fork();
    assert_true(child >= 0);
    if (child == 0) {
        close(done[1]);
        int fd = -1;
        char c = wlnk_lock_take(&r, &fd) == WLNK_OK ? 'y' : 'n';
        (void)!write(ready[1], &c, 1);
        (void)!read(done[0], &c, 1); /* until the parent has looked */
        _exit(0);                    /* never dropped */
    }
    close(ready[1]);
    close(done[0]);
    char c = 0;
    assert_int_equal(read(ready[0], &c, 1), 1);
    assert_int_equal(c, 'y');

    int fd = 99;
    assert_int_equal(wlnk_lock_take(&r, &fd), WLNK_E_BUSY);
    assert_int_equal(fd, -1);
    char held[64];
    snprintf(held, sizeof(held), "phy99: radio held by pid %ld", (long)child);
    assert_string_equal(wlnk_err_msg(), held);

    close(done[1]);
    assert_int_equal(waitpid(child, NULL, 0), child);
    assert_int_equal(wlnk_lock_take(&r, &fd), WLNK_OK);
    wlnk_lock_drop(&fd);
    close(ready[0]);
}

/** A symlink planted at the lock path is never followed. */
static void test_symlink_is_refused(void **state)
{
    (void)state;
    wlnk_radio_t r = test_radio();
    make_victim();
    assert_int_equal(symlink(VICTIM, LOCK_FILE), 0);

    int fd = 99;
    assert_int_equal(wlnk_lock_take(&r, &fd), WLNK_E_IO);
    assert_int_equal(fd, -1);
    assert_non_null(strstr(wlnk_err_msg(), "unsafe"));
    assert_victim_intact();
}

/** A hard link planted at the lock path is refused before it is written. */
static void test_hard_link_is_refused(void **state)
{
    (void)state;
    wlnk_radio_t r = test_radio();
    make_victim();
    assert_int_equal(link(VICTIM, LOCK_FILE), 0);

    int fd = 99;
    assert_int_equal(wlnk_lock_take(&r, &fd), WLNK_E_IO);
    assert_int_equal(fd, -1);
    assert_victim_intact();
}

/** A FIFO at the lock path is refused, and does not block the claim. */
static void test_fifo_is_refused(void **state)
{
    (void)state;
    wlnk_radio_t r = test_radio();
    assert_int_equal(mkfifo(LOCK_FILE, 0600), 0);

    int fd = 99;
    assert_int_equal(wlnk_lock_take(&r, &fd), WLNK_E_IO);
    assert_int_equal(fd, -1);
}

/** A run directory that is a symlink is never followed. */
static void test_symlinked_run_directory_is_refused(void **state)
{
    (void)state;
    wlnk_radio_t r = test_radio();
    const char *elsewhere = WLNK_RUNDIR "-elsewhere";

    assert_int_equal(rmdir(WLNK_RUNDIR), 0);
    assert_true(mkdir(elsewhere, 0755) == 0 || errno == EEXIST);
    assert_int_equal(symlink(elsewhere, WLNK_RUNDIR), 0);

    int fd = 99;
    int rc = wlnk_lock_take(&r, &fd);
    unlink(WLNK_RUNDIR);
    assert_int_equal(rc, WLNK_E_IO);
    assert_int_equal(fd, -1);
    assert_non_null(strstr(wlnk_err_msg(), "unsafe"));
    assert_int_equal(rmdir(elsewhere), 0); /* nothing was created there */
}

/** Another user's lock cannot be written, but still excludes; only the pid
 * is unknown. */
static void test_read_only_lock_still_excludes(void **state)
{
    (void)state;
    if (geteuid() == 0) {
        skip(); /* root writes a 0444 file anyway */
        return;
    }
    wlnk_radio_t r = test_radio();
    int raw = open(LOCK_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0444);
    assert_true(raw >= 0);
    close(raw);

    int first = -1;
    assert_int_equal(wlnk_lock_take(&r, &first), WLNK_OK);
    int second = 99;
    assert_int_equal(wlnk_lock_take(&r, &second), WLNK_E_BUSY);
    assert_int_equal(second, -1);
    assert_string_equal(wlnk_err_msg(), "phy99: radio held by another claim");
    wlnk_lock_drop(&first);
}

/** Missing arguments, or a radio that never resolved, are refused. */
static void test_needs_arguments(void **state)
{
    (void)state;
    wlnk_radio_t r = test_radio();
    wlnk_radio_t unresolved;
    memset(&unresolved, 0, sizeof(unresolved));

    int fd = 99;
    assert_int_equal(wlnk_lock_take(NULL, &fd), WLNK_E_INVALID_ARG);
    assert_int_equal(fd, -1);
    assert_int_equal(wlnk_lock_take(&r, NULL), WLNK_E_INVALID_ARG);
    fd = 99;
    assert_int_equal(wlnk_lock_take(&unresolved, &fd), WLNK_E_INVALID_ARG);
    assert_int_equal(fd, -1);

    wlnk_lock_drop(NULL);
    wlnk_lock_drop(&fd);
    assert_int_equal(fd, -1);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_creates_run_directory, clean),
        cmocka_unit_test_setup(test_lock_excludes, clean),
        cmocka_unit_test_setup(test_holder_exit_frees_the_lock, clean),
        cmocka_unit_test_setup(test_symlink_is_refused, clean),
        cmocka_unit_test_setup(test_hard_link_is_refused, clean),
        cmocka_unit_test_setup(test_fifo_is_refused, clean),
        cmocka_unit_test_setup(test_symlinked_run_directory_is_refused, clean),
        cmocka_unit_test_setup(test_read_only_lock_still_excludes, clean),
        cmocka_unit_test_setup(test_needs_arguments, clean),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
