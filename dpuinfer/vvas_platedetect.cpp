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

#include "vvas_platedetect.hpp"
#include <algorithm>

vvas_platedetect::vvas_platedetect (void * handle,
    const std::string & model_name, bool need_preprocess)
{
  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *)handle;
  log_level = kpriv->log_level;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter");

  model = vitis::ai::PlateDetect::create (model_name, need_preprocess);
}

int
vvas_platedetect::run (void * handle, std::vector < cv::Mat > &images,
    VvasInferPrediction ** predictions)
{
  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *)handle;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter batch");
  auto results = model->run (images);

  char *pstr;                   /* prediction string */

  for (auto i = 0u; i < results.size (); i++) {
    VvasInferPrediction *parent_predict = NULL;

    //if (results[i].box.size()) //TODO
    if (results[i].box.score >= (float) 0.1) {
      VvasBoundingBox parent_bbox = { 0 };
      int cols = images[i].cols;
      int rows = images[i].rows;

      parent_bbox.x = parent_bbox.y = 0;
      parent_bbox.width = cols;
      parent_bbox.height = rows;

      parent_predict = predictions[i];
      if (!parent_predict) {
        parent_predict = vvas_inferprediction_new ();
        parent_predict->bbox = parent_bbox;
      }

      auto box = results[i].box;
      {
        float xmin = box.x * cols + 1;
        float ymin = box.y * rows + 1;
        float xmax = xmin + box.width * cols;
        float ymax = ymin + box.height * rows;
        if (xmin < 0.)
          xmin = 1.;
        if (ymin < 0.)
          ymin = 1.;
        if (xmax > cols)
          xmax = cols;
        if (ymax > rows)
          ymax = rows;
        float confidence = box.score;

        VvasBoundingBox bbox = { 0 };
        VvasInferPrediction *predict;

        bbox.x = xmin;
        bbox.y = ymin;
        bbox.width = xmax - xmin;
        bbox.height = ymax - ymin;
        //TODO use 4 coordinates because the plate may be skew
        //As we do not have point in inference meta so for now it is not added

        predict = vvas_inferprediction_new ();
        predict->bbox = bbox;

        /* add class and name in prediction node */
        predict->model_class = (VvasClass) kpriv->modelclass;
        predict->model_name = strdup (kpriv->modelname.c_str ());
        vvas_inferprediction_append (parent_predict, predict);

        LOG_MESSAGE (LOG_LEVEL_INFO, kpriv->log_level,
            "RESULT: %f %f %f %f (%f)", xmin, ymin, xmax, ymax, confidence);

      }

      if (kpriv->log_level >= LOG_LEVEL_DEBUG) {
        pstr = vvas_inferprediction_to_string (parent_predict);
        LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level,
          "prediction tree : \n%s", pstr);
        free (pstr);
      }
    }
    predictions[i] = parent_predict;

  }

  LOG_MESSAGE (LOG_LEVEL_INFO, kpriv->log_level, " ");

  return true;
}

int
vvas_platedetect::requiredwidth (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->getInputWidth ();
}

int
vvas_platedetect::requiredheight (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->getInputHeight ();
}

int
vvas_platedetect::supportedbatchsz (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->get_input_batch ();
}

int
vvas_platedetect::close (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return true;
}

vvas_platedetect::~vvas_platedetect ()
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
}
