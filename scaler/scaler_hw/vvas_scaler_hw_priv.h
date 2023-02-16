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
*  DOC: Contains structures and macros related to VVAS HW Accelerated Scaler implementation.
*/

#ifndef __VVAS_SCALER_PRIV_H__
#define __VVAS_SCALER_PRIV_H__

#include "vvas_core/vvas_scaler.h"
#include <vvas_core/vvas_context.h>
#include <vvas_core/vvas_common.h>
#include <vvas_core/vvas_memory.h>
#include <vvas_core/vvas_device.h>
#include <vvas_utils/vvas_utils.h>

/** @def VVAS_SCALE_DEFAULT_MEMORY_BANK
 *  @brief Default memory bank
 */
#define VVAS_SCALE_DEFAULT_MEMORY_BANK 0

/** @def VVAS_SCALER_MIN_WIDTH
 *  @brief This is the minimum width which can be processed using MultiScaler IP
 */
#define VVAS_SCALER_MIN_WIDTH    16

/** @def VVAS_SCALER_MIN_HEIGHT
 *  @brief This is the minimum height which can be processed using MultiScaler IP
 */
#define VVAS_SCALER_MIN_HEIGHT   16

#ifdef XLNX_PCIe_PLATFORM
/** @def KERNEL_NAME
 *  @brief Default kernel name
 */
//TODO: Need to keep only one scaler name for both the platforms
#ifdef XLNX_U30_PLATFORM
#define KERNEL_NAME "scaler"
#else
#define KERNEL_NAME "image_processing"
#endif

/** @def VVAS_SCALER_DEFAULT_PPC
 *  @brief Default pixel per clock value
 */
#define VVAS_SCALER_DEFAULT_PPC 4

/** @def VVAS_SCALER_DEFAULT_COEF_LOAD_TYPE
 *  @brief Default coefficients loading type
 */
#define VVAS_SCALER_DEFAULT_COEF_LOAD_TYPE VVAS_SCALER_COEF_AUTO_GENERATE

/** @def VVAS_SCALER_DEFAULT_NUM_TAPS
 *  @brief Default number of filter taps
 */
#define VVAS_SCALER_DEFAULT_NUM_TAPS VVAS_SCALER_FILTER_TAPS_12

/** @def VVAS_SCALER_SCALE_MODE
 *  @brief Default scaling mode
 */
#define VVAS_SCALER_SCALE_MODE VVAS_SCALER_MODE_POLYPHASE
#else
/** @def KERNEL_NAME
 *  @brief Default kernel name
 */
#define KERNEL_NAME "image_processing"

/** @def VVAS_SCALER_DEFAULT_PPC
 *  @brief Default pixel per clock value
 */
#define VVAS_SCALER_DEFAULT_PPC 2

/** @def VVAS_SCALER_DEFAULT_COEF_LOAD_TYPE
 *  @brief Default coefficients loading type
 */
#define VVAS_SCALER_DEFAULT_COEF_LOAD_TYPE VVAS_SCALER_COEF_FIXED

/** @def VVAS_SCALER_DEFAULT_NUM_TAPS
 *  @brief Default number of filter taps
 */
#define VVAS_SCALER_DEFAULT_NUM_TAPS VVAS_SCALER_FILTER_TAPS_6

/** @def VVAS_SCALER_SCALE_MODE
 *  @brief Default scaling mode
 */
#define VVAS_SCALER_SCALE_MODE VVAS_SCALER_MODE_BILINEAR
#endif

/**
 * struct VvasScalerInternlBuffer -  Contains info of internal buffers.
 * @Hcoff: XRT Buffer for storing horizontal coefficient
 * @Vcoff: XRT Buffer for storing vertical coefficient
 * @descriptor: XRT Buffer for storing processing descriptor
 * @outvideo_frame: Reference of output video frame
 */
typedef struct {
  xrt_buffer            Hcoff;
  xrt_buffer            Vcoff;
  xrt_buffer            descriptor;
  VvasVideoFrame *      outvideo_frame;
} VvasScalerInternlBuffer;

/**
 * struct VvasScalerImpl - Contains info of scaler instance.
 * @vvas_ctx: VVAS Context
 * @props: Scaler Properties
 * @kernel_handle: Kernel handle
 * @run_handle: Run Handle
 * @internal_buf_list: Internal buffers
 * @num_of_allocated_buffers: Numbers of XRT buffers allocated
 * @num_of_channels: Number of processing to be done
 * @log_level: Log level for Scaler
 */
typedef struct {
  VvasContext         * vvas_ctx;
  VvasScalerProp        props;
  vvasKernelHandle      kernel_handle;
  vvasRunHandle         run_handle;
  VvasList            * internal_buf_list;
  uint32_t              num_of_allocated_buffers;
  uint32_t              num_of_channels;
  VvasLogLevel          log_level;
} VvasScalerImpl;

#endif /* __VVAS_SCALER_PRIV_H__ */
