/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * iface.h
 * Interface kinds, channel widths, capabilities and frequency flags.
 */

#ifndef WLNK_IFACE_H
#define WLNK_IFACE_H

#include <stdint.h>
#include "api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Interface kinds.
 */
enum wlnk_ifkind {
    /** Station (client). */
    WLNK_IF_STATION = 0,
    /** Access point. */
    WLNK_IF_AP = 1,
    /** Monitor. */
    WLNK_IF_MONITOR = 2,
    /** Wi-Fi Direct (P2P) client or group owner. */
    WLNK_IF_P2P = 3,
    /** Mesh point. */
    WLNK_IF_MESH = 4,
    /** Ad-hoc (IBSS). */
    WLNK_IF_IBSS = 5,
    /** Any other kind. */
    WLNK_IF_OTHER = 6
};

/**
 * @brief Display name of an interface kind.
 * @param kind WLNK_IF_* value.
 * @return Static name, such as "station"; "unknown" for unknown values.
 */
WLNK_API const char *wlnk_ifkind_str(uint32_t kind);

/**
 * @brief Channel widths.
 */
enum wlnk_chwidth {
    /** 20 MHz without HT, the usual monitor setting. */
    WLNK_CHW_20_NOHT = 0,
    /** 20 MHz. */
    WLNK_CHW_20 = 1,
    /** 40 MHz. */
    WLNK_CHW_40 = 2,
    /** 80 MHz. */
    WLNK_CHW_80 = 3,
    /** 80+80 MHz. */
    WLNK_CHW_80P80 = 4,
    /** 160 MHz. */
    WLNK_CHW_160 = 5,
    /** 320 MHz. */
    WLNK_CHW_320 = 6,
    /** 5 MHz. */
    WLNK_CHW_5 = 7,
    /** 10 MHz. */
    WLNK_CHW_10 = 8
};

/**
 * @brief Display name of a channel width.
 * @param width WLNK_CHW_* value.
 * @return Static name, such as "20 MHz (no HT)"; "unknown" for unknown
 * values.
 */
WLNK_API const char *wlnk_chwidth_str(uint32_t width);

/**
 * @brief Radio capabilities, a mask of WLNK_CAP_* bits.
 */
typedef uint32_t wlnk_capsmask_t;

/** Monitor interfaces can be created. */
#define WLNK_CAP_MONITOR    (1u << 0)
/** Frames can be transmitted on monitor interfaces. */
#define WLNK_CAP_MONITOR_TX (1u << 1)
/** Scanning is supported. */
#define WLNK_CAP_SCAN       (1u << 2)
/** 2.4 GHz band. */
#define WLNK_CAP_BAND_2GHZ  (1u << 3)
/** 5 GHz band. */
#define WLNK_CAP_BAND_5GHZ  (1u << 4)
/** 6 GHz band. */
#define WLNK_CAP_BAND_6GHZ  (1u << 5)
/** 60 GHz band. */
#define WLNK_CAP_BAND_60GHZ (1u << 6)

/**
 * @brief Display name of one capability bit.
 * @param cap One WLNK_CAP_* bit.
 * @return Static name, such as "monitor"; "unknown" for zero, several bits
 * or an unknown bit.
 */
WLNK_API const char *wlnk_cap_str(wlnk_capsmask_t cap);

/** Frequency not usable here at all. */
#define WLNK_FREQ_DISABLED (1u << 0)
/** No transmission may be initiated: receive only. */
#define WLNK_FREQ_NO_IR    (1u << 1)
/** Radar detection (DFS) required. */
#define WLNK_FREQ_RADAR    (1u << 2)

/**
 * @brief Display name of one frequency flag.
 * @param flag One WLNK_FREQ_* bit.
 * @return Static name, such as "no IR"; "unknown" for zero, several bits or
 * an unknown bit.
 */
WLNK_API const char *wlnk_freq_flag_str(uint32_t flag);

#ifdef __cplusplus
}
#endif

#endif /* WLNK_IFACE_H */
