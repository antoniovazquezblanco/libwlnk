/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * nl.h
 * Netlink transport. Not installed.
 */

#ifndef WLNK_NL_H
#define WLNK_NL_H

/** An nl80211 connection: one generic netlink socket and the family id. */
typedef struct wlnk_nl wlnk_nl_t;

/**
 * @brief Status for a libnl result.
 *
 * libnl reports its own negative NLE_* codes, not errno.
 *
 * @param nle libnl result.
 * @return WLNK_OK for 0 or a positive result, else the matching status.
 */
int wlnk_nle_status(int nle);

/**
 * @brief Open a netlink socket and resolve the nl80211 family.
 * @param out Receives the handle; NULL on failure.
 * @return WLNK_OK; WLNK_E_NOT_FOUND when the kernel has no nl80211.
 */
int wlnk_nl_open(wlnk_nl_t **out);

/**
 * @brief Close a handle from wlnk_nl_open().
 * @param h Handle to close, NULL on return. NULL or a NULL handle is a no-op.
 */
void wlnk_nl_close(wlnk_nl_t **h);

#endif /* WLNK_NL_H */
