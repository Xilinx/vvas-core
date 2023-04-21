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

#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vvas_core/vvas_video.h>
#include <vvas_core/vvas_video_priv.h>
#include <vvas_core/vvas_device.h>
#include <vvas_core/vvas_memory_priv.h>

#include "vvas_core/vvas_scaler.h"
#include "vvas_core/vvas_scaler_interface.h"
#include "multi_scaler_hw.h"
#include "vvas_scaler_hw_priv.h"

/** @def VVAS_SCALER_DEFAULT_TIMEOUT
 *  @brief This much time we should wait for getting the response of IP
 */
#define VVAS_SCALER_MAX_EXEC_WAIT_RETRY_CNT     10

/** @def VVAS_SCALER_DEFAULT_TIMEOUT
 *  @brief This much time we should wait for getting the response of IP
 */
#define VVAS_SCALER_DEFAULT_TIMEOUT             1000    /* 1 sec */

/** @def WIDTH_ALIGN
 *  @brief Width alignment requirement for MultiScaler IP
 */
#define WIDTH_ALIGN (8 * self->props.ppc)
/** @def HEIGHT_ALIGN
 *  @brief Height alignment requirement for MultiScaler IP
 */
#define HEIGHT_ALIGN 2

/** @def SCALER_MAX(a, b)
 *  @param [in] a - First input value
 *  @param [in] b - Second input value
 *  @returns    Maximum value.
 *  @details    This macro computes maximum of two value
 */
#define SCALER_MAX(a,b) ( (a)>(b) ? (a) : (b) )

/** @def SCALER_MIN(a, b)
 *  @param [in] a - First input value
 *  @param [in] b - Second input value
 *  @returns    Minimum value.
 *  @details    This macro computes minimum of two value
 */
#define SCALER_MIN(a,b) ( (a)<(b) ? (a) : (b) )

/** @def VVAS_SCALER_DEFAULT_MEAN
 *  @brief Default values of mean/alpha
 */
#define VVAS_SCALER_DEFAULT_MEAN    0

/** @def VVAS_SCALER_DEFAULT_SCALE
 *  @brief Default values of scale/beta
 */
#define VVAS_SCALER_DEFAULT_SCALE   1


/** @def VVAS_SCALER_FIXED_FILTER_COEF_SIZE
 *  @brief Size of fixed filter coefficients
 */
#define VVAS_SCALER_FIXED_FILTER_COEF_SIZE \
  ((sizeof(int16_t) * VVAS_SCALER_MAX_PHASES * VVAS_SCALER_FILTER_TAPS_12))


#define GET_INTERNAL_BUFFERS(idx) \
  (VvasScalerInternlBuffer *) \
                    vvas_list_nth_data (self->internal_buf_list, idx);

/**
 *  @fn static int64_t vvas_scaler_abs (int64_t val)
 *  @param [in] val - Input value
 *  @return Absolute value
 *  @brief  This function returns the absolute value.
 */
static inline int64_t
vvas_scaler_abs (int64_t val)
{
  return ((val) > 0) ? (val) : (-(val));
}

/**
 *  @fn static uint32_t vvas_scaler_align (uint32_t stride_in, uint16_t alignment)
 *  @param [in] stride_in   - Input stride
 *  @param [in] alignment   - alignment requirement
 *  @return aligned value
 *  @brief  This function aligns the stride_in value to the next aligned integer value
 */
static inline uint32_t
vvas_scaler_align (uint32_t stride_in, uint16_t alignment)
{
  uint32_t stride;
  /* Align the passed value (stride_in) by passed alignment value, this will
   * return the next aligned integer */
  stride = (((stride_in) + alignment - 1) / alignment) * alignment;
  return stride;
}

/**
 *  @fn static uint32_t vvas_scaler_colorformat (const VvasScalerImpl *self,
 *                                               VvasVideoFormat vvas_format)
 *  @param [in] self        -   VvasScalerImpl instance pointer
 *  @param [in] vvas_format -   VVAS video formats
 *  @return MuliScaler color format
 *  @brief  This function converts VVAS video format to MultiScaler's video format
 */
static uint32_t
vvas_scaler_colorformat (const VvasScalerImpl * self,
    VvasVideoFormat vvas_format)
{
  /* Convert VVAS video format to Scaler's video format */
  uint32_t scaler_video_format = XV_MULTI_SCALER_NONE;

  switch (vvas_format) {
    case VVAS_VIDEO_FORMAT_Y_UV8_420:
      /* NV12 */
      scaler_video_format = XV_MULTI_SCALER_Y_UV8_420;
      break;

    case VVAS_VIDEO_FORMAT_NV12_10LE32:
      /* NV12 10 Bit LE */
      scaler_video_format = XV_MULTI_SCALER_Y_UV10_420;
      break;

    case VVAS_VIDEO_FORMAT_RGB:
      scaler_video_format = XV_MULTI_SCALER_RGB8;
      break;

    case VVAS_VIDEO_FORMAT_BGR:
      scaler_video_format = XV_MULTI_SCALER_BGR8;
      break;

    case VVAS_VIDEO_FORMAT_I420:
      scaler_video_format = XV_MULTI_SCALER_I420;
      break;

    case VVAS_VIDEO_FORMAT_GRAY8:
      scaler_video_format = XV_MULTI_SCALER_Y8;
      break;

    case VVAS_VIDEO_FORMAT_GRAY10_LE32:
      scaler_video_format = XV_MULTI_SCALER_Y10;
      break;

    default:
      LOG_ERROR (self->log_level, "Not supporting %d format yet", vvas_format);
      break;
  }
  return scaler_video_format;
}

/**
 *  @fn static bool vvas_scaler_allocate_internal_buffers (VvasScalerImpl * self)
 *  @param [in] self            - VvasScalerImpl handle
 *  @return true on success\n false on failure
 *  @brief  This function allocates XRT buffers for descriptor, horizontal and vertical coefficients.
 */
static bool
vvas_scaler_allocate_internal_buffers (VvasScalerImpl * self)
{
  VvasScalerInternlBuffer *internal_buf;
  int32_t iret;

  internal_buf = (VvasScalerInternlBuffer *)
      calloc (1, sizeof (VvasScalerInternlBuffer));

  if (!internal_buf) {
    return false;
  }

  /* Allocate XRT buffer for holding horizontal coefficients */
  iret = vvas_xrt_alloc_xrt_buffer (self->vvas_ctx->dev_handle, COEFF_SIZE,
      VVAS_BO_FLAGS_NONE, self->props.mem_bank, &internal_buf->Hcoff);
  if (iret < 0) {
    LOG_ERROR (self->log_level,
        "failed to allocate horizontal coefficients command buffer");
    goto error;
  }

  /* Allocate XRT buffer for holding vertical coefficients */
  iret = vvas_xrt_alloc_xrt_buffer (self->vvas_ctx->dev_handle, COEFF_SIZE,
      VVAS_BO_FLAGS_NONE, self->props.mem_bank, &internal_buf->Vcoff);
  if (iret < 0) {
    LOG_ERROR (self->log_level,
        "failed to allocate vertical coefficients command buffer");
    goto error;
  }

  /* Allocate XRT buffer for holding processing descriptors */
  iret = vvas_xrt_alloc_xrt_buffer (self->vvas_ctx->dev_handle, DESC_SIZE,
      VVAS_BO_FLAGS_NONE, self->props.mem_bank, &internal_buf->descriptor);
  if (iret < 0) {
    LOG_ERROR (self->log_level, "failed to allocate descriptor command buffer");
    goto error;
  }

  LOG_DEBUG (self->log_level, "DESC phy 0x%lX  virt  %p",
      internal_buf->descriptor.phy_addr, internal_buf->descriptor.user_ptr);
  LOG_DEBUG (self->log_level, "HCoef phy 0x%lX  virt  %p",
      internal_buf->Hcoff.phy_addr, internal_buf->Hcoff.user_ptr);
  LOG_DEBUG (self->log_level, "VCoef phy 0x%lX  virt  %p",
      internal_buf->Vcoff.phy_addr, internal_buf->Vcoff.user_ptr);

  self->internal_buf_list =
      vvas_list_append (self->internal_buf_list, internal_buf);
  LOG_DEBUG (self->log_level, "allocated internal buffer total: %u",
      vvas_list_length (self->internal_buf_list));
  return true;

error:
  if (internal_buf->Hcoff.user_ptr) {
    vvas_xrt_free_xrt_buffer (&internal_buf->Hcoff);
  }
  if (internal_buf->Vcoff.user_ptr) {
    vvas_xrt_free_xrt_buffer (&internal_buf->Vcoff);
  }
  if (internal_buf->descriptor.user_ptr) {
    vvas_xrt_free_xrt_buffer (&internal_buf->descriptor);
  }
  if (internal_buf) {
    free (internal_buf);
  }
  return false;
}

/**
 *  @fn static void vvas_scaler_free_internal_buffers (VvasScalerImpl * self)
 *  @param [in] self    - VvasScalerImpl handle
 *  @return None
 *  @brief  This function frees XRT buffers allocated for descriptor, horizontal
 *          and vertical coefficients.
 */
static void
vvas_scaler_free_internal_buffers (void *data, void *user_data)
{
  VvasScalerInternlBuffer *internal_buf;

  if (!data) {
    return;
  }

  internal_buf = (VvasScalerInternlBuffer *) data;

  if (internal_buf->Hcoff.user_ptr) {
    /* Free XRT buffers for Horizontal coefficients */
    vvas_xrt_free_xrt_buffer (&internal_buf->Hcoff);
  }
  if (internal_buf->Vcoff.user_ptr) {
    /* Free XRT buffers for vertical coefficients */
    vvas_xrt_free_xrt_buffer (&internal_buf->Vcoff);
  }
  if (internal_buf->descriptor.user_ptr) {
    /* Free XRT buffers for the descriptors */
    vvas_xrt_free_xrt_buffer (&internal_buf->descriptor);
  }
  free (internal_buf);
}

/**
 *  @fn static int32_t vvas_scaler_log2_val (uint32_t val)
 *  @param [in] val - value
 *  @return log to the core 2 of passed val
 *  @brief  This function calculates the log to the core 2 of the passed value,
 *          In simple terms it returns the number of bits required for passed val.
 */
static inline int32_t
vvas_scaler_log2_val (uint32_t val)
{
  /* Calculate log to the core 2 */
  int32_t cnt = 0;
  while (val > 1) {
    val = val >> 1;
    cnt++;
  }
  return cnt;
}

