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

parser_sources = ['vvas_parser.c', 'parser_h265.c', 'parser_common.c',
                  'parser_h264.c']

vvascore_parser = library('vvascore_parser-' + core_version,
  parser_sources,
  cpp_args : vvas_core_args,
  c_args : vvas_core_args,
  include_directories : [configinc, core_common_inc, core_utils_inc,
      core_decoder_inc, core_parser_inc],
  install : true,
  dependencies : [core_utils_dep, core_common_dep]
)

parser_dep = declare_dependency(link_with : [vvascore_parser],
                    dependencies: [xrt_dep])

vvas_parser_headers = ['vvas_core/vvas_parser.h']

install_headers(vvas_parser_headers, subdir : 'vvas_core/')

modules += ['vvascore_parser']
