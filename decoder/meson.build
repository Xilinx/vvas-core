########################################################################
#
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

decoder_sources = ['vvas_decoder.c']

vvascore_decoder = library('vvascore_decoder-' + core_version,
  decoder_sources,
  cpp_args : vvas_core_args,
  c_args : vvas_core_args,
  include_directories : [configinc, core_common_inc, core_utils_inc,
      core_decoder_inc],
  install : true,
  dependencies : [core_utils_dep, core_common_dep]
)


decoder_dep = declare_dependency(link_with : [vvascore_decoder],
                    dependencies: [xrt_dep])

vvas_decoder_headers = ['vvas_core/vvas_decoder.h']

install_headers(vvas_decoder_headers, subdir : 'vvas_core/')

modules += ['vvascore_decoder']
