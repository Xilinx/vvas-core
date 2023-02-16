
vvas_metaconvert = library('vvascore_metaconvert-'+core_version, 'vvas_metaconvert.c',
  cpp_args : vvas_core_args,
  c_args : vvas_core_args,
  include_directories : [configinc, core_common_inc, core_utils_inc, core_overlay_inc],
  install : true,
  dependencies : [core_common_dep, core_overlay_dep]
)

core_metaconvert_dep = declare_dependency(link_with : [vvas_metaconvert], dependencies : [core_common_dep, core_utils_dep, core_overlay_dep])
install_headers('vvas_core/vvas_metaconvert.h', subdir : 'vvas_core/')
modules += ['vvascore_metaconvert']