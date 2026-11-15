# Testing

# Dependencies
cmocka_dep = dependency('cmocka',
  required: get_option('tests'),
  default_options: ['default_library=static', 'werror=false'])
if not cmocka_dep.found()
  subdir_done()
endif

wlnk_test_deps = [wlnk_dep, cmocka_dep]

# Tests
test('wlnk_err_test',
  executable(
    'wlnk_err_test', 'err_test.c',
    dependencies: wlnk_test_deps))

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
