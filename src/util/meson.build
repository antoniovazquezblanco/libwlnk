# Library utilities

# Internal version configuration
wlnk_version_core = meson.project_version().split('+')[0].split('-')[0]
wlnk_version_parts = wlnk_version_core.split('.')
if wlnk_version_parts.length() != 3
  error('project version must start with X.Y.Z, got: ' + meson.project_version())
endif

wlnk_version_conf = configuration_data({
  'WLNK_VERSION_MAJOR': wlnk_version_parts[0].to_int(),
  'WLNK_VERSION_MINOR': wlnk_version_parts[1].to_int(),
  'WLNK_VERSION_PATCH': wlnk_version_parts[2].to_int(),
  'WLNK_VERSION_STRING': meson.project_version(),
})

configure_file(
  input: 'version_internal.h.in',
  output: 'version_internal.h',
  configuration: wlnk_version_conf)