/**
 *  @fn static void vvas_scaler_coff_fill (void *Hcoeff_BufAddr, void *Vcoeff_BufAddr, float scale)
 *  @param [out] Hcoeff_BufAddr - Address of horizontal coefficients buffer
 *  @param [out] Hcoeff_BufAddr - Address of vertical coefficients buffer
 *  @param [in] scale           - Scaling ratio
 *  @return None
 *  @brief  This function fills predefined horizontal and vertical coefficients cored on the passed
 *          scaling ratio.
 */
static void
vvas_scaler_coff_fill (void *Hcoeff_BufAddr, void *Vcoeff_BufAddr, float scale)
{
  uint16_t *hpoly_coeffs, *vpoly_coeffs;        /* int32_t need to check */
  int32_t uy = 0;
  int32_t temp_p;
  int32_t temp_t;

  hpoly_coeffs = (uint16_t *) Hcoeff_BufAddr;
  vpoly_coeffs = (uint16_t *) Vcoeff_BufAddr;

  if ((scale >= 2) && (scale < 2.5)) {
    if (XPAR_V_MULTI_SCALER_0_TAPS == 6) {
      /* Run for 64 phase */
      for (temp_p = 0; temp_p < 64; temp_p++)
        /* Run for XPAR_V_MULTI_SCALER_0_TAPS taps */
        for (temp_t = 0; temp_t < XPAR_V_MULTI_SCALER_0_TAPS; temp_t++) {
          *(hpoly_coeffs + uy) =
              XV_multiscaler_fixedcoeff_taps6_6C[temp_p][temp_t];
          *(vpoly_coeffs + uy) =
              XV_multiscaler_fixedcoeff_taps6_6C[temp_p][temp_t];
          uy = uy + 1;
        }
    } else {
      /* Run for 64 phase */
      for (temp_p = 0; temp_p < 64; temp_p++)
        /* Run for XPAR_V_MULTI_SCALER_0_TAPS taps */
        for (temp_t = 0; temp_t < XPAR_V_MULTI_SCALER_0_TAPS; temp_t++) {
          *(hpoly_coeffs + uy) =
              XV_multiscaler_fixedcoeff_taps8_12C[temp_p][temp_t];
          *(vpoly_coeffs + uy) =
              XV_multiscaler_fixedcoeff_taps8_12C[temp_p][temp_t];
          uy = uy + 1;
        }
    }
  }

  if ((scale >= 2.5) && (scale < 3)) {
    if (XPAR_V_MULTI_SCALER_0_TAPS >= 10) {
      /* Run for 64 phase */
      for (temp_p = 0; temp_p < 64; temp_p++)
        /* Run for XPAR_V_MULTI_SCALER_0_TAPS taps */
        for (temp_t = 0; temp_t < XPAR_V_MULTI_SCALER_0_TAPS; temp_t++) {
          *(hpoly_coeffs + uy) =
              XV_multiscaler_fixedcoeff_taps10_12C[temp_p][temp_t];
          *(vpoly_coeffs + uy) =
              XV_multiscaler_fixedcoeff_taps10_12C[temp_p][temp_t];
          uy = uy + 1;
        }
    } else {
      if (XPAR_V_MULTI_SCALER_0_TAPS == 6) {
        /* Run for 64 phase */
        for (temp_p = 0; temp_p < 64; temp_p++)
          /* Run for XPAR_V_MULTI_SCALER_0_TAPS taps */
          for (temp_t = 0; temp_t < XPAR_V_MULTI_SCALER_0_TAPS; temp_t++) {
            *(hpoly_coeffs + uy) =
                XV_multiscaler_fixedcoeff_taps6_6C[temp_p][temp_t];
            *(vpoly_coeffs + uy) =
                XV_multiscaler_fixedcoeff_taps6_6C[temp_p][temp_t];
            uy = uy + 1;
          }
      } else {
        /* Run for 64 phase */
        for (temp_p = 0; temp_p < 64; temp_p++)
          /* Run for XPAR_V_MULTI_SCALER_0_TAPS taps */
          for (temp_t = 0; temp_t < XPAR_V_MULTI_SCALER_0_TAPS; temp_t++) {
            *(hpoly_coeffs + uy) =
                XV_multiscaler_fixedcoeff_taps8_8C[temp_p][temp_t];
            *(vpoly_coeffs + uy) =
                XV_multiscaler_fixedcoeff_taps8_8C[temp_p][temp_t];
            uy = uy + 1;
          }
      }
    }
  }

  if ((scale >= 3) && (scale < 3.5)) {
    if (XPAR_V_MULTI_SCALER_0_TAPS == 12) {
      /* Run for 64 phase */
      for (temp_p = 0; temp_p < 64; temp_p++)
        /* Run for XPAR_V_MULTI_SCALER_0_TAPS taps */
        for (temp_t = 0; temp_t < XPAR_V_MULTI_SCALER_0_TAPS; temp_t++) {
          *(hpoly_coeffs + uy) =
              XV_multiscaler_fixedcoeff_taps12_12C[temp_p][temp_t];
          *(vpoly_coeffs + uy) =
              XV_multiscaler_fixedcoeff_taps12_12C[temp_p][temp_t];
          uy = uy + 1;
        }
    } else {
      if (XPAR_V_MULTI_SCALER_0_TAPS == 6) {
        /* Run for 64 phase */
        for (temp_p = 0; temp_p < 64; temp_p++)
          /* Run for XPAR_V_MULTI_SCALER_0_TAPS taps */
          for (temp_t = 0; temp_t < XPAR_V_MULTI_SCALER_0_TAPS; temp_t++) {
            *(hpoly_coeffs + uy) =
                XV_multiscaler_fixedcoeff_taps6_6C[temp_p][temp_t];
            *(vpoly_coeffs + uy) =
                XV_multiscaler_fixedcoeff_taps6_6C[temp_p][temp_t];
            uy = uy + 1;
          }
      }
      if (XPAR_V_MULTI_SCALER_0_TAPS == 8) {
        /* Run for 64 phase */
        for (temp_p = 0; temp_p < 64; temp_p++)
          /* Run for XPAR_V_MULTI_SCALER_0_TAPS taps */
          for (temp_t = 0; temp_t < XPAR_V_MULTI_SCALER_0_TAPS; temp_t++) {
            *(hpoly_coeffs + uy) =
                XV_multiscaler_fixedcoeff_taps8_8C[temp_p][temp_t];
            *(vpoly_coeffs + uy) =
                XV_multiscaler_fixedcoeff_taps8_8C[temp_p][temp_t];
            uy = uy + 1;
          }
      }
      if (XPAR_V_MULTI_SCALER_0_TAPS == 10) {
        /* Run for 64 phase */
        for (temp_p = 0; temp_p < 64; temp_p++)
          /* Run for XPAR_V_MULTI_SCALER_0_TAPS taps */
          for (temp_t = 0; temp_t < XPAR_V_MULTI_SCALER_0_TAPS; temp_t++) {
            *(hpoly_coeffs + uy) =
                XV_multiscaler_fixedcoeff_taps10_10C[temp_p][temp_t];
            *(vpoly_coeffs + uy) =
                XV_multiscaler_fixedcoeff_taps10_10C[temp_p][temp_t];
            uy = uy + 1;
          }
      }
    }
  }

  if ((scale >= 3.5) || (scale < 2 && scale >= 1)) {
    if (XPAR_V_MULTI_SCALER_0_TAPS == 6) {
      /* Run for 64 phase */
      for (temp_p = 0; temp_p < 64; temp_p++) {
        if (temp_p > 60) {
        }
        /* Run for XPAR_V_MULTI_SCALER_0_TAPS taps */
        for (temp_t = 0; temp_t < XPAR_V_MULTI_SCALER_0_TAPS; temp_t++) {
          *(hpoly_coeffs + uy) =
              XV_multiscaler_fixedcoeff_taps6_6C[temp_p][temp_t];
          *(vpoly_coeffs + uy) =
              XV_multiscaler_fixedcoeff_taps6_6C[temp_p][temp_t];
          uy = uy + 1;
        }
      }
    }
    if (XPAR_V_MULTI_SCALER_0_TAPS == 8) {
      /* Run for 64 phase */
      for (temp_p = 0; temp_p < 64; temp_p++)
        /* Run for XPAR_V_MULTI_SCALER_0_TAPS taps */
        for (temp_t = 0; temp_t < XPAR_V_MULTI_SCALER_0_TAPS; temp_t++) {
          *(hpoly_coeffs + uy) =
              XV_multiscaler_fixedcoeff_taps8_8C[temp_p][temp_t];
          *(vpoly_coeffs + uy) =
              XV_multiscaler_fixedcoeff_taps8_8C[temp_p][temp_t];
          uy = uy + 1;
        }
    }
    if (XPAR_V_MULTI_SCALER_0_TAPS == 10) {
      /* Run for 64 phase */
      for (temp_p = 0; temp_p < 64; temp_p++)
        /* Run for XPAR_V_MULTI_SCALER_0_TAPS taps */
        for (temp_t = 0; temp_t < XPAR_V_MULTI_SCALER_0_TAPS; temp_t++) {
          *(hpoly_coeffs + uy) =
              XV_multiscaler_fixedcoeff_taps10_10C[temp_p][temp_t];
          *(vpoly_coeffs + uy) =
              XV_multiscaler_fixedcoeff_taps10_10C[temp_p][temp_t];
          uy = uy + 1;
        }
    }
    if (XPAR_V_MULTI_SCALER_0_TAPS == 12) {
      /* Run for 64 phase */
      for (temp_p = 0; temp_p < 64; temp_p++)
        /* Run for XPAR_V_MULTI_SCALER_0_TAPS taps */
        for (temp_t = 0; temp_t < XPAR_V_MULTI_SCALER_0_TAPS; temp_t++) {
          *(hpoly_coeffs + uy) =
              XV_multiscaler_fixedcoeff_taps12_12C[temp_p][temp_t];
          *(vpoly_coeffs + uy) =
              XV_multiscaler_fixedcoeff_taps12_12C[temp_p][temp_t];
          uy = uy + 1;
        }
    }
  }

  if (scale < 1) {
    /* Run for 64 phase */
    for (temp_p = 0; temp_p < 64; temp_p++)
      /* Run for XPAR_V_MULTI_SCALER_0_TAPS taps */
      for (temp_t = 0; temp_t < XPAR_V_MULTI_SCALER_0_TAPS; temp_t++) {
        *(hpoly_coeffs + uy) =
            XV_multiscaler_fixedcoeff_taps6_6C[temp_p][temp_t];
        *(vpoly_coeffs + uy) =
            XV_multiscaler_fixedcoeff_taps6_6C[temp_p][temp_t];
        uy = uy + 1;
      }
  }
}

