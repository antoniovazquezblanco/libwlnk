/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * radio.h
 * Radios: their identity and rfkill state.
 */

#ifndef WLNK_RADIO_H
#define WLNK_RADIO_H

#include <linux/nl80211.h>
#include <stdint.h>
#include "iface.h"

/** Size of wlnk_radio_t::id. Bus paths are the longest identities in
 * practice (PCI "0000:02:00.0", USB "1-1.4:1.0"), plus an ordinal. */
#define WLNK_RADIO_ID_MAX 72

/** Size of wlnk_radio_t::phy. A renamed phy can be longer than an
 * interface name, up to the nl80211 limit. */
#define WLNK_PHYNAME_MAX (NL80211_WIPHY_NAME_MAXLEN + 1)

/**
 * @brief The radio behind an interface.
 */
typedef struct wlnk_radio {
    /** Identity: the bus path of the phy's device, which survives a driver
     * reload that renames the phy and renumbers its wiphy. */
    char id[WLNK_RADIO_ID_MAX];
    /** Phy name: phyN, or what it was renamed to. */
    char phy[WLNK_PHYNAME_MAX];
    /** The interface it was resolved from. */
    char ifname[WLNK_IFNAME_MAX];
    /** nl80211 index, or -1 when not known. */
    int32_t wiphy;
} wlnk_radio_t;

/**
 * @brief The radio behind an interface, from sysfs. Needs no privileges.
 * @param ifname Interface name.
 * @param out Receives the radio.
 * @return WLNK_OK; WLNK_E_NOT_FOUND when it is not a wireless interface.
 */
int wlnk_radio_resolve(const char *ifname, wlnk_radio_t *out);

/**
 * @brief Whether a radio is blocked by rfkill. Needs no privileges.
 *
 * Read, never changed: unblocking is the decision of whoever runs the
 * program, and a hard block is a physical switch.
 *
 * @param r The radio.
 * @return WLNK_OK when it is usable or has no rfkill switch; WLNK_E_RF_KILL
 * when it is hard or soft blocked.
 */
int wlnk_rfkill_check(const wlnk_radio_t *r);

#endif /* WLNK_RADIO_H */
