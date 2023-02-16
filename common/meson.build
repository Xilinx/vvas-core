common_sources = ['vvas_device.cpp',
                  'vvas_context.c',
                  'vvas_memory.c',
                  'vvas_video.c',
                  'vvas_infer_classification.c',
                  'vvas_infer_prediction.c',
                  'vvas_log.c',
                  'vvas_overlay_shape_info.c']

vvascore_common = library('vvascore_common-' + core_version,
  common_sources,
  cpp_args : vvas_core_args,
  c_args : vvas_core_args,
  include_directories : [configinc, core_utils_inc],
  install : true,
  dependencies : [xrt_dep, uuid_dep, core_utils_dep]
)

core_common_dep = declare_dependency(link_with : [vvascore_common],
                      dependencies : [xrt_dep, uuid_dep, core_utils_dep])

vvas_core_headers = ['vvas_core/vvas_device.h',
                     'vvas_core/vvas_context.h',
                     'vvas_core/vvas_common.h',
                     'vvas_core/vvas_log.h',
                     'vvas_core/vvas_memory.h',
                     'vvas_core/vvas_video.h',
                     'vvas_core/vvas_memory_priv.h',
                     'vvas_core/vvas_infer_classification.h',
                     'vvas_core/vvas_infer_prediction.h',
                     'vvas_core/vvas_dpucommon.h',
                     'vvas_core/vvas_video_priv.h',
                     'vvas_core/vvas_overlay_shape_info.h']

install_headers(vvas_core_headers, subdir : 'vvas_core/')
