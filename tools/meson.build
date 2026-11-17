# Tools

# Dependencies
argtable_dep = dependency('argtable3',
  required: get_option('tools'),
  default_options: ['default_library=static', 'werror=false'],
  include_type: 'system')
if not argtable_dep.found()
  subdir_done()
endif

subdir('wlnk')
