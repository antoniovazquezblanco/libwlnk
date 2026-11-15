/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * chandef.h
 * Channel definitions.
 */

#ifndef WLNK_CHANDEF_H
#define WLNK_CHANDEF_H

#include "iface.h"

/**
 * @brief Fill in the segment centres a channel leaves at 0, where exactly
 * one placement fits.
 *
 * A centre the caller gave is kept. Pure: it records no error detail, the
 * calling entry point does.
 *
 * @param in Channel to resolve.
 * @param out Receives the resolved channel.
 * @return WLNK_OK; WLNK_E_INVALID_ARG for a width that needs a centre it
 * was not given, an unknown width or a control frequency outside every
 * band.
 */
int wlnk_chandef_resolve(const wlnk_chandef_t *in, wlnk_chandef_t *out);

#endif /* WLNK_CHANDEF_H */
