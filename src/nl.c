/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * nl.c
 * Netlink transport over libnl.
 */

#include <errno.h>
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

/**
 * Times a libnl call that reports NLE_AGAIN is retried.
 * The socket is blocking, so it can only be a spurious wakeup; the bound
 * keeps a broken socket from spinning forever.
 */
#define NL_AGAIN_RETRIES 3

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
    case -NLE_AGAIN: /* retried at the call; still here means it persisted */
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

struct nl_msg *wlnk_nl_msg(wlnk_nl_t *h, uint8_t cmd, bool dump)
{
    if (h == NULL)
        return NULL;
    struct nl_msg *msg = nlmsg_alloc();
    if (msg == NULL)
        return NULL;
    if (genlmsg_put(msg, 0, 0, h->family, 0, dump ? NLM_F_DUMP : 0, cmd, 0) ==
        NULL) {
        nlmsg_free(msg);
        return NULL;
    }
    return msg;
}

/**
 * State shared by the libnl callbacks of one exchange.
 * done is a flag of its own rather than a sentinel inside status: every
 * value a status can take is a real one, WLNK_OK included.
 */
struct run_state {
    /** Reply callback, or NULL. */
    wlnk_nl_cb cb;
    /** Passed to cb. */
    void *user;
    /** Set once the exchange is over: its last message read, or the
     * transport failed. */
    bool done;
    /** Set once cb stopped; later replies are read and dropped. */
    bool stopped;
    /** The wlnk status, once done; cb's status once stopped. */
    int status;
    /** errno reported by the kernel, 0 if none. */
    int syserrno;
    /** libnl error from the transport, 0 if none. */
    int nle;
};

/**
 * @brief libnl error callback: the kernel answered with an error message.
 * @param nla Sender address, unused.
 * @param nlerr The kernel's error message.
 * @param arg The exchange's run_state.
 * @return NL_STOP: the exchange is over.
 */
static int on_error(struct sockaddr_nl *nla, struct nlmsgerr *nlerr, void *arg)
{
    (void)nla;
    struct run_state *st = arg;
    st->done = true;
    if (st->stopped)
        return NL_STOP;
    st->syserrno = nlerr->error < 0 ? -nlerr->error : nlerr->error;
    switch (st->syserrno) {
    case EPERM:
    case EACCES:
        st->status = WLNK_E_PERM;
        break;
    case ENODEV:
    case ENOENT:
        st->status = WLNK_E_NOT_FOUND;
        break;
    case ENOBUFS:
    case ENOMEM:
        st->status = WLNK_E_NO_MEMORY;
        break;
    case EOPNOTSUPP: /* ENOTSUP is the same value on Linux */
        st->status = WLNK_E_UNSUPPORTED;
        break;
    case EINVAL:
        st->status = WLNK_E_INVALID_ARG;
        break;
    case 0:
        st->status = WLNK_OK;
        break;
    default:
        st->status = WLNK_E_IO;
        break;
    }
    return NL_STOP;
}

/**
 * @brief libnl finish callback: the end of a dump.
 * @param msg The message, unused.
 * @param arg The exchange's run_state.
 * @return NL_SKIP.
 */
static int on_finish(struct nl_msg *msg, void *arg)
{
    (void)msg;
    struct run_state *st = arg;
    if (!st->done) {
        st->done = true;
        if (!st->stopped)
            st->status = WLNK_OK;
    }
    return NL_SKIP;
}

/**
 * @brief libnl ack callback: the kernel acknowledged the request.
 * @param msg The message, unused.
 * @param arg The exchange's run_state.
 * @return NL_STOP: the exchange is over.
 */
static int on_ack(struct nl_msg *msg, void *arg)
{
    (void)msg;
    struct run_state *st = arg;
    if (!st->done) {
        st->done = true;
        if (!st->stopped)
            st->status = WLNK_OK;
    }
    return NL_STOP;
}

/**
 * @brief libnl valid callback: one reply, parsed and handed to the caller.
 * @param msg The reply.
 * @param arg The exchange's run_state.
 * @return NL_SKIP: the exchange is read to its end even after a stop.
 */
