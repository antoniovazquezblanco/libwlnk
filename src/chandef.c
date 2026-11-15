/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * chandef.c
 * Channel definitions.
 *
 * Channel-centre derivation follows 802.11 Annex E as iw does in util.c
 * get_cf1(): outside 2.4 GHz every width has one block grid, so the centre
 * follows from the control frequency; on 2.4 GHz a 40 MHz channel can sit
 * above or below its control channel, and in the middle of the band both
 * fit, which is the one case the caller must resolve.
 */

#include <stdbool.h>
#include <stdint.h>
#include "err.h"
#include "iface.h"

#include "chandef.h"

/**
 * Lower edge of the 20 MHz grid of 5 GHz channels 36 to 144.
 * The 5 GHz band has two grids: U-NII-3 and above (channel 149 at 5745)
 * restarts 5 MHz off this one, so a single edge would put every wide
 * channel above 5725 on the wrong block.
 */
#define EDGE_5GHZ_LOW  5170
/** Lower edge of the 20 MHz grid of 5 GHz channels 149 and up. */
#define EDGE_5GHZ_HIGH 5735
/** Lower edge of the 6 GHz grid, 10 MHz below channel 1 (5955). */
#define EDGE_6GHZ      5945
/** Channel 149, the first of U-NII-3. */
#define FIRST_UNII3    5745

/**
 * @brief Centre of the block a frequency sits in.
 * @param freq Control frequency.
 * @param edge Lower edge of its grid.
 * @param span Block width.
 * @param center Receives the block's centre.
 * @return Whether @p freq is on the grid at all.
 */
static bool block_center(uint32_t freq, uint32_t edge, uint32_t span,
                         uint32_t *center)
{
    if (freq < edge)
        return false;
    uint32_t off = freq - edge;
    *center = edge + span / 2 + span * (off / span);
    return true;
}

/**
 * @brief Edge of the grid a frequency belongs to.
 * @param freq Control frequency.
 * @param edge Receives the grid's lower edge.
 * @return Whether a grid is defined there: 2.4 GHz, the 4.9 GHz block and
 * 60 GHz all need the caller's centre.
 */
static bool grid_edge(uint32_t freq, uint32_t *edge)
{
    if (freq >= 5000 && freq < FIRST_UNII3)
        *edge = EDGE_5GHZ_LOW;
    else if (freq >= FIRST_UNII3 && freq < EDGE_6GHZ)
        *edge = EDGE_5GHZ_HIGH;
    else if (freq >= EDGE_6GHZ && freq <= 7125)
        *edge = EDGE_6GHZ;
    else
        return false;
    return true;
}

/**
 * @brief The 2.4 GHz 40 MHz centre, where only one of the two placements
 * fits.
 *
 * The band numbers channels 1 to 13 from 2407 MHz in 5 MHz steps; channel
 * 14 (2484) is outside that run and has no 40 MHz placement at all.
 *
 * @param freq Control frequency.
 * @param center Receives the centre.
 * @return Whether exactly one placement fits.
 */
static bool center_2ghz_40(uint32_t freq, uint32_t *center)
{
    uint32_t chan = (freq >= 2412 && freq <= 2472) ? (freq - 2407) / 5 : 0;
    if (chan >= 1 && chan <= 4)
        *center = freq + 10; /* only HT40+ fits below channel 5 */
    else if (chan >= 10 && chan <= 13)
        *center = freq - 10; /* only HT40- fits above channel 9 */
    else
        return false; /* channels 5 to 9 (and 14): the caller must say which */
    return true;
}

int wlnk_chandef_resolve(const wlnk_chandef_t *in, wlnk_chandef_t *out)
{
    if (in == NULL || out == NULL)
        return WLNK_E_INVALID_ARG;
    *out = *in;
    if (in->freq_mhz == 0)
        return WLNK_E_INVALID_ARG;

    uint32_t span;
    switch (in->width) {
    case WLNK_CHW_20_NOHT:
    case WLNK_CHW_20:
    case WLNK_CHW_5:
    case WLNK_CHW_10:
        /* One segment, centred on the control channel itself. */
        out->center1_mhz = in->freq_mhz;
        out->center2_mhz = 0;
        return WLNK_OK;
    case WLNK_CHW_40:
        span = 40;
        break;
    case WLNK_CHW_80:
        span = 80;
        break;
    case WLNK_CHW_160:
        span = 160;
        break;
    case WLNK_CHW_80P80:
        /* Two independent 80 MHz segments: neither centre is derivable. */
        if (in->center1_mhz == 0 || in->center2_mhz == 0)
            return WLNK_E_INVALID_ARG;
        return WLNK_OK;
    case WLNK_CHW_320:
        /* The 320 MHz blocks overlap, so the control channel never picks
         * one on its own. */
        if (in->center1_mhz == 0)
            return WLNK_E_INVALID_ARG;
        out->center2_mhz = 0;
        return WLNK_OK;
    default:
        return WLNK_E_INVALID_ARG;
    }

    out->center2_mhz = 0;
    if (in->center1_mhz != 0)
        return WLNK_OK; /* the caller already said which block */

    if (span == 40 && in->freq_mhz < 3000)
        return center_2ghz_40(in->freq_mhz, &out->center1_mhz)
                   ? WLNK_OK
                   : WLNK_E_INVALID_ARG;

    uint32_t edge;
    if (!grid_edge(in->freq_mhz, &edge))
        return WLNK_E_INVALID_ARG;
    return block_center(in->freq_mhz, edge, span, &out->center1_mhz)
               ? WLNK_OK
               : WLNK_E_INVALID_ARG;
}
