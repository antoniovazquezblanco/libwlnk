/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * cxx_headers_test.cpp
 * Check that the public headers compile and link as C++.
 *
 * Every public function is referenced, so a header missing its extern "C"
 * guard fails at link time (mangled names). No library behavior runs.
 */

#include "err.h"
#include "iface.h"
#include "version.h"

/* void (*)(void) is the cast target -Wcast-function-type accepts. */
using wlnk_fn = void (*)();

/* External linkage keeps the table, and its symbol references, in the
 * object even though nothing reads it. Add new public functions here. */
extern const wlnk_fn wlnk_cxx_fns[];
const wlnk_fn wlnk_cxx_fns[] = {
    reinterpret_cast<wlnk_fn>(&wlnk_strerr),
    reinterpret_cast<wlnk_fn>(&wlnk_ifkind_str),
    reinterpret_cast<wlnk_fn>(&wlnk_chwidth_str),
    reinterpret_cast<wlnk_fn>(&wlnk_cap_str),
    reinterpret_cast<wlnk_fn>(&wlnk_freq_flag_str),
    reinterpret_cast<wlnk_fn>(&wlnk_version),
    reinterpret_cast<wlnk_fn>(&wlnk_version_string),
};

int main()
{
    return 0;
}
