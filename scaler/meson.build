scaler_sources = ['vvas_scaler.c']

scaler_impl_lib_install_dir = join_paths(get_option('prefix'), get_option('libdir'), 'vvas_core')

vvascore_scaler = library('vvascore_scaler-' + core_version,
  scaler_sources,
  cpp_args : vvas_core_args,
  c_args : vvas_core_args,
  include_directories : [configinc, core_common_inc],
  install : true,
  dependencies : [dl_dep, core_common_dep]
)

vvas_core_scaler_header= ['vvas_core/vvas_scaler.h',
		          'vvas_core/vvas_scaler_interface.h']

install_headers(vvas_core_scaler_header, subdir : 'vvas_core/')

core_scaler_dep = declare_dependency(link_with : [vvascore_scaler])

subdir('scaler_hw')
subdir('scaler_sw')

modules += ['vvascore_scaler']
