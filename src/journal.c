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
 * Undo reads it back newest first. A record holding anything else this
 * build would not have written is refused whole and left as it is. After a
 * partial undo the record is replaced, never edited in place, so it always
 * lists every step still to undo.
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
/** Message for a record path that is not a plain file. */
#define UNSAFE_RECORD "journal: unsafe record (not a plain file)"

/** A step, as read back. */
struct step {
    /** What it did. */
    char kind[KIND_MAX];
    /** What it did it to. */
    char arg[ARG_MAX];
    /** Whether its undo failed, so it stays recorded. */
    bool keep;
};

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

/**
 * @brief Create a record and write its header.
 *
 * Exclusive, so the file is a new one of ours: never one left behind or
 * planted, and never followed through a symlink.
 *
 * @param dfd The run directory.
 * @param name The record's name in it.
 * @param out Receives the open record.
 * @return 0, or the errno of the failure; a record half made is removed.
 */
static int create_record(int dfd, const char *name, FILE **out)
{
    *out = NULL;
    int fd = openat(dfd, name,
                    O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0644);
    if (fd < 0)
        return errno;
    FILE *fp = fdopen(fd, "w");
    int e = 0;
    if (fp == NULL) {
        e = errno;
        close(fd);
    } else if (fprintf(fp, JOURNAL_MAGIC " pid=%ld\n", (long)getpid()) < 0 ||
               fflush(fp) != 0) {
        e = errno != 0 ? errno : EIO;
        fclose(fp);
    }
    if (e != 0) {
        (void)unlinkat(dfd, name, 0);
        return e;
    }
    *out = fp;
    return 0;
}

/**
 * @brief Read a record's steps, oldest first.
 *
 * A last line without its newline was being written when its writer died:
 * that step was never applied, so it is dropped.
 *
 * @param fp The record.
 * @param out Receives the steps; free() them.
 * @param n Receives how many.
 * @return WLNK_OK; WLNK_E_UNSUPPORTED when it holds anything else this
 * build would not have written.
 */
static int read_steps(FILE *fp, struct step **out, size_t *n)
{
    static const char magic[] = JOURNAL_MAGIC " ";
    struct step *steps = NULL;
    size_t count = 0;
    size_t cap = 0;
    /* The longest line this build writes, its newline and a NUL. */
    char line[KIND_MAX + ARG_MAX + 1];
    const char *why = NULL;
    int status = WLNK_E_UNSUPPORTED;
    bool header = true;

    while (why == NULL && fgets(line, sizeof(line), fp) != NULL) {
        size_t len = strlen(line);
        if (len == 0 || line[len - 1] != '\n') {
            /* Only a last line cut short is let through, and a header cut
             * short still has to start like ours. */
            size_t m = len < sizeof(magic) - 1 ? len : sizeof(magic) - 1;
            if (len == 0 || !feof(fp))
                why = "journal: malformed record";
            else if (header && strncmp(line, magic, m) != 0)
                why = "journal: not a wlnk record";
            break;
        }
        line[--len] = '\0';
        if (header) {
            header = false;
            if (strncmp(line, magic, sizeof(magic) - 1) != 0)
                why = "journal: not a wlnk record";
            continue;
        }

        /* Exactly as wlnk_journal_step() writes it: two words. */
        char *sep = strchr(line, ' ');
        if (sep != NULL)
            *sep = '\0';
        if (sep == NULL || !is_word(line, KIND_MAX) ||
            !is_word(sep + 1, ARG_MAX)) {
            why = "journal: malformed step";
            break;
        }
        if (count == cap) {
            size_t grow = cap != 0 ? cap * 2 : 8;
            struct step *bigger = realloc(steps, grow * sizeof(*bigger));
            if (bigger == NULL) {
                status = WLNK_E_NO_MEMORY;
                why = "journal: out of memory";
                break;
            }
            steps = bigger;
            cap = grow;
        }
        struct step *s = &steps[count++];
        memcpy(s->kind, line, strlen(line) + 1);
        memcpy(s->arg, sep + 1, strlen(sep + 1) + 1);
        s->keep = false;
    }

    if (why == NULL && ferror(fp)) {
        int e = errno != 0 ? errno : EIO;
        free(steps);
        return wlnk_fail_sys(WLNK_E_IO, e, "journal: read");
    }
    if (why != NULL) {
        free(steps);
        return wlnk_fail(status, 0, why);
    }
    *out = steps;
    *n = count;
    return wlnk_ok();
}

/**
 * @brief Read a radio's record.
 * @param dfd The run directory.
 * @param name The record's name in it.
 * @param out Receives its steps, oldest first; free() them. None when there
 * is no record.
 * @param n Receives how many.
 * @return WLNK_OK; WLNK_E_IO when it is not a plain file;
 * WLNK_E_UNSUPPORTED when this build cannot read it.
 */