/**
 *  @fn static bool vvas_scaler_feasibility_check (int src, int dst, int *filterSize)
 *  @param [in] src              - Input width/height
 *  @param [in] dst              - Output width/height
 *  @param [out] filtersize      - Filter tap size.
 *  @return Returns true if its feasible to generate co-efficients.\n
 *          Returns false if its not feasible to generate co-efficients.
 *  @brief  Calculates the filter tap size if its feasible to generate co-efficients.
 */
static bool
vvas_scaler_feasibility_check (const VvasScalerImpl * self,
    int src, int dst, int *filterSize)
{
  int sizeFactor = 4;
  int xInc = (((int64_t) src << 16) + (dst >> 1)) / dst;
  if (xInc <= 1 << 16)
    *filterSize = 1 + sizeFactor;       /* upscale */
  else
    *filterSize = 1 + (sizeFactor * src + dst - 1) / dst;

  if (*filterSize > MAX_FILTER_SIZE) {
    LOG_DEBUG (self->log_level,
        "FilterSize %d for %d to %d is greater than maximum taps(%d)",
        *filterSize, src, dst, MAX_FILTER_SIZE);
    return false;
  }

  return true;
}


/**
 *  @fn static void vvas_scaler_generate_cardinal_cubic_spline (int src,
 *                                                              int dst,
 *                                                              int filterSize,
 *                                                              int64_t B,
 *                                                              int64_t C,
 *                                                              int16_t * ccs_filtCoeff)
 *  @param [in] src              - Input width/height
 *  @param [in] dst              - Output width/height
 *  @param [in] filtersize       - Filter tap size.
 *  @param [in] B                - B Spline
 *  @param [in] C                - Cubic spline
 *  @param [out] ccs_filtCoeff   - Output buffer for filter co-efficients.
 *  @return None
 *  @brief  This function will generate co-efficients for scaler operations.
 */
static void
vvas_scaler_generate_cardinal_cubic_spline (int src, int dst,
    int filterSize, int64_t B, int64_t C, int16_t * ccs_filtCoeff)
{
#ifdef COEFF_DUMP
  FILE *fp;
  char fname[512];
  sprintf (fname, "coeff_%dTO%d.csv", src, dst);
  fp = fopen (fname, "w");
  /*FILE *fph;
     sprintf(fname,"phase_%dTO%d_2Inc.txt",src,dst);
     fph=fopen(fname,"w"); */
  //fprintf(fp,"src:%d => dst:%d\n\n",src,dst);
#endif
  int one = (1 << 14);
  int64_t *coeffFilter = NULL;
  int64_t *coeffFilter_reduced = NULL;
  int16_t *outFilter = NULL;
  int16_t *coeffFilter_normalized = NULL;
  int lumXInc = (((int64_t) src << 16) + (dst >> 1)) / dst;
  int srt = src / dst;
  int lval = vvas_scaler_log2_val (srt);
  int th0 = 8;
  int lv0 = SCALER_MIN (lval, th0);
  const int64_t fone = (int64_t) 1 << (54 - lv0);
  int64_t thr1 = ((int64_t) 1 << 31);
  int64_t thr2 = ((int64_t) 1 << 54) / fone;
  int i, xInc, outFilterSize;
  int num_phases = 64;
  int phase_set[64] = { 0 };
  int64_t xDstInSrc;
  int xx, j, p, t;
  int64_t d = 0, coeff = 0, dd = 0, ddd = 0;
  int phase_cnt = 0;
  int64_t error = 0, sum = 0, v = 0;
  int intV = 0;
  int fstart_Idx = 0, fend_Idx = 0, half_Idx = 0, middleIdx = 0;
  unsigned int PhaseH = 0, offset = 0, WriteLoc = 0, WriteLocNext = 0, ReadLoc =
      0, OutputWrite_En = 0;
  int OutPixels = dst;
  int PixelRate = (int) ((float) ((src * STEP_PRECISION) + (dst / 2))
      / (float) dst);
  int ph_max_sum = 1 << MAX_FILTER_SIZE;
  int sumVal = 0, maxIdx = 0, maxVal = 0, diffVal = 0;

  xInc = lumXInc;
  filterSize = SCALER_MAX (filterSize, 1);
  coeffFilter = (int64_t *) calloc (num_phases * filterSize, sizeof (int64_t));
  xDstInSrc = xInc - (1 << 16);

  /* coefficient generation cored on scaler IP */
  for (i = 0; i < src; i++) {
    PhaseH = ((offset >> (STEP_PRECISION_SHIFT - NR_PHASE_BITS)))
        & (NR_PHASES - 1);
    WriteLoc = WriteLocNext;

    if ((offset >> STEP_PRECISION_SHIFT) != 0) {
      /* Take a new sample from input, but don't process anything */
      ReadLoc++;
      offset = offset - (1 << STEP_PRECISION_SHIFT);
      OutputWrite_En = 0;
      WriteLocNext = WriteLoc;
    }

    if (((offset >> STEP_PRECISION_SHIFT) == 0) && (WriteLoc < OutPixels)) {
      /* Produce a new output sample */
      offset += PixelRate;
      OutputWrite_En = 1;
      WriteLocNext = WriteLoc + 1;
    }

    if (OutputWrite_En) {
      xDstInSrc = (ReadLoc * (int64_t) (1 << 17)) + PhaseH * (1 << 11);
      xx = ReadLoc - (filterSize - 2) / 2;

      d = (vvas_scaler_abs (((int64_t) xx * (1 << 17)) - xDstInSrc)) << 13;

      /* count number of phases used for this SR */
      if (phase_set[PhaseH] == 0)
        phase_cnt += 1;

      /* Filter coeff generation */
      for (j = 0; j < filterSize; j++) {
        d = (vvas_scaler_abs (((int64_t) xx * (1 << 17)) - xDstInSrc)) << 13;
        if (xInc > 1 << 16) {
          d = (int64_t) (d * dst / src);
        }

        if (d >= thr1) {
          coeff = 0.0;
        } else {
          dd = (int64_t) (d * d) >> 30;
          ddd = (int64_t) (dd * d) >> 30;
          if (d < 1 << 30) {
            coeff = (12 * (1 << 24) - 9 * B - 6 * C) * ddd
                + (-18 * (1 << 24) + 12 * B + 6 * C) * dd
                + (6 * (1 << 24) - 2 * B) * (1 << 30);
          } else {
            coeff = (-B - 6 * C) * ddd + (6 * B + 30 * C) * dd
                + (-12 * B - 48 * C) * d + (8 * B + 24 * C) * (1 << 30);
          }
        }

        coeff = coeff / thr2;
        coeffFilter[PhaseH * filterSize + j] = coeff;
        xx++;
      }
      if (phase_set[PhaseH] == 0) {
        phase_set[PhaseH] = 1;
      }
    }
  }

  coeffFilter_reduced = (int64_t *) calloc ((num_phases * filterSize),
      sizeof (int64_t));
  memcpy (coeffFilter_reduced, coeffFilter,
      sizeof (int64_t) * num_phases * filterSize);
  outFilterSize = filterSize;
  outFilter =
      (int16_t *) calloc ((num_phases * outFilterSize), sizeof (int16_t));
  coeffFilter_normalized =
      (int16_t *) calloc ((num_phases * outFilterSize), sizeof (int16_t));

  /* normalize & store in outFilter */
  for (i = 0; i < num_phases; i++) {
    error = 0;
    sum = 0;

    for (j = 0; j < filterSize; j++) {
      sum += coeffFilter_reduced[i * filterSize + j];
    }
    sum = (sum + one / 2) / one;
    if (!sum) {
      sum = 1;
    }
    for (j = 0; j < outFilterSize; j++) {
      v = coeffFilter_reduced[i * filterSize + j] + error;
      intV = ROUNDED_DIV (v, sum);
      coeffFilter_normalized[i * (outFilterSize) + j] = intV;
      /* added to negate double increment and match our precision */
      coeffFilter_normalized[i * (outFilterSize) + j] = coeffFilter_normalized[i
          * (outFilterSize) + j] >> 2;
      error = v - intV * sum;
    }
  }

  for (p = 0; p < num_phases; p++) {
    for (t = 0; t < filterSize; t++) {
      outFilter[p * filterSize + t] =
          coeffFilter_normalized[p * filterSize + t];
    }
  }

  /*incorporate filter less than 12 tap into a 12 tap */
  fstart_Idx = 0, fend_Idx = 0, half_Idx = 0;
  middleIdx = (MAX_FILTER_SIZE / 2);    /* center location for 12 tap */
  half_Idx = (outFilterSize / 2);
  if ((outFilterSize - (half_Idx << 1)) == 0) { /* evenOdd */
    fstart_Idx = middleIdx - half_Idx;
    fend_Idx = middleIdx + half_Idx;
  } else {
    fstart_Idx = middleIdx - (half_Idx);
    fend_Idx = middleIdx + half_Idx + 1;
  }

  for (i = 0; i < num_phases; i++) {
    for (j = 0; j < MAX_FILTER_SIZE; j++) {

      ccs_filtCoeff[i * MAX_FILTER_SIZE + j] = 0;
      if ((j >= fstart_Idx) && (j < fend_Idx))
        ccs_filtCoeff[i * MAX_FILTER_SIZE + j] = outFilter[i * (outFilterSize)
            + (j - fstart_Idx)];
    }
  }

  /*Make sure filterCoeffs within a phase sum to 4096 */
  for (i = 0; i < num_phases; i++) {
    sumVal = 0;
    maxVal = 0;
    for (j = 0; j < MAX_FILTER_SIZE; j++) {
      sumVal += ccs_filtCoeff[i * MAX_FILTER_SIZE + j];
      if (ccs_filtCoeff[i * MAX_FILTER_SIZE + j] > maxVal) {
        maxVal = ccs_filtCoeff[i * MAX_FILTER_SIZE + j];
        maxIdx = j;
      }
    }
    diffVal = ph_max_sum - sumVal;
    if (diffVal > 0)
      ccs_filtCoeff[i * MAX_FILTER_SIZE + maxIdx] = ccs_filtCoeff[i
          * MAX_FILTER_SIZE + maxIdx] + diffVal;
  }

#ifdef COEFF_DUMP
  fprintf (fp, "taps/phases, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12\n");
  for (i = 0; i < num_phases; i++) {
    fprintf (fp, "%d, ", i + 1);
    for (j = 0; j < MAX_FILTER_SIZE; j++) {
      fprintf (fp, "%d,  ", ccs_filtCoeff[i * MAX_FILTER_SIZE + j]);
    }
    fprintf (fp, "\n");
  }
#endif

  free (coeffFilter);
  free (coeffFilter_reduced);
  free (outFilter);
  free (coeffFilter_normalized);
#ifdef COEFF_DUMP
  fclose (fp);
#endif
}

