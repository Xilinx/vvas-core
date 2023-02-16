########################################################################
 # Copyright 2020 - 2022 Xilinx, Inc.
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
    message('dpuinfer dependent library is not available. Skipping the build.')
    subdir_done()
  else
    # for embbeded, throw error if dep library is not available
    error('Failed to find dpuinfer dep library')
  endif
endif

dpuinfer_sources = [
  'vvas_dpuinfer.cpp'
]

# ADD CLASSIFICATION
if get_option('CLASSIFICATION') != '0'
  classi_dep = cc.find_library('vitis_ai_library-classification')
  dpuinfer_sources += [
    'vvas_classification.cpp',
  ]
else
  classi_dep = []
endif

# ADD VEHICLECLASSIFICATION
if get_option('VEHICLECLASSIFICATION') != '0'
  vehicleclassi_dep = cc.find_library('vitis_ai_library-vehicleclassification')
  dpuinfer_sources += [
    'vvas_vehicleclassification.cpp',
  ]
else
  vehicleclassi_dep = []
endif

#ADD YOLOV2
if get_option('YOLOV2') != '0'
  yolov2_dep = cc.find_library('vitis_ai_library-yolov2')
  dpuinfer_sources += [
    'vvas_yolov2.cpp',
  ]
else
  yolov2_dep = []
endif

#ADD YOLOV3
if get_option('YOLOV3') != '0'
  yolov3_dep = cc.find_library('vitis_ai_library-yolov3')
  dpuinfer_sources += [
    'vvas_yolov3.cpp',
  ]
else
  yolov3_dep = []
endif

#ADD REFINEDET
if get_option('REFINEDET') != '0'
  refinedet_dep = cc.find_library('vitis_ai_library-refinedet')
  dpuinfer_sources += [
    'vvas_refinedet.cpp',
  ]
else
  refinedet_dep = []
endif

# ADD PLATEDETECT
if get_option('PLATEDETECT') != '0'
  platedetect_dep = cc.find_library('vitis_ai_library-platedetect')
  dpuinfer_sources += [
    'vvas_platedetect.cpp',
  ]
else
  platedetect_dep = []
endif

# ADD PLATENUM
if get_option('PLATENUM') != '0'
  platenum_dep = cc.find_library('vitis_ai_library-platenum')
  dpuinfer_sources += [
    'vvas_platenum.cpp',
  ]
else
  platenum_dep = []
endif

# ADD FACEDETECT
if get_option('FACEDETECT') != '0'
  facedetect_dep = cc.find_library('vitis_ai_library-facedetect')
  dpuinfer_sources += [
    'vvas_facedetect.cpp',
  ]
else
  facedetect_dep = []
endif

# ADD EFFICIENTDETD2
if get_option('EFFICIENTDETD2') != '0'
  effdetd2_dep = cc.find_library('vitis_ai_library-efficientdet_d2')
  dpuinfer_sources += [
    'vvas_efficientdetd2.cpp',
  ]
else
  effdetd2_dep = []
endif

# ADD Bayesian Crowd Counting
if get_option('BCC') != '0'
  bcc_dep = cc.find_library('vitis_ai_library-bcc')
  dpuinfer_sources += [
    'vvas_bcc.cpp',
  ]
else
  bcc_dep = []
endif

#ADD SSD
if get_option('SSD') != '0'
  ssd_dep = cc.find_library('vitis_ai_library-ssd')
  dpuinfer_sources += [
    'vvas_ssd.cpp',
  ]
else
  ssd_dep = []
endif

#ADD TFSSD
if get_option('TFSSD') != '0'
  tfssd_dep = cc.find_library('vitis_ai_library-tfssd')
  dpuinfer_sources += [
    'vvas_tfssd.cpp',
  ]
else
  tfssd_dep = []
endif

# ADD ULTRAFAST
if get_option('ULTRAFAST') != '0'
  ultrafast_dep = cc.find_library('vitis_ai_library-ultrafast')
  dpuinfer_sources += [
    'vvas_ultrafast.cpp',
  ]
else
  ultrafast_dep = []
endif

# ADD ROADLINE
if get_option('ROADLINE') != '0'
  roadline_dep = cc.find_library('vitis_ai_library-lanedetect')
  dpuinfer_sources += [
    'vvas_roadline.cpp',
  ]
else
  roadline_dep = []
endif

# ADD FACEFEATURE
if get_option('FACEFEATURE') != '0'
  facefeat_dep = cc.find_library('vitis_ai_library-facefeature')
  dpuinfer_sources += [
    'vvas_facefeature.cpp',
  ]
else
  facefeat_dep = []
endif

# ADD FACELANDMARK
if get_option('FACELANDMARK') != '0'
  facelandmark_dep = cc.find_library('vitis_ai_library-facelandmark')
  dpuinfer_sources += [
    'vvas_facelandmark.cpp',
  ]
else
  facelandmark_dep = []
endif

# ADD POSEDET
if get_option('POSEDETECT') != '0'
  posedetect_dep = cc.find_library('vitis_ai_library-posedetect')
  dpuinfer_sources += [
    'vvas_posedetect.cpp',
  ]
else
  posedetect_dep = []
endif

# ADD REID
if get_option('REID') != '0'
  reid_dep = cc.find_library('vitis_ai_library-reid')
  dpuinfer_sources += [
    'vvas_reid.cpp',
  ]
else
  reid_dep = []
endif

#ADD SEGMENTATION
if get_option('SEGMENTATION') != '0'
  segmentation_dep = cc.find_library('vitis_ai_library-segmentation')
  dpuinfer_sources += [
    'vvas_segmentation.cpp',
  ]
else
  segmentation_dep = []
endif

#ADD RAWTENSOR
if get_option('RAWTENSOR') != '0'
  rawtensor_dep = cc.find_library('vitis_ai_library-graph_runner')
  dpuinfer_sources += [
    'vvas_rawtensor.cpp',
  ]
else
  rawtensor_dep = []
endif

protobuf_dep = cc.find_library('protobuf')
glog_dep = cc.find_library('glog')
jansson_dep = dependency('jansson', version : '>= 2.7', required: true)
opencv_dep = dependency('opencv4', version : '>=4.2.0', required: false)
if not opencv_dep.found()
  opencv_dep = dependency('opencv', version : '>=3.2.0', required: true)
endif

vvascore_dpuinfer = library('vvascore_dpuinfer-' + core_version,
  dpuinfer_sources,
  cpp_args : [vvas_core_args, '-std=c++17'],
  include_directories : [configinc, core_common_inc, core_utils_inc],
  dependencies : [xrt_dep, core_common_dep, core_utils_dep, jansson_dep, opencv_dep, dpuinfer_dep, protobuf_dep, glog_dep, classi_dep, vehicleclassi_dep, yolov2_dep, yolov3_dep, refinedet_dep, platedetect_dep, platenum_dep, facedetect_dep, effdetd2_dep, bcc_dep, ssd_dep, tfssd_dep, ultrafast_dep, roadline_dep, facefeat_dep, facelandmark_dep, posedetect_dep, reid_dep, segmentation_dep, rawtensor_dep],
  install : true,
)

core_dpuinfer_dep = declare_dependency(link_with : [vvascore_dpuinfer],
                      dependencies: [core_common_dep, opencv_dep])

vvas_dpuinfer_headers = ['vvas_core/vvas_dpuinfer.hpp']

install_headers(vvas_dpuinfer_headers, subdir : 'vvas_core/')

modules += ['vvascore_dpuinfer']
