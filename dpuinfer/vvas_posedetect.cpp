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

#include "vvas_posedetect.hpp"
#include <algorithm>

vvas_posedetect::vvas_posedetect (void * handle,
    const std::string & model_name, bool need_preprocess)
{
  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *)handle;
  log_level = kpriv->log_level;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter");

  model = vitis::ai::PoseDetect::create (model_name, need_preprocess);
}

/* print for debug purpose */
void
print_pose14pt (VvasDpuInferPrivate * kpriv, vitis::ai::PoseDetectResult::Pose14Pt * in,
    Pose14Pt * pose14pt)
{
  int set_loglevel = kpriv->log_level;
  LOG_MESSAGE (LOG_LEVEL_INFO, set_loglevel, "RESULT:");
  LOG_MESSAGE (LOG_LEVEL_INFO, set_loglevel, "%f, %f [%f, %f]",
      in->right_shoulder.x, in->right_shoulder.y,
      pose14pt->right_shoulder.x, pose14pt->right_shoulder.y);
  LOG_MESSAGE (LOG_LEVEL_INFO, set_loglevel, "%f, %f [%f, %f]",
      in->right_elbow.x, in->right_elbow.y,
      pose14pt->right_elbow.x, pose14pt->right_elbow.y);
  LOG_MESSAGE (LOG_LEVEL_INFO, set_loglevel, "%f, %f [%f, %f]",
      in->right_wrist.x, in->right_wrist.y,
      pose14pt->right_wrist.x, pose14pt->right_wrist.y);
  LOG_MESSAGE (LOG_LEVEL_INFO, set_loglevel, "%f, %f [%f, %f]",
      in->left_shoulder.x, in->left_shoulder.y,
      pose14pt->left_shoulder.x, pose14pt->left_shoulder.y);
  LOG_MESSAGE (LOG_LEVEL_INFO, set_loglevel, "%f, %f [%f, %f]",
      in->left_elbow.x, in->left_elbow.y,
      pose14pt->left_elbow.x, pose14pt->left_elbow.y);
  LOG_MESSAGE (LOG_LEVEL_INFO, set_loglevel, "%f, %f [%f, %f]",
      in->left_wrist.x, in->left_wrist.y,
      pose14pt->left_wrist.x, pose14pt->left_wrist.y);
  LOG_MESSAGE (LOG_LEVEL_INFO, set_loglevel, "%f, %f [%f, %f]",
      in->right_hip.x, in->right_hip.y,
      pose14pt->right_hip.x, pose14pt->right_hip.y);
  LOG_MESSAGE (LOG_LEVEL_INFO, set_loglevel, "%f, %f [%f, %f]",
      in->right_knee.x, in->right_knee.y,
      pose14pt->right_knee.x, pose14pt->right_knee.y);
  LOG_MESSAGE (LOG_LEVEL_INFO, set_loglevel, "%f, %f [%f, %f]",
      in->right_ankle.x, in->right_ankle.y,
      pose14pt->right_ankle.x, pose14pt->right_ankle.y);
  LOG_MESSAGE (LOG_LEVEL_INFO, set_loglevel, "%f, %f [%f, %f]",
      in->left_hip.x, in->left_hip.y,
      pose14pt->left_hip.x, pose14pt->left_hip.y);
  LOG_MESSAGE (LOG_LEVEL_INFO, set_loglevel, "%f, %f [%f, %f]",
      in->left_knee.x, in->left_knee.y,
      pose14pt->left_knee.x, pose14pt->left_knee.y);
  LOG_MESSAGE (LOG_LEVEL_INFO, set_loglevel, "%f, %f [%f, %f]",
      in->left_ankle.x, in->left_ankle.y,
      pose14pt->left_ankle.x, pose14pt->left_ankle.y);
  LOG_MESSAGE (LOG_LEVEL_INFO, set_loglevel, "%f, %f [%f, %f]",
      in->head.x, in->head.y, pose14pt->head.x, pose14pt->head.y);
  LOG_MESSAGE (LOG_LEVEL_INFO, set_loglevel, "%f, %f [%f, %f]",
      in->neck.x, in->neck.y, pose14pt->neck.x, pose14pt->neck.y);
}


/* return void as model always generate output but it might be wrong */
void
copy_pose14pt_from_result (VvasDpuInferPrivate * kpriv,
    vitis::ai::PoseDetectResult::Pose14Pt * in, Pose14Pt * out, double cols,
    double rows)
{
  out->right_shoulder.x = in->right_shoulder.x * cols;
  out->right_shoulder.y = in->right_shoulder.y * rows;
  out->right_elbow.x = in->right_elbow.x * cols;
  out->right_elbow.y = in->right_elbow.y * rows;
  out->right_wrist.x = in->right_wrist.x * cols;
  out->right_wrist.y = in->right_wrist.y * rows;
  out->left_shoulder.x = in->left_shoulder.x * cols;
  out->left_shoulder.y = in->left_shoulder.y * rows;
  out->left_elbow.x = in->left_elbow.x * cols;
  out->left_elbow.y = in->left_elbow.y * rows;
  out->left_wrist.x = in->left_wrist.x * cols;
  out->left_wrist.y = in->left_wrist.y * rows;
  out->right_hip.x = in->right_hip.x * cols;
  out->right_hip.y = in->right_hip.y * rows;
  out->right_knee.x = in->right_knee.x * cols;
  out->right_knee.y = in->right_knee.y * rows;
  out->right_ankle.x = in->right_ankle.x * cols;
  out->right_ankle.y = in->right_ankle.y * rows;
  out->left_hip.x = in->left_hip.x * cols;
  out->left_hip.y = in->left_hip.y * rows;
  out->left_knee.x = in->left_knee.x * cols;
  out->left_knee.y = in->left_knee.y * rows;
  out->left_ankle.x = in->left_ankle.x * cols;
  out->left_ankle.y = in->left_ankle.y * rows;
  out->head.x = in->head.x * cols;
  out->head.y = in->head.y * rows;
  out->neck.x = in->neck.x * cols;
  out->neck.y = in->neck.y * rows;
  print_pose14pt (kpriv, in, out);
}

int
vvas_posedetect::run (void * handle, std::vector < cv::Mat > &images,
    VvasInferPrediction ** predictions)
{
  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *)handle;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter batch");
  auto results = model->run (images);

  char *pstr;                   /* prediction string */

  for (auto i = 0u; i < results.size (); i++) {
    VvasInferPrediction *parent_predict = NULL;

    {
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

      Pose14Pt *pose14pt;
      VvasInferPrediction *predict;

      predict = vvas_inferprediction_new ();
      pose14pt = &predict->pose14pt;
      copy_pose14pt_from_result (kpriv, &results[i].pose14pt, pose14pt, cols,
          rows);

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

  LOG_MESSAGE (LOG_LEVEL_INFO, kpriv->log_level, " ");
  return true;
}

int
vvas_posedetect::requiredwidth (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->getInputWidth ();
}

int
vvas_posedetect::requiredheight (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->getInputHeight ();
}

int
vvas_posedetect::supportedbatchsz (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return model->get_input_batch ();
}

int
vvas_posedetect::close (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return true;
}

vvas_posedetect::~vvas_posedetect ()
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
}