/**
 *  @fn static void vvas_scaler_copy_filt_set (int16_t dest_filt[64][12], int set)
 *  @param [out] dest_filt  - Address of filter coefficients buffer
 *  @param [in] set         - filter set
 *  @return None
 *  @brief  This function fills predefined horizontal and vertical coefficients cored on the passed filter set.
 */
static void
vvas_scaler_copy_filt_set (int16_t dest_filt[64][12], int set)
{
  switch (set) {
    case XLXN_FIXED_COEFF_SR13:
      /* <1.5SR */
      memcpy (dest_filt, &XV_multiscaler_fixed_coeff_SR13_0,
          VVAS_SCALER_FIXED_FILTER_COEF_SIZE);
      break;
    case XLXN_FIXED_COEFF_SR15:
      /* 1.5SR */
      memcpy (dest_filt, &XV_multiscaler_fixed_coeff_SR15_0,
          VVAS_SCALER_FIXED_FILTER_COEF_SIZE);
      break;
    case XLXN_FIXED_COEFF_SR2:
      /* 2SR, 8tap */
      memcpy (dest_filt, &XV_multiscaler_fixedcoeff_taps8_12C,
          VVAS_SCALER_FIXED_FILTER_COEF_SIZE);
      break;
    case XLXN_FIXED_COEFF_SR25:
      /* 2.5SR */
      memcpy (dest_filt, &XV_multiscaler_fixed_coeff_SR25_0,
          VVAS_SCALER_FIXED_FILTER_COEF_SIZE);
      break;
    case XLXN_FIXED_COEFF_TAPS_10:
      /* 10tap */
      memcpy (dest_filt, &XV_multiscaler_fixedcoeff_taps10_12C,
          VVAS_SCALER_FIXED_FILTER_COEF_SIZE);
      break;
    case XLXN_FIXED_COEFF_TAPS_12:
      /* 12tap */
      memcpy (dest_filt, &XV_multiscaler_fixedcoeff_taps12_12C,
          VVAS_SCALER_FIXED_FILTER_COEF_SIZE);
      break;
    case XLXN_FIXED_COEFF_TAPS_6:
      /* 6tap: Always used for up scale */
      memcpy (dest_filt, XV_multiscaler_fixedcoeff_taps6_12C,
          VVAS_SCALER_FIXED_FILTER_COEF_SIZE);
      break;
    default:
      /* 12tap */
      memcpy (dest_filt, XV_multiscaler_fixedcoeff_taps12_12C,
          VVAS_SCALER_FIXED_FILTER_COEF_SIZE);
      break;
  }
}

/**
 *  @fn static void vvas_scaler_prepare_coefficients_with_12tap (VvasScalerImpl * self,
 *                                                               uint32_t chan_id,
 *                                                               uint32_t in_width,
 *                                                               uint32_t in_height,
 *                                                               uint32_t out_width,
 *                                                               uint32_t out_height)
 *  @param [in, out] self   - VvasScalerImpl instance pointer
 *  @param [in] chan_id     - Channel ID
 *  @param [in] in_width    - Input width
 *  @param [in] in_height   - Input height
 *  @param [in] out_width   - Output width
 *  @param [in] out_height  - Output height
 *  @return None
 *  @brief  This function calculates scaling mode (UpScale/DownScale) and scaling ratio,
 *          cored on these info it fills the filter coefficients values for scaling.
 */
static void
vvas_scaler_prepare_coefficients_with_12tap (VvasScalerImpl * self,
    uint32_t chan_id, uint32_t in_width, uint32_t in_height,
    uint32_t out_width, uint32_t out_height)
{
  VvasScalerInternlBuffer *internal_buf;
  int32_t filter_size;
  int64_t B = 0 * (1 << 24);
  int64_t C = 0.6 * (1 << 24);
  float scale_ratio[2] = { 0, 0 };
  int32_t upscale_enable[2] = { 0, 0 };
  int32_t filterSet[2] = { 0, 0 };
  uint32_t d;
  bool bret;

  /* store width scaling ratio  */
  if (in_width >= out_width) {
    scale_ratio[0] = (float) in_width / (float) out_width;      /* downscale */
  } else {
    scale_ratio[0] = (float) out_width / (float) in_width;      /* upscale */
    upscale_enable[0] = 1;
  }

  /* store height scaling ratio */
  if (in_height >= out_height) {
    scale_ratio[1] = (float) in_height / (float) out_height;    /* downscale */
  } else {
    scale_ratio[1] = (float) out_height / (float) in_height;    /* upscale */
    upscale_enable[1] = 1;
  }

  for (d = 0; d < 2; d++) {
    if (upscale_enable[d] == 1) {
      /* upscaling default use 6 taps */
      filterSet[d] = XLXN_FIXED_COEFF_TAPS_6;
    } else {
      /* Get index of downscale fixed filter */
      if (scale_ratio[d] < 1.5)
        filterSet[d] = XLXN_FIXED_COEFF_SR13;
      else if ((scale_ratio[d] >= 1.5) && (scale_ratio[d] < 2))
        filterSet[d] = XLXN_FIXED_COEFF_SR15;
      else if ((scale_ratio[d] >= 2) && (scale_ratio[d] < 2.5))
        filterSet[d] = XLXN_FIXED_COEFF_SR2;
      else if ((scale_ratio[d] >= 2.5) && (scale_ratio[d] < 3))
        filterSet[d] = XLXN_FIXED_COEFF_SR25;
      else if ((scale_ratio[d] >= 3) && (scale_ratio[d] < 3.5))
        filterSet[d] = XLXN_FIXED_COEFF_TAPS_10;
      else
        filterSet[d] = XLXN_FIXED_COEFF_TAPS_12;
    }
    LOG_DEBUG (self->log_level,
        "%s scaling ratio = %f and chosen filter type = %d",
        d == 0 ? "width" : "height", scale_ratio[d], filterSet[d]);
  }

  internal_buf = GET_INTERNAL_BUFFERS (chan_id);
  if (!internal_buf) {
    LOG_ERROR (self->log_level, "Couldn't get internal buffer");
    return;
  }

  if (VVAS_SCALER_COEF_AUTO_GENERATE == self->props.coef_load_type) {
    /* prepare horizontal coefficients */
    bret = vvas_scaler_feasibility_check (self, in_width, out_width,
        &filter_size);
    if (bret && !upscale_enable[0]) {
      LOG_DEBUG (self->log_level,
          "Generate cardinal cubic horizontal coefficients"
          "with filter size %d", filter_size);
      vvas_scaler_generate_cardinal_cubic_spline (in_width, out_width,
          filter_size, B, C, (int16_t *) internal_buf->Hcoff.user_ptr);
    } else {
      /* get fixed horizontal filters */
      LOG_DEBUG (self->log_level,
          "Consider predefined horizontal filter coefficients");
      vvas_scaler_copy_filt_set (internal_buf->Hcoff.user_ptr, filterSet[0]);
    }

    /* prepare vertical coefficients */
    bret = vvas_scaler_feasibility_check (self, in_height, out_height,
        &filter_size);
    if (bret && !upscale_enable[1]) {
      LOG_DEBUG (self->log_level,
          "Generate cardinal cubic vertical coefficients "
          "with filter size %d", filter_size);
      vvas_scaler_generate_cardinal_cubic_spline (in_height, out_height,
          filter_size, B, C, (int16_t *) internal_buf->Vcoff.user_ptr);
    } else {
      /* get fixed vertical filters */
      LOG_DEBUG (self->log_level,
          "Consider predefined vertical filter coefficients");
      vvas_scaler_copy_filt_set (internal_buf->Vcoff.user_ptr, filterSet[1]);
    }
  } else if (VVAS_SCALER_COEF_FIXED == self->props.coef_load_type) {
    /* get fixed horizontal filters */
    LOG_DEBUG (self->log_level,
        "Consider predefined horizontal/vertical filter coefficients");
    vvas_scaler_copy_filt_set (internal_buf->Hcoff.user_ptr, filterSet[0]);

    /* get fixed vertical filters */
    vvas_scaler_copy_filt_set (internal_buf->Vcoff.user_ptr, filterSet[1]);
  }
}

/**
 *  @fn static int32_t vvas_scaler_exec_device_buf (vvasDeviceHandle dev_handle,
 *                                                  vvasKernelHandle kern_handle,
 *                                                  vvasRunHandle * run_handle,
 *                                                  const char *format, ...)
 *  @param [in] dev_handle  - vvasDeviceHandle handle
 *  @param [in] kern_handle - vvasKernelHandle handle
 *  @param [in] run_handle  - vvasRunHandle handle
 *  @param [in] format      - format of the commands
 *  @param [in] ...         - variable arguments
 *  @return 0 in case of success\n Non zero in case of failure.
 *  @brief  This function executes the XRT commands.
 */
static int32_t
vvas_scaler_exec_device_buf (vvasDeviceHandle dev_handle,
    vvasKernelHandle kern_handle, vvasRunHandle * run_handle,
    const char *format, ...)
{
  va_list args;
  int32_t iret;

  va_start (args, format);
  /* Execute VVAs XRT Buffer */
  iret = vvas_xrt_exec_buf (dev_handle, kern_handle, run_handle, format, args);

  va_end (args);

  return iret;
}

/**
 *  @fn static bool vvas_scaler_coeff_sync_bo (VvasScalerImpl * self)
 *  @param [in] self    - VvasScalerImpl instance pointer
 *  @return true on success\n false on failure
 *  @brief  This function writes and syncs horizontal and vertical coefficients
 *          to the device.
 */
static bool
vvas_scaler_coeff_sync_bo (VvasScalerImpl * self)
{
  int32_t iret;
  uint32_t idx;

  for (idx = 0; idx < self->num_of_channels; idx++) {
    VvasScalerInternlBuffer *internal_buf;
    internal_buf = GET_INTERNAL_BUFFERS (idx);
    if (!internal_buf) {
      LOG_ERROR (self->log_level, "Couldn't get internal buffer");
      return false;
    }

    /* sync horizontal coefficients to device */
    iret = vvas_xrt_sync_bo (internal_buf->Hcoff.bo,
        VVAS_BO_SYNC_BO_TO_DEVICE, internal_buf->Hcoff.size, 0);
    if (iret != 0) {
      LOG_ERROR (self->log_level,
          "couldn't sync horizontal coefficients, reason: %s",
          strerror (errno));
      return false;
    }

    /* sync vertical coefficients to device */
    iret = vvas_xrt_sync_bo (internal_buf->Vcoff.bo,
        VVAS_BO_SYNC_BO_TO_DEVICE, internal_buf->Vcoff.size, 0);
    if (iret != 0) {
      LOG_ERROR (self->log_level,
          "couldn't sync vertical coefficients, reason: %s", strerror (errno));
      return false;
    }
  }
  return true;
}

