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

#include "vvas_roadline.hpp"
#include <algorithm>

vvas_roadline::vvas_roadline (void * handle,
    const std::string & model_name, bool need_preprocess)
{
  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *)handle;
  log_level = kpriv->log_level;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter");

  model = vitis::ai::RoadLine::create (model_name, need_preprocess);
}

int
vvas_roadline::run (void * handle, std::vector < cv::Mat > &images,
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

    LOG_MESSAGE (LOG_LEVEL_INFO, kpriv->log_level, "lanes detected %lu",
        results[i].lines.size ());

    parent_predict = predictions[i];

    for (auto & line:results[i].lines) {
      Feature *feat;
      VvasInferPrediction *predict;
      int line_size;

      char *pstr;               /* prediction string */

      if (line.type == 2 && line.points_cluster[0].x < rows * 0.5)
        continue;

      if (!parent_predict) {
        parent_bbox.x = parent_bbox.y = 0;
        parent_bbox.width = cols;
        parent_bbox.height = rows;
        parent_predict = vvas_inferprediction_new ();
        parent_predict->bbox = parent_bbox;
      }

      predict = vvas_inferprediction_new ();
      feat = &predict->feature;
      feat->type = ROADLINE;
      feat->line_type = road_line_type (line.type);
      line_size = line.points_cluster.size ();

      if (line_size > VVAS_MAX_FEATURES) {
        LOG_MESSAGE (LOG_LEVEL_WARNING, kpriv->log_level, "Number of road_line"
          "(%d) bigger than max supported(%d), ignoring road_line data post "
          "idx=%d\n", line_size, VVAS_MAX_FEATURES, VVAS_MAX_FEATURES);

        feat->line_size = VVAS_MAX_FEATURES;
      } else {
        feat->line_size = line_size;
      }

      for (auto j = 0u; j < feat->line_size; j++) {
        feat->road_line[j].x = line.points_cluster[j].x;
        feat->road_line[j].y = line.points_cluster[j].y;
      }

      if ((LOG_LEVEL_INFO <= kpriv->log_level)) {
        LOG_MESSAGE (LOG_LEVEL_INFO, kpriv->log_level, "lane type: %d",
            feat->line_type);
        LOG_MESSAGE (LOG_LEVEL_INFO, kpriv->log_level, "lane coordinates(%d):",
            feat->line_size);
        for (auto j = 0u; j < feat->line_size; j++) {
          printf (" (%d, %d)", line.points_cluster[j].x,
              line.points_cluster[j].y);
        }
        printf ("\n");
      }

      /* add class and name in prediction node */
      predict->model_class = (VvasClass) kpriv->modelclass;
      predict->model_name = strdup (kpriv->modelname.c_str ());
      vvas_inferprediction_append (parent_predict, predict);

      if( kpriv->log_level >= LOG_LEVEL_DEBUG) {
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
vvas_roadline::requiredwidth (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->getInputWidth ();
}

int
vvas_roadline::requiredheight (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->getInputHeight ();
}

int
vvas_roadline::supportedbatchsz (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->get_input_batch ();
}

int
vvas_roadline::close (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return true;
}

vvas_roadline::~vvas_roadline ()
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
}
