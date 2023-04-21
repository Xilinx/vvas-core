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
 * DOC: VVAS Scaler APIs
 * This file contains public methods and data structures related to VVAS scaler.
 */

#ifndef __VVAS_SCALER_H__
#ifdef __cplusplus
extern "C" {
#endif
#define __VVAS_SCALER_H__

#include <vvas_core/vvas_video.h>
#include <vvas_core/vvas_context.h>
#include <vvas_core/vvas_common.h>

/* Maximum number of phases */
#define VVAS_SCALER_MAX_PHASES    64

/* Environment variable name for VVAS Core Scaler libraries */
#define VVAS_CORE_LIB_PATH_ENV_VAR "VVAS_CORE_LIBS_PATH"

/* Environment variable name for VVAS Core Scaler Configuration file */
#define VVAS_CORE_CFG_PATH_ENV_VAR "VVAS_CORE_CFG_PATH"

#define VVAS_SCALER_MAX_SUPPORT_FMT 64

#ifdef XLNX_PCIe_PLATFORM
  #define VVAS_SCALER_STATIC_CONFIG_FILE_PATH "/opt/xilinx/vvas/share/image_processing.cfg"
#else
  #define VVAS_SCALER_STATIC_CONFIG_FILE_PATH "/run/media/mmcblk0p1/image_processing.cfg"
#endif

/**
 * enum VvasScalerCoefLoadType - Enum for holding type of filter coefficients loading type
 * @VVAS_SCALER_COEF_FIXED: Fixed filter coefficients type
 * @VVAS_SCALER_COEF_AUTO_GENERATE: Auto generate filter coefficients type
 */
typedef enum {
  VVAS_SCALER_COEF_FIXED,
  VVAS_SCALER_COEF_AUTO_GENERATE,
} VvasScalerCoefLoadType;

/**
 * enum VvasScalerMode - Enum for holding scaling modes.
 * @VVAS_SCALER_MODE_BILINEAR: Bilinear scaling mode
 * @VVAS_SCALER_MODE_BICUBIC: BiCubic scaling mode
 * @VVAS_SCALER_MODE_POLYPHASE: PolyPhase scaling mode
 */
typedef enum {
  VVAS_SCALER_MODE_BILINEAR,
  VVAS_SCALER_MODE_BICUBIC,
  VVAS_SCALER_MODE_POLYPHASE
} VvasScalerMode;

/**
 * enum VvasScalerType - Enum for holding scaling types.
 * @VVAS_SCALER_DEFAULT: Default Scale Type
 * @VVAS_SCALER_LETTERBOX: LetterBox Scale which maintain aspect ratio
 * @VVAS_SCALER_ENVELOPE_CROPPED: Envelope scale and center cropped
 */
typedef enum {
  VVAS_SCALER_DEFAULT,
  VVAS_SCALER_LETTERBOX,
  VVAS_SCALER_ENVELOPE_CROPPED
} VvasScalerType;

/**
 * enum VvasScalerHorizontalAlign - Enum for holding horizontal alignment options.
 * @VVAS_SCALER_HORZ_ALIGN_CENTER: Center Alignment into output frame
 * @VVAS_SCALER_HORZ_ALIGN_LEFT: Left Alignment into output frame
 * @VVAS_SCALER_HORZ_ALIGN_RIGHT: Right Alignment into output frame
 */
typedef enum {
  VVAS_SCALER_HORZ_ALIGN_CENTER,
  VVAS_SCALER_HORZ_ALIGN_LEFT,
  VVAS_SCALER_HORZ_ALIGN_RIGHT
} VvasScalerHorizontalAlign;

/**
 * enum VvasScalerVerticalAlign - Enum for holding vertical alignment options.
 * @VVAS_SCALER_VERT_ALIGN_CENTER: Center Alignment into output frame
 * @VVAS_SCALER_VERT_ALIGN_TOP: Top Alignment into output frame
 * @VVAS_SCALER_VERT_ALIGN_BOTTOM: Bottom Alignment into output frame
 */
typedef enum {
  VVAS_SCALER_VERT_ALIGN_CENTER,
  VVAS_SCALER_VERT_ALIGN_TOP,
  VVAS_SCALER_VERT_ALIGN_BOTTOM
} VvasScalerVerticalAlign;

/**
 * struct VvasScalerParam - Contains Information related to Scaler Parameters
 * @type: Scale Type
 * @horz_align: Horizontal Alignment
 * @vert_align: Vertical Alignment
 * @smallest_side_num: Smallest side numerator to calculate scale ratio for envelope scale
 */
typedef struct {
  VvasScalerType type;
  VvasScalerHorizontalAlign horz_align;
  VvasScalerVerticalAlign vert_align;
  uint16_t smallest_side_num;
} VvasScalerParam;

/**
 * enum VvasScalerFilterTaps - Enum for holding number of filter taps.
 * @VVAS_SCALER_FILTER_TAPS_6: 6 filter taps
 * @VVAS_SCALER_FILTER_TAPS_8: 8 filter taps
 * @VVAS_SCALER_FILTER_TAPS_10: 10 filter taps
 * @VVAS_SCALER_FILTER_TAPS_12: 12 filter taps
 */
typedef enum {
  VVAS_SCALER_FILTER_TAPS_6   = 6,
  VVAS_SCALER_FILTER_TAPS_8   = 8,
  VVAS_SCALER_FILTER_TAPS_10  = 10,
  VVAS_SCALER_FILTER_TAPS_12  = 12,
} VvasScalerFilterTaps;

/**
 * struct VvasScalerProp - Contains Scaler Properties.
 * @coef_load_type: Coefficient loading type
 * @smode: Scaling mode
 * @ftaps: Filter taps
 * @ppc: Pixel per clock
 * @mem_bank: Memory bank on which the internal buffers should be allocated
 * @n_fmts: Number of color formats supported by scaler
 * @supported_fmts: Array of video formats supported by scaler (valid from 0 @n_fmts-1)
 */
typedef struct {
  VvasScalerCoefLoadType    coef_load_type;
  VvasScalerMode            smode;
  VvasScalerFilterTaps      ftaps;
  uint32_t                  ppc;
  uint32_t                  mem_bank;
  uint8_t n_fmts;
  VvasVideoFormat supported_fmts[VVAS_SCALER_MAX_SUPPORT_FMT];
} VvasScalerProp;

/**
 * enum VvasScalerFilterCoefType - Enum for holding filter coefficients type.
 * @VVAS_SCALER_FILTER_COEF_SR13: Scaling ration 1.3
 * @VVAS_SCALER_FILTER_COEF_SR15: Scaling ration 1.5
 * @VVAS_SCALER_FILTER_COEF_SR2: Scaling ration 2, 8 tap
 * @VVAS_SCALER_FILTER_COEF_SR25: Scaling ration 2.5
 * @VVAS_SCALER_FILTER_COEF_TAPS_10: 10 tap
 * @VVAS_SCALER_FILTER_COEF_TAPS_12: 12 tap
 * @VVAS_SCALER_FILTER_COEF_TAPS_6: 6 tap, Always used for up scale
 */
typedef enum {
  VVAS_SCALER_FILTER_COEF_SR13,
  VVAS_SCALER_FILTER_COEF_SR15,
  VVAS_SCALER_FILTER_COEF_SR2,
  VVAS_SCALER_FILTER_COEF_SR25,
  VVAS_SCALER_FILTER_COEF_TAPS_10,
  VVAS_SCALER_FILTER_COEF_TAPS_12,
  VVAS_SCALER_FILTER_COEF_TAPS_6,
} VvasScalerFilterCoefType;

/**
 * struct VvasScalerRect - Contains Information related to frame region of interest.
 * @frame: VvasVideoFrame
 * @x: X coordinate
 * @y: Y coordinate
 * @width: Width of Rect
 * @height: Height of Rect
 */
typedef struct {
  VvasVideoFrame *frame;
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
} VvasScalerRect;

/**
 * struct VvasScalerPpe - Contains Information related to Pre-processing parameters
 * @mean_r: PreProcessing parameter alpha/mean red channel value
 * @mean_g: PreProcessing parameter alpha/mean green channel value
 * @mean_b: PreProcessing parameter alpha/mean blue channel value
 * @scale_r: PreProcessing parameter beta/scale red channel value
 * @scale_g: PreProcessing parameter beta/scale green channel value
 * @scale_b: PreProcessing parameter beta/scale blue channel value
 */
typedef struct {
  float mean_r;
  float mean_g;
  float mean_b;
  float scale_r;
  float scale_g;
  float scale_b;
} VvasScalerPpe;

/**
 *  typedef VvasScaler - Opaque handle for the VvasScaler instance
 */
typedef void VvasScaler;

/**
 *  vvas_scaler_create() - Creates Scaler's instance.
 *
 *  @ctx: VvasContext handle created using @vvas_context_create
 *  @kernel_name: Scaler kernel name
 *  @log_level: Logging level
 *
 *  Return: On Success returns VvasScaler handle pointer, on Failure returns NULL
 *
 */
VvasScaler * vvas_scaler_create (VvasContext * ctx,
                                 const char * kernel_name,
                                 VvasLogLevel log_level);

/**
 *  vvas_scaler_channel_add() - This API adds one processing channel configuration.
 *  One channel represents a set of operations, like resize, color space conversion,
 *  PPE etc. to be performed on the input buffer.
 *  Hardware Scaler may have alignment requirement. In such case this API will adjust
 *  x, y, width and height of src_rect and dst_rect. Adjusted values will be updated
 *  in the src_rect and dst_rect.
 *
 *  @hndl: VvasContext handle created using @vvas_context_create
 *  @src_rect: Source Rect @VvasScalerRect
 *  @dst_rect: Destination Rect @VvasScalerRect
 *  @ppe: Preprocessing parameters @VvasScalerPpe, NULL if no PPE is needed
 *  @param: Scaler type and Alignment parameters @VvasScalerParam
 *
 *  Return: VvasReturnType
 *
 */
VvasReturnType vvas_scaler_channel_add (VvasScaler * hndl,
                                        VvasScalerRect * src_rect,
                                        VvasScalerRect * dst_rect,
                                        VvasScalerPpe * ppe,
                                        VvasScalerParam * param);


/**
 *  vvas_scaler_process_frame() - This API does processing of channels added using @vvas_scaler_channel_add
 *  There can be multiple channels added to perform different operations on the input frame.
 *  All these operations are performed in context of this API call.
 *
 *  @hndl: VvasScaler handle pointer created using @vvas_scaler_create
 *
 *  Return: VvasReturnType
 *
 */
VvasReturnType vvas_scaler_process_frame (VvasScaler * hndl);


/**
 *  vvas_scaler_destroy() - This API destroys the scaler instance created using @vvas_scaler_create
 *
 *  @hndl: VvasScaler handle pointer created using @vvas_scaler_create
 *
 *  Return: VvasReturnType
 *
 */
VvasReturnType vvas_scaler_destroy (VvasScaler * hndl);


/**
 *  vvas_scaler_set_filter_coef() - This API can be used to overwrite default filter coefficients.
 *
 *  @hndl: VvasScaler handle pointer created using @vvas_scaler_create
 *  @coef_type: Filter coefficients type @VvasScalerFilterCoefType
 *  @tbl: Filter coefficients, Reference of VVAS_SCALER_MAX_PHASESxVVAS_SCALER_FILTER_TAPS_12 array of short
 *
 *  Return: VvasReturnType
 *
 */
VvasReturnType vvas_scaler_set_filter_coef (VvasScaler * hndl,
              VvasScalerFilterCoefType coef_type,
              const int16_t tbl[VVAS_SCALER_MAX_PHASES][VVAS_SCALER_FILTER_TAPS_12]);


/**
 *  vvas_scaler_prop_get() - This API will fill current scaler properties.
 *  This API returns the default properties if called before setting these properties.
 *
 *  @hndl: VvasScaler handle pointer created using @vvas_scaler_create.
 *         If @hndl is null, then static configurations will be returned in @prop by parsing scaler config file "/opt/xilinx/vvas/share/image_processing.cfg"
 *  @prop: Scaler properties @VvasScalerProp
 *
 *  Return: VvasReturnType
 *
 */
VvasReturnType vvas_scaler_prop_get (VvasScaler * hndl, VvasScalerProp * prop);


/**
 *  vvas_scaler_prop_set() - This API is used to set properties of VvasScaler
 *
 *  @hndl: VvasScaler handle pointer created using @vvas_scaler_create
 *  @prop: Scaler properties @VvasScalerProp
 *
 *  Return: VvasReturnType
 *
 */
VvasReturnType vvas_scaler_prop_set (VvasScaler * hndl, VvasScalerProp * prop);

#ifdef __cplusplus
}
#endif
#endif /* __VVAS_SCALER_H__ */