/**
 *  @fn static bool vvas_scaler_descriptor_sync_bo (VvasScalerImpl * self)
 *  @param [in] self    - VvasScalerImpl instance pointer
 *  @return true on success\n false on failure
 *  @brief  This function syncs descriptors to the device
 */
static bool
vvas_scaler_descriptor_sync_bo (VvasScalerImpl * self)
{
  int32_t idx;
  int32_t iret;
  /* sync processing descriptors to device */
  for (idx = 0; idx < self->num_of_channels; idx++) {
    VvasScalerInternlBuffer *internal_buf;
    internal_buf = GET_INTERNAL_BUFFERS (idx);
    if (!internal_buf) {
      LOG_ERROR (self->log_level, "Couldn't get internal buffer");
      return false;
    }
    iret = vvas_xrt_sync_bo (internal_buf->descriptor.bo,
        VVAS_BO_SYNC_BO_TO_DEVICE, internal_buf->descriptor.size, 0);
    if (iret != 0) {
      LOG_ERROR (self->log_level,
          "couldn't sync processing descriptor, reason: %s", strerror (errno));
      return false;
    }
  }
  return true;
}

/**
 *  @fn static bool vvas_scaler_validate_rect_params (VvasScalerImpl *self, VvasScalerRect *rect, bool can_align)
 *  @param [in] self        - VvasScalerImpl instance pointer
 *  @param [in, out] rect   - VvasScalerRect to be validated and aligned
 *  @return true on success\n false on failure
 *  @brief  This function validates and aligns the given rect id \p can_align is true
 */
static bool
vvas_scaler_validate_rect_params (VvasScalerImpl * self, VvasScalerRect * rect)
{
  uint32_t x_aligned, y_aligned, width_aligned, height_aligned;
  uint32_t ppc = self->props.ppc;
  VvasVideoInfo info = { 0 };

  LOG_DEBUG (self->log_level, "Crop params x: %u, y: %u, width: %u, height: %u",
      rect->x, rect->y, rect->width, rect->height);

  /* Align values as per the IP requirement
   * Align x by 8 * PPC, width by PPC
   */
  x_aligned = (rect->x / (8 * ppc)) * (8 * ppc);

  width_aligned = rect->x + rect->width - x_aligned;
  width_aligned = vvas_scaler_align (width_aligned, ppc);

  vvas_video_frame_get_videoinfo (rect->frame, &info);

  if (VVAS_VIDEO_FORMAT_UNKNOWN == info.fmt) {
    LOG_ERROR (self->log_level, "video format unknown");
    return false;
  }

  /* For below formats height of UV plane is height of Y plane / 2,
   * hence align height and y coordinate by 2
   *---------------------------------------
   * Format                        Scaling
   *---------------------------------------
   * 1.VVAS_VIDEO_FORMAT_Y_UV8_420   4:2:0
   * 2.VVAS_VIDEO_FORMAT_I420        4:2:0
   * 3.VVAS_VIDEO_FORMAT_NV16        4:2:2
   * 4.VVAS_VIDEO_FORMAT_I422_10LE   4:2:2
   * 5.VVAS_VIDEO_FORMAT_NV12_10LE32 4:2:0
   */
  if ((info.fmt == VVAS_VIDEO_FORMAT_Y_UV8_420) ||
      (info.fmt == VVAS_VIDEO_FORMAT_I420) ||
      (info.fmt == VVAS_VIDEO_FORMAT_NV16) ||
      (info.fmt == VVAS_VIDEO_FORMAT_I422_10LE) ||
      (info.fmt == VVAS_VIDEO_FORMAT_NV12_10LE32)) {
    y_aligned = (rect->y / 2) * 2;
    height_aligned = rect->y + rect->height - y_aligned;
    height_aligned = vvas_scaler_align (height_aligned, 2);
  } else {
    y_aligned = rect->y;
    height_aligned = rect->height;
  }

  rect->x = x_aligned;
  rect->y = y_aligned;
  rect->width = width_aligned;
  rect->height = height_aligned;

  LOG_DEBUG (self->log_level, "Aligned x: %u, y: %u, width: %u, height: %u",
      rect->x, rect->y, rect->width, rect->height);

  return true;
}

static void
vvas_scaler_print_descriptors (VvasScalerImpl * self)
{
  uint32_t idx;
  MULTI_SCALER_DESC_STRUCT *descriptor;
  for (idx = 0; idx < self->num_of_channels; idx++) {
    VvasScalerInternlBuffer *internal_buf;
    internal_buf = GET_INTERNAL_BUFFERS (idx);
    if (!internal_buf) {
      LOG_ERROR (self->log_level, "Couldn't get internal buffer");
      return;
    }
    descriptor =
        (MULTI_SCALER_DESC_STRUCT *) (internal_buf->descriptor.user_ptr);

    LOG_DEBUG (self->log_level,
        "[%u] Input physical address: 0x%lX, 0x%lX, 0x%lX", idx,
        descriptor->msc_srcImgBuf0, descriptor->msc_srcImgBuf1,
        descriptor->msc_srcImgBuf2);
    LOG_DEBUG (self->log_level, "[%u] Input width x height: %u x %u", idx,
        descriptor->msc_widthIn, descriptor->msc_heightIn);
    LOG_DEBUG (self->log_level, "[%u] Input stride: %u, format: %u", idx,
        descriptor->msc_strideIn, descriptor->msc_inPixelFmt);
    LOG_DEBUG (self->log_level,
        "[%u] Output physical address: 0x%lX, 0x%lX, 0x%lX", idx,
        descriptor->msc_dstImgBuf0, descriptor->msc_dstImgBuf1,
        descriptor->msc_dstImgBuf2);
    LOG_DEBUG (self->log_level, "[%u] Output width x height: %u x %u", idx,
        descriptor->msc_widthOut, descriptor->msc_heightOut);
    LOG_DEBUG (self->log_level, "[%u] Output stride: %u, format: %u", idx,
        descriptor->msc_strideOut, descriptor->msc_outPixelFmt);
    LOG_DEBUG (self->log_level, "[%u] HCoff: 0x%lX, Vcoff: 0x%lX", idx,
        descriptor->msc_blkmm_hfltCoeff, descriptor->msc_blkmm_vfltCoeff);
    LOG_DEBUG (self->log_level, "[%u] LineRate: %u, PixelRate: %u", idx,
        descriptor->msc_lineRate, descriptor->msc_pixelRate);
    LOG_DEBUG (self->log_level, "[%u] Next physical address: 0x%lX", idx,
        descriptor->msc_nxtaddr);
  }
}

/**
 *  @fn static bool vvas_scaler_calculate_plane_offset (VvasScalerImpl * self,
 *                                                      VvasVideoInfo *vinfo,
 *                                                      VvasScalerRect *rect,
 *                                                      uint32_t * plane_offset)
 *  @param [in] self            - VvasScalerImpl instance pointer
 *  @param [in] vinfo           - VvasVideoInfo
 *  @param [in] rect            - VvasScalerRect
 *  @param [out] plane_offset   - Plane offset of each plane, max plane is VVAS_VIDEO_MAX_PLANES
 *  @return true on success\n false on failure.
 *  @brief  This function calculates the plane offset for each plane based on given video format,
 *          stride, x_cord and y_cord.
 */
static bool
vvas_scaler_calculate_plane_offset (VvasScalerImpl * self,
    VvasVideoInfo * vinfo, VvasScalerRect * rect, uint32_t * plane_offset)
{
  uint32_t stride = vinfo->stride[0];
  bool ret = true;
  uint32_t x_cord, y_cord;

  x_cord = rect->x;
  y_cord = rect->y;

  switch (vinfo->fmt) {
    case VVAS_VIDEO_FORMAT_Y_UV8_420:{
      /* NV12 data, we will have 2 planes,
       * 1st for Y and 2nd for U&V interleaved */
      plane_offset[0] = ((stride * y_cord) + x_cord);
      plane_offset[1] = vvas_scaler_align (
          ((stride * y_cord / 2) + x_cord), (8 * self->props.ppc));
    }
      break;

    case VVAS_VIDEO_FORMAT_I420:{
      /* I420 data, we will have 3 planes,
       * 1st for Y and 2nd for U and 3rd for V */
      plane_offset[0] = ((stride * y_cord) + x_cord);
      plane_offset[1] = vvas_scaler_align (
          ((stride * y_cord / 2) + (x_cord / 2)), (8 * self->props.ppc));
      plane_offset[2] = vvas_scaler_align (
          ((stride * y_cord / 2) + (x_cord / 2)), (8 * self->props.ppc));
    }
      break;
    case VVAS_VIDEO_FORMAT_NV12_10LE32:{
      /* 10 bit NV12 */
      uint32_t x_bytes, pixels_before_alignment, pixels_after_alignment,
          extra_pixels;
      /* convert pixels into bytes aligned on 4 bytes boundary, In 10 bit format
       * every 4 bytes has 3 pixels, so we must always start on 4 bytes boundary */
      x_bytes = x_cord / 3 * 4;
      /* convert 4 bytes aligned byte value back to pixel */
      pixels_before_alignment = x_bytes / 4 * 3;
      /* Align the byte value by 8 * PPC as per the IP requirement */
      x_bytes = (x_bytes / (8 * self->props.ppc)) * (8 * self->props.ppc);
      /* By above alignment, x will shift on left, let's calculate by how many
       * pixels X shifted left */
      pixels_after_alignment = x_bytes / 4 * 3;
      /* Since X pos if shifted left, we must add this shift into the actual
       * width, otherwise crop will not have right most data */
      extra_pixels = pixels_before_alignment - pixels_after_alignment;
      /* As x_bytes is adjusted as per alignment requirement, add extra width */
      rect->width += extra_pixels;
      /* Align this width width by PPC as per IP requirement */
      rect->width = vvas_scaler_align (rect->width, self->props.ppc);
      /* Update the x co-odinate after alignment */
      rect->x = pixels_after_alignment;
      plane_offset[0] = ((stride * y_cord) + x_bytes);
      plane_offset[1] = vvas_scaler_align (
          ((stride * y_cord / 2) + x_bytes), (8 * self->props.ppc));
      LOG_DEBUG (self->log_level,
          "NV12_10LE32: extra_pixels: %u, new_aligned width: %u", extra_pixels,
          rect->width);
    }
      break;

    case VVAS_VIDEO_FORMAT_RGB:
    case VVAS_VIDEO_FORMAT_BGR:{
      /* Packed RGB/BGR, we have only one plane */
      x_cord *= 3;
      plane_offset[0] = ((stride * y_cord) + x_cord);
    }
      break;

    case VVAS_VIDEO_FORMAT_GRAY8:{
      /* Gray8 format has only Y plane */
      plane_offset[0] = ((stride * y_cord) + x_cord);
    }
      break;

    case VVAS_VIDEO_FORMAT_GRAY10_LE32:{
      /* 10 bit GRAY */
      //TODO: Need to validate GRAY8 and GRAY10_LE32
      uint32_t x_bytes, pixels_before_alignment, pixels_after_alignment,
          extra_pixels;
      /* convert pixels into bytes aligned on 4 bytes boundary, In 10 bit format
       * every 4 bytes has 3 pixels, so we must always start on 4 bytes boundary */
      x_bytes = x_cord / 3 * 4;
      /* convert 4 bytes aligned byte value back to pixel */
      pixels_before_alignment = x_bytes / 4 * 3;
      /* Align the byte value by 8 * PPC as per the IP requirement */
      x_bytes = (x_bytes / (8 * self->props.ppc)) * (8 * self->props.ppc);
      /* By above alignment, x will shift on left, let's calculate by how many
       * pixels X shifted left */
      pixels_after_alignment = x_bytes / 4 * 3;
      /* Since X pos if shifted left, we must add this shift into the actual
       * width, otherwise crop will not have right most data */
      extra_pixels = pixels_before_alignment - pixels_after_alignment;
      /* As x_bytes is adjusted as per alignment requirement, add extra width */
      rect->width += extra_pixels;
      /* Align this width width by PPC as per IP requirement */
      rect->width = vvas_scaler_align (rect->width, self->props.ppc);
      /* Update the x co-odinate after alignment */
      rect->x = pixels_after_alignment;
      plane_offset[0] = ((stride * y_cord) + x_bytes);
      LOG_DEBUG (self->log_level,
          "GRAY10LE32: extra_pixels: %u, new_aligned width: %u", extra_pixels,
          rect->width);
    }
      break;

    default:{
      LOG_ERROR (self->log_level, "%d format is not supported yet", vinfo->fmt);
      ret = false;
    }
      break;
  }
  return ret;
}

