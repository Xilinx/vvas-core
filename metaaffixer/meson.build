
src = ['vvas_metaaffixer.c']



vvas_metaaffixer = library('vvascore_metaaffixer-'+core_version, src,
  c_args : vvas_core_args,
  include_directories : [configinc, core_common_inc, core_utils_inc],
  install : true,
  dependencies : [core_common_dep, core_utils_dep ]
)

core_metaaffixer_dep = declare_dependency(link_with : [vvas_metaaffixer], dependencies : [core_common_dep, core_utils_dep])

vvas_metaaffixer_header= ['vvas_core/vvas_metaaffixer.h']

install_headers(vvas_metaaffixer_header, subdir : 'vvas_core/')

modules += ['vvascore_metaaffixer']
