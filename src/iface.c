/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * iface.c
 * Interface enumeration and display names for interface values.
 */

#include <linux/version.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "err.h"
#include "iface.h"

#include "err_internal.h"
#include "nl.h"

/* Interface names cross the API in WLNK_IFNAME_MAX buffers, so they must
 * hold any name the kernel accepts. */
_Static_assert(WLNK_IFNAME_MAX == IFNAMSIZ, "WLNK_IFNAME_MAX is IFNAMSIZ");

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

/**
 * @brief An nl80211 channel width in the public numbering.
 * @param nl_width NL80211_CHAN_WIDTH_* value.
 * @return The WLNK_CHW_* width. Widths without one read as 20 MHz without
 * HT, the conservative reading.
 */
static uint32_t chwidth(uint32_t nl_width)
{
    switch (nl_width) {
    case NL80211_CHAN_WIDTH_20_NOHT:
        return WLNK_CHW_20_NOHT;
    case NL80211_CHAN_WIDTH_20:
        return WLNK_CHW_20;
    case NL80211_CHAN_WIDTH_40:
        return WLNK_CHW_40;
    case NL80211_CHAN_WIDTH_80:
        return WLNK_CHW_80;
    case NL80211_CHAN_WIDTH_80P80:
        return WLNK_CHW_80P80;
    case NL80211_CHAN_WIDTH_160:
        return WLNK_CHW_160;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
    case NL80211_CHAN_WIDTH_320: /* in the kernel headers since 5.18 */
        return WLNK_CHW_320;
#endif
    case NL80211_CHAN_WIDTH_5:
        return WLNK_CHW_5;
    case NL80211_CHAN_WIDTH_10:
        return WLNK_CHW_10;
    default:
        return WLNK_CHW_20_NOHT;
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

    /* A channel is reported only while the interface is tuned. */
    if (attrs[NL80211_ATTR_WIPHY_FREQ] == NULL)
        return WLNK_OK;
    d->chan.freq_mhz = nla_get_u32(attrs[NL80211_ATTR_WIPHY_FREQ]);
    if (attrs[NL80211_ATTR_CHANNEL_WIDTH] != NULL)
        d->chan.width = chwidth(nla_get_u32(attrs[NL80211_ATTR_CHANNEL_WIDTH]));
    if (attrs[NL80211_ATTR_CENTER_FREQ1] != NULL)
        d->chan.center1_mhz = nla_get_u32(attrs[NL80211_ATTR_CENTER_FREQ1]);
    if (attrs[NL80211_ATTR_CENTER_FREQ2] != NULL)
        d->chan.center2_mhz = nla_get_u32(attrs[NL80211_ATTR_CENTER_FREQ2]);
    return WLNK_OK;
}

/**
 * @brief An nl80211 band as a capability bit.
 *
 * nl80211 numbers its bands in the order they were added to the kernel,
 * which is not the public order: map rather than cast.
 *
 * @param nl_band NL80211_BAND_* value.
 * @return The WLNK_CAP_BAND_* bit, or 0 for bands without one.
 */
static wlnk_capsmask_t band_cap(uint32_t nl_band)
{
    switch (nl_band) {
    case NL80211_BAND_2GHZ:
        return WLNK_CAP_BAND_2GHZ;
    case NL80211_BAND_5GHZ:
        return WLNK_CAP_BAND_5GHZ;
    case NL80211_BAND_6GHZ:
        return WLNK_CAP_BAND_6GHZ;
    case NL80211_BAND_60GHZ:
        return WLNK_CAP_BAND_60GHZ;
    default:
        return 0;
    }
}

/** One radio's capabilities, gathered across the messages of a split dump. */
struct wiphy_ctx {
    /** The radio asked for. */
    int32_t want;
    /** Whether any message was about it. */
    bool seen;
    /** What it can do so far. */
    wlnk_caps_t caps;
};

/**
 * @brief Read the supported interface types: monitor, and so monitor TX.
 * @param c Context to update.
 * @param types NL80211_ATTR_SUPPORTED_IFTYPES.
 */
static void read_iftypes(struct wiphy_ctx *c, struct nlattr *types)
{
    struct nlattr *t;
    int rem;
    nla_for_each_nested(t, types, rem)
    {
        if (nla_type(t) == NL80211_IFTYPE_MONITOR)
            c->caps.caps |= WLNK_CAP_MONITOR | WLNK_CAP_MONITOR_TX;
    }
}

/**
 * @brief Read the largest number of interfaces any combination allows.
 * @param c Context to update.
 * @param combs NL80211_ATTR_INTERFACE_COMBINATIONS.
 */
static void read_combinations(struct wiphy_ctx *c, struct nlattr *combs)
{
    struct nlattr *comb;
    int rem;
    nla_for_each_nested(comb, combs, rem)
    {
        struct nlattr *attrs[NUM_NL80211_IFACE_COMB];
        if (nla_parse_nested(attrs, NUM_NL80211_IFACE_COMB - 1, comb, NULL) !=
            0)
            continue;
        /* No policy is passed, so the length is checked here. */
        if (attrs[NL80211_IFACE_COMB_MAXNUM] == NULL ||
            nla_len(attrs[NL80211_IFACE_COMB_MAXNUM]) != sizeof(uint32_t))
            continue;
        uint32_t maxnum = nla_get_u32(attrs[NL80211_IFACE_COMB_MAXNUM]);
        if (maxnum > c->caps.vif_budget)
            c->caps.vif_budget = maxnum;
    }
}

/**
 * @brief Read the bands the radio supports.
 * @param c Context to update.
 * @param bands NL80211_ATTR_WIPHY_BANDS.
 */
static void read_bands(struct wiphy_ctx *c, struct nlattr *bands)
{
    struct nlattr *band;
    int rem;
    nla_for_each_nested(band, bands, rem)
    {
        c->caps.caps |= band_cap((uint32_t)nla_type(band));
    }
}

/**
 * @brief Split dump callback: add one message about the wanted radio.
 * @param attrs Parsed message.
 * @param user The wiphy_ctx.
 * @return WLNK_OK.
 */
static int on_wiphy(wlnk_nl_attrs_t attrs, void *user)
{
    struct wiphy_ctx *c = user;
    if (attrs[NL80211_ATTR_WIPHY] == NULL)
        return WLNK_OK;
    if ((int32_t)nla_get_u32(attrs[NL80211_ATTR_WIPHY]) != c->want)
        return WLNK_OK; /* older kernels ignore the dump filter */
    c->seen = true;

    if (attrs[NL80211_ATTR_SUPPORTED_IFTYPES] != NULL)
        read_iftypes(c, attrs[NL80211_ATTR_SUPPORTED_IFTYPES]);
    /* NL80211_ATTR_SUPPORTED_COMMANDS is not the scan signal: the kernel
     * builds that list from driver callbacks that do not include scanning,
     * so a scanning radio still leaves TRIGGER_SCAN out of it. The number
     * of SSIDs a scan may carry tracks the driver's scan support, and it is
     * zero where there is none. */
    if (attrs[NL80211_ATTR_MAX_NUM_SCAN_SSIDS] != NULL &&
        nla_get_u8(attrs[NL80211_ATTR_MAX_NUM_SCAN_SSIDS]) > 0)
        c->caps.caps |= WLNK_CAP_SCAN;
    if (attrs[NL80211_ATTR_INTERFACE_COMBINATIONS] != NULL)
        read_combinations(c, attrs[NL80211_ATTR_INTERFACE_COMBINATIONS]);
    if (attrs[NL80211_ATTR_WIPHY_BANDS] != NULL)
        read_bands(c, attrs[NL80211_ATTR_WIPHY_BANDS]);
    return WLNK_OK;
}

/**
 * @brief One radio's capabilities.
 *
 * The dump is requested split, as iw does: a modern radio's description
 * does not fit one netlink message, and without splitting the kernel
 * answers large radios with ENOBUFS. Its attributes then arrive spread
 * over several messages, so they are accumulated.
 *
 * @param nl Open handle.
 * @param wiphy The radio.
 * @param out Receives its capabilities.
 * @return WLNK_OK; WLNK_E_NOT_FOUND when the radio is gone.
 */
static int radio_caps(wlnk_nl_t *nl, int32_t wiphy, wlnk_caps_t *out)
{
    struct wiphy_ctx c = {.want = wiphy};
    struct nl_msg *msg = wlnk_nl_msg(nl, NL80211_CMD_GET_WIPHY, true);
    if (msg != NULL &&
        (nla_put_u32(msg, NL80211_ATTR_WIPHY, (uint32_t)wiphy) != 0 ||
         nla_put_flag(msg, NL80211_ATTR_SPLIT_WIPHY_DUMP) != 0)) {
        nlmsg_free(msg);
        msg = NULL;
    }
    int rc = wlnk_nl_run(nl, msg, on_wiphy, &c, "get wiphy");
    if (rc != WLNK_OK)
        return rc;
    if (!c.seen)
        return wlnk_fail(WLNK_E_NOT_FOUND, 0, "nl80211: no such radio");
    *out = c.caps;
    return WLNK_OK;
}

/**
 * @brief Fill each row's radio capabilities, asking each radio once.
 *
 * A radio removed since the interface dump takes its rows with it. Any
 * other failure fails the list: a row without capabilities would read as a
 * radio that has none.
 *
 * @param nl Open handle.
 * @param rows Rows to fill, compacted in place.
 * @param n Number of rows; on return, the number kept.
 * @return WLNK_OK or the failure's status.
 */
static int fill_radios(wlnk_nl_t *nl, wlnk_iface_t *rows, size_t *n)
{
    size_t kept = 0;
    for (size_t i = 0; i < *n; i++) {
        wlnk_iface_t row = rows[i];
        size_t twin = 0;
        while (twin < kept && rows[twin].wiphy != row.wiphy)
            twin++;
        if (twin < kept) {
            row.radio = rows[twin].radio;
        } else {
            int rc = radio_caps(nl, row.wiphy, &row.radio);
            if (rc == WLNK_E_NOT_FOUND)
                continue;
            if (rc != WLNK_OK)
                return rc;
        }
        rows[kept++] = row;
    }
    *n = kept;
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
    if (rc == WLNK_OK)
        rc = fill_radios(nl, c.rows, &c.n);
    wlnk_nl_close(&nl);
    if (rc != WLNK_OK || c.n == 0) {
        free(c.rows);
        return rc == WLNK_OK ? wlnk_ok() : rc;
    }
    *out = c.rows;
    *n = c.n;
    return wlnk_ok();
}

void wlnk_iface_free(wlnk_iface_t *ifaces)
{
    free(ifaces);
}

int wlnk_iface_info(const char *ifname, wlnk_iface_t *out)
{
    if (out != NULL)
        memset(out, 0, sizeof(*out));
    if (ifname == NULL || out == NULL)
        return wlnk_fail(WLNK_E_INVALID_ARG, 0,
                         "info: no interface name or output");

    unsigned int idx = if_nametoindex(ifname);
    if (idx == 0)
        return wlnk_fail(WLNK_E_NOT_FOUND, 0, "info: no such interface");

    wlnk_nl_t *nl = NULL;
    int rc = wlnk_nl_open(&nl);
    if (rc != WLNK_OK)
        return rc;

    /* The same requests a list row is built from, for one index. */
    struct list_ctx c = {0};
    struct nl_msg *msg = wlnk_nl_msg(nl, NL80211_CMD_GET_INTERFACE, false);
    if (msg != NULL && nla_put_u32(msg, NL80211_ATTR_IFINDEX, idx) != 0) {
        nlmsg_free(msg);
        msg = NULL;
    }
    rc = wlnk_nl_run(nl, msg, on_iface, &c, "get interface");
    if (rc == WLNK_OK)
        rc = fill_radios(nl, c.rows, &c.n);
    wlnk_nl_close(&nl);
    if (rc == WLNK_OK && c.n == 0)
        rc = wlnk_fail(WLNK_E_NOT_FOUND, 0, "info: not a wireless interface");
    if (rc == WLNK_OK)
        *out = c.rows[0];
    free(c.rows);
    return rc == WLNK_OK ? wlnk_ok() : rc;
}
