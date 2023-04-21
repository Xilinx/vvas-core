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

#include "vvas_reid.hpp"

bool
reid_free (void *ptr)
{
  Reid *reid = (Reid *) ptr;
  if (!reid)
    return false;

  reid->width = -1;
  reid->height = -1;
  reid->type = -1;

  if (reid->data)
    free (reid->data);
  reid->data = NULL;

  return true;
}

bool
reid_copy (const void *frm, void *to)
{
  Reid *frm_reid = (Reid *) frm;
  Reid *to_reid = (Reid *) to;

  if (!to_reid || !frm_reid)
    return false;

  to_reid->width = frm_reid->width;
  to_reid->height = frm_reid->height;
  to_reid->size = frm_reid->size;
  to_reid->type = frm_reid->type;
  to_reid->copy = frm_reid->copy;
  to_reid->free = frm_reid->free;

  to_reid->data = NULL;

  if((NULL != frm_reid->data) &&
     (0 != frm_reid->size)) {
    
     void *mem = NULL;
     mem = malloc(frm_reid->size);

     if(mem) {
       memcpy(mem, frm_reid->data, frm_reid->size);
       to_reid->data = mem;
     }
  }
  
  return true;

}


vvas_reid::vvas_reid (void * handle, const std::string & model_name,
    bool need_preprocess)
{
  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *)handle;
  log_level = kpriv->log_level;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter");
  model = vitis::ai::Reid::create (model_name, need_preprocess);
}

int
vvas_reid::run (void * handle, std::vector < cv::Mat > &images,
    VvasInferPrediction ** predictions)
{

  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *)handle;
  std::vector < vitis::ai::ReidResult > results;

  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter");
  results = model->run (images);

  for (auto i = 0u; i < results.size (); i++) {
    VvasBoundingBox parent_bbox = { 0 };
    VvasInferPrediction *parent_predict = NULL;
    int cols = results[i].width;
    int rows = results[i].height;

    parent_predict = predictions[i];

    if (!parent_predict) {
      parent_bbox.x = parent_bbox.y = 0;
      parent_bbox.width = cols;
      parent_bbox.height = rows;
      parent_predict = vvas_inferprediction_new ();
      parent_predict->bbox = parent_bbox;
    }

    {
      int size;
      Reid *reid;
      VvasInferPrediction *predict;
      predict = vvas_inferprediction_new ();
      char *pstr;               /* prediction string */

      reid = &predict->reid;
      reid->width = results[i].feat.cols;
      reid->height = results[i].feat.rows;
      reid->type = results[i].feat.type ();
      reid->copy = reid_copy;
      reid->free = reid_free;

      size = results[i].feat.total () * results[i].feat.elemSize ();
      reid->size = size;
      LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level,
          "output : WxH:type:Size = %dx%d:%d:%d, ", results[i].feat.cols,
          results[i].feat.rows, results[i].feat.type (), size);

      reid->data = NULL;
      if((NULL != results[i].feat.data) &&
         (0 != size)) {
        
         void *mem = NULL;
         mem = malloc(size);

         if(mem) {
           memcpy(mem, results[i].feat.data, size);
           reid->data = mem;
         }
      }
      else {
        LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "Failed to copy");
      }

      LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level,
          "data = %p append to metadata", reid->data);

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
vvas_reid::requiredwidth (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->getInputWidth ();
}

int
vvas_reid::requiredheight (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->getInputHeight ();
}

int
vvas_reid::supportedbatchsz (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->get_input_batch ();
}

int
vvas_reid::close (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return true;
}

vvas_reid::~vvas_reid ()
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
}
