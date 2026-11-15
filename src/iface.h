/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * iface.h
 * Interface enumeration, kinds, channel widths, capabilities and frequency
 * flags.
 */

#ifndef WLNK_IFACE_H
#define WLNK_IFACE_H

#include <stddef.h>
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
 * @brief A channel as nl80211 describes it. Frequencies in MHz.
 */
typedef struct wlnk_chandef {
    /** Control (primary) channel. */
    uint32_t freq_mhz;
    /** Channel width, a WLNK_CHW_* value. */
    uint32_t width;
    /** Centre of the (first) segment, or 0 when not known. */
    uint32_t center1_mhz;
    /** Centre of the second segment (80+80 only), or 0. */
    uint32_t center2_mhz;
} wlnk_chandef_t;

/**
 * @brief Radio capabilities, a mask of WLNK_CAP_* bits.
 */
typedef uint32_t wlnk_capsmask_t;

/** Monitor interfaces can be created. */
#define WLNK_CAP_MONITOR    (1u << 0)
/** Frames can be transmitted on monitor interfaces. nl80211 has no separate
 * signal for it, so it follows WLNK_CAP_MONITOR; a driver may still refuse
 * a frame. */
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

/**
 * @brief What a radio can do.
 */
typedef struct wlnk_caps {
    /** WLNK_CAP_* bits. */
    wlnk_capsmask_t caps;
    /** Most interfaces the radio allows at once; 0 when the driver reports
     * no interface combination. */
    uint32_t vif_budget;
} wlnk_caps_t;

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

/**
 * @brief One frequency a radio supports.
 */
typedef struct wlnk_freq {
    /** Centre frequency in MHz. */
    uint32_t freq_mhz;
    /** WLNK_FREQ_* bits from the regulatory domain. */
    uint32_t flags;
} wlnk_freq_t;

/** Size of wlnk_iface_t::ifname: the kernel's IFNAMSIZ, NUL included. */
#define WLNK_IFNAME_MAX 16
/** Size of wlnk_iface_t::mac. */
#define WLNK_MAC_LEN    6

/**
 * @brief One wireless network interface.
 */
typedef struct wlnk_iface {
    /** Interface name, NUL-terminated. */
    char ifname[WLNK_IFNAME_MAX];
    /** MAC address. */
    uint8_t mac[WLNK_MAC_LEN];
    /** nl80211 index of the radio, shared by every interface on it. */
    int32_t wiphy;
    /** Interface kind, a WLNK_IF_* value. */
    uint32_t kind;
    /** Current channel. All 0 when the interface is not tuned. */
    wlnk_chandef_t chan;
    /** What its radio can do, the same for every interface on it. */
    wlnk_caps_t radio;
} wlnk_iface_t;

/**
 * @brief Every wireless network interface, in the kernel's order, each with
 * its radio's capabilities.
 *
 * Kernel entries that are no network interface (P2P-device, NAN) have no
 * name to capture on and are left out, and so are the interfaces of a radio
 * removed while the call runs. Needs no privileges.
 *
 * @param out Receives a caller-owned array, NULL when there are none. Free
 * it with wlnk_iface_free().
 * @param n Receives the number of entries.
 * @return WLNK_OK; WLNK_E_NOT_FOUND when the kernel has no nl80211.
 */
WLNK_API int wlnk_iface_list(wlnk_iface_t **out, size_t *n);

/**
 * @brief Free a wlnk_iface_list() result.
 * @param ifaces Array to free; NULL is a no-op.
 */
WLNK_API void wlnk_iface_free(wlnk_iface_t *ifaces);

/**
 * @brief One wireless network interface by name, as wlnk_iface_list()
 * reports it. Needs no privileges.
 * @param ifname Interface name.
 * @param out Receives the interface; zeroed on failure.
 * @return WLNK_OK; WLNK_E_NOT_FOUND when there is no such wireless
 * interface, or the kernel has no nl80211.
 */
WLNK_API int wlnk_iface_info(const char *ifname, wlnk_iface_t *out);

/**
 * @brief Frequencies the radio behind an interface supports, with their
 * regulatory flags, in ascending order. Needs no privileges.
 * @param ifname Interface name.
 * @param out Receives a caller-owned array, NULL when there are none. Free
 * it with wlnk_freq_free().
 * @param n Receives the number of entries.
 * @return WLNK_OK; WLNK_E_NOT_FOUND when there is no such wireless
 * interface, or the kernel has no nl80211.
 */
WLNK_API int wlnk_freq_list(const char *ifname, wlnk_freq_t **out, size_t *n);

/**
 * @brief Free a wlnk_freq_list() result.
 * @param freqs Array to free; NULL is a no-op.
 */
WLNK_API void wlnk_freq_free(wlnk_freq_t *freqs);

#ifdef __cplusplus
}
#endif

#endif /* WLNK_IFACE_H */
