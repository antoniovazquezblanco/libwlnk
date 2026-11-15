# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco

project('libwlnk', 'c',
  version: run_command('meson_version.sh', check: true).stdout().strip(),
  license: 'BSD-3-Clause',
  meson_version: '>= 0.61',
  default_options: [
    'c_std=c11',
    'warning_level=2',   # -Wall -Wextra
    'default_library=both',
  ])

meson.add_dist_script('meson_version.sh', '--dist', meson.project_version())

cc = meson.get_compiler('c')
add_project_arguments('-D_GNU_SOURCE', language: 'c')
add_project_arguments(cc.get_supported_arguments([
    '-Wvla',
    '-Wmissing-prototypes',
    '-Wstrict-prototypes',
    '-Wshadow',
    '-Wcast-qual',
    '-Wwrite-strings',
    '-Wformat=2',
    '-Wundef',
    '-Wpointer-arith',
  ]), language: 'c')
