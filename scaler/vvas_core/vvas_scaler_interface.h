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
 * DOC: VVAS Scaler Interfaces
 * VVAS Scaler Interfaces for handling HW and SW library.
 */

#ifndef _VVAS_SCALER_INTERFACE_H_
#ifdef __cplusplus
extern "C" {
#endif
#define _VVAS_SCALER_INTERFACE_H_

#include "vvas_scaler.h"

/** @def VVAS_SCALER_INTERFACE
 *  @brief Symbol name of scaler interface, the implementation library must name the interface (
 *         @ref VvasScalerInterface) variable as this.
 */
#define VVAS_SCALER_INTERFACE  "VVAS_SCALER"

/**
 *  typedef VvasScalerInstace - Opaque handle to VvasScaler instance
 */
typedef void VvasScalerInstace;

/**
 *  typedef vvas_scaler_create_fptr - Function pointer to the implementation of @vvas_scaler_create()
 *  @ctx: VvasContext handle created using @vvas_context_create()
 *  @kernel_name: Scaler kernel name
 *  @log_level: Log level for VvasScaler
 *
 *  Return: Scaler instance pointer
 */

typedef VvasScalerInstace * (* vvas_scaler_create_fptr) (VvasContext *ctx,
    const char *kernel_name, VvasLogLevel log_level);

/**
 *  typedef vvas_scaler_channel_add_fptr -  Function pointer to the implementation of @vvas_scaler_channel_add()
 *  @hndl:  @VvasScalerInstace handle pointer created using @vvas_scaler_create()
 *  @src_rect: Source Rect @VvasScalerRect
 *  @dst_rect: Destination Rect @VvasScalerRect.
 *  @ppe:  Pre processing parameters @VvasScalerPpe\n NULL if no PPE is needed
 *
 *  Return: VvasReturnType
 */

typedef VvasReturnType (* vvas_scaler_channel_add_fptr) (VvasScalerInstace * hndl,
    VvasScalerRect *src_rect, VvasScalerRect *dst_rect, VvasScalerPpe *ppe, VvasScalerParam *param);

/**
 *  typedef vvas_scaler_process_frame_fptr - Function pointer to the implementation of @vvas_scaler_process_frame()
 *  @hndl: VvasScaler handle pointer created using @vvas_scaler_create()
 *
 *  Return: VvasReturnType
 */
typedef VvasReturnType (* vvas_scaler_process_frame_fptr) (VvasScalerInstace * hndl);

/**
 *  typedef vvas_scaler_destroy_fptr - Function pointer to the implementation of @vvas_scaler_destroy()
 *  @hndl:    VvasScaler handle pointer created using @vvas_scaler_create()
 *
 *  Return: VvasReturnType
 */
typedef VvasReturnType (* vvas_scaler_destroy_fptr) (VvasScalerInstace * hndl);

/**
 *  typedef vvas_scaler_prop_get_fptr -  Function pointer to the implementation of @vvas_scaler_prop_get()
 *  @hndl:  VvasScaler handle pointer created using @vvas_scaler_create()
 *  @prop:   Scaler properties @VvasScalerProp
 *
 *  Return: VvasReturnType
 */
typedef VvasReturnType (* vvas_scaler_prop_get_fptr) (VvasScalerInstace * hndl,
    VvasScalerProp *prop);

/**
 *  typedef vvas_scaler_prop_set_fptr - Function pointer to the implementation of @vvas_scaler_prop_set()
 *  @hndl:  VvasScaler handle pointer created using @vvas_scaler_create()
 *  @prop:  Scaler properties @VvasScalerProp
 *
 *  Return: VvasReturnType
 */
typedef VvasReturnType (* vvas_scaler_prop_set_fptr) (VvasScalerInstace * hndl,
    VvasScalerProp *prop);

/**
 *  typedef vvas_scaler_set_filter_coef_fptr - Function pointer to the implementation of @vvas_scaler_set_filter_coef()
 *  @hndl:  VvasScaler handle pointer created using @vvas_scaler_create()
 *  @coef_type:   coef_type @VvasScalerFilterCoefType
 *  @tbl:   Reference of VVAS_SCALER_MAX_PHASESxVVAS_SCALER_FILTER_TAPS_12 array of short
 *
 *  Return: VvasReturnType
 */
typedef VvasReturnType (* vvas_scaler_set_filter_coef_fptr) (VvasScalerInstace * hndl,
    VvasScalerFilterCoefType coef_type,
    const int16_t tbl[VVAS_SCALER_MAX_PHASES][VVAS_SCALER_FILTER_TAPS_12]);

/**
 * struct VvasScalerInterface -  Scaler Implementation library must export this structure with variable name as @ref VVAS_SCALER_INTERFACE.
 *                               The scaler wrapper lib looks for this name in implementation library.
 * @kernel_name: Name of the kernel which library implements
 * @vvas_scaler_create_impl: Function pointer to create scaler
 * @vvas_scaler_prop_get_impl: Function pointer to get property
 * @vvas_scaler_prop_set_impl: Function pointer to set property
 * @vvas_scaler_channel_add_impl: Function pointer to add channel
 * @vvas_scaler_process_frame_impl: Function pointer to process frame
 * @vvas_scaler_set_filter_coef_impl: Function pointer to set filter coefficients
 * @vvas_scaler_destroy_impl: Function pointer to destroy scaler
 */
typedef struct {
  const char                        *   kernel_name;
  vvas_scaler_create_fptr               vvas_scaler_create_impl;
  vvas_scaler_prop_get_fptr             vvas_scaler_prop_get_impl;
  vvas_scaler_prop_set_fptr             vvas_scaler_prop_set_impl;
  vvas_scaler_channel_add_fptr          vvas_scaler_channel_add_impl;
  vvas_scaler_process_frame_fptr        vvas_scaler_process_frame_impl;
  vvas_scaler_set_filter_coef_fptr      vvas_scaler_set_filter_coef_impl;
  vvas_scaler_destroy_fptr              vvas_scaler_destroy_impl;
} VvasScalerInterface;

#ifdef __cplusplus
}
#endif
#endif /* _VVAS_SCALER_INTERFACE_H_ */
