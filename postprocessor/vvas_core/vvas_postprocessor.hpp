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

/**
 * DOC: VVAS Post-processing APIs
 * This file contains structures and methods related to VVAS inference.
 */

#include <vvas_core/vvas_log.h>
#include <vvas_core/vvas_infer_prediction.h>
#include <vvas_core/vvas_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  typedef VvasPostProcessor - Holds the reference to post-processing instance.
 */
typedef void VvasPostProcessor;

/**
 * struct VvasPostProcessConf - Contains information related to post-processing library configurable parameters
 * @model_path: Model path
 * @model_name: Model name
*/
typedef struct {
  char * model_path;
  char * model_name;
} VvasPostProcessConf;

/**
 *  vvas_postprocess_create () - Upon success initializes post-processor instance with config parameters.
 *
 *  @postproc_conf: VvasPostProcessConf structure.
 *  @log_level: VvasLogLevel enum.
 *
 *  This instance must be freed using @vvas_postprocess_destroy.
 *
 *  Return:
 *  * On Success returns VvasPostProcessor handle.
 *  * On Failure returns NULL.
 */
VvasPostProcessor * vvas_postprocess_create (VvasPostProcessConf * postproc_conf, VvasLogLevel log_level);

/**
 *  vvas_postprocess_tensor ()
 *
 *  @postproc_handle: post-processing handle created using @vvas_postprocess_create.
 *  @src: Pointer to VvasInferPrediction containing rawtensors.
 *
 *  Return: VvasInferPrediction tree with post-processed results
 */
VvasInferPrediction * vvas_postprocess_tensor (VvasPostProcessor * postproc_handle, VvasInferPrediction *src);

/**
 *  vvas_postprocess_destroy () - Free all resources allocated
 *
 *  @postproc_handle: post-processing handle created using @vvas_postprocess_create.
 *
 *  Return: VvasReturnType
 */
VvasReturnType vvas_postprocess_destroy (VvasPostProcessor * postproc_handle);

#ifdef __cplusplus
}
#endif
