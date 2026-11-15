/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * iface.c
 * Interface enumeration and display names for interface values.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "err.h"
#include "iface.h"

#include "err_internal.h"
#include "nl.h"

#define UNKNOWN "unknown"

const char *wlnk_ifkind_str(uint32_t kind)
{
    switch (kind) {
    case WLNK_IF_STATION:
        return "station";
    case WLNK_IF_AP:
        return "AP";
    case WLNK_IF_MONITOR:
        return "monitor";
    case WLNK_IF_P2P:
        return "P2P";
    case WLNK_IF_MESH:
        return "mesh";
    case WLNK_IF_IBSS:
        return "IBSS";
    case WLNK_IF_OTHER:
        return "other";
    default:
        return UNKNOWN;
    }
}

const char *wlnk_chwidth_str(uint32_t width)
{
    switch (width) {
    case WLNK_CHW_20_NOHT:
        return "20 MHz (no HT)";
    case WLNK_CHW_20:
        return "20 MHz";
    case WLNK_CHW_40:
        return "40 MHz";
    case WLNK_CHW_80:
        return "80 MHz";
    case WLNK_CHW_80P80:
        return "80+80 MHz";
    case WLNK_CHW_160:
        return "160 MHz";
    case WLNK_CHW_320:
        return "320 MHz";
    case WLNK_CHW_5:
        return "5 MHz";
    case WLNK_CHW_10:
        return "10 MHz";
    default:
        return UNKNOWN;
    }
}

const char *wlnk_cap_str(wlnk_capsmask_t cap)
{
    switch (cap) {
    case WLNK_CAP_MONITOR:
        return "monitor";
    case WLNK_CAP_MONITOR_TX:
        return "monitor TX";
    case WLNK_CAP_SCAN:
        return "scan";
    case WLNK_CAP_BAND_2GHZ:
        return "2.4 GHz";
    case WLNK_CAP_BAND_5GHZ:
        return "5 GHz";
    case WLNK_CAP_BAND_6GHZ:
        return "6 GHz";
    case WLNK_CAP_BAND_60GHZ:
        return "60 GHz";
    default:
        return UNKNOWN; /* also zero and several bits at once */
    }
}

const char *wlnk_freq_flag_str(uint32_t flag)
{
    switch (flag) {
    case WLNK_FREQ_DISABLED:
        return "disabled";
    case WLNK_FREQ_NO_IR:
        return "no IR";
    case WLNK_FREQ_RADAR:
        return "radar";
    default:
        return UNKNOWN; /* also zero and several bits at once */
    }
}

/**
 * @brief An nl80211 interface type in the public numbering.
 * @param iftype NL80211_IFTYPE_* value.
 * @return The WLNK_IF_* kind; WLNK_IF_OTHER for types without one.
 */
static uint32_t ifkind(uint32_t iftype)
{
    switch (iftype) {
    case NL80211_IFTYPE_STATION:
        return WLNK_IF_STATION;
    case NL80211_IFTYPE_AP:
    case NL80211_IFTYPE_AP_VLAN:
        return WLNK_IF_AP;
    case NL80211_IFTYPE_MONITOR:
        return WLNK_IF_MONITOR;
    case NL80211_IFTYPE_P2P_CLIENT:
    case NL80211_IFTYPE_P2P_GO:
    case NL80211_IFTYPE_P2P_DEVICE:
        return WLNK_IF_P2P;
    case NL80211_IFTYPE_MESH_POINT:
        return WLNK_IF_MESH;
    case NL80211_IFTYPE_ADHOC:
        return WLNK_IF_IBSS;
    default:
        return WLNK_IF_OTHER;
    }
}

/** Interfaces gathered from one dump. */
struct list_ctx {
    /** Entries so far. */
    wlnk_iface_t *rows;
    /** Entries used. */
    size_t n;
    /** Entries allocated. */
    size_t cap;
};

/**
 * @brief Dump callback: keep one kernel entry if it is a network interface.
 * @param attrs Parsed entry.
 * @param user The list_ctx.
 * @return WLNK_OK, or WLNK_E_NO_MEMORY when the list cannot grow.
 */
static int on_iface(wlnk_nl_attrs_t attrs, void *user)
{
    struct list_ctx *c = user;

    /* P2P-device and NAN entries have no index or name. */
    if (attrs[NL80211_ATTR_IFINDEX] == NULL ||
        attrs[NL80211_ATTR_IFNAME] == NULL)
        return WLNK_OK;

    if (c->n == c->cap) {
        size_t cap = c->cap != 0 ? c->cap * 2 : 8;
        wlnk_iface_t *grown = realloc(c->rows, cap * sizeof(*grown));
        if (grown == NULL)
            return WLNK_E_NO_MEMORY;
        c->rows = grown;
        c->cap = cap;
    }

    wlnk_iface_t *d = &c->rows[c->n++];
    memset(d, 0, sizeof(*d));
    d->wiphy = -1;
    snprintf(d->ifname, sizeof(d->ifname), "%s",
             nla_get_string(attrs[NL80211_ATTR_IFNAME]));
    if (attrs[NL80211_ATTR_MAC] != NULL &&
        nla_len(attrs[NL80211_ATTR_MAC]) == WLNK_MAC_LEN)
        memcpy(d->mac, nla_data(attrs[NL80211_ATTR_MAC]), WLNK_MAC_LEN);
    if (attrs[NL80211_ATTR_WIPHY] != NULL)
        d->wiphy = (int32_t)nla_get_u32(attrs[NL80211_ATTR_WIPHY]);
    d->kind = attrs[NL80211_ATTR_IFTYPE] != NULL
                  ? ifkind(nla_get_u32(attrs[NL80211_ATTR_IFTYPE]))
                  : WLNK_IF_OTHER;
    return WLNK_OK;
}

int wlnk_iface_list(wlnk_iface_t **out, size_t *n)
{
    if (out != NULL)
        *out = NULL;
    if (n != NULL)
        *n = 0;
    if (out == NULL || n == NULL)
        return wlnk_fail(WLNK_E_INVALID_ARG, 0, "list: no output");

    wlnk_nl_t *nl = NULL;
    int rc = wlnk_nl_open(&nl);
    if (rc != WLNK_OK)
        return rc;

    struct list_ctx c = {0};
    struct nl_msg *msg = wlnk_nl_msg(nl, NL80211_CMD_GET_INTERFACE, true);
    rc = wlnk_nl_run(nl, msg, on_iface, &c, "get interfaces");
    wlnk_nl_close(&nl);
    if (rc != WLNK_OK || c.n == 0) {
        free(c.rows);
        return rc;
    }
    *out = c.rows;
    *n = c.n;
    return wlnk_ok();
}

void wlnk_iface_free(wlnk_iface_t *ifaces)
{
    free(ifaces);
}