/**
 *  @fn static bool vvas_scaler_prepare_processing_descriptor (VvasScalerImpl *self,
                                                               VvasScalerRect *src_rect,
                                                               VvasScalerRect *dst_rect,
                                                               VvasScalerPpe *ppe,
                                                               uint32_t idx)
 *  @param [in] self        - VvasScalerImpl instance pointer
 *  @param [in] src_rect    - Input rect
 *  @param [in] dst_rect    - Output rect
 *  @param [in] ppe         - Pre-processing values
 *  @param [in idx          - channel index
 *  @return true on success\n false on failure.
 *  @brief  This function fills the descriptor for processing \p src_rect and \p dst_rect using MultiScaler IP
 */
static bool
vvas_scaler_prepare_processing_descriptor (VvasScalerImpl * self,
    VvasScalerRect * src_rect, VvasScalerRect * dst_rect, VvasScalerPpe * ppe,
    uint32_t idx)
{
  MULTI_SCALER_DESC_STRUCT *descriptor;
  VvasScalerInternlBuffer *internal_buf;
  VvasVideoInfo in_vinfo = { 0 }, out_vinfo = {
  0};
  uint32_t inplane_offset[3] = { 0 }, outplane_offset[3] = {
  0};
  uint64_t in_phy_addr[3] = { 0 }, out_phy_addr[3] = {
  0};
  uint32_t input_stride, input_elevation, output_stride, output_elevation;
  bool ret = false;

#ifdef ENABLE_PPE_SUPPORT
  uint32_t val;
  VvasScalerPpe ppe_param = {
    .mean_r = VVAS_SCALER_DEFAULT_MEAN,
    .mean_g = VVAS_SCALER_DEFAULT_MEAN,
    .mean_b = VVAS_SCALER_DEFAULT_MEAN,
    .scale_r = VVAS_SCALER_DEFAULT_SCALE,
    .scale_g = VVAS_SCALER_DEFAULT_SCALE,
    .scale_b = VVAS_SCALER_DEFAULT_SCALE
  };
#endif

  vvas_video_frame_get_videoinfo (src_rect->frame, &in_vinfo);
  vvas_video_frame_get_videoinfo (dst_rect->frame, &out_vinfo);

  /* Considering stride as stride of the first plane */
  input_stride = in_vinfo.stride[0];
  input_elevation = in_vinfo.elevation[0];

  output_stride = out_vinfo.stride[0];
  output_elevation = out_vinfo.elevation[0];

  if (input_stride % WIDTH_ALIGN) {
    LOG_ERROR (self->log_level, "Input stride[%u] must be aligned to %u",
        input_stride, WIDTH_ALIGN);
    goto error;
  }

  if (output_stride % WIDTH_ALIGN) {
    LOG_ERROR (self->log_level, "Output stride[%u] must be aligned to %u",
        output_stride, WIDTH_ALIGN);
    goto error;
  }

  /* Validate aligned parameters against boundary conditions
   */
  if (((src_rect->x + src_rect->width) > input_stride)
      || ((src_rect->y + src_rect->height) > input_elevation)) {
    LOG_ERROR (self->log_level, "Src Rect param is beyond original input video");
    goto error;
  }

  if (((dst_rect->x + dst_rect->width) > output_stride)
      || ((dst_rect->y + dst_rect->height) > output_elevation)) {
    LOG_ERROR (self->log_level, "Dst Rect param is beyond original input video");
    goto error;
  }


  internal_buf = GET_INTERNAL_BUFFERS (idx);
  if (!internal_buf) {
    LOG_ERROR (self->log_level,
        "internal buffers are not allocated for this operation");
    goto error;
  }

  /* Based on given X and Y coordinates calculate the plane offset for each plane */
  if (!vvas_scaler_calculate_plane_offset (self, &in_vinfo,
          src_rect, inplane_offset)) {
    goto error;
  }

  if (!vvas_scaler_calculate_plane_offset (self, &out_vinfo,
          dst_rect, outplane_offset)) {
    goto error;
  }

  /* Prepare filter co-efficients cored on scale factor */
  if (VVAS_SCALER_FILTER_TAPS_12 == self->props.ftaps) {
    vvas_scaler_prepare_coefficients_with_12tap (self, idx, src_rect->width,
        src_rect->height, dst_rect->width, dst_rect->height);
  } else {
    if (VVAS_SCALER_MODE_POLYPHASE == self->props.smode) {
      float scale = (float) src_rect->height / (float) dst_rect->height;
      LOG_DEBUG (self->log_level,
          "preparing coefficients with scaling ration %f" " and taps %d\n",
          scale, self->props.ftaps);
      vvas_scaler_coff_fill (internal_buf->Hcoff.user_ptr,
          internal_buf->Vcoff.user_ptr, scale);
    }
  }

  /* Get physical address of input buffer's planes */
  switch (in_vinfo.n_planes) {
    case 3:
      in_phy_addr[2] = vvas_video_frame_get_plane_paddr (src_rect->frame, 2);

      /* Fall back */
    case 2:
      in_phy_addr[1] = vvas_video_frame_get_plane_paddr (src_rect->frame, 1);

      /* Fall back */
    case 1:
      in_phy_addr[0] = vvas_video_frame_get_plane_paddr (src_rect->frame, 0);
      break;

    default:{
      LOG_ERROR (self->log_level, "No supporting %u planes video",
          in_vinfo.n_planes);
      goto error;
    }
      break;
  }

  /* Get physical address of output buffer's planes */
  switch (out_vinfo.n_planes) {
    case 3:
      out_phy_addr[2] = vvas_video_frame_get_plane_paddr (dst_rect->frame, 2);

      /* Fall back */
    case 2:
      out_phy_addr[1] = vvas_video_frame_get_plane_paddr (dst_rect->frame, 1);

      /* Fall back */
    case 1:
      out_phy_addr[0] = vvas_video_frame_get_plane_paddr (dst_rect->frame, 0);
      break;

    default:{
      LOG_ERROR (self->log_level, "No supporting %u planes video",
          out_vinfo.n_planes);
      goto error;
    }
      break;
  }

  /* Preparing descriptor for processing */
  LOG_DEBUG (self->log_level, "Preparing descriptor for index: %u", idx);
  descriptor = (MULTI_SCALER_DESC_STRUCT *) (internal_buf->descriptor.user_ptr);

  /* Input plane 0 */
  descriptor->msc_srcImgBuf0 = (uint64_t) in_phy_addr[0] + inplane_offset[0];
  /* Input plane 1 */
  descriptor->msc_srcImgBuf1 = (uint64_t) in_phy_addr[1] + inplane_offset[1];
  /* Input plane 2 */
  descriptor->msc_srcImgBuf2 = (uint64_t) in_phy_addr[2] + inplane_offset[2];

  /* Output plane 0 */
  descriptor->msc_dstImgBuf0 = (uint64_t) out_phy_addr[0] + outplane_offset[0];
  /* Output plane 1 */
  descriptor->msc_dstImgBuf1 = (uint64_t) out_phy_addr[1] + outplane_offset[1];
  /* Output plane 2 */
  descriptor->msc_dstImgBuf2 = (uint64_t) out_phy_addr[2] + outplane_offset[2];

  /* Fill all the information in the descriptor */
  descriptor->msc_widthIn = src_rect->width;
  descriptor->msc_heightIn = src_rect->height;
  descriptor->msc_inPixelFmt = vvas_scaler_colorformat (self, in_vinfo.fmt);
  descriptor->msc_strideIn = input_stride;

  descriptor->msc_widthOut = dst_rect->width;
  descriptor->msc_heightOut = dst_rect->height;
  descriptor->msc_outPixelFmt = vvas_scaler_colorformat (self, out_vinfo.fmt);
  descriptor->msc_strideOut = output_stride;

  descriptor->msc_lineRate = (uint32_t) (
      (float) ((descriptor->msc_heightIn * STEP_PRECISION)
          + ((descriptor->msc_heightOut) / 2))
      / (float) descriptor->msc_heightOut);
  descriptor->msc_pixelRate = (uint32_t) (
      (float) (((descriptor->msc_widthIn) * STEP_PRECISION)
          + ((descriptor->msc_widthOut) / 2))
      / (float) descriptor->msc_widthOut);

  descriptor->msc_blkmm_hfltCoeff = internal_buf->Hcoff.phy_addr;
  descriptor->msc_blkmm_vfltCoeff = internal_buf->Vcoff.phy_addr;
#ifdef ENABLE_PPE_SUPPORT
  if (ppe) {
    /* User has provided PPE parameters */
    ppe_param = *ppe;
  }
  LOG_DEBUG (self->log_level, "PPE: alpha_r: %f, alpha_g: %f, alpha_b: %f",
      ppe_param.mean_r, ppe_param.mean_g, ppe_param.mean_b);
  LOG_DEBUG (self->log_level, "PPE: beta_r: %f, beta_g: %f, beta_b: %f",
      ppe_param.scale_r, ppe_param.scale_g, ppe_param.scale_b);

  descriptor->msc_alpha_0 = ppe_param.mean_r;
  descriptor->msc_alpha_1 = ppe_param.mean_g;
  descriptor->msc_alpha_2 = ppe_param.mean_b;
  val = (ppe_param.scale_r * (1 << 16));
  descriptor->msc_beta_0 = val;
  val = (ppe_param.scale_g * (1 << 16));
  descriptor->msc_beta_1 = val;
  val = (ppe_param.scale_b * (1 << 16));
  descriptor->msc_beta_2 = val;
#endif
  /* Store next buffers' physical address, will be set before processing */
  descriptor->msc_nxtaddr = 0;

  internal_buf->outvideo_frame = dst_rect->frame;
  ret = true;

error:
  return ret;
}

