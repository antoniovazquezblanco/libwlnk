/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * iface.c
 * Display names for interface values.
 */

#include "iface.h"

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