static int on_valid(struct nl_msg *msg, void *arg)
{
    struct run_state *st = arg;
    if (st->cb == NULL || st->stopped)
        return NL_SKIP;

    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    wlnk_nl_attrs_t attrs;
    if (nla_parse(attrs, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
                  genlmsg_attrlen(gnlh, 0), NULL) != 0)
        return NL_SKIP; /* a reply we cannot parse is not a fatal error */

    int rc = st->cb(attrs, st->user);
    if (rc != WLNK_OK) {
        /* Stop delivering but keep reading: what the kernel already sent
         * would otherwise reach the next request on this socket. */
        st->stopped = true;
        st->status = rc;
    }
    return NL_SKIP;
}

/**
 * @brief Record the detail of a failed exchange.
 *
 * A libnl error gets libnl's text, a refusal the capability it needs, an
 * errno the system's text and a status of our own its name.
 *
 * @param status Status of the failure.
 * @param syserrno errno reported by the kernel, or 0.
 * @param nle libnl error, or 0.
 * @param step Names the operation.
 * @return @p status.
 */
static int run_fail(int status, int syserrno, int nle, const char *step)
{
    char msg[WLNK_ERR_MSG_MAX];

    if (status == WLNK_E_PERM || syserrno == 0) {
        snprintf(msg, sizeof(msg), "nl80211 %s: %s", step,
                 status == WLNK_E_PERM
                     ? "not permitted (CAP_NET_ADMIN/CAP_NET_RAW)"
                 : nle < 0 ? nl_geterror(nle)
                           : wlnk_strerr(status));
        return wlnk_fail(status, syserrno, msg);
    }
    snprintf(msg, sizeof(msg), "nl80211 %s", step);
    return wlnk_fail_sys(status, syserrno, msg);
}

/**
 * @brief Send one message, retrying a transient NLE_AGAIN.
 * @param h Handle.
 * @param msg Message to send; not consumed.
 * @return libnl's result.
 */
static int nl_send_msg(wlnk_nl_t *h, struct nl_msg *msg)
{
    for (int i = 0;; i++) {
        int rc = nl_send_auto_complete(h->sk, msg);
        if (rc != -NLE_AGAIN || i == NL_AGAIN_RETRIES)
            return rc;
    }
}

int wlnk_nl_run(wlnk_nl_t *h, struct nl_msg *msg, wlnk_nl_cb cb, void *user,
                const char *step)
{
    if (h == NULL || msg == NULL) {
        nlmsg_free(msg);
        return wlnk_fail(WLNK_E_NO_MEMORY, 0, "nl80211: message setup");
    }

    struct nl_cb *nlcb = nl_cb_alloc(NL_CB_DEFAULT);
    if (nlcb == NULL) {
        nlmsg_free(msg);
        return wlnk_fail(WLNK_E_NO_MEMORY, 0, "nl80211: callback setup");
    }

    struct run_state st = {.cb = cb, .user = user};
    nl_cb_err(nlcb, NL_CB_CUSTOM, on_error, &st);
    nl_cb_set(nlcb, NL_CB_FINISH, NL_CB_CUSTOM, on_finish, &st);
    nl_cb_set(nlcb, NL_CB_ACK, NL_CB_CUSTOM, on_ack, &st);
    nl_cb_set(nlcb, NL_CB_VALID, NL_CB_CUSTOM, on_valid, &st);

    int sent = nl_send_msg(h, msg);
    nlmsg_free(msg);
    if (sent < 0) {
        nl_cb_put(nlcb);
        return run_fail(wlnk_nle_status(sent), 0, sent, step);
    }

    int again = 0;
    while (!st.done) {
        int rc = nl_recvmsgs(h->sk, nlcb);
        if (rc == -NLE_AGAIN && again++ < NL_AGAIN_RETRIES)
            continue;
        if (rc < 0 && !st.done) {
            /* libnl reports its own negative NLE_* codes here, never
             * errno; a signal is not retried, so the caller sees it. */
            st.done = true;
            if (!st.stopped) {
                st.status = wlnk_nle_status(rc);
                st.nle = rc;
            }
        }
    }
    nl_cb_put(nlcb);

    if (st.status != WLNK_OK)
        return run_fail(st.status, st.syserrno, st.nle, step);
    return wlnk_ok();
}
