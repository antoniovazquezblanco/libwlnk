/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * version.c
 * Run-time view of the library version.
 */

#include "version.h"

#include "version_internal.h"

uint32_t wlnk_version(void)
{
    return ((uint32_t)WLNK_VERSION_MAJOR << 16) |
           ((uint32_t)WLNK_VERSION_MINOR << 8) | (uint32_t)WLNK_VERSION_PATCH;
}

const char *wlnk_version_string(void)
{
    return WLNK_VERSION_STRING;
}
