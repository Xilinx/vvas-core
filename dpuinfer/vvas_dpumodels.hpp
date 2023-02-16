/*
 * Copyright (C) 2020-2022 Xilinx, Inc.
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#ifndef DPUMODELS_H
#define DPUMODELS_H

#include <vvas_core/vvas_infer_prediction.h>

static const char *vvas_xmodelclass[VVAS_XCLASS_NOTFOUND + 1] = {
  [VVAS_XCLASS_YOLOV3] = "YOLOV3",
  [VVAS_XCLASS_FACEDETECT] = "FACEDETECT",
  [VVAS_XCLASS_CLASSIFICATION] = "CLASSIFICATION",
  [VVAS_XCLASS_VEHICLECLASSIFICATION] = "VEHICLECLASSIFICATION",
  [VVAS_XCLASS_SSD] = "SSD",
  [VVAS_XCLASS_REID] = "REID",
  [VVAS_XCLASS_REFINEDET] = "REFINEDET",
  [VVAS_XCLASS_TFSSD] = "TFSSD",
  [VVAS_XCLASS_YOLOV2] = "YOLOV2",
  [VVAS_XCLASS_SEGMENTATION] = "SEGMENTATION",
  [VVAS_XCLASS_PLATEDETECT] = "PLATEDETECT",
  [VVAS_XCLASS_PLATENUM] = "PLATENUM",
  [VVAS_XCLASS_POSEDETECT] = "POSEDETECT",
  [VVAS_XCLASS_BCC] = "BCC",
  [VVAS_XCLASS_EFFICIENTDETD2] = "EFFICIENTDETD2",
  [VVAS_XCLASS_FACEFEATURE] = "FACEFEATURE",
  [VVAS_XCLASS_FACELANDMARK] = "FACELANDMARK",
  [VVAS_XCLASS_ROADLINE] = "ROADLINE",
  [VVAS_XCLASS_ULTRAFAST] = "ULTRAFAST",
  [VVAS_XCLASS_RAWTENSOR] = "RAWTENSOR",

  /* Add model above this */
  [VVAS_XCLASS_NOTFOUND] = ""
};

#endif
