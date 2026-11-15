/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * journal.c
 * Per-radio undo records.
 *
 * A claim records each change it makes to a radio in
 * <run dir>/<radio id>.journal, one "kind arg" line per change, written and
 * flushed before the change is applied. A claim that dies mid-append leaves
 * a partial last line, for a change that was never applied. The first line
 * is a header: the format and the pid that wrote the record.
 *
 * Not fsynced: the run directory is tmpfs, the risk covered is a crashed
 * process, and a reboot clears both the record and what it describes.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "err.h"

#include "err_internal.h"
#include "journal.h"
#include "rundir.h"

/** Format of the records this build writes and reads. */
#define JOURNAL_MAGIC "wlnk1"
/** Room for a step's kind, as records are read back. */
#define KIND_MAX      24
/** Room for a step's argument, as records are read back. */
#define ARG_MAX       256
/** Room for a record's name in the run directory. */
#define NAME_MAX_LEN  (WLNK_RADIO_ID_MAX + 16)

/**
 * @brief Name of a radio's record in the run directory.
 * @param r The radio.
 * @param out Receives the name.
 * @param cap Size of @p out.
 */
static void record_name(const wlnk_radio_t *r, char *out, size_t cap)
{
    /* The identity is safe as a file name, so this stays in the directory. */
    snprintf(out, cap, "%s.journal", r->id);
}

/**
 * @brief The pid in a record's header.
 * @param fd The record.
 * @return The pid; 0 when it has no complete header of this format.
 */
static uint32_t record_pid(int fd)
{
    char buf[32];
    ssize_t n = pread(fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0)
        return 0;
    buf[n] = '\0';
    static const char prefix[] = JOURNAL_MAGIC " pid=";
    if (strncmp(buf, prefix, sizeof(prefix) - 1) != 0)
        return 0;
    char *end = NULL;
    long pid = strtol(buf + sizeof(prefix) - 1, &end, 10);
    /* A header cut short by a dying writer may hold only part of the pid. */
    if (*end != '\n' && *end != ' ')
        return 0;
    return pid > 0 && pid <= INT32_MAX ? (uint32_t)pid : 0;
}

/**
 * @brief Whether a step's kind or argument reads back as written.
 * @param s The word.
 * @param cap Room for it when read back.
 * @return True when it is not empty, fits, and has no separator in it.
 */
static bool is_word(const char *s, size_t cap)
{
    if (s == NULL || s[0] == '\0' || strnlen(s, cap) >= cap)
        return false;
    return strpbrk(s, " \n") == NULL;
}

int wlnk_journal_check(const wlnk_radio_t *r)
{
    if (r == NULL || r->id[0] == '\0')
        return wlnk_fail(WLNK_E_INVALID_ARG, 0, "journal: no radio");

    int dfd = -1;
    int rc = wlnk_rundir_open(false, &dfd);
    if (rc != WLNK_OK || dfd < 0)
        return rc; /* no run directory, no record */

    char name[NAME_MAX_LEN];
    record_name(r, name, sizeof(name));
    /* Anything at the path counts, so a claim reports it rather than
     * writing over it. Only a header of ours names a pid. */
    int fd = openat(dfd, name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    int e = errno;
    close(dfd);
    if (fd < 0 && e == ENOENT)
        return wlnk_ok();
    uint32_t pid = 0;
    if (fd >= 0) {
        pid = record_pid(fd);
        close(fd);
    }

    /* The phy name is bounded so the rest always fits. */
    char msg[WLNK_ERR_MSG_MAX];
    if (pid != 0)
        snprintf(msg, sizeof(msg),
                 "%.40s: radio claimed by pid %u and not released", r->phy,
                 (unsigned)pid);
    else
        snprintf(msg, sizeof(msg), "%.40s: radio claimed and not released",
                 r->phy);
    return wlnk_fail(WLNK_E_STALE_STATE, 0, msg);
}

int wlnk_journal_start(const wlnk_radio_t *r, wlnk_journal_t *j)
{
    if (j != NULL)
        j->fp = NULL;
    if (r == NULL || j == NULL || r->id[0] == '\0')
        return wlnk_fail(WLNK_E_INVALID_ARG, 0, "journal: no radio");

    int dfd = -1;
    int rc = wlnk_rundir_open(true, &dfd);
    if (rc != WLNK_OK)
        return rc;

    char name[NAME_MAX_LEN];
    record_name(r, name, sizeof(name));
    /* Exclusive, so the file is a new one of ours: never one left behind
     * or planted, and never followed through a symlink. */
    int fd = openat(dfd, name,
                    O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0644);
    if (fd < 0) {
        int e = errno;
        close(dfd);
        if (e == EEXIST)
            return wlnk_fail(WLNK_E_STALE_STATE, 0,
                             "journal: record already exists");
        return wlnk_fail_sys(wlnk_errno_status(e), e, "journal: create");
    }

    FILE *fp = fdopen(fd, "w");
    if (fp == NULL) {
        int e = errno;
        close(fd);
        (void)unlinkat(dfd, name, 0);
        close(dfd);
        return wlnk_fail_sys(wlnk_errno_status(e), e, "journal: create");
    }
    if (fprintf(fp, JOURNAL_MAGIC " pid=%ld\n", (long)getpid()) < 0 ||
        fflush(fp) != 0) {
        int e = errno;
        fclose(fp);
        (void)unlinkat(dfd, name, 0);
        close(dfd);
        return wlnk_fail_sys(WLNK_E_IO, e, "journal: write");
    }
    close(dfd);
    j->fp = fp;
    return wlnk_ok();
}

int wlnk_journal_step(wlnk_journal_t *j, const char *kind, const char *arg)
{
    if (j == NULL || j->fp == NULL)
        return wlnk_fail(WLNK_E_INVALID_ARG, 0, "journal: not open");
    if (!is_word(kind, KIND_MAX) || !is_word(arg, ARG_MAX))
        return wlnk_fail(WLNK_E_INVALID_ARG, 0,
                         "journal: a step is two short words");
    /* Flushed, so it is on disk before it is applied. */
    if (fprintf(j->fp, "%s %s\n", kind, arg) < 0 || fflush(j->fp) != 0) {
        int e = errno;
        return wlnk_fail_sys(WLNK_E_IO, e, "journal: write");
    }
    return wlnk_ok();
}

void wlnk_journal_close(wlnk_journal_t *j)
{
    if (j == NULL || j->fp == NULL)
        return;
    fclose(j->fp);
    j->fp = NULL;
}
