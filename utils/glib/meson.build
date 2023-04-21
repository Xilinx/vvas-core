

#
# All includes for Glib based vvas_utils 
#
vvas_utils_sources = ['vvas_hash.c',
                      'vvas_node.c',
                      'vvas_mutex.c',
                      'vvas_list.c',
                      'vvas_queue.c']

vvas_utils_headers = ['vvas_utils/vvas_hash.h',
                       'vvas_utils/vvas_list.h',
                       'vvas_utils/vvas_mutex.h',
                       'vvas_utils/vvas_node.h',
                       'vvas_utils/vvas_queue.h']
glib_req = '>= 2.60.0'
glib_deps = dependency('glib-2.0', version : glib_req,
              fallback: ['glib', 'libglib_dep'])


vvascore_utils = library('vvascore_utils-' + core_version,
vvas_utils_sources,
cpp_args : vvas_core_args,
c_args : vvas_core_args,
include_directories : [configinc, core_common_inc],
install : true,
dependencies : [glib_deps])

core_utils_dep = declare_dependency(link_with : [vvascore_utils],
              dependencies: [glib_deps])


install_headers(vvas_utils_headers, subdir : 'vvas_utils/')

modules += ['vvascore_utils']
