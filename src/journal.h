/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * journal.h
 * Per-radio undo records.
 */

#ifndef WLNK_JOURNAL_H
#define WLNK_JOURNAL_H

#include <stdio.h>
#include "radio.h"

/**
 * @brief An open record.
 */
typedef struct wlnk_journal {
    /** The record, or NULL when closed. */
    FILE *fp;
} wlnk_journal_t;

/**
 * @brief Whether a radio has a record left by an earlier claim.
 *
 * A released radio has none, so one means a claim did not finish. Never
 * creates the run directory.
 *
 * @param r The radio.
 * @return WLNK_OK when there is none; WLNK_E_STALE_STATE when there is, the
 * message naming the pid that left it when known.
 */
int wlnk_journal_check(const wlnk_radio_t *r);

/**
 * @brief Start a radio's record. The radio's lock must be held.
 * @param r The radio.
 * @param j Receives the open record.
 * @return WLNK_OK; WLNK_E_STALE_STATE when it already has one.
 */
int wlnk_journal_start(const wlnk_radio_t *r, wlnk_journal_t *j);

/**
 * @brief Record a step before it is applied.
 *
 * On disk on return, so a claim that dies once the step is applied leaves
 * it recorded.
 *
 * @param j The record.
 * @param kind What the step does, one word.
 * @param arg What it does it to, one word.
 * @return WLNK_OK; WLNK_E_INVALID_ARG when either is not one word or is too
 * long to read back.
 */
int wlnk_journal_step(wlnk_journal_t *j, const char *kind, const char *arg);

/**
 * @brief Close a record, leaving it in place.
 * @param j The record. A closed one is left alone.
 */
void wlnk_journal_close(wlnk_journal_t *j);

#endif /* WLNK_JOURNAL_H */