/**
 *  @fn static void vvas_scaler_fill_physical_address (VvasScalerImpl *self)
 *  @param [in, out] self   - VvasScalerImpl instance pointer
 *  @return None
 *  @brief  This function fills the next physical address field of all the descriptors
 */
static void
vvas_scaler_fill_physical_address (VvasScalerImpl * self)
{
  uint32_t idx = 0;
  for (idx = 0; idx <= (self->num_of_channels - 1); idx++) {
    VvasScalerInternlBuffer *internal_buf;
    MULTI_SCALER_DESC_STRUCT *descriptor;

    internal_buf = GET_INTERNAL_BUFFERS (idx);
    if (!internal_buf) {
      LOG_ERROR (self->log_level, "Couldn't get internal buffer");
      return;
    }
    descriptor =
        (MULTI_SCALER_DESC_STRUCT *) (internal_buf->descriptor.user_ptr);

    if (idx == (self->num_of_channels - 1)) {
      descriptor->msc_nxtaddr = 0;
    } else {
      VvasScalerInternlBuffer *next_internal_buf;
      next_internal_buf = GET_INTERNAL_BUFFERS (idx + 1);
      if (!next_internal_buf) {
        LOG_ERROR (self->log_level, "Couldn't get next internal buffer");
        return;
      }
      descriptor->msc_nxtaddr = next_internal_buf->descriptor.phy_addr;
    }
    LOG_DEBUG (self->log_level, "[%u] next physical address: 0x%lX",
        idx, descriptor->msc_nxtaddr);
  }
}

/**
 *  @fn VvasScalerInstace * vvas_scaler_create (VvasContext * ctx, const char * kernel_name, VvasLogLevel log_level)
 *  @param [in] ctx         - VvasContext handle created using @ref vvas_context_create
 *  @param [in] kernel_name - Scaler kernel name
 *  @param [in] log_level   - Log level for VvasScalerInstace
 *  @return Scaler instance
 *  @brief  This API allocates Scaler instance.
 *  @note   This instance must be freed using @ref vvas_scaler_destroy
 */
static VvasScalerInstace *
vvas_scaler_create_impl (VvasContext * ctx, const char *kernel_name,
    VvasLogLevel log_level)
{
  VvasScalerImpl *self;

  if (!ctx || !kernel_name) {
    return NULL;
  }

  if (!ctx->dev_handle) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, ctx->log_level, "Device handle not found");
    return NULL;
  }

  self = (VvasScalerImpl *) calloc (1, sizeof (VvasScalerImpl));

  if (!self) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, ctx->log_level, "Couldn't allocate scaler");
    return NULL;
  }

  /* VvasContext is expected to be alive at least till VvasScalerInstace is alive,
   * hence storing ctx instead of creating a copy of it. */
  self->vvas_ctx = ctx;
  self->props.smode = VVAS_SCALER_SCALE_MODE;
  self->props.ftaps = VVAS_SCALER_DEFAULT_NUM_TAPS;
  self->props.coef_load_type = VVAS_SCALER_DEFAULT_COEF_LOAD_TYPE;
  self->props.ppc = VVAS_SCALER_DEFAULT_PPC;
  self->log_level = log_level;

  /* Open Kernel Context */
  LOG_DEBUG (self->log_level, "Opening kernel: %s", kernel_name);

  if (vvas_xrt_open_context (self->vvas_ctx->dev_handle, self->vvas_ctx->uuid,
          &self->kernel_handle, kernel_name, true)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, ctx->log_level,
        "Couldn't create kernel context");
    free (self);
    return NULL;
  }

  LOG_DEBUG (self->log_level, "Created Scaler...");
  LOG_DEBUG (self->log_level, "Coefficient loading type: %d",
      self->props.coef_load_type);
  LOG_DEBUG (self->log_level, "Scaling Mode: %d", self->props.smode);
  LOG_DEBUG (self->log_level, "Number of filter taps: %d", self->props.ftaps);

  return (VvasScalerInstace *) self;
}

/**
 *  @fn VvasReturnType vvas_scaler_channel_add (VvasScalerInstace * hndl,
 *                                              VvasScalerRect * src_rect,
 *                                              VvasScalerRect * dst_rect,
 *                                              VvasScalerPpe * ppe)
 *  @param [in] hndl        - VvasScalerInstace handle pointer created using @ref vvas_scaler_create
 *  @param [in] src_rect    - Source Rect @ref VvasScalerRect
 *  @param [in] dst_rect    - Destination Rect @ref VvasScalerRect.
 *  @param [in] ppe         - Pre processing parameters @ref VvasScalerPpe\n NULL if no PPE is needed
 *  @return VvasReturnType
 *  @brief  This API adds one processing channel configuration.
 */
static VvasReturnType
vvas_scaler_channel_add_impl (VvasScalerInstace * hndl,
    VvasScalerRect * src_rect, VvasScalerRect * dst_rect,
    VvasScalerPpe * ppe, VvasScalerParam *param)
{
  VvasScalerImpl *self;
  VvasReturnType ret = VVAS_RET_ERROR;
  VvasAllocationType alloc_type;
  VvasAllocationFlags alloc_flag;
  bool bret;
  VvasVideoInfo out_vinfo;

  if (!hndl || !src_rect || !dst_rect || !src_rect->frame || !dst_rect->frame) {
    return VVAS_RET_INVALID_ARGS;
  }

  self = (VvasScalerImpl *) hndl;

  /* Validate input and output video frames, they must be CMA(HW) buffer, and
   * must be allocated for both the host and the device */
  alloc_type = vvas_video_frame_get_allocation_type (src_rect->frame);
  alloc_flag = vvas_video_frame_get_allocation_flag (src_rect->frame);
  if ((VVAS_ALLOC_TYPE_CMA != alloc_type) ||
      (VVAS_ALLOC_FLAG_NONE != alloc_flag)) {
    LOG_ERROR (self->log_level,
        "src_rect->frame must be of CMA type and it must be allocated "
        "on both host and device");
    goto error_;
  }

  alloc_type = vvas_video_frame_get_allocation_type (dst_rect->frame);
  alloc_flag = vvas_video_frame_get_allocation_flag (dst_rect->frame);
  if ((VVAS_ALLOC_TYPE_CMA != alloc_type) ||
      (VVAS_ALLOC_FLAG_NONE != alloc_flag)) {
    LOG_ERROR (self->log_level,
        "dst_rect->frame must be of CMA type and it must be allocated "
        "on both host and device");
    goto error_;
  }

  /* Validate Rect parameters */
  if ((src_rect->width < VVAS_SCALER_MIN_WIDTH)
      || (src_rect->height < VVAS_SCALER_MIN_HEIGHT)) {
    LOG_ERROR (self->log_level, "Input width/height must be at least: %d",
        VVAS_SCALER_MIN_WIDTH);
    goto error_;
  }

  /* Let's validate and align src and dst bbox rectangles */
  bret = vvas_scaler_validate_rect_params (self, src_rect);
  bret &= vvas_scaler_validate_rect_params (self, dst_rect);
  if (!bret) {
    LOG_ERROR (self->log_level, "Failed to validate rect params");
    goto error_;
  }

  vvas_video_frame_get_videoinfo (dst_rect->frame, &out_vinfo);

  if (param && param->type == VVAS_SCALER_LETTERBOX &&  !(out_vinfo.width < vvas_scaler_align (dst_rect->width, 8 * self->props.ppc))) {
    dst_rect->width = vvas_scaler_align (dst_rect->width, 8 * self->props.ppc);

  }

  if (vvas_list_length (self->internal_buf_list) <= self->num_of_channels) {
    /* Don't have sufficient internal buffers for operation,
     * need to allocate more internal buffers for processing this operation */
    if (!vvas_scaler_allocate_internal_buffers (self)) {
      LOG_ERROR (self->log_level, "Failed to allocate internal buffers");
      ret = VVAS_RET_ALLOC_ERROR;
      goto error_;
    }
  }

  if (!vvas_scaler_prepare_processing_descriptor (self, src_rect, dst_rect, ppe,
          self->num_of_channels)) {
    LOG_ERROR (self->log_level, "Failed to prepare processing descriptors");
    goto error_;
  }

  /* Sync input host frame to the device */
  vvas_video_frame_sync_data (src_rect->frame, VVAS_DATA_SYNC_TO_DEVICE);

  self->num_of_channels++;
  ret = VVAS_RET_SUCCESS;
  LOG_DEBUG (self->log_level, "success, total channels: %u",
      self->num_of_channels);

error_:
  return ret;
}

/**
 *  @fn VvasReturnType vvas_scaler_process_frame (VvasScalerInstace * hndl)
 *  @param [in] hndl    - VvasScalerInstace handle pointer created using @ref vvas_scaler_create
 *  @return VvasReturnType
 *  @brief  This API does processing of channels added using @ref vvas_scaler_channel_add
 */
