/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * nl.h
 * Netlink transport. Not installed.
 */

#ifndef WLNK_NL_H
#define WLNK_NL_H

#include <linux/nl80211.h>
#include <netlink/attr.h>
#include <netlink/msg.h>
#include <stdbool.h>
#include <stdint.h>

/** An nl80211 connection: one generic netlink socket and the family id. */
typedef struct wlnk_nl wlnk_nl_t;

/** Parsed top-level attributes of one nl80211 reply. */
typedef struct nlattr *wlnk_nl_attrs_t[NL80211_ATTR_MAX + 1];

/**
 * @brief Called once per reply message.
 * @param attrs Parsed attributes of the reply.
 * @param user User data given to wlnk_nl_run().
 * @return WLNK_OK to go on; any other status stops the exchange and becomes
 * wlnk_nl_run()'s result.
 */
typedef int (*wlnk_nl_cb)(wlnk_nl_attrs_t attrs, void *user);

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

/**
 * @brief Start an nl80211 request.
 * @param h Handle from wlnk_nl_open().
 * @param cmd NL80211_CMD_* command.
 * @param dump Ask for every matching object.
 * @return The message to fill with attributes, or NULL when out of memory.
 */
struct nl_msg *wlnk_nl_msg(wlnk_nl_t *h, uint8_t cmd, bool dump);

/**
 * @brief Send a request and deliver each reply to a callback.
 *
 * Synchronous: every reply is delivered before this returns. A libnl
 * failure maps through wlnk_nle_status() with no errno; a kernel refusal
 * maps to WLNK_E_PERM, WLNK_E_NOT_FOUND, WLNK_E_IO and the like, with its
 * errno recorded.
 *
 * @param h Handle from wlnk_nl_open().
 * @param msg Request from wlnk_nl_msg(); always consumed.
 * @param cb Called for each reply, or NULL to ignore replies.
 * @param user Passed to @p cb.
 * @param step Names the operation in the error detail.
 * @return WLNK_OK, the status @p cb stopped with, or the failure's status.
 */
int wlnk_nl_run(wlnk_nl_t *h, struct nl_msg *msg, wlnk_nl_cb cb, void *user,
                const char *step);

#endif /* WLNK_NL_H */
