scaler_sources = ['vvas_scaler_hw.c']

scaler_inc = include_directories ('../')

vvascore_scaler = library('vvascore_scaler_hw-' + core_version,
  scaler_sources,
  cpp_args : vvas_core_args,
  c_args : vvas_core_args,
  include_directories : [configinc, scaler_inc, core_common_inc, core_utils_inc],
  install : true,
  install_dir : scaler_impl_lib_install_dir,
  dependencies : [core_common_dep, core_utils_dep]
)