static int load(int dfd, const char *name, struct step **out, size_t *n)
{
    *out = NULL;
    *n = 0;
    int fd = openat(dfd, name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) {
        int e = errno;
        if (e == ENOENT)
            return wlnk_ok();
        if (e == ELOOP)
            return wlnk_fail(WLNK_E_IO, 0, UNSAFE_RECORD);
        return wlnk_fail_sys(wlnk_errno_status(e), e, "journal: read");
    }
    if (!wlnk_rundir_is_plain(fd)) {
        close(fd);
        return wlnk_fail(WLNK_E_IO, 0, UNSAFE_RECORD);
    }
    FILE *fp = fdopen(fd, "r");
    if (fp == NULL) {
        int e = errno;
        close(fd);
        return wlnk_fail_sys(wlnk_errno_status(e), e, "journal: read");
    }
    int rc = read_steps(fp, out, n);
    fclose(fp);
    return rc;
}

/**
 * @brief Replace a record with the steps still to undo, or remove it when
 * none are left.
 *
 * The new record is written beside the old one and renamed over it, so the
 * record is always one or the other: a rewrite cut short leaves the old one
 * whole, and its steps undone since undo again harmlessly.
 *
 * @param dfd The run directory.
 * @param name The record's name in it.
 * @param steps Its steps; those to keep are written, in order.
 * @param n How many.
 * @return WLNK_OK, or the failure.
 */
static int rewrite(int dfd, const char *name, const struct step *steps,
                   size_t n)
{
    size_t kept = 0;
    for (size_t i = 0; i < n; i++)
        if (steps[i].keep)
            kept++;
    if (kept == 0) {
        if (unlinkat(dfd, name, 0) != 0 && errno != ENOENT) {
            int e = errno;
            return wlnk_fail_sys(WLNK_E_IO, e, "journal: remove");
        }
        return wlnk_ok();
    }

    char tmp[NAME_MAX_LEN + 4];
    snprintf(tmp, sizeof(tmp), "%s.new", name);
    /* A rewrite that died leaves this behind, and another writer of the
     * directory can plant it: removed, then created exclusively. */
    if (unlinkat(dfd, tmp, 0) != 0 && errno != ENOENT) {
        int e = errno;
        return wlnk_fail_sys(WLNK_E_IO, e, "journal: rewrite");
    }
    FILE *fp = NULL;
    int e = create_record(dfd, tmp, &fp);
    for (size_t i = 0; e == 0 && i < n; i++)
        if (steps[i].keep &&
            fprintf(fp, "%s %s\n", steps[i].kind, steps[i].arg) < 0)
            e = errno != 0 ? errno : EIO;
    if (fp != NULL && fclose(fp) != 0 && e == 0)
        e = errno;
    if (e == 0 && renameat(dfd, tmp, dfd, name) != 0)
        e = errno;
    if (e != 0) {
        (void)unlinkat(dfd, tmp, 0);
        return wlnk_fail_sys(WLNK_E_IO, e, "journal: rewrite");
    }
    return wlnk_ok();
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
    int e = create_record(dfd, name, &j->fp);
    close(dfd);
    if (e == EEXIST)
        return wlnk_fail(WLNK_E_STALE_STATE, 0,
                         "journal: record already exists");
    if (e != 0)
        return wlnk_fail_sys(wlnk_errno_status(e), e, "journal: create");
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

int wlnk_journal_undo(const wlnk_radio_t *r, wlnk_journal_undo_fn undo,
                      void *user)
{
    if (r == NULL || r->id[0] == '\0' || undo == NULL)
        return wlnk_fail(WLNK_E_INVALID_ARG, 0, "journal: no radio or undo");

    int dfd = -1;
    int rc = wlnk_rundir_open(false, &dfd);
    if (rc != WLNK_OK || dfd < 0)
        return rc; /* no run directory, no record */

    char name[NAME_MAX_LEN];
    record_name(r, name, sizeof(name));
    struct step *steps = NULL;
    size_t n = 0;
    rc = load(dfd, name, &steps, &n);
    if (rc != WLNK_OK) {
        close(dfd);
        return rc;
    }

    /* The first failure is kept as it was: the steps after it overwrite
     * the error detail. */
    int first = WLNK_OK;
    int first_errno = 0;
    char first_msg[WLNK_ERR_MSG_MAX] = "";
    for (size_t i = n; i-- > 0;) {
        wlnk_ok();
        int step_rc = undo(user, steps[i].kind, steps[i].arg);
        if (step_rc == WLNK_OK)
            continue;
        steps[i].keep = true;
        if (first == WLNK_OK) {
            first = step_rc;
            first_errno = wlnk_err_syserrno();
            snprintf(first_msg, sizeof(first_msg), "%s", wlnk_err_msg());
        }
    }

    rc = rewrite(dfd, name, steps, n);
    free(steps);
    close(dfd);
    /* Ahead of a failed rewrite, which leaves the old record and with it
     * every step still to undo. */
    if (first != WLNK_OK)
        return wlnk_fail(first, first_errno, first_msg);
    return rc;
}
