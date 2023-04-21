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

/**
 * DOC: VVAS DPU Infer APIs
 * This file contains structures and methods related to VVAS Inference.
 */
 
#include <vvas_core/vvas_infer_prediction.h>
#include <vvas_core/vvas_video.h>

#define MAX_NUM_OBJECT 512

#ifdef __cplusplus
extern "C" {
#endif

/**
 * struct VvasDpuInferConf - Contains information related to model and configurable parameters
 * @model_path: Model path
 * @model_name: Model name
 * @model_format: Color format of the input image, like BGR, RGB etc., expected by the model
 * @modelclass: Model class
 * @batch_size: Batch size
 * @need_preprocess: If this is set to true, then software pre-processing will be performed using Vitis-AI library
 * @performance_test: Performance test
 * @objs_detection_max: Sort the detected objects based on area of the bounding box, from highest to lowest area.
 * @filter_labels: Array of labels to process
 * @num_filter_labels: Number of labels to process
 * @float_feature: Float feature
 * @segoutfmt: Segmentation output format
 * @segoutfactor: Multiplication factor for Y8 output to look bright
*/
typedef struct {
  char * model_path;
  char * model_name;
  VvasVideoFormat model_format;
  char * modelclass;
  unsigned int batch_size;
  bool need_preprocess;
  bool performance_test;
  unsigned int objs_detection_max;
  char **filter_labels;
  int num_filter_labels;
  bool float_feature;
  VvasVideoFormat segoutfmt;
  int segoutfactor;
} VvasDpuInferConf;

/**
 * struct VvasModelConf - Contains information related to model requirements
 * @model_width: Model required width
 * @model_height: Model required height
 * @batch_size: Model supported batch size
 * @mean_r: Mean value of R channel
 * @mean_g: Mean value of G channel
 * @mean_b: Mean value of B channel
 * @scale_r: Scale value of R channel
 * @scale_g: Scale value of G channel
 * @scale_b: Scale value of B channel
*/
typedef struct {
  int model_width;
  int model_height;
  unsigned int batch_size;
  float mean_r;
  float mean_g;
  float mean_b;
  float scale_r;
  float scale_g;
  float scale_b;
} VvasModelConf;

/**
 *  typedef VvasDpuInfer - Holds the reference to dpu instance.
 */
typedef void VvasDpuInfer;

/**
 *  vvas_dpuinfer_create () - Initializes DPU with config parameters and allocates DpuInfer instance
 *
 *  @dpu_conf: VvasDpuInferConf structure.
 *  @log_level: VvasLogLevel enum.
 *
 *  This instance must be freed using @vvas_dpuinfer_destroy.
 *
 *  Return:
 *  * On Success returns VvasDpuInfer handle.
 *  * On Failure returns NULL.
 */
VvasDpuInfer * vvas_dpuinfer_create (VvasDpuInferConf * dpu_conf, VvasLogLevel log_level);

/**
 *  vvas_dpuinfer_process_frames () - This API processes frames in a batch.
 *
 *  @dpu_handle: VvasDpuInfer handle created using @vvas_dpuinfer_create.
 *  @inputs: Array of @VvasVideoFrame
 *  @predictions: Array of @VvasInferPrediction. MAX_NUM_OBJECT is defined as 512.
 *  @batch_size: Batch size.
 *
 *  This API returns VvasInferPrediction to each frame.
 *  It is user's responsibility to free the VvasInferPrediction of each frame.
 *
 *  Return: VvasReturnType
 */
VvasReturnType vvas_dpuinfer_process_frames (VvasDpuInfer * dpu_handle, VvasVideoFrame *inputs[MAX_NUM_OBJECT], VvasInferPrediction *predictions[MAX_NUM_OBJECT], int batch_size);

/**
 *  vvas_dpuinfer_destroy () - De-initialises the model and free all other resources allocated
 *
 *  @dpu_handle: VvasDpuInfer handle created using @vvas_dpuinfer_create.
 *
 *  Return: VvasReturnType
 */
VvasReturnType vvas_dpuinfer_destroy (VvasDpuInfer * dpu_handle);

/**
 *  vvas_dpuinfer_get_config () - Returns the VvasModelConf structure with all fields populated
 *
 *  @dpu_handle: VvasDpuInfer handle created using @vvas_dpuinfer_create.
 *  @model_conf: VvasModelConf structure
 *
 *  Return: VvasReturnType
 */
VvasReturnType vvas_dpuinfer_get_config (VvasDpuInfer * dpu_handle, VvasModelConf *model_conf);

#ifdef __cplusplus
}
#endif

