/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * nl.h
 * Netlink transport. Not installed.
 */

#ifndef WLNK_NL_H
#define WLNK_NL_H

/**
 * @brief Status for a libnl result.
 *
 * libnl reports its own negative NLE_* codes, not errno.
 *
 * @param nle libnl result.
 * @return WLNK_OK for 0 or a positive result, else the matching status.
 */
int wlnk_nle_status(int nle);

#endif /* WLNK_NL_H */
