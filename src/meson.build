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

# Library
wlnk_src = files(
  'version.c',
)

wlnk_lib = library('wlnk', wlnk_src,
  version: wlnk_version_core,
  soversion: wlnk_version_parts[0],
  gnu_symbol_visibility: 'hidden',
  install: true)

# Dependency
wlnk_dep = declare_dependency(
  link_with: wlnk_lib,
  include_directories: include_directories('.'))

# Installation
install_headers('api.h', 'version.h', subdir: 'wlnk')
