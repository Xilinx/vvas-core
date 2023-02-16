########################################################################
 # Copyright (C) 2020-2022 Xilinx, Inc. 
 # Copyright (C) 2022-2023 Advanced Micro Devices, Inc.
 #
 # Licensed under the Apache License, Version 2.0 (the "License");
 # you may not use this file except in compliance with the License.
 # You may obtain a copy of the License at
 #
 #     http://www.apache.org/licenses/LICENSE-2.0
 #
 # Unless required by applicable law or agreed to in writing, software
 # distributed under the License is distributed on an "AS IS" BASIS,
 # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 # See the License for the specific language governing permissions and
 # limitations under the License.
#########################################################################

project('vvas-libs-core', 'c', 'cpp',
  version : '1.0',
  meson_version : '>= 0.54.1',
  default_options : [ 'warning_level=1', 'cpp_std=c++14',
                      'buildtype=debugoptimized' ])
modules = []
project_version = meson.project_version()
version_arr = project_version.split('.')
core_version_major = version_arr[0].to_int()
core_version_minor = version_arr[1].to_int()

core_version = '@0@.@1@'.format(core_version_major, core_version_minor)

vvas_core_args = ['-DHAVE_CONFIG_H']

configinc = include_directories('.')
core_common_inc = include_directories('common')
core_utils_inc = [include_directories('utils')]
core_scaler_inc = include_directories('scaler')
core_overlay_inc = include_directories('overlay')
core_metaaffixer_inc = include_directories('metaaffixer')
core_decoder_inc = include_directories('decoder')
core_tracker_inc = include_directories('tracker')
core_parser_inc = include_directories('parser')
core_dpuinfer_inc = include_directories('dpuinfer')
core_metaconvert_inc = include_directories('metaconvert')

soversion = 0
# maintaining compatibility with the previous libtool versioning
# current = minor * 100 + micro
curversion = core_version_major * 100 + core_version_minor
libversion = '@0@.@1@.0'.format(soversion, curversion)

cdata = configuration_data()
cc = meson.get_compiler('cpp')

cdata.set_quoted('VVAS_CORE_VERSION', core_version)
cdata.set_quoted('VVAS_CORE_PACKAGE', 'vvas-lib-core')
cdata.set_quoted('VVAS_CORE_PACKAGE_VERSION', core_version)
cdata.set_quoted('VVAS_CORE_PACKAGE_NAME', 'VVAS Base libraries package')
cdata.set_quoted('VVAS_CORE_GETTEXT_PACKAGE', 'vvas-libs-core-' + core_version)

#TODO: currently proprietary until release
cdata.set_quoted('LIBDIR', join_paths(get_option('prefix'), get_option('libdir')))
cdata.set_quoted('LOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))

# supporting 64-bit x86 family & ARM 64-bit compilers only

if host_machine.cpu_family() == 'x86_64'
  message ('compiling for PCIe platform')
  cdata.set('XLNX_PCIe_PLATFORM', 1)
elif host_machine.cpu_family() == 'aarch64'
  message ('compiling for Embedded platform')
  cdata.set('XLNX_EMBEDDED_PLATFORM ', 1)
else
  error('not supporting host machine families other than x86_64 & aarch64')
endif

if get_option('enable_ppe') != -1
  if get_option('enable_ppe') == 1
    cdata.set('ENABLE_PPE_SUPPORT ', 1)
    message ('Enabling PPE')
  endif
endif

if get_option('vvas_core_utils') == 'GLIB'
  add_project_arguments('-DVVAS_GLIB_UTILS', language : 'c')
  add_project_arguments('-DVVAS_GLIB_UTILS', language : 'cpp')
endif

warning_flags = [
  '-Wmissing-declarations',
  '-Wredundant-decls',
  '-Wwrite-strings',
  '-Winit-self',
  '-Wno-multichar',
  '-Wvla',
  '-Wpointer-arith',
]

warning_c_flags = [
  '-Wmissing-prototypes',
  '-Wdeclaration-after-statement',
  '-Wold-style-definition',
  '-Waggregate-return',
]

foreach extra_arg : warning_flags
  if cc.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'c')
  endif
endforeach

foreach extra_arg : warning_c_flags
  if cc.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'c')
  endif
endforeach

check_headers = [
  ['HAVE_STDIO_H', 'stdio.h'],
  ['HAVE_DLFCN_H', 'dlfcn.h'],
]

foreach h : check_headers
  if cc.has_header(h.get(1))
    cdata.set(h.get(0), 1)
  endif
endforeach

xrt_req = '>= 2.6' #XRT 2020.1
xrt_dep = dependency('xrt', version : xrt_req, required : true)
uuid_dep = cc.find_library('uuid', required : true)

# External dependency
dl_dep = cc.find_library('dl', required : true)
pthread_dep = cc.find_library('pthread', required : true)

python3 = import('python').find_installation()
pkgconfig = import('pkgconfig')

