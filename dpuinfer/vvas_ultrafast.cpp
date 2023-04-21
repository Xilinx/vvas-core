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

#include "vvas_ultrafast.hpp"
#include <algorithm>

vvas_ultrafast::vvas_ultrafast (void * handle,
    const std::string & model_name, bool need_preprocess)
{
  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *)handle;
  log_level = kpriv->log_level;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter");

  model = vitis::ai::UltraFast::create (model_name, need_preprocess);
}

int
vvas_ultrafast::run (void * handle, std::vector < cv::Mat > &images,
    VvasInferPrediction ** predictions)
{
  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *)handle;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter batch");
  auto results = model->run (images);

  for (auto i = 0u; i < results.size (); i++) {
    VvasBoundingBox parent_bbox = { 0 };
    VvasInferPrediction *parent_predict = NULL;
    int cols = images[i].cols;
    int rows = images[i].rows;
    int lane_num = 0;

    LOG_MESSAGE (LOG_LEVEL_INFO, kpriv->log_level, "lanes detected %lu",
        results[i].lanes.size ());

    parent_predict = predictions[i];

  for (auto & lane:results[i].lanes) {
      Feature *feat;
      VvasInferPrediction *predict;
      predict = vvas_inferprediction_new ();
      char *pstr;               /* prediction string */

      if (!parent_predict) {
        parent_bbox.x = parent_bbox.y = 0;
        parent_bbox.width = cols;
        parent_bbox.height = rows;
        parent_predict = vvas_inferprediction_new ();
        parent_predict->bbox = parent_bbox;
      }

      feat = &predict->feature;
      feat->type = ULTRAFAST;
      feat->line_type = road_line_type (lane_num);
      feat->line_size = lane.size ();
      for (auto j = 0u; j < feat->line_size; j++) {
        feat->road_line[j].x = lane[j].first;
        feat->road_line[j].y = lane[j].second;
      }

      if ((LOG_LEVEL_INFO <= kpriv->log_level)) {
        LOG_MESSAGE (LOG_LEVEL_INFO, kpriv->log_level, "lane: (%d)", lane_num);
        for (auto j = 0u; j < feat->line_size; j++) {
          printf (" (%f, %f)", lane[j].first, lane[j].second);
        }
        printf ("\n");
      }
      lane_num++;

      /* add class and name in prediction node */
      predict->model_class = (VvasClass) kpriv->modelclass;
      predict->model_name = strdup (kpriv->modelname.c_str ());
      vvas_inferprediction_append (parent_predict, predict);

      if (kpriv->log_level >= LOG_LEVEL_DEBUG) {
        pstr = vvas_inferprediction_to_string (parent_predict);
        LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level,
          "prediction tree : \n%s", pstr);
        free (pstr);
      }
    }
    predictions[i] = parent_predict;
  }

  return true;
}

int
vvas_ultrafast::requiredwidth (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->getInputWidth ();
}

int
vvas_ultrafast::requiredheight (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->getInputHeight ();
}

int
vvas_ultrafast::supportedbatchsz (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->get_input_batch ();
}

int
vvas_ultrafast::close (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return true;
}

vvas_ultrafast::~vvas_ultrafast ()
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
}