static VvasReturnType
vvas_scaler_process_frame_impl (VvasScalerInstace * hndl)
{
  VvasScalerImpl *self;
  VvasScalerInternlBuffer *internal_buf;
  uint64_t desc_addr = 0;
  uint32_t idx, ms_status = 0;
  int32_t iret;
  int32_t retry_count = VVAS_SCALER_MAX_EXEC_WAIT_RETRY_CNT;
  VvasReturnType ret = VVAS_RET_ERROR;

  if (!hndl) {
    return VVAS_RET_INVALID_ARGS;
  }

  self = (VvasScalerImpl *) hndl;

  if (!self->num_of_channels) {
    LOG_DEBUG (self->log_level, "No channel added");
    return VVAS_RET_SUCCESS;
  }

  /* Create link between descriptors by filling the next physical address of
   * each descriptor */
  vvas_scaler_fill_physical_address (self);

  /* Sync Filter coefficients and processing descriptors to device */
  if (!vvas_scaler_descriptor_sync_bo (self))
    goto error;

  if (!vvas_scaler_coeff_sync_bo (self))
    goto error;

  vvas_scaler_print_descriptors (self);
  LOG_DEBUG (self->log_level, "Processing %u number of channel(s)",
      self->num_of_channels);

  internal_buf = GET_INTERNAL_BUFFERS (0);
  if (!internal_buf) {
    LOG_ERROR (self->log_level, "Couldn't get internal buffer");
    goto error;
  }

  /* Pass descriptor to the IP */
  desc_addr = internal_buf->descriptor.phy_addr;
  iret = vvas_scaler_exec_device_buf (self->vvas_ctx->dev_handle,
      self->kernel_handle, &self->run_handle, "ulppu", self->num_of_channels,
      desc_addr, NULL, NULL, ms_status);
  if (iret) {
    LOG_ERROR (self->log_level, "failed to execute command %d, reason: %s\n",
        iret, strerror (errno));
    goto error;
  }

  do {
    /* Wait for the IP's response */
    iret = vvas_xrt_exec_wait (self->vvas_ctx->dev_handle, self->run_handle,
        VVAS_SCALER_DEFAULT_TIMEOUT);
    /* Lets try for MAX count unless there is a error or completion */
    if (iret == ERT_CMD_STATE_TIMEOUT) {
      LOG_ERROR (self->log_level, "Timeout...retry execwait\n");
      if (retry_count-- <= 0) {
        LOG_ERROR (self->log_level,
            "Max retry count %d reached..returning error\n",
            VVAS_SCALER_MAX_EXEC_WAIT_RETRY_CNT);
        goto error;
      }
    } else if (iret == ERT_CMD_STATE_ERROR) {
      LOG_ERROR (self->log_level, "ExecWait ret = %d\n", iret);
      goto error;
    }
  } while (iret != ERT_CMD_STATE_COMPLETED);

  ret = VVAS_RET_SUCCESS;

  for (idx = 0; idx < self->num_of_channels; idx++) {
    VvasScalerInternlBuffer *internal_buf;
    internal_buf = GET_INTERNAL_BUFFERS (idx);
    if (!internal_buf) {
      LOG_ERROR (self->log_level, "Couldn't get internal buffer");
      goto error;
    }
    /* Set SYNC_FROM_DEVICE flag on all output VideoFrames, so that
     * when someone maps this video frame in READ mode, the frames
     * will get sync to the Host */
    vvas_video_frame_set_sync_flag (internal_buf->outvideo_frame,
        VVAS_DATA_SYNC_FROM_DEVICE);
  }

error:
  /* Free run_handle allocated in vvas_scaler_exec_device_buf() */
  if (self->run_handle) {
    vvas_xrt_free_run_handle (self->run_handle);
  }
  self->num_of_channels = 0;
  return ret;
}

/**
 *  @fn VvasReturnType vvas_scaler_destroy (VvasScalerInstace * hndl)
 *  @param [in] hndl    - VvasScalerInstace pointer created using @ref vvas_scaler_create
 *  @return VvasReturnType
 *  @brief  This API destroys the scaler instance created using @ref vvas_scaler_create
 */
static VvasReturnType
vvas_scaler_destroy_impl (VvasScalerInstace * hndl)
{
  VvasScalerImpl *self;
  int32_t iret;
  VvasReturnType ret = VVAS_RET_SUCCESS;

  if (!hndl) {
    return VVAS_RET_INVALID_ARGS;
  }

  self = (VvasScalerImpl *) hndl;

  if (self->internal_buf_list) {
    /* Free internally allocated XRT buffers */
    vvas_list_foreach (self->internal_buf_list,
        vvas_scaler_free_internal_buffers, NULL);

    vvas_list_free (self->internal_buf_list);
  }

  if (self->kernel_handle) {
    iret = vvas_xrt_close_context (self->kernel_handle);
    if (iret != 0) {
      LOG_ERROR (self->log_level, "Failed to close XRT context");
      ret = VVAS_RET_ERROR;
    }
  }

  LOG_DEBUG (self->log_level, "Scaler Destroyed");
  /* Freeing myself :) */
  free (self);
  return ret;
}

/**
 *  @fn VvasReturnType vvas_scaler_set_filter_coef (VvasScalerInstace * hndl,
                                                    VvasScalerFilterCoefType coef_type,
                                                    const int16_t tbl[VVAS_SCALER_MAX_PHASES][VVAS_SCALER_FILTER_TAPS_12])
 *  @param [in] hndl        - VvasScalerInstace handle pointer created using @ref vvas_scaler_create
 *  @param [in] coef_type   - coef_type @ref VvasScalerFilterCoefType
 *  @param [in] tbl         - tbl Reference of VVAS_SCALER_MAX_PHASESxVVAS_SCALER_FILTER_TAPS_12 array of short
 *  @return VvasReturnType
 *  @brief  This API can be used to over write default filter coefficients.
 */
static VvasReturnType
vvas_scaler_set_filter_coef_impl (VvasScalerInstace * hndl,
    VvasScalerFilterCoefType coef_type,
    const int16_t tbl[VVAS_SCALER_MAX_PHASES][VVAS_SCALER_FILTER_TAPS_12])
{
  VvasScalerImpl *self;
  VvasReturnType ret = VVAS_RET_SUCCESS;

  if (!hndl) {
    return VVAS_RET_INVALID_ARGS;
  }

  self = (VvasScalerImpl *) hndl;

  switch (coef_type) {
    case VVAS_SCALER_FILTER_COEF_SR13:{
      memcpy (&XV_multiscaler_fixed_coeff_SR13_0, tbl,
          VVAS_SCALER_FIXED_FILTER_COEF_SIZE);
    }
      break;

    case VVAS_SCALER_FILTER_COEF_SR15:{
      memcpy (&XV_multiscaler_fixed_coeff_SR15_0, tbl,
          VVAS_SCALER_FIXED_FILTER_COEF_SIZE);
    }
      break;

    case VVAS_SCALER_FILTER_COEF_SR2:{
      memcpy (&XV_multiscaler_fixedcoeff_taps8_12C, tbl,
          VVAS_SCALER_FIXED_FILTER_COEF_SIZE);
    }
      break;

    case VVAS_SCALER_FILTER_COEF_SR25:{
      memcpy (&XV_multiscaler_fixed_coeff_SR25_0, tbl,
          VVAS_SCALER_FIXED_FILTER_COEF_SIZE);
    }
      break;

    case VVAS_SCALER_FILTER_COEF_TAPS_10:{
      memcpy (&XV_multiscaler_fixedcoeff_taps10_12C, tbl,
          VVAS_SCALER_FIXED_FILTER_COEF_SIZE);
    }
      break;

    case VVAS_SCALER_FILTER_COEF_TAPS_12:{
      memcpy (&XV_multiscaler_fixedcoeff_taps12_12C, tbl,
          VVAS_SCALER_FIXED_FILTER_COEF_SIZE);
    }
      break;

    case VVAS_SCALER_FILTER_COEF_TAPS_6:{
      memcpy (&XV_multiscaler_fixedcoeff_taps6_12C, tbl,
          VVAS_SCALER_FIXED_FILTER_COEF_SIZE);
    }
      break;

    default:{
      LOG_ERROR (self->log_level, "Invalid coefficient type");
      ret = VVAS_RET_INVALID_ARGS;
    }
      break;
  }
  return ret;
}

/**
 *  @fn VvasReturnType vvas_scaler_prop_get (VvasScalerInstace * hndl, VvasScalerProp* prop)
 *  @param [in] hndl    - VvasScalerInstace handle pointer created using @ref vvas_scaler_create
 *  @param [out] prop   - Scaler properties @ref VvasScalerProp
 *  @return VvasReturnType
 *  @brief  This API will fill Scaler properties in \p prop. This API returns the default
 *          properties if called before setting these properties.
 */
static VvasReturnType
vvas_scaler_prop_get_impl (VvasScalerInstace * hndl, VvasScalerProp * prop)
{
  VvasScalerImpl *self;

  if (!hndl || !prop) {
    return VVAS_RET_INVALID_ARGS;
  }

  self = (VvasScalerImpl *) hndl;
  *prop = self->props;

  return VVAS_RET_SUCCESS;
}

/**
 *  @fn VvasReturnType vvas_scaler_prop_set (VvasScalerInstace * hndl, VvasScalerProp* prop)
 *  @param [in] hndl    - VvasScalerInstace handle pointer created using @ref vvas_scaler_create
 *  @param [in] prop    - Scaler properties @ref VvasScalerProp
 *  @return VvasReturnType
 *  @brief  This API is used to set properties of VvasScalerInstace
 */
static VvasReturnType
vvas_scaler_prop_set_impl (VvasScalerInstace * hndl, VvasScalerProp * prop)
{
  VvasScalerImpl *self;

  if (!hndl || !prop) {
    return VVAS_RET_INVALID_ARGS;
  }

  self = (VvasScalerImpl *) hndl;

  self->props = *prop;

  LOG_DEBUG (self->log_level, "Coefficient loading type: %d",
      self->props.coef_load_type);
  LOG_DEBUG (self->log_level, "Scaling Mode: %d", self->props.smode);
  LOG_DEBUG (self->log_level, "Number of filter taps: %d", self->props.ftaps);
  LOG_DEBUG (self->log_level, "Memory bank: %u", self->props.mem_bank);

  return VVAS_RET_SUCCESS;
}

VvasScalerInterface VVAS_SCALER = {
  .kernel_name = KERNEL_NAME,
  .vvas_scaler_create_impl = vvas_scaler_create_impl,
  .vvas_scaler_channel_add_impl = vvas_scaler_channel_add_impl,
  .vvas_scaler_process_frame_impl = vvas_scaler_process_frame_impl,
  .vvas_scaler_prop_get_impl = vvas_scaler_prop_get_impl,
  .vvas_scaler_prop_set_impl = vvas_scaler_prop_set_impl,
  .vvas_scaler_set_filter_coef_impl = vvas_scaler_set_filter_coef_impl,
  .vvas_scaler_destroy_impl = vvas_scaler_destroy_impl
};
