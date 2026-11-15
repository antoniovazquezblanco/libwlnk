/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * link.h
 * Interface link state.
 */

#ifndef WLNK_LINK_H
#define WLNK_LINK_H

#include <stdbool.h>

/**
 * @brief Whether an interface's link is up. Needs no privileges.
 * @param ifname Interface name.
 * @param up Receives the state; false on failure.
 * @return WLNK_OK; WLNK_E_NOT_FOUND when there is no such interface.
 */
int wlnk_if_is_up(const char *ifname, bool *up);

/**
 * @brief Bring an interface's link up or down; the interface itself is
 * untouched. Needs CAP_NET_ADMIN unless it is already in that state.
 * @param ifname Interface name.
 * @param up Wanted state.
 * @return WLNK_OK; WLNK_E_NOT_FOUND when there is no such interface;
 * WLNK_E_PERM without the privilege.
 */
int wlnk_if_set_up(const char *ifname, bool up);

#endif /* WLNK_LINK_H */
