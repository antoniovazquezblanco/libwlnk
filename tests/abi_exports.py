#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
#
# abi_exports.py
# Exported symbols of the shared library == functions the public headers
# declare with WLNK_API: every one of them is exported, and nothing else.
#
# Usage: abi_exports.py <nm> <libwlnk.so> <public header>...

import re
import subprocess
import sys

# Symbols every ELF shared library defines; not part of our ABI.
LINKER_SYMBOLS = {"_init", "_fini", "_edata", "_end", "__bss_start"}

# The linker synthesises __start_NAME / __stop_NAME for any section whose
# name is a C identifier. Clang's AddressSanitizer collects instrumented
# globals into an "asan_globals" section, so a clang sanitizer build
# exports __start_asan_globals and __stop_asan_globals. They are section
# bounds the linker made up, never ours.
SECTION_BOUNDS = re.compile(r"^__(?:start|stop)_\w+$")

DECLARATION = re.compile(r"\bWLNK_API\b[^;{}]*?\b(wlnk_\w+)\s*\(")


def declared(headers):
    names = set()
    for path in headers:
        with open(path, encoding="utf-8") as f:
            names.update(DECLARATION.findall(f.read()))
    return names


def exported(nm, lib):
    out = subprocess.run([nm, "-D", "--defined-only", lib], check=True,
                         capture_output=True, text=True).stdout
    names = set()
    for line in out.splitlines():
        fields = line.split()
        # "<address> <type> <name>"; lines without an address are
        # undefined symbols, not exports.
        if len(fields) == 3:
            name = fields[2].split("@")[0]
            if not SECTION_BOUNDS.match(name):
                names.add(name)
    return names - LINKER_SYMBOLS


def main(argv):
    if len(argv) < 4:
        print("usage: abi_exports.py <nm> <lib> <header>...")
        return 2
    want = declared(argv[3:])
    have = exported(argv[1], argv[2])
    ok = True
    for name in sorted(want - have):
        print(f"declared WLNK_API but not exported: {name}")
        ok = False
    for name in sorted(have - want):
        print(f"exported but not a public WLNK_API function: {name}")
        ok = False
    if not want:
        print("no WLNK_API declarations found in the headers")
        ok = False
    print(f"{len(want)} public functions, {len(have)} exported symbols")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
