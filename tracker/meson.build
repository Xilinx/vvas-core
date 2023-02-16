
if get_option('tracker_use_simd') == 1
  message ('compiling with simd optimization')
  simd_dep = cc.find_library('Simd')
  cdata.set('TRACKER_USE_SIMD_OPTIMIZATION', true)
  src = ['vvas_tracker.cpp', 'tracker_algo/correlation_filter.cpp',
         'tracker_algo/mosse_filter.cpp', 'tracker_algo/fft.cpp',
         'tracker_algo/Hungarian.cpp', 'tracker_algo/tracker.cpp']
else
  message ('compiling without simd optimization')
  simd_dep = []
  cdata.set('TRACKER_USE_SIMD_OPTIMIZATION', false)
  src = ['vvas_tracker.cpp', 'tracker_algo/correlation_filter.cpp',
         'tracker_algo/mosse_filter.cpp', 'tracker_algo/fft.cpp',
         'tracker_algo/Hungarian.cpp', 'tracker_algo/tracker.cpp',
         'tracker_algo/features.cpp']
endif

if host_machine.cpu_family() == 'aarch64'
  ne10_dep = cc.find_library('NE10')
else
  ne10_dep = []
endif

vvas_tracker = library('vvascore_tracker-'+core_version, src,
  cpp_args : vvas_core_args,
  c_args : vvas_core_args,
  include_directories : [configinc, core_common_inc, core_utils_inc],
  install : true,
  dependencies : [core_common_dep,  simd_dep, core_utils_dep, ne10_dep]
)

core_tracker_dep = declare_dependency(link_with : [vvas_tracker], dependencies : [core_common_dep, core_utils_dep])

vvas_tracker_header= ['vvas_core/vvas_tracker.hpp']

install_headers(vvas_tracker_header, subdir : 'vvas_core/')

modules += ['vvascore_tracker']
