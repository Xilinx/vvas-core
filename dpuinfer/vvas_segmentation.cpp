/*
 * Copyright (C) 2021-2022 Xilinx, IncD
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

#include "vvas_segmentation.hpp"

bool
seg_free (void *ptr)
{
  Segmentation *seg = (Segmentation *) ptr;
  if (!seg)
    return false;

  if (seg->data)
    free (seg->data);
  seg->data = NULL;

  return true;
}

bool
seg_copy (const void *frm, void *to)
{
  int i, size;
  Segmentation *frm_seg = (Segmentation *) frm;
  Segmentation *to_seg = (Segmentation *) to;
  int sizeoffmt = sizeof (frm_seg->fmt) / sizeof (frm_seg->fmt[0]);

  if (!to_seg || !frm_seg)
    return false;

  to_seg->width = frm_seg->width;
  to_seg->height = frm_seg->height;
  to_seg->type = frm_seg->type;
  to_seg->copy = frm_seg->copy;
  to_seg->free = frm_seg->free;
  for (i = 0; i < sizeoffmt; i++)
    to_seg->fmt[i] = frm_seg->fmt[i];

  if (!strcmp (to_seg->fmt, "BGR"))
    size = (to_seg->width * to_seg->height * 3);
  else
    size = (to_seg->width * to_seg->height);

  to_seg->data = NULL;

  if((NULL != frm_seg->data) &&
     (0 != size)) {
    
     void *mem = NULL;
     mem = malloc(size);

     if(mem) {
       memcpy(mem, frm_seg->data, size);
       to_seg->data = mem;
     }
  }
  return true;
}

vvas_segmentation::vvas_segmentation (void * handle,
    const std::string & model_name, bool need_preprocess)
{
  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *)handle;
  log_level = kpriv->log_level;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter");

  model = vitis::ai::Segmentation::create (model_name, need_preprocess);
}

int
vvas_segmentation::run (void * handle, std::vector < cv::Mat > &images,
    VvasInferPrediction ** predictions)
{
  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *)handle;
  std::vector < vitis::ai::SegmentationResult > results;

  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter");
  if (kpriv->segoutfmt == VVAS_VIDEO_FORMAT_BGR)
    results = model->run_8UC3 (images);
  else if (kpriv->segoutfmt == VVAS_VIDEO_FORMAT_GRAY8) {
    results = model->run_8UC1 (images);
    for (auto i = 0u; i < results.size (); i++) {
      if (!(kpriv->segoutfactor == 0 || kpriv->segoutfactor == 1)) {
        for (auto y = 0; y < results[i].segmentation.rows; y++) {
          for (auto x = 0; x < results[i].segmentation.cols; x++) {
            results[i].segmentation.at < uchar > (y, x) *= kpriv->segoutfactor;
          }
        }
      }
    }
  } else {
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level, "unsupported fmt");
    return false;
  }
  for (auto i = 0u; i < results.size (); i++) {
    VvasBoundingBox parent_bbox = { 0 };
    VvasInferPrediction *parent_predict = NULL;
    int cols = results[i].segmentation.cols;
    int rows = results[i].segmentation.rows;

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
      Segmentation *seg;
      VvasInferPrediction *predict;
      predict = vvas_inferprediction_new ();
      char *pstr;               /* prediction string */

      seg = &predict->segmentation;
      seg->width = cols;
      seg->height = rows;
      kpriv->segoutfmt == VVAS_VIDEO_FORMAT_BGR ? strcpy (seg->fmt,
          "BGR") : strcpy (seg->fmt, "GRAY8");
      if (!strcmp (seg->fmt, "BGR"))
        size = (seg->width * seg->height * 3);
      else
        size = (seg->width * seg->height);
      seg->copy = seg_copy;
      seg->free = seg_free;
      seg->data = NULL;

     if((NULL != results[i].segmentation.data) &&
        (0 != size)) {
      
        void *mem = NULL;
        mem = malloc(size);

        if(mem) {
          memcpy(mem, results[i].segmentation.data,size);
          seg->data = mem;
        }
      }
      else {
        LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "Failed to copy");
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
vvas_segmentation::requiredwidth (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->getInputWidth ();
}

int
vvas_segmentation::requiredheight (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->getInputHeight ();
}

int
vvas_segmentation::supportedbatchsz (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->get_input_batch ();
}

int
vvas_segmentation::close (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return true;
}

vvas_segmentation::~vvas_segmentation ()
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
}
