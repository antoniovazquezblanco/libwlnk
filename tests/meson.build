# Testing

# Dependencies
cmocka_dep = dependency('cmocka',
  required: get_option('tests'),
  default_options: ['default_library=static', 'werror=false'])
if not cmocka_dep.found()
  subdir_done()
endif

wlnk_test_deps = [wlnk_dep, cmocka_dep]

# ABI exports
nm = find_program('nm', required: false)
if host_machine.system() == 'linux' and get_option('default_library') != 'static' and nm.found()
  wlnk_shared_lib = get_option('default_library') == 'both' ? wlnk_lib.get_shared_lib() : wlnk_lib
  test('wlnk_abi_exports',
    import('python').find_installation(),
    args: [files('abi_exports.py'), nm.full_path(), wlnk_shared_lib, wlnk_headers])
endif

# C++ headers
if add_languages('cpp', required: false, native: false)
  test('wlnk_cxx_headers_test',
    executable(
      'wlnk_cxx_headers_test', 'cxx_headers_test.cpp',
      dependencies: wlnk_dep,
      override_options: ['cpp_std=c++11']))
endif

# Error tests
test('wlnk_err_test',
  executable(
    'wlnk_err_test', 'err_test.c',
    dependencies: wlnk_test_deps))

# Internal: the error detail helpers, through the library's own err.c object.
test('wlnk_err_detail_test',
  executable(
    'wlnk_err_detail_test', 'err_detail_test.c',
    objects: wlnk_lib.extract_objects('err.c'),
    dependencies: [wlnk_dep.partial_dependency(includes: true), cmocka_dep,
      dependency('threads')]))

# Interface tests
test('wlnk_iface_test',
  executable(
    'wlnk_iface_test', 'iface_test.c',
    dependencies: wlnk_test_deps))

# Netlink tests
test('wlnk_nl_test',
  executable(
    'wlnk_nl_test', 'nl_test.c',
    objects: wlnk_lib.extract_objects('nl.c', 'err.c'),
    dependencies: [wlnk_dep.partial_dependency(includes: true), cmocka_dep,
      nl_deps]))

# Version tests
test('wlnk_version_test',
  executable(
    'wlnk_version_test', 'version_test.c',
    c_args: [
      '-DWLNK_TEST_VERSION_STRING="@0@"'.format(meson.project_version()),
      '-DWLNK_TEST_VERSION_MAJOR=@0@'.format(wlnk_version_parts[0].to_int()),
      '-DWLNK_TEST_VERSION_MINOR=@0@'.format(wlnk_version_parts[1].to_int()),
      '-DWLNK_TEST_VERSION_PATCH=@0@'.format(wlnk_version_parts[2].to_int()),
    ],
    dependencies: wlnk_test_deps))
