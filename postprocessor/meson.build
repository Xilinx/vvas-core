########################################################################
 # Copyright (C) 2022 Xilinx, Inc.
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

core_dpuinfer_dep = dependency('', required: false)

dpuinfer_dep = [cc.find_library('vart-util', required: false),
                cc.find_library('vitis_ai_library-xnnpp', required: false),
                cc.find_library('vitis_ai_library-model_config', required: false),
                cc.find_library('vitis_ai_library-dpu_task', required: false),
                cc.find_library('opencv_core', required: false),
                cc.find_library('vart-runner', required: false),
                cc.find_library('libvitis_ai_library-runner_helper', required: false),
                cc.find_library('xir', required: false)]
if not ( dpuinfer_dep[0].found() and
         dpuinfer_dep[1].found() and
         dpuinfer_dep[2].found() and
         dpuinfer_dep[3].found() and
         dpuinfer_dep[4].found() and
         dpuinfer_dep[5].found() and
         dpuinfer_dep[6].found() and
         dpuinfer_dep[7].found() )
  # for PCIe do not build if dep library is not available
  if host_machine.cpu_family() == 'x86_64'
    message('postprocessor dependent library is not available. Skipping the build.')
    subdir_done()
  else
    # for embbeded, throw error if dep library is not available
    error('Failed to find postprocessor dependent library')
  endif
endif

postprocessor_sources = [
  'vvas_postprocessor.cpp'
]

protobuf_dep = cc.find_library('protobuf')
glog_dep = cc.find_library('glog')
jansson_dep = dependency('jansson', version : '>= 2.7', required: true)
opencv_dep = dependency('opencv4', version : '>=4.2.0', required: false)
if not opencv_dep.found()
  opencv_dep = dependency('opencv', version : '>=3.2.0', required: true)
endif

vvascore_postprocessor = library('vvascore_postprocessor',
  postprocessor_sources,
  cpp_args : [vvas_core_args, '-std=c++17'],
  include_directories : [configinc, core_common_inc, core_utils_inc],
  dependencies : [jansson_dep, core_common_dep, core_utils_dep, dpuinfer_dep, opencv_dep, protobuf_dep, glog_dep],
  install : true,
)

vvas_postprocessor_headers = ['vvas_core/vvas_postprocessor.hpp']

install_headers(vvas_postprocessor_headers, subdir : 'vvas_core/')
