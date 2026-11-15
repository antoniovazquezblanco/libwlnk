/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * err_detail_test.c
 * Check the internal wlnk_ok()/wlnk_fail() helpers behind the error detail
 * rule: every call that returns a status records its outcome for the
 * calling thread, and only for it.
 *
 * Internal test: linked from the library's own err.c object.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "err.h"

#include "err_internal.h"

/** Success clears what an earlier failure left. */
static void test_ok_clears(void **state)
{
    (void)state;
    wlnk_fail(WLNK_E_IO, EIO, "earlier");
    assert_int_equal(wlnk_ok(), WLNK_OK);
    assert_int_equal(wlnk_err_syserrno(), 0);
    assert_string_equal(wlnk_err_msg(), "");
}

/** Failure returns the status and records errno and message. */
static void test_fail_records(void **state)
{
    (void)state;
    assert_int_equal(wlnk_fail(WLNK_E_IO, 19, "nl80211: no device"), WLNK_E_IO);
    assert_int_equal(wlnk_err_syserrno(), 19);
    assert_string_equal(wlnk_err_msg(), "nl80211: no device");
}

/** A failure replaces every part of an earlier one: a NULL message leaves
 * msg empty and no errno leaves 0, never stale values. */
static void test_fail_replaces(void **state)
{
    (void)state;
    wlnk_fail(WLNK_E_IO, EIO, "earlier");
    wlnk_fail(WLNK_E_TIMEOUT, 0, NULL);
    assert_int_equal(wlnk_err_syserrno(), 0);
    assert_string_equal(wlnk_err_msg(), "");
}

/** Long messages are truncated and stay NUL-terminated. */
static void test_fail_truncates(void **state)
{
    (void)state;
    char long_msg[WLNK_ERR_MSG_MAX * 2];
    memset(long_msg, 'x', sizeof(long_msg) - 1);
    long_msg[sizeof(long_msg) - 1] = '\0';

    wlnk_fail(WLNK_E_IO, 0, long_msg);
    assert_int_equal(strlen(wlnk_err_msg()), WLNK_ERR_MSG_MAX - 1);
    assert_int_equal(strncmp(wlnk_err_msg(), long_msg, WLNK_ERR_MSG_MAX - 1),
                     0);
}

/** A system failure keeps the errno and puts its text after the step, so
 * the status message followed by msg shows the cause. */
static void test_fail_sys_appends_text(void **state)
{
    (void)state;
    char text[WLNK_ERR_MSG_MAX];
    char want[WLNK_ERR_MSG_MAX * 2];
    snprintf(want, sizeof(want), "claim: open lock: %s",
             wlnk_errno_text(ENOENT, text, sizeof(text)));

    assert_int_equal(wlnk_fail_sys(WLNK_E_IO, ENOENT, "claim: open lock"),
                     WLNK_E_IO);
    assert_int_equal(wlnk_err_syserrno(), ENOENT);
    assert_true(strlen(text) > 0);
    assert_string_equal(wlnk_err_msg(), want);
}

/** With no errno there is no system text: msg is the step alone. */
static void test_fail_sys_without_errno(void **state)
{
    (void)state;
    wlnk_fail_sys(WLNK_E_NO_MEMORY, 0, "claim: read record");
    assert_int_equal(wlnk_err_syserrno(), 0);
    assert_string_equal(wlnk_err_msg(), "claim: read record");
}

/** Refusals are WLNK_E_PERM, everything else an I/O error. */
static void test_errno_status(void **state)
{
    (void)state;
    assert_int_equal(wlnk_errno_status(EPERM), WLNK_E_PERM);
    assert_int_equal(wlnk_errno_status(EACCES), WLNK_E_PERM);
    assert_int_equal(wlnk_errno_status(ENOENT), WLNK_E_IO);
    assert_int_equal(wlnk_errno_status(EIO), WLNK_E_IO);
}

/** The text always fits its buffer, even for an errno with no name. */
static void test_errno_text_fits(void **state)
{
    (void)state;
    char buf[WLNK_ERR_MSG_MAX];
    assert_true(strlen(wlnk_errno_text(99999, buf, sizeof(buf))) > 0);

    char tiny[4];
    memset(tiny, 'x', sizeof(tiny));
    wlnk_errno_text(EACCES, tiny, sizeof(tiny));
    assert_non_null(memchr(tiny, '\0', sizeof(tiny)));
    assert_string_equal(wlnk_errno_text(EACCES, buf, 0), "");
}

/** What another thread sees, recorded there and checked here. */
struct seen {
    int syserrno;
    char msg[WLNK_ERR_MSG_MAX];
};

static void *other_thread(void *arg)
{
    struct seen *seen = arg;
    /* Nothing recorded on this thread yet: the main thread's failure must
     * not show here. */
    seen->syserrno = wlnk_err_syserrno();
    snprintf(seen->msg, sizeof(seen->msg), "%s", wlnk_err_msg());
    wlnk_fail(WLNK_E_BUSY, EBUSY, "other thread");
    return NULL;
}

/** Each thread has its own detail: neither sees the other's failure. */
static void test_detail_is_per_thread(void **state)
{
    (void)state;
    wlnk_fail(WLNK_E_IO, EIO, "main thread");

    struct seen seen;
    pthread_t t;
    assert_int_equal(pthread_create(&t, NULL, other_thread, &seen), 0);
    assert_int_equal(pthread_join(t, NULL), 0);

    assert_int_equal(seen.syserrno, 0);
    assert_string_equal(seen.msg, "");
    assert_int_equal(wlnk_err_syserrno(), EIO);
    assert_string_equal(wlnk_err_msg(), "main thread");
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ok_clears),
        cmocka_unit_test(test_fail_records),
        cmocka_unit_test(test_fail_replaces),
        cmocka_unit_test(test_fail_truncates),
        cmocka_unit_test(test_fail_sys_appends_text),
        cmocka_unit_test(test_fail_sys_without_errno),
        cmocka_unit_test(test_errno_status),
        cmocka_unit_test(test_errno_text_fits),
        cmocka_unit_test(test_detail_is_per_thread),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
