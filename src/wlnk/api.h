/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * api.h
 * Export marker for public functions.
 */

#ifndef WLNK_API_H
#define WLNK_API_H

#if defined(__GNUC__) && !defined(_WIN32) && !defined(__CYGWIN__)
#define WLNK_API __attribute__((visibility("default")))
#else
#define WLNK_API
#endif

#endif /* WLNK_API_H */
