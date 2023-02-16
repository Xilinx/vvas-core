opencv_dep = dependency('opencv4', version : '>=4.2.0', required: false)

src = ['vvas_overlay.cpp']


if not opencv_dep.found()
  opencv_dep = dependency('opencv', version : '>=3.2.0', required: true)
endif

vvas_overlay = library('vvascore_overlay-'+core_version, src,
  cpp_args : vvas_core_args,
  c_args : vvas_core_args,
  include_directories : [configinc, core_common_inc, core_utils_inc],
  install : true,
  dependencies : [core_common_dep, opencv_dep]
)

core_overlay_dep = declare_dependency(link_with : [vvas_overlay], dependencies : [core_common_dep])

vvas_overlay_header= ['vvas_core/vvas_overlay.h']

install_headers(vvas_overlay_header, subdir : 'vvas_core/')

modules += ['vvascore_overlay']
