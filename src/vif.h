/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * vif.h
 * Virtual interfaces.
 */

#ifndef WLNK_VIF_H
#define WLNK_VIF_H

#include <stdint.h>
#include "nl.h"

/**
 * @brief Create a monitor interface on a radio and bring it up.
 *
 * A monitor interface that never comes up delivers nothing, so if bringing
 * it up fails it is deleted again and that failure is reported. Needs
 * CAP_NET_ADMIN.
 *
 * @param nl Open handle.
 * @param wiphy The radio.
 * @param name Name for the new interface.
 * @return WLNK_OK; WLNK_E_VIF_LIMIT when the radio has no room for another
 * interface; WLNK_E_PERM without the privilege.
 */
int wlnk_vif_add_monitor(wlnk_nl_t *nl, int32_t wiphy, const char *name);

/**
 * @brief Delete an interface by name.
 *
 * An interface already gone is WLNK_OK, so undoing converges. Needs
 * CAP_NET_ADMIN.
 *
 * @param nl Open handle.
 * @param name Interface name.
 * @return WLNK_OK; WLNK_E_PERM without the privilege.
 */
int wlnk_vif_del(wlnk_nl_t *nl, const char *name);

#endif /* WLNK_VIF_H */
