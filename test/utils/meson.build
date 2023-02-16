vvas_pool_sources = ['vvas_buffer_pool.c']

vvascore_pool = library('vvascore_pool-' + core_version,
  vvas_pool_sources,
  cpp_args : vvas_core_args,
  c_args : vvas_core_args,
  include_directories : [configinc, core_common_inc, core_utils_inc],
  install : true,
  dependencies : [glib_deps, core_utils_dep, core_common_dep]
)

core_pool_dep = declare_dependency(link_with : [vvascore_pool],
			           dependencies: [glib_deps, core_utils_dep, core_common_dep ])

vvas_utils_headers = ['vvas_utils/vvas_buffer_pool.h']

install_headers(vvas_utils_headers, subdir : 'vvas_utils/')

test_utils_inc = include_directories ('.')
