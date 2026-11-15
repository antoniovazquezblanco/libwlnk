/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * vif.c
 * Virtual interfaces.
 */

#include <errno.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include "err.h"

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
