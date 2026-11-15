# libwlnk library

# Version
wlnk_version_core = meson.project_version().split('+')[0].split('-')[0]
wlnk_version_parts = wlnk_version_core.split('.')
if wlnk_version_parts.length() != 3
  error('project version must start with X.Y.Z, got: ' + meson.project_version())
endif

configure_file(
  input: 'version_internal.h.in',
  output: 'version_internal.h',
  configuration: {
    'WLNK_VERSION_MAJOR': wlnk_version_parts[0].to_int(),
    'WLNK_VERSION_MINOR': wlnk_version_parts[1].to_int(),
    'WLNK_VERSION_PATCH': wlnk_version_parts[2].to_int(),
    'WLNK_VERSION_STRING': meson.project_version(),
  })

# Dependencies
nl_deps = [dependency('libnl-3.0'), dependency('libnl-genl-3.0')]

# Library
wlnk_src = files(
  'chandef.c',
  'err.c',
  'iface.c',
  'link.c',
  'lock.c',
  'nl.c',
  'radio.c',
  'version.c',
  'vif.c',
)

wlnk_lib = library('wlnk', wlnk_src,
  dependencies: nl_deps,
  version: wlnk_version_core,
  soversion: wlnk_version_parts[0],
  gnu_symbol_visibility: 'hidden',
  install: true)

# Dependency
wlnk_dep = declare_dependency(
  link_with: wlnk_lib,
  include_directories: include_directories('.'))

# Installation
wlnk_headers = files(
  'api.h',
  'err.h',
  'iface.h',
  'version.h',
  'wlnk.h',
)
install_headers(wlnk_headers, subdir: 'wlnk')
