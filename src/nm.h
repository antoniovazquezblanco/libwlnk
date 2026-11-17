/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * nm.h
 * NetworkManager's hold on interfaces.
 */

#ifndef WLNK_NM_H
#define WLNK_NM_H

#include <stdbool.h>

/**
 * @brief Whether NetworkManager manages an interface. Needs no privileges.
 * @param ifname Interface name.
 * @param managed Receives the answer: false also when NetworkManager is not
 * running or does not know the interface; false on failure.
 * @return WLNK_OK, or the failure.
 */
int wlnk_nm_is_managed(const char *ifname, bool *managed);

/**
 * @brief Make NetworkManager manage an interface, or leave it alone.
 *
 * Runtime only: never written to a profile, and gone at reboot. With
 * NetworkManager not running, or not knowing the interface, there is
 * nothing to change.
 *
 * @param ifname Interface name.
 * @param managed Wanted state.
 * @return WLNK_OK; WLNK_E_PERM when NetworkManager's policy refuses.
 */
int wlnk_nm_set_managed(const char *ifname, bool managed);

#endif /* WLNK_NM_H */
