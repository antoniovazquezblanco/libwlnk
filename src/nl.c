/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * nl.c
 * Netlink transport over libnl.
 */

#include <netlink/errno.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>
#include <netlink/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include "err.h"

#include "err_internal.h"
#include "nl.h"

/**
 * Netlink receive buffer.
 * One wiphy's split-dump parts are a few kB each so
 * 256 kB keeps a busy dump from overrunning the socket.
 */
#define NL_RX_BUF (256 * 1024)

struct wlnk_nl {
    /** Generic netlink socket. */
    struct nl_sock *sk;
    /** Resolved nl80211 family id. */
    int family;
};

int wlnk_nle_status(int nle)
{
    if (nle >= 0)
        return WLNK_OK;

    switch (nle) {
    case -NLE_INTR:
        return WLNK_E_INTERRUPTED;
    case -NLE_NOMEM:
        return WLNK_E_NO_MEMORY;
    case -NLE_PERM:
    case -NLE_NOACCESS:
        return WLNK_E_PERM;
    case -NLE_OBJ_NOTFOUND:
    case -NLE_NODEV:
        return WLNK_E_NOT_FOUND;
    case -NLE_OPNOTSUPP:
        return WLNK_E_UNSUPPORTED;
    case -NLE_INVAL:
    case -NLE_RANGE:
        return WLNK_E_INVALID_ARG;
    default:
        return WLNK_E_IO;
    }
}

/**
 * @brief Failure from libnl itself.
 *
 * The message is libnl's text; an NLE code is not an errno, so no errno is
 * recorded.
 *
 * @param nle libnl's negative NLE_* result.
 * @param what Where it happened.
 * @return The matching status.
 */
static int nle_fail(int nle, const char *what)
{
    char msg[WLNK_ERR_MSG_MAX];
    int status = wlnk_nle_status(nle);

    if (status == WLNK_E_PERM)
        snprintf(msg, sizeof(msg), "%s: %s", what,
                 "not permitted (CAP_NET_ADMIN/CAP_NET_RAW)");
    else
        snprintf(msg, sizeof(msg), "%s: %s", what, nl_geterror(nle));
    return wlnk_fail(status, 0, msg);
}

int wlnk_nl_open(wlnk_nl_t **out)
{
    if (out == NULL)
        return wlnk_fail(WLNK_E_INVALID_ARG, 0, "nl: no output handle");
    *out = NULL;

    wlnk_nl_t *h = calloc(1, sizeof(*h));
    if (h == NULL)
        return wlnk_fail(WLNK_E_NO_MEMORY, 0, "nl: out of memory");

    h->sk = nl_socket_alloc();
    if (h->sk == NULL) {
        free(h);
        return wlnk_fail(WLNK_E_NO_MEMORY, 0, "nl: socket allocation");
    }
    int rc = genl_connect(h->sk);
    if (rc != 0) {
        nl_socket_free(h->sk);
        free(h);
        return nle_fail(rc, "netlink: connect");
    }
    rc = nl_socket_set_buffer_size(h->sk, NL_RX_BUF, NL_RX_BUF);
    if (rc < 0) {
        nl_socket_free(h->sk);
        free(h);
        return nle_fail(rc, "netlink: buffer size");
    }

    h->family = genl_ctrl_resolve(h->sk, "nl80211");
    if (h->family < 0) {
        int nle = h->family;
        nl_socket_free(h->sk);
        free(h);
        if (nle == -NLE_OBJ_NOTFOUND)
            return wlnk_fail(WLNK_E_NOT_FOUND, 0,
                             "nl80211: family not present (no cfg80211?)");
        return nle_fail(nle, "nl80211: family");
    }
    *out = h;
    return wlnk_ok();
}

void wlnk_nl_close(wlnk_nl_t **h)
{
    if (h == NULL || *h == NULL)
        return;
    nl_socket_free((*h)->sk);
    free(*h);
    *h = NULL;
}
