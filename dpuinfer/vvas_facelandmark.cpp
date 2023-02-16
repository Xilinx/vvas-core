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

#include "vvas_facelandmark.hpp"

vvas_facelandmark::vvas_facelandmark (void * handle,
    const std::string & model_name, bool need_preprocess)
{
  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *)handle;
  log_level = kpriv->log_level;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter");

  model = vitis::ai::FaceLandmark::create (model_name, need_preprocess);
}

int
vvas_facelandmark::run (void * handle, std::vector < cv::Mat > &images,
    VvasInferPrediction ** predictions)
{
  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *)handle;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter");

  auto results = model->run (images);

  for (auto i = 0u; i < results.size (); i++) {
    VvasBoundingBox parent_bbox;
    VvasInferPrediction *parent_predict = NULL;
    int cols = images[i].cols;
    int rows = images[i].rows;

    parent_predict = predictions[i];

    if (!parent_predict) {
      parent_bbox.x = parent_bbox.y = 0;
      parent_bbox.width = cols;
      parent_bbox.height = rows;
      parent_predict = vvas_inferprediction_new ();
      parent_predict->bbox = parent_bbox;
    }

    {
      Feature *feat;
      char *pstr;               /* prediction string */
      VvasInferPrediction *predict;
      predict = vvas_inferprediction_new ();

      feat = &predict->feature;
      auto points = results[i].points;
      feat->type = LANDMARK;

      LOG_MESSAGE (LOG_LEVEL_INFO, kpriv->log_level, "RESULT: ");
      for (int i = 0; i < 5; ++i) {
        LOG_MESSAGE (LOG_LEVEL_INFO, kpriv->log_level, "%f %f",
            points[i].first, points[i].second);
        feat->landmark[i].x = points[i].first * cols;
        feat->landmark[i].y = points[i].second * rows;
        LOG_MESSAGE (LOG_LEVEL_INFO, kpriv->log_level, "[%f %f]",
            feat->landmark[i].x, feat->landmark[i].y);
      }

      /* add class and name in prediction node */
      predict->model_class = (VvasClass) kpriv->modelclass;
      predict->model_name = strdup (kpriv->modelname.c_str ());
      vvas_inferprediction_append (parent_predict, predict);

      pstr = vvas_inferprediction_to_string (parent_predict);
      LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "prediction tree : \n%s",
          pstr);
      free (pstr);

      predictions[i] = parent_predict;
    }
  }
  LOG_MESSAGE (LOG_LEVEL_INFO, kpriv->log_level, " ");

  return true;
}


int
vvas_facelandmark::requiredwidth (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->getInputWidth ();
}

int
vvas_facelandmark::requiredheight (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->getInputHeight ();
}

int
vvas_facelandmark::supportedbatchsz (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->get_input_batch ();
}

int
vvas_facelandmark::close (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return true;
}

vvas_facelandmark::~vvas_facelandmark ()
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
}
