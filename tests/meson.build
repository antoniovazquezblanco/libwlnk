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

# Error tests
test('wlnk_err_test',
  executable(
    'wlnk_err_test', 'err_test.c',
    dependencies: wlnk_test_deps))

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
