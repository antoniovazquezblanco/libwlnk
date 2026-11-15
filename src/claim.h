/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * claim.h
 * Exclusive claims on radios.
 */

#ifndef WLNK_CLAIM_H
#define WLNK_CLAIM_H

#include "api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief A claim on a radio, from wlnk_claim() until wlnk_release().
 */
typedef struct wlnk_claim wlnk_claim_t;

/**
 * @brief A monitor interface of a claim, from wlnk_claim_monitor() until
 * the claim is released.
 */
typedef struct wlnk_vif wlnk_vif_t;

/**
 * @brief Claim the radio behind an interface.
 *
 * The radio is what is claimed, so every interface on it is affected: each
 * non-monitor interface whose link is up is taken down, since mac80211 will
 * not tune a radio for monitoring while one is up. Monitor interfaces are
 * left alone. Each change is recorded before it is made, so the claim of a
 * process that dies can still be undone, with wlnk_recover().
 *
 * Network managers on the radio are not suspended yet: keep them off it.
 *
 * @param ifname An interface on the radio.
 * @param h Receives the claim; NULL on failure.
 * @return WLNK_OK; WLNK_E_BUSY when another claim holds the radio;
 * WLNK_E_STALE_STATE when an earlier claim on it was never released;
 * WLNK_E_RF_KILL when it is blocked; WLNK_E_PERM without CAP_NET_ADMIN;
 * WLNK_E_NOT_FOUND when @p ifname is not a wireless interface.
 */
WLNK_API int wlnk_claim(const char *ifname, wlnk_claim_t **h);

/**
 * @brief Release a claim, putting its radio back as it was found.
 *
 * Its monitor interfaces are deleted, so close whatever captures on them
 * first. The claim is gone on return, even on failure: what could not be
 * undone stays recorded, for wlnk_recover().
 *
 * @param h The claim; NULL on return.
 * @return WLNK_OK; the first step that could not be undone otherwise.
 */
WLNK_API int wlnk_release(wlnk_claim_t **h);

/**
 * @brief Undo an earlier claim on a radio that was never released, because
 * its process died or its release failed.
 *
 * Never done by wlnk_claim(): undoing someone else's changes is the
 * caller's decision. Having nothing to undo is no error.
 *
 * @param ifname An interface on the radio.
 * @return WLNK_OK when nothing is left to undo; WLNK_E_BUSY when a live
 * claim holds the radio, which is then left alone; otherwise the first step
 * that could not be undone, which stays recorded for a retry.
 */
WLNK_API int wlnk_recover(const char *ifname);

/**
 * @brief Create a monitor interface on the claimed radio.
 *
 * Each call creates another, as many as the radio has room for, all
 * deleted when the claim is released.
 *
 * @param h The claim.
 * @param vif Receives the interface, owned by the claim; NULL on failure.
 * @return WLNK_OK; WLNK_E_VIF_LIMIT when the radio has no room for another;
 * WLNK_E_PERM without CAP_NET_ADMIN.
 */
WLNK_API int wlnk_claim_monitor(wlnk_claim_t *h, wlnk_vif_t **vif);

/**
 * @brief Name of a claim's monitor interface, the one to capture on.
 *
 * It is an ordinary monitor interface: libpcap, a packet socket or any
 * capture tool can use it.
 *
 * @param vif The interface.
 * @return Its name, valid until the claim is released; NULL for NULL.
 */
WLNK_API const char *wlnk_vif_ifname(const wlnk_vif_t *vif);

#ifdef __cplusplus
}
#endif

#endif /* WLNK_CLAIM_H */
