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

#include "vvas_platenum.hpp"


vvas_platenum::vvas_platenum (void * handle,
    const std::string & model_name, bool need_preprocess)
{
  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *)handle;
  log_level = kpriv->log_level;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter");
  model = vitis::ai::PlateNum::create (model_name, need_preprocess);
}

int
vvas_platenum::run (void * handle, std::vector < cv::Mat > &images,
    VvasInferPrediction ** predictions)
{
  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *)handle;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter");

  auto results = model->run (images);

  char *pstr;                   /* prediction string */

  for (auto i = 0u; i < results.size (); i++) {
    VvasInferPrediction *parent_predict = NULL;

    //if (results[i].scores.size()) //TODO
    {
      VvasBoundingBox parent_bbox = { 0 };
      VvasBoundingBox child_bbox = { 0 };
      VvasInferPrediction *child_predict;

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

      child_bbox.x = 0;
      child_bbox.y = 0;
      child_bbox.width = 0;
      child_bbox.height = 0;
      child_predict = vvas_inferprediction_new ();
      child_predict->bbox = child_bbox;

      VvasInferClassification *c = NULL;

      c = vvas_inferclassification_new ();
      c->class_id = -1;
      c->class_prob = 1;
      c->class_label = strdup (results[i].plate_number.c_str ());
      c->num_classes = 0;
      child_predict->classifications = vvas_list_append (child_predict->classifications, c);

      if (parent_predict->node == NULL)
        LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
            "parent_predict->predictions is NULL");

      LOG_MESSAGE (LOG_LEVEL_INFO, kpriv->log_level,
          " num %s, color %s", results[i].plate_number.c_str (),
          results[i].plate_color.c_str ());
      /* add class and name in prediction node */
      child_predict->model_class = (VvasClass) kpriv->modelclass;
      child_predict->model_name = strdup (kpriv->modelname.c_str ());
      vvas_inferprediction_append (parent_predict, child_predict);

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
vvas_platenum::requiredwidth (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->getInputWidth ();
}

int
vvas_platenum::requiredheight (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->getInputHeight ();
}

int
vvas_platenum::supportedbatchsz (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->get_input_batch ();
}

int
vvas_platenum::close (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return true;
}

vvas_platenum::~vvas_platenum ()
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
}
