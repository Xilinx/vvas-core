/*
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
#include "vvas_dpupriv.hpp"
#include <vitis/ai/graph_runner.hpp>

using namespace std;
using namespace cv;

class vvas_rawtensor:public vvas_dpumodel
{
  int log_level = 0;
  int runner_created = 0;

  std::unique_ptr < vart::RunnerExt > runner;
  std::vector < vart::TensorBuffer * >input_tensor_buffers;
  std::vector < vart::TensorBuffer * >output_tensor_buffers;

  std::unique_ptr < xir::Graph > graph;
  std::unique_ptr < xir::Attrs > attrs;

public:
  vvas_rawtensor (void * handle, const std::string & xmodel);
  virtual int run (void * handle, std::vector < cv::Mat > &images,
      VvasInferPrediction ** predictions);

  virtual int requiredwidth (void);
  virtual int requiredheight (void);
  virtual int supportedbatchsz (void);
  virtual int close (void);
  virtual ~ vvas_rawtensor ();
};

