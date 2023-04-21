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

#include "vvas_facefeature.hpp"

vvas_facefeature::vvas_facefeature (void * handle,
    const std::string & model_name, bool need_preprocess)
{
  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *)handle;
  log_level = kpriv->log_level;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter");

  model = vitis::ai::FaceFeature::create (model_name, need_preprocess);
}

int
vvas_facefeature::run (void * handle, std::vector < cv::Mat > &images,
    VvasInferPrediction ** predictions)
{
  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *)handle;
  uint32_t size;                   /* to get vector size */
  std::vector < vitis::ai::FaceFeatureFixedResult > results_fixed;
  std::vector < vitis::ai::FaceFeatureFloatResult > results_float;

  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter");

  if (kpriv->float_feature) {
    results_float = model->run (images);
    size = results_float.size ();
  } else {
    results_fixed = model->run_fixed (images);
    size = results_fixed.size ();
  }

  for (auto i = 0u; i < size; i++) {
    VvasBoundingBox parent_bbox = { 0 };
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
      VvasInferPrediction *predict;
      predict = vvas_inferprediction_new ();
      char *pstr;               /* prediction string */

      feat = &predict->feature;
      if (kpriv->float_feature) {
        feat->type = FLOAT_FEATURE;
        memcpy ((void *) &feat->float_feature,
            (void *) &(*results_float[i].feature)[0], 512 * sizeof (float));

        if ((LOG_LEVEL_INFO <= kpriv->log_level)) {
          LOG_MESSAGE (LOG_LEVEL_INFO, kpriv->log_level, "float_features:");
          for (int i = 0; i < 512; i++)
            printf (" %f ", feat->float_feature[i]);

          printf ("\n");
        }

      } else {
        feat->type = FIXED_FEATURE;
        memcpy ((void *) &feat->fixed_feature,
            (void *) &(*results_fixed[i].feature)[0], 512 * sizeof (int8_t));

        if ((LOG_LEVEL_INFO <= kpriv->log_level)) {
          LOG_MESSAGE (LOG_LEVEL_INFO, kpriv->log_level, "fixed_features:");
          for (int i = 0; i < 512; i++) {
            printf (" %d ", feat->fixed_feature[i]);
          }
          printf ("\n");
        }
      }

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
      predictions[i] = parent_predict;
    }
  }
  LOG_MESSAGE (LOG_LEVEL_INFO, kpriv->log_level, " ");

  return true;
}


int
vvas_facefeature::requiredwidth (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->getInputWidth ();
}

int
vvas_facefeature::requiredheight (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->getInputHeight ();
}

int
vvas_facefeature::supportedbatchsz (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->get_input_batch ();
}

int
vvas_facefeature::close (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return true;
}

vvas_facefeature::~vvas_facefeature ()
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
}
