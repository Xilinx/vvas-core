/*
* Copyright (C) 2020-2022 Xilinx, Inc. 
* Copyright (C) 2022-2023 Advanced Micro Devices, Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation the
* rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
* sell copies of the Software, and to permit persons to whom the Software
* is furnished to do so, subject to the following conditions:
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
* KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO
* EVENT SHALL XILINX BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
* OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE. Except as contained in this notice, the name of the Xilinx shall
* not be used in advertising or otherwise to promote the sale, use or other
* dealings in this Software without prior written authorization from Xilinx.
*/
#ifndef __MULTI_SCALER_SW_H__
#define __MULTI_SCALER_SW_H__

#include "config.h"

#define HSC_PHASES 64
#define VSC_PHASES 64
#define HSC_TAPS 12
#define VSC_TAPS 12
#define XV_MULTISCALER_MAX_V_TAPS 12
#define XV_MULTISCALER_MAX_V_PHASES 64
#define XV_MULTI_SCALER_CTRL_ADDR_HWREG_NUM_OUTS_DATA   0x20
#define XV_MULTI_SCALER_CTRL_ADDR_HWREG_START_ADDR_DATA 0x30
#define STEP_PRECISION_SHIFT 16
#define STEP_PRECISION (1<<STEP_PRECISION_SHIFT)

/************** configs ***********************/
#define NR_PHASE_BITS           6
#define NR_PHASES               (1<<NR_PHASE_BITS)

#define BILINEAR 0
#define BICUBIC 1
#define POLYPHASE 2

#define HSC_PHASE_SHIFT			6
#define XPAR_V_MULTI_SCALER_0_TAPS	6
/************************************************/

typedef enum
{
  XV_MULTISCALER_TAPS_6  = 6,
  XV_MULTISCALER_TAPS_8  = 8,
  XV_MULTISCALER_TAPS_10 = 10,
  XV_MULTISCALER_TAPS_12 = 12
} XV_MULTISCALER_TAPS;

typedef enum
{
  XV_MULTI_SCALER_NONE = -1,
  XV_MULTI_SCALER_RGBX8 = 10,
  XV_MULTI_SCALER_YUVX8 = 11,
  XV_MULTI_SCALER_YUYV8 = 12,
  XV_MULTI_SCALER_RGBX10 = 15,
  XV_MULTI_SCALER_YUVX10 = 16,
  XV_MULTI_SCALER_Y_UV8 = 18,
  XV_MULTI_SCALER_Y_UV8_420 = 19, /* NV12 */
  XV_MULTI_SCALER_RGB8 = 20,
  XV_MULTI_SCALER_YUV8 = 21,
  XV_MULTI_SCALER_Y_UV10 = 22,
  XV_MULTI_SCALER_Y_UV10_420 = 23,
  XV_MULTI_SCALER_Y8 = 24,
  XV_MULTI_SCALER_Y10 = 25,
  XV_MULTI_SCALER_BGRX8 = 27,
  XV_MULTI_SCALER_UYVY8 = 28,
  XV_MULTI_SCALER_BGR8 = 29, /* BGR */
  XV_MULTI_SCALER_RGBA8 = 13, /* BGR */
  XV_MULTI_SCALER_BGRA8 = 26, /* BGR */
  XV_MULTI_SCALER_R_G_B8 = 40, /* [7:0] R, [7:0] G, [7:0] B */
  XV_MULTI_SCALER_I420 = 41,  /* [15:0] Y:Y 8:8, [7:0] U, [7:0] V */
} XV_MULTISCALER_MEMORY_FORMATS;

typedef struct {
  uint32_t msc_widthIn;
  uint32_t msc_widthOut;
  uint32_t msc_heightIn;
  uint32_t msc_heightOut;
  uint32_t msc_lineRate;
  uint32_t msc_pixelRate;
  uint32_t msc_inPixelFmt;
  uint32_t msc_outPixelFmt;
  uint32_t msc_strideIn;
  uint32_t msc_strideOut;
  void *msc_srcImgBuf0;
  void *msc_srcImgBuf1;
  void *msc_srcImgBuf2;
  void *msc_dstImgBuf0;
  void *msc_dstImgBuf1;
  void *msc_dstImgBuf2;
  void *msc_blkmm_hfltCoeff;
  void *msc_blkmm_vfltCoeff;
#ifdef ENABLE_PPE_SUPPORT
  /** Mean subtraction value for R component */
  uint32_t msc_alpha_0;
  /** Mean subtraction value for G component */
  uint32_t msc_alpha_1;
  /** Mean subtraction value for B component */
  uint32_t msc_alpha_2;
  /** Scale value for R component */
  uint32_t msc_beta_0;
  /** Scale value for G component */
  uint32_t msc_beta_1;
  /** Scale value for B component */
  uint32_t msc_beta_2;
#endif
  void *msc_nxtaddr;
} MULTI_SCALER_DESC_STRUCT;

#define DESC_SIZE  (sizeof(MULTI_SCALER_DESC_STRUCT))
#define COEFF_SIZE  (sizeof(short)*64*12)

#define MAX_FILTER_SIZE         12
#define SWS_MAX_REDUCE_CUTOFF   0.000001
#define ROUNDED_DIV(a,b)        (((a)>0 ? (a) + ((b)>>1) : (a) - ((b)>>1))/(b))

typedef enum {
  XLXN_FIXED_COEFF_SR13,
  XLXN_FIXED_COEFF_SR15,
  XLXN_FIXED_COEFF_SR2,
  XLXN_FIXED_COEFF_SR25,
  XLXN_FIXED_COEFF_TAPS_10,
  XLXN_FIXED_COEFF_TAPS_12,
  XLXN_FIXED_COEFF_TAPS_6,
} XLNX_FIXED_FILTER_COEFF_TYPE;


#endif /* __GST_MULTI_SCALER_HW_H__ */
