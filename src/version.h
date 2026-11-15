/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * version.h
 * Version of the library loaded at run time.
 */

#ifndef WLNK_VERSION_H
#define WLNK_VERSION_H

#include <stdint.h>
#include "api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* (major << 16) | (minor << 8) | patch. */
WLNK_API uint32_t wlnk_version(void);

/* Full version, including any pre-release or build metadata. */
WLNK_API const char *wlnk_version_string(void);

#ifdef __cplusplus
}
#endif

#endif /* WLNK_VERSION_H */
