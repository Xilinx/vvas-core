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
#include "vvas_dpupriv.hpp"


#include <vitis/ai/reid.hpp>
#include <vitis/ai/nnpp/reid.hpp>

using namespace std;
using namespace cv;

class vvas_reid:public vvas_dpumodel
{

  int log_level = 0;
  std::unique_ptr < vitis::ai::Reid > model;

public:

  vvas_reid (void * handle, const std::string & model_name,
      bool need_preprocess);

  virtual int run (void * handle, std::vector < cv::Mat > &images,
      VvasInferPrediction ** predictions);

  virtual int requiredwidth (void);
  virtual int requiredheight (void);
  virtual int supportedbatchsz (void);
  virtual int close (void);
  virtual ~ vvas_reid ();
};