if get_option('CLASSIFICATION') != '0'
  add_project_arguments('-DENABLE_CLASSIFICATION', language : 'c')
  add_project_arguments('-DENABLE_CLASSIFICATION', language : 'cpp')
endif

if get_option('VEHICLECLASSIFICATION') != '0'
  add_project_arguments('-DENABLE_VEHICLECLASSIFICATION', language : 'c')
  add_project_arguments('-DENABLE_VEHICLECLASSIFICATION', language : 'cpp')
endif

if get_option('YOLOV2') != '0'
  add_project_arguments('-DENABLE_YOLOV2', language : 'c')
  add_project_arguments('-DENABLE_YOLOV2', language : 'cpp')
endif

if get_option('YOLOV3') != '0'
  add_project_arguments('-DENABLE_YOLOV3', language : 'c')
  add_project_arguments('-DENABLE_YOLOV3', language : 'cpp')
endif

if get_option('REFINEDET') != '0'
  add_project_arguments('-DENABLE_REFINEDET', language : 'c')
  add_project_arguments('-DENABLE_REFINEDET', language : 'cpp')
endif

if get_option('PLATEDETECT') != '0'
  add_project_arguments('-DENABLE_PLATEDETECT', language : 'c')
  add_project_arguments('-DENABLE_PLATEDETECT', language : 'cpp')
endif

if get_option('PLATENUM') != '0'
  add_project_arguments('-DENABLE_PLATENUM', language : 'c')
  add_project_arguments('-DENABLE_PLATENUM', language : 'cpp')
endif

if get_option('FACEDETECT') != '0'
  add_project_arguments('-DENABLE_FACEDETECT', language : 'c')
  add_project_arguments('-DENABLE_FACEDETECT', language : 'cpp')
endif

if get_option('EFFICIENTDETD2') != '0'
  add_project_arguments('-DENABLE_EFFICIENTDETD2', language : 'c')
  add_project_arguments('-DENABLE_EFFICIENTDETD2', language : 'cpp')
endif

if get_option('BCC') != '0'
  add_project_arguments('-DENABLE_BCC', language : 'c')
  add_project_arguments('-DENABLE_BCC', language : 'cpp')
endif

if get_option('SSD') != '0'
  add_project_arguments('-DENABLE_SSD', language : 'c')
  add_project_arguments('-DENABLE_SSD', language : 'cpp')
endif

if get_option('TFSSD') != '0'
  add_project_arguments('-DENABLE_TFSSD', language : 'c')
  add_project_arguments('-DENABLE_TFSSD', language : 'cpp')
endif

if get_option('ULTRAFAST') != '0'
  add_project_arguments('-DENABLE_ULTRAFAST', language : 'c')
  add_project_arguments('-DENABLE_ULTRAFAST', language : 'cpp')
endif

if get_option('ROADLINE') != '0'
  add_project_arguments('-DENABLE_ROADLINE', language : 'c')
  add_project_arguments('-DENABLE_ROADLINE', language : 'cpp')
endif

if get_option('POSEDETECT') != '0'
  add_project_arguments('-DENABLE_POSEDETECT', language : 'c')
  add_project_arguments('-DENABLE_POSEDETECT', language : 'cpp')
endif

if get_option('FACEFEATURE') != '0'
  add_project_arguments('-DENABLE_FACEFEATURE', language : 'c')
  add_project_arguments('-DENABLE_FACEFEATURE', language : 'cpp')
endif

if get_option('FACELANDMARK') != '0'
  add_project_arguments('-DENABLE_FACELANDMARK', language : 'c')
  add_project_arguments('-DENABLE_FACELANDMARK', language : 'cpp')
endif

if get_option('REID') != '0'
  add_project_arguments('-DENABLE_REID', language : 'c')
  add_project_arguments('-DENABLE_REID', language : 'cpp')
endif

if get_option('SEGMENTATION') != '0'
  add_project_arguments('-DENABLE_SEGMENTATION', language : 'c')
  add_project_arguments('-DENABLE_SEGMENTATION', language : 'cpp')
endif

if get_option('RAWTENSOR') != '0'
  add_project_arguments('-DENABLE_RAWTENSOR', language : 'c')
  add_project_arguments('-DENABLE_RAWTENSOR', language : 'cpp')
endif

#include directories
subdir('utils')
subdir('common')

subdir('dpuinfer')
if get_option('pci_platform') == 'U30'
    cdata.set('XLNX_U30_PLATFORM ', 1)
    subdir('decoder_u30')
else
    cdata.set('XLNX_V70_PLATFORM ', 1)
    subdir('decoder')
endif
subdir('parser')
subdir('metaaffixer')
subdir('overlay')
subdir('scaler')
subdir('tracker')
subdir('metaconvert')
subdir('pkgconfig')
subdir('test')
subdir('postprocessor')

configure_file(output : 'config.h', configuration : cdata)
vvas_config_header = ['build/config.h']
install_headers(vvas_config_header, subdir : 'vvas_core/')
