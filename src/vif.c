/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * vif.c
 * Virtual interfaces.
 */

#include <errno.h>
#include <linux/version.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include "err.h"
#include "iface.h"

#include "chandef.h"
#include "err_internal.h"
#include "link.h"
#include "nl.h"
#include "vif.h"

/**
 * @brief Check an interface name.
 * @param name Interface name.
 * @return WLNK_OK, or WLNK_E_INVALID_ARG with the detail recorded.
 */
static int check_name(const char *name)
{
    if (name == NULL || name[0] == '\0')
        return wlnk_fail(WLNK_E_INVALID_ARG, 0, "vif: no name");
    if (strlen(name) >= IFNAMSIZ)
        return wlnk_fail(WLNK_E_INVALID_ARG, 0, "vif: name too long");
    return WLNK_OK;
}

int wlnk_vif_add_monitor(wlnk_nl_t *nl, int32_t wiphy, const char *name)
{
    int rc = check_name(name);
    if (rc != WLNK_OK)
        return rc;

    struct nl_msg *msg = wlnk_nl_msg(nl, NL80211_CMD_NEW_INTERFACE, false);
    if (msg != NULL &&
        (nla_put_u32(msg, NL80211_ATTR_WIPHY, (uint32_t)wiphy) != 0 ||
         nla_put_string(msg, NL80211_ATTR_IFNAME, name) != 0 ||
         nla_put_u32(msg, NL80211_ATTR_IFTYPE, NL80211_IFTYPE_MONITOR) != 0)) {
        nlmsg_free(msg);
        msg = NULL;
    }
    rc = wlnk_nl_run(nl, msg, NULL, NULL, "create monitor vif");
    if (rc != WLNK_OK) {
        /* The kernel refuses an interface that does not fit the driver's
         * combinations with EBUSY, and one it has no room for with ENOBUFS
         * or ENOSPC: all are the vif budget, whatever the driver
         * advertised. */
        int e = wlnk_err_syserrno();
        if (e == EBUSY || e == ENOBUFS || e == ENOSPC)
            return wlnk_fail(WLNK_E_VIF_LIMIT, e,
                             "nl80211 create monitor vif: no room for "
                             "another interface on this radio");
        return rc;
    }

    rc = wlnk_if_set_up(name, true);
    if (rc != WLNK_OK) {
        /* Deleting records its own outcome: keep the one that matters. */
        int e = wlnk_err_syserrno();
        char why[WLNK_ERR_MSG_MAX];
        snprintf(why, sizeof(why), "%s", wlnk_err_msg());
        (void)wlnk_vif_del(nl, name);
        return wlnk_fail(rc, e, why);
    }
    return wlnk_ok();
}

int wlnk_vif_del(wlnk_nl_t *nl, const char *name)
{
    int rc = check_name(name);
    if (rc != WLNK_OK)
        return rc;
    unsigned int idx = if_nametoindex(name);
    if (idx == 0)
        return wlnk_ok(); /* already gone: undo has converged */

    struct nl_msg *msg = wlnk_nl_msg(nl, NL80211_CMD_DEL_INTERFACE, false);
    if (msg != NULL && nla_put_u32(msg, NL80211_ATTR_IFINDEX, idx) != 0) {
        nlmsg_free(msg);
        msg = NULL;
    }
    rc = wlnk_nl_run(nl, msg, NULL, NULL, "delete interface");
    if (rc == WLNK_E_NOT_FOUND)
        return wlnk_ok(); /* gone between the lookup and the request */
    return rc;
}

/**
 * @brief A public channel width in nl80211's numbering.
 * @param width WLNK_CHW_* value.
 * @return The NL80211_CHAN_WIDTH_* value, or -1 where the kernel headers
 * this was built against have none.
 */
static int nl_chwidth(uint32_t width)
{
    switch (width) {
    case WLNK_CHW_20_NOHT:
        return NL80211_CHAN_WIDTH_20_NOHT;
    case WLNK_CHW_20:
        return NL80211_CHAN_WIDTH_20;
    case WLNK_CHW_40:
        return NL80211_CHAN_WIDTH_40;
    case WLNK_CHW_80:
        return NL80211_CHAN_WIDTH_80;
    case WLNK_CHW_80P80:
        return NL80211_CHAN_WIDTH_80P80;
    case WLNK_CHW_160:
        return NL80211_CHAN_WIDTH_160;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
    case WLNK_CHW_320: /* in the kernel headers since 5.18 */
        return NL80211_CHAN_WIDTH_320;
#endif
    case WLNK_CHW_5:
        return NL80211_CHAN_WIDTH_5;
    case WLNK_CHW_10:
        return NL80211_CHAN_WIDTH_10;
    default:
        return -1;
    }
}

int wlnk_vif_set_channel(wlnk_nl_t *nl, const char *name,
                         const wlnk_chandef_t *cd)
{
    int rc = check_name(name);
    if (rc != WLNK_OK)
        return rc;
    wlnk_chandef_t res;
    if (wlnk_chandef_resolve(cd, &res) != WLNK_OK)
        return wlnk_fail(WLNK_E_INVALID_ARG, 0,
                         "channel: invalid, or needs a centre it was not "
                         "given");
    int width = nl_chwidth(res.width);
    if (width < 0)
        return wlnk_fail(WLNK_E_UNSUPPORTED, 0,
                         "channel: width unknown to this build");
    unsigned int idx = if_nametoindex(name);
    if (idx == 0)
        return wlnk_fail(WLNK_E_NOT_FOUND, 0, "channel: no such interface");

    struct nl_msg *msg = wlnk_nl_msg(nl, NL80211_CMD_SET_CHANNEL, false);
    if (msg != NULL &&
        (nla_put_u32(msg, NL80211_ATTR_IFINDEX, idx) != 0 ||
         nla_put_u32(msg, NL80211_ATTR_WIPHY_FREQ, res.freq_mhz) != 0 ||
         nla_put_u32(msg, NL80211_ATTR_CHANNEL_WIDTH, (uint32_t)width) != 0 ||
         nla_put_u32(msg, NL80211_ATTR_CENTER_FREQ1, res.center1_mhz) != 0 ||
         (res.center2_mhz != 0 &&
          nla_put_u32(msg, NL80211_ATTR_CENTER_FREQ2, res.center2_mhz) != 0))) {
        nlmsg_free(msg);
        msg = NULL;
    }
    rc = wlnk_nl_run(nl, msg, NULL, NULL, "set channel");
    /* mac80211 refuses to tune while a non-monitor interface on the radio
     * is up: the caller's to see, not a generic I/O failure. */
    if (rc != WLNK_OK && wlnk_err_syserrno() == EBUSY)
        return wlnk_fail_sys(WLNK_E_BUSY, EBUSY,
                             "channel: another interface on the radio is up");
    return rc;
}
