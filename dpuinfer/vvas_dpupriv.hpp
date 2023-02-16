/*
 *
 * Copyright (C) 2022 Xilinx, Inc.
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

#ifndef DPU2_H
#define DPU2_H

#include <vector>
#include <stdio.h>
#include <string>
#include <opencv2/opencv.hpp>

#include <vvas_core/vvas_log.h>
#include <vvas_core/vvas_video.h>
#include <vvas_core/vvas_infer_prediction.h>

using namespace cv;
using namespace std;

class vvas_dpumodel
{
public:
  virtual int run (void *handle, std::vector < cv::Mat > &images, VvasInferPrediction ** predictions) = 0;
  virtual int requiredwidth (void) = 0;
  virtual int requiredheight (void) = 0;
  virtual int supportedbatchsz (void) = 0;
  virtual int close (void) = 0;
  virtual ~ vvas_dpumodel () = 0;
};

typedef struct
{
  float mean_r;
  float mean_g;
  float mean_b;
  float scale_r;
  float scale_g;
  float scale_b;
} dpu_pp_config;

typedef struct {
  std::string name;
  int label;
  std::string display_name;
}labels;

enum
{
  VVAS_XLABEL_NOT_REQUIRED = 0,
  VVAS_XLABEL_REQUIRED = 1,
  VVAS_XLABEL_NOT_FOUND = 2,
  VVAS_XLABEL_FOUND = 4
};

typedef struct {
  int test_started = 0;
  unsigned long frames = 0;
  unsigned long last_displayed_frame = 0;
  long long timer_start;
  long long last_displayed_time;
}vvas_perf;

typedef struct {
  vvas_dpumodel *model;
  int modelclass;
  int modelnum;
  unsigned int num_labels;
  std::string modelpath;
  std::string modelname;
  std::string elfname;
  VvasVideoFormat modelfmt;
  int batch_size;
  bool need_preprocess;
  VvasLogLevel log_level;
  dpu_pp_config pp_config;
  unsigned int objs_detection_max;
  int model_width;
  int model_height;
  int max_labels;
  labels *labelptr;
  int labelflags;
  std::vector <std::string> filter_labels;
  int performance_test;
  bool float_feature;
  vvas_perf pf;
  VvasVideoFormat segoutfmt;
  int segoutfactor;
} VvasDpuInferPrivate;

#endif
