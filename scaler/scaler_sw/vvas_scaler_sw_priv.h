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
*  DOC: Contains structures and macros related to VVAS Software Scaler implementation.
*/

#ifndef __VVAS_SCALER_PRIV_H__
#define __VVAS_SCALER_PRIV_H__

#include "vvas_core/vvas_scaler.h"
#include <vvas_core/vvas_context.h>
#include <vvas_core/vvas_common.h>
#include <vvas_core/vvas_memory.h>
#include <vvas_core/vvas_device.h>
#include <vvas_utils/vvas_utils.h>

/** @def DEFAULT_KERNEL_NAME
 *  @brief Default kernel name
 */
#define DEFAULT_KERNEL_NAME "image_processing_sw"

/** @def VVAS_SCALER_DEFAULT_COEF_LOAD_TYPE
 *  @brief Default coefficients loading type
 */
#define VVAS_SCALER_DEFAULT_COEF_LOAD_TYPE VVAS_SCALER_COEF_FIXED

/** @def VVAS_SCALER_DEFAULT_NUM_TAPS
 *  @brief Default number of filter taps
 */
#define VVAS_SCALER_DEFAULT_NUM_TAPS VVAS_SCALER_FILTER_TAPS_12

/** @def VVAS_SCALER_SCALE_MODE
 *  @brief Default scaling mode
 */
#define VVAS_SCALER_SCALE_MODE VVAS_SCALER_MODE_POLYPHASE

/**
 * struct VvasScalerInternlBuffer -  Contains info of internal buffers.
 * @Hcoff: Buffer for storing horizontal coefficient
 * @Vcoff: Buffer for storing vertical coefficient
 * @descriptor: Buffer for storing processing descriptor
 * @outvideo_frame: Reference of output video frame
 */
typedef struct {
  void            *Hcoff;
  void            *Vcoff;
  void            *descriptor;
  VvasVideoFrame *      outvideo_frame;
} VvasScalerInternlBuffer;

/**
 * struct VvasScalerImpl - Contains info of scaler instance.
 * @vvas_ctx: VVAS Context
 * @props: Scaler Properties
 * @internal_buf_list: Internal buffers
 * @num_of_allocated_buffers: Numbers of XRT buffers allocated
 * @num_of_channels: Number of processing to be done
 * @need_preprocess: Flag to enable/disable pre-processing in soft multiscaler
 * @log_level: Log level for Scaler
 */
typedef struct {
  VvasContext         * vvas_ctx;
  VvasScalerProp        props;
  VvasList            * internal_buf_list;
  uint32_t              num_of_allocated_buffers;
  uint32_t              num_of_channels;
  bool                  need_preprocess;
  VvasLogLevel          log_level;
} VvasScalerImpl;

#endif /* __VVAS_SCALER_PRIV_H__ */
