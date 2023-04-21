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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include "multiscaler_x86.h"
#include "config.h"

/** @def DUMP_OUTPUT
 *  @brief Enable below DUMP_OUTPUT macro to file dump the output
 */
//#define DUMP_OUTPUT

VvasLogLevel g_log_level = LOG_LEVEL_WARNING;

#define VD_MAX_COMPONENTS  3
#define MAX_POOL_BUFFERS  5

#define CLAMP(a,lo,hi) ((a)<(lo)?(lo) : ((a)>(hi) ? (hi) : (a)))
#ifndef MAX
#define MAX(a,b)       (((a)>(b))?(a):(b))
#endif
#ifndef MIN
#define MIN(a,b)       (((a)<(b))?(a):(b))
#endif

typedef struct _video_format_desc video_format_desc;

void
vertical_chroma_resample_u16 (void *in_buf, U32 in_width, U32 in_height,
    bool passthru, video_format_desc * fmt, void **out_buf,
    MULTI_SCALER_DESC_STRUCT * desc);
void vertical_chroma_resample_u8 (void *in_buf, U32 in_width, U32 in_height,
    bool passthru, video_format_desc * fmt, void **out_buf,
    MULTI_SCALER_DESC_STRUCT * desc);
void horizontal_chroma_resample_u16 (void *in_buf, U32 in_width, U32 in_height,
    bool passthru, video_format_desc * fmt, void **out_buf);
void horizontal_chroma_resample_u8 (void *in_buf, U32 in_width, U32 in_height,
    bool passthru, video_format_desc * fmt, void **out_buf);
void vertical_scale_u16 (void *in_buf, U32 in_width, U32 in_height,
    U32 out_height, void **out_buf, I16 coeff[64][12], video_format_desc * fmt);
void vertical_scale_u8 (void *in_buf, U32 in_width, U32 in_height,
    U32 out_height, void **out_buf, I16 coeff[64][12], video_format_desc * fmt);
void horizontal_scale_u16 (void *in_buf, U32 in_width, U32 in_height,
    U32 out_width, void **out_buf, I16 coeff[64][12], video_format_desc * fmt);
void horizontal_scale_u8 (void *in_buf, U32 in_width, U32 in_height,
    U32 out_width, void **out_buf, I16 coeff[64][12], video_format_desc * fmt);
void pack_into_3sample_pixel_u16 (void *in_buf, U32 in_width, U32 in_height,
    bool passthru, video_format_desc * fmt, void **out_buf);
void pack_into_3sample_pixel_u8 (void *in_buf, U32 in_width, U32 in_height,
    bool passthru, video_format_desc * fmt, void **out_buf);
void csc_u8 (void *in_buf, U32 width, U32 height, bool passthru,
    video_format_desc * fmt, void **out_buf);
void csc_u16 (void *in_buf, U32 width, U32 height, bool passthru,
    video_format_desc * fmt, void **out_buf);
void unpack_into_yuv444_u8 (void *in_buf, U32 in_width, U32 in_height,
    bool passthru, video_format_desc * fmt, void **out_buf);
void unpack_into_yuv444_u16 (void *in_buf, U32 in_width, U32 in_height,
    bool passthru, video_format_desc * fmt, void **out_buf);



typedef void (*vc_resample) (void *in_buf, U32 in_width, U32 in_height,
    bool passthru, video_format_desc * fmt, void **out_buf,
    MULTI_SCALER_DESC_STRUCT * desc);
typedef void (*hc_resample) (void *in_buf, U32 in_width, U32 in_height,
    bool passthru, video_format_desc * fmt, void **out_buf);
typedef void (*pack_yuv444) (void *in_buf, U32 in_width, U32 in_height,
    bool passthru, video_format_desc * fmt, void **out_buf);

typedef void (*vertical_scale) (void *in_buf, U32 in_width, U32 in_height,
    U32 out_height, void **out_buf, I16 coeff[64][12], video_format_desc * fmt);

typedef void (*horizontal_scale) (void *in_buf, U32 in_width, U32 in_height,
    U32 out_width, void **out_buf, I16 coeff[64][12], video_format_desc * fmt);

typedef void (*unpack_yuv444) (void *in_buf, U32 in_width, U32 in_height,
    bool passthru, video_format_desc * fmt, void **out_buf);

typedef void (*csc) (void *in_buf, U32 width, U32 height, bool passthru,
    video_format_desc * fmt, void **out_buf);


typedef enum
{
  /** RGB 8 format */
  VD_PIX_FMT_RGB_RGB8 = 0,
  /** BGR 8 format */
  VD_PIX_FMT_BGR_BGR8 = 1,
  /** NV12 format */
  VD_PIX_FMT_NV12_Y_UV8 = 2,
  /** I420 format */
  VD_PIX_FMT_I420_Y_U_V8 = 3,
  /** Gray 8 format */
  VD_PIX_FMT_GRAY_GRAY8 = 4,
  /** YUV 422 format */
  VD_PIX_FMT_YUV422 = 5,
  /** YUV 444 format */
  VD_PIX_FMT_YUV444 = 6,
  /** NV12 10bit LE format */
  VD_PIX_FMT_NV12_Y_UV10 = 7
} video_pix_format;

typedef enum
{
  VD_CS_RGB = 0,
  VD_CS_YUV444 = 1,
  VD_CS_YUV422 = 2,
  VD_CS_YUV420 = 3
} video_cs;

typedef struct
{
  U8 *mem_ptr[MAX_POOL_BUFFERS];
  U32 free_flag[MAX_POOL_BUFFERS];
} mem_pool;

struct _video_format_desc
{
  /** Input pixel format */
  video_pix_format in_pix_fmt;
  /** Current pixel format, used in the course of conversion */
  video_pix_format curr_pix_fmt;
  /** Output pixel format */
  video_pix_format out_pix_fmt;
  /** Vertical re-sample function pointer */
  vc_resample vc_resample_fn;
  /** Horizontal re-sample function pointer */
  hc_resample hc_resample_fn;
  /** Function pointer to pack into YUV */
  pack_yuv444 pack_yuv444_fn;
  /** Function pointer to scale vertically */
  vertical_scale vertical_scale_fn;
  /** Function pointer to scale horizontally */
  horizontal_scale horizontal_scale_fn;
  /** Function pointer to unpack YUV */
  unpack_yuv444 unpack_yuv444_fn;
  /** Function pointer to do color space conversion */
  csc csc_fn;
  /** Bits per sample */
  U8 bits_per_sample;
  /** Input color space */
  video_cs in_cs;
  /** Output color space */
  video_cs out_cs;
  /** Number of components in a given format */
  U8 num_components;
  /** Number of planes in a given format */
  U8 num_planes;
  /** mean */
  U32 alpha[3];
  /** scale */
  U32 beta[3];
  /** Memory pool */
  mem_pool pool;
};

struct
{
  video_pix_format pix_format;
  const char *format_str;
} conversion[] = {
  {VD_PIX_FMT_RGB_RGB8, "RGB"},
  {VD_PIX_FMT_NV12_Y_UV8, "NV12"},
  {VD_PIX_FMT_I420_Y_U_V8, "I420"},
  {VD_PIX_FMT_BGR_BGR8, "BGR"},
  {VD_PIX_FMT_GRAY_GRAY8, "GRAY8"},
  {VD_PIX_FMT_NV12_Y_UV10, "NV12_10"},
};

static void
mem_pool_create (mem_pool * pool, U32 each_buf_size, int num_buffers)
{
  U32 i = 0;
  for (i = 0; i < num_buffers; i++) {
    pool->mem_ptr[i] = malloc (each_buf_size);
    pool->free_flag[i] = 1;
  }

  for (U32 j = i; j < MAX_POOL_BUFFERS; j++) {
    pool->mem_ptr[j] = NULL;
    pool->free_flag[j] = 0;
  }
}

static void *
mem_pool_get_free_mem (mem_pool * pool)
{
  void *free_mem = NULL;

  for (U32 i = 0; i < MAX_POOL_BUFFERS; i++) {
    if (pool->free_flag[i]) {
      free_mem = pool->mem_ptr[i];
      pool->free_flag[i] = 0;
      break;
    }
  }

  if (!free_mem) {
    LOG_MESSAGE (LOG_LEVEL_WARNING, g_log_level,
        "No free memory left in the pool");
  }

  return free_mem;
}

static void
mem_pool_release_mem (mem_pool * pool, void *mem)
{

  for (U32 i = 0; i < MAX_POOL_BUFFERS; i++) {
    if (pool->mem_ptr[i] == mem) {
      pool->free_flag[i] = 1;
      break;
    }
  }
}

static void
mem_pool_destroy (mem_pool * pool)
{

  for (U32 i = 0; i < MAX_POOL_BUFFERS; i++) {
    if (pool->mem_ptr[i]) {
      free (pool->mem_ptr[i]);
    }
  }
}

static video_cs
map_cs (video_pix_format pix_fmt)
{
  video_cs ret = VD_CS_RGB;
  switch (pix_fmt) {
    case VD_PIX_FMT_RGB_RGB8:
      ret = VD_CS_RGB;
      break;
    case VD_PIX_FMT_BGR_BGR8:
      ret = VD_CS_RGB;
      break;
    case VD_PIX_FMT_NV12_Y_UV8:
      ret = VD_CS_YUV420;
      break;
    case VD_PIX_FMT_I420_Y_U_V8:
      ret = VD_CS_YUV420;
      break;
    case VD_PIX_FMT_GRAY_GRAY8:
      ret = VD_CS_YUV444;
      break;
    case VD_PIX_FMT_YUV422:
      ret = VD_CS_YUV422;
      break;
    case VD_PIX_FMT_YUV444:
      ret = VD_CS_YUV444;
      break;
    case VD_PIX_FMT_NV12_Y_UV10:
      ret = VD_CS_YUV420;
      break;
    default:
      LOG_MESSAGE (LOG_LEVEL_ERROR, g_log_level, "Wrong pixel format");
  }
  return ret;
}

static const char *
format_enum_to_str (video_pix_format fmt)
{
  int j;
  for (j = 0; j < sizeof (conversion) / sizeof (conversion[0]); ++j) {
    if (conversion[j].pix_format == fmt) {
      return conversion[j].format_str;
    }
  }
  LOG_MESSAGE (LOG_LEVEL_ERROR, g_log_level, "Wrong pixel format");
  return NULL;
}

static video_pix_format
xv_fmt_to_pix_enum (U32 xv_fmt)
{
  video_pix_format ret = VD_PIX_FMT_RGB_RGB8;
  switch (xv_fmt) {
    case XV_MULTI_SCALER_RGB8:
      ret = VD_PIX_FMT_RGB_RGB8;
      break;
    case XV_MULTI_SCALER_BGR8:
      ret = VD_PIX_FMT_BGR_BGR8;
      break;
    case XV_MULTI_SCALER_Y_UV8_420:
      ret = VD_PIX_FMT_NV12_Y_UV8;
      break;
    case XV_MULTI_SCALER_I420:
      ret = VD_PIX_FMT_I420_Y_U_V8;
      break;
    case XV_MULTI_SCALER_Y8:
      ret = VD_PIX_FMT_GRAY_GRAY8;
      break;
    case XV_MULTI_SCALER_Y_UV10_420:
      ret = VD_PIX_FMT_NV12_Y_UV10;
      break;
    default:
      LOG_MESSAGE (LOG_LEVEL_ERROR, g_log_level, "Unsupported format");
  }
  return ret;
}

static void
bytes_to_pixel_array (void *in_buf, U32 in_width, U32 in_height,
    video_format_desc * fmt, void **out_array)
{

  if (fmt->in_pix_fmt == VD_PIX_FMT_NV12_Y_UV10) {
    U32 *in_buf_32 = (U32 *) in_buf;
    U32 *in_buf_uv_32 = in_buf_32 + ((in_width + 2) / 3 * in_height);
    /* 3 samples per word or U32, so number of words to be processed is width/3 rounded off */
    U32 num_words = (in_width + 2) / 3;
    U16 *out_buf = (U16 *) mem_pool_get_free_mem (&fmt->pool);
    U16 *out_buf_uv = out_buf + (in_width * in_height);
    U32 UV = 0;
    U16 Un = 0, Vn = 0;
    U32 v_uv = 0;


    for (U32 v = 0; v < in_height; v++) {
      for (U32 h = 0; h < num_words; h++) {
        /* Every time we compute 3 samples, for the last 
         * iteration it may be less than 3 (in_width - h * 3) */
        U32 num_comps = MIN (3, in_width - h * 3);
        U32 pix = h * 3;
        U32 Y;
        Y = in_buf_32[v * num_words + h];
        for (U32 c = 0; c < num_comps; c++) {
          U16 Yn = 0;

          Yn = (Y & 0x03ff) << 6;
          Y >>= 10;
          out_buf[(v * in_width) + (pix + c)] = Yn;
          /* Lets extract UV for every even times */
          if (!(v & 1)) {
            /* Unpacking UV has been reduced to a cycle of 6 states. The following
             * code is a reduce version of:
             * 0: - Read first UV word (UVU)
             *      Unpack U and V
             * 1: - Reused U/V from 1 (sub-sampling)
             * 2: - Unpack remaining U value
             *    - Read following UV word (VUV)
             *    - Unpack V value
             * 3: - Reuse U/V from 2 (sub-sampling)
             * 4: - Unpack remaining U
             *    - Unpack remaining V
             * 5: - Reuse UV/V from 4 (sub-sampling)
             */
            switch ((pix + c) % 6) {
              case 0:
                UV = in_buf_uv_32[(v_uv * num_words) + h];
                /* fallthrough */
              case 4:
                Un = (UV & 0x03ff) << 6;
                UV >>= 10;
                out_buf_uv[(v_uv * in_width) + (pix + c)] = Un;
                Vn = (UV & 0x03ff) << 6;
                UV >>= 10;
                out_buf_uv[(v_uv * in_width) + (pix + c + 1)] = Vn;
                break;
              case 2:
                Un = (UV & 0x03ff) << 6;
                out_buf_uv[(v_uv * in_width) + (pix + c)] = Un;
                UV = in_buf_uv_32[(v_uv * num_words) + h + 1];
                Vn = (UV & 0x03ff) << 6;
                UV >>= 10;
                out_buf_uv[(v_uv * in_width) + (pix + c + 1)] = Vn;
                break;
              default:
                /* keep value */
                break;
            }
          }
        }                       /* Ends num_comps loop */
      }                         /* Ends num_words loop */
      if (!(v & 1)) {
        v_uv++;
      }
    }                           /* Ends in_height loop */
    /* Update the output buffer */
    *out_array = out_buf;
    mem_pool_release_mem (&fmt->pool, in_buf);
  } else {
    *out_array = in_buf;
  }
}

static void
pixel_array_to_bytes (void *in_buf, U32 in_width, U32 in_height, U32 stride,
    video_format_desc * fmt, void **out_array)
{

  if (fmt->in_pix_fmt == VD_PIX_FMT_NV12_Y_UV10) {
    U16 *in_buf_16 = (U16 *) in_buf;
    /* 3 samples per word or U32, so number of words to be processed is width/3 rounded off */
    U32 num_words = (in_width + 2) / 3;
    U16 *in_buf_uv_16 = in_buf_16 + (in_width * in_height);
    U32 *out_buf = (U32 *) out_array[0];
    U32 *out_buf_uv = (U32 *) out_array[1];
    U32 v_uv = 0;

    for (U32 v = 0; v < in_height; v++) {
      for (U32 h = 0; h < num_words; h++) {
        /* Every time we compute 3 samples, for the last 
         * iteration it may be less than 3 (in_width - h * 3) */
        U32 num_comps = MIN (3, (in_width - (h * 3)));
        U32 pix = h * 3;
        U32 Y = 0;
        U32 UV = 0;

        for (U32 c = 0; c < num_comps; c++) {
          Y |= (in_buf_16[v * in_width + pix + c] >> 6) << (10 * c);

          /* For chroma (UV), run only for half of the totoal iterations,
           * as UV is half of Y in case of NV12 */
          if (!(v & 1)) {
            switch ((pix + c) % 6) {
              case 0:
                UV = 0;
                UV |= in_buf_uv_16[v_uv * in_width + pix + c] >> 6;
                UV |= (in_buf_uv_16[v_uv * in_width + pix + c + 1] >> 6) << 10;
                break;
              case 2:
                UV |= (in_buf_uv_16[v_uv * in_width + pix + c] >> 6) << 20;
                out_buf_uv[v_uv * num_words + h] = UV;
                UV = 0;
                UV |= in_buf_uv_16[v_uv * in_width + pix + c + 1] >> 6;
                break;
              case 4:
                UV |= (in_buf_uv_16[v_uv * in_width + pix + c] >> 6) << 10;
                UV |= (in_buf_uv_16[v_uv * in_width + pix + c + 1] >> 6) << 20;
                out_buf_uv[v_uv * num_words + h] = UV;
                break;
              default:
                break;
            }
          }
        }
        out_buf[v * num_words + h] = Y;
      }
      if (!(v & 1))
        v_uv++;
    }
  } else if (fmt->out_pix_fmt == VD_PIX_FMT_NV12_Y_UV8) {
    U8 *out_buf_y = out_array[0];
    U8 *out_buf_uv = out_array[1];
    U8 *in_buf_y = in_buf;
    U8 *in_buf_uv = in_buf_y + (in_width * in_height);
    /* Let's copy straightaway if stride is same as width */
    if (in_width == stride) {
      memcpy (out_buf_y, in_buf, ((in_width * in_height)));
      memcpy (out_buf_uv, in_buf_uv, (((in_width * in_height) / 2)));
    } else {
      /* Otherwise we have to copy line by line */
      for (U32 v = 0; v < in_height; v++) {
        memcpy (out_buf_y, in_buf_y, in_width);
        out_buf_y += stride;
        in_buf_y += in_width;
        if (!(v & 1)) {
          /* Copy the UV line */
          memcpy (out_buf_uv, in_buf_uv, in_width);
          in_buf_uv += in_width;
          out_buf_uv += stride;
        }
      }
    }
  } else if (fmt->out_pix_fmt == VD_PIX_FMT_I420_Y_U_V8) {
    U8 *out_buf_y = out_array[0];
    U8 *out_buf_u = out_array[1];
    U8 *out_buf_v = out_array[2];
    U8 *in_buf_y = in_buf;
    U8 *in_buf_u = in_buf_y + (in_width * in_height);
    U8 *in_buf_v = in_buf_u + ((in_width / 2) * (in_height / 2));

    /* Let's copy straightaway if stride is same as width */
    if (in_width == stride) {
      memcpy (out_buf_y, in_buf, (in_width * in_height));
      memcpy (out_buf_u, in_buf_u, ((in_width * in_height) / 4));
      memcpy (out_buf_v, in_buf_v, ((in_width * in_height) / 4));
    } else {
      /* Otherwise we have to copy line by line */
      for (U32 v = 0; v < in_height; v++) {
        memcpy (out_buf_y, in_buf_y, in_width);
        out_buf_y += stride;
        in_buf_y += in_width;
        if (!(v & 1)) {
          /* Copy a U line */
          memcpy (out_buf_u, in_buf_u, (in_width / 2));
          in_buf_u += (in_width / 2);
          out_buf_u += (stride / 2);
          /* Copy a V line */
          memcpy (out_buf_v, in_buf_v, (in_width / 2));
          in_buf_v += (in_width / 2);
          out_buf_v += (stride / 2);
        }
      }
    }
  } else if (fmt->out_pix_fmt == VD_PIX_FMT_RGB_RGB8 ||
      fmt->out_pix_fmt == VD_PIX_FMT_BGR_BGR8) {
    U8 *out_buf = out_array[0];
    U8 *in_buf_temp = in_buf;
    /* Let's copy straightaway if stride is same as width */
    if ((in_width * 3) == stride) {
      memcpy (out_buf, in_buf_temp, ((in_width * in_height * 3)));
    } else {
      /* Otherwise we have to copy line by line */
      for (U32 v = 0; v < in_height; v++) {
        memcpy (out_buf, in_buf_temp, (in_width * 3));
        out_buf += stride;
        in_buf_temp += (in_width * 3);
      }
    }
  } else if (fmt->out_pix_fmt == VD_PIX_FMT_GRAY_GRAY8) {
    U8 *out_buf = out_array[0];
    U8 *in_buf_temp = in_buf;
    /* Let's copy straightaway if stride is same as width */
    if (in_width == stride) {
      memcpy (out_buf, in_buf_temp, (in_width * in_height));
    } else {
      /* Otherwise we have to copy line by line */
      for (U32 v = 0; v < in_height; v++) {
        memcpy (out_buf, in_buf_temp, in_width);
        out_buf += stride;
        in_buf_temp += in_width;
      }
    }
  }
}

static U16
get_array_value_u16 (U16 * buf, I32 x, I32 y, U32 pix_width, U32 pix_height,
    U8 num_components)
{
  I32 bytes_width = pix_width * VD_MAX_COMPONENTS;
  I32 bytes_height = pix_height;

  if (x < 0 || y < 0 || y >= bytes_width || x >= bytes_height)
    return 0;
  return buf[x * bytes_width + y];
}

void
vertical_scale_u16 (void *in_buf, U32 in_width, U32 in_height, U32 out_height,
    void **out_buf, I16 coeff[64][12], video_format_desc * fmt)
{
  U16 *in_buf_16 = (U16 *) in_buf;
  float scaling_factor = ((float) in_height / out_height);
  U32 coeff_idx = 0;
  U32 src_pix_idx_h = 0, src_pix_idx_v = 0;
  long long sum[VD_MAX_COMPONENTS] = { 0 };
  U16 *vs_out_buf = NULL;

  if (in_height == out_height) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, g_log_level,
        "Input and Output heights are same, no need to scale");
    *out_buf = in_buf;
    return;
  }

  vs_out_buf = mem_pool_get_free_mem (&fmt->pool);
  for (int h = 0; h < in_width; h++) {  /* Run for all input width */
    for (int v = 0; v < out_height; v++) {      /* Run for all output height */
      src_pix_idx_h = h;
      /* Get the scaled height co-ordinate */
      src_pix_idx_v = v * scaling_factor;
      /* Find out the coefficient precision index value to be used */
      coeff_idx = ((v * scaling_factor) - src_pix_idx_v) * 64;
      for (int i = 0; i < 12; i++) {    /* Run the loop for all 12 tap values */
        /* Get sample idx from pix index */
        int byte_h = src_pix_idx_h * VD_MAX_COMPONENTS;
        int byte_v = src_pix_idx_v;
        for (int k = 0; k < VD_MAX_COMPONENTS; k++) {
          /* Get indexes of input source buffers to be reffered at sample level */
          int src_byte_v = byte_v - 6 + i;
          int src_byte_h = byte_h + k;
          sum[k] += (I32) (coeff[coeff_idx][i]) *
              (I32) (get_array_value_u16 (in_buf_16, src_byte_v, src_byte_h,
                  in_width, in_height, VD_MAX_COMPONENTS));
        }
      }
      for (int k = 0; k < VD_MAX_COMPONENTS; k++) {
        /* Normalize and add to the output buffer value */
        sum[k] /= 4096;
        vs_out_buf[(v * in_width * VD_MAX_COMPONENTS) + h * VD_MAX_COMPONENTS +
            k] = sum[k];
        sum[k] = 0;
      }
    }
  }
  *out_buf = vs_out_buf;
  mem_pool_release_mem (&fmt->pool, in_buf);
}

static void
populate_v8_array_for_scaling (U8 * in_buf, U32 width, U32 height,
    U8 pix_array[12][VD_MAX_COMPONENTS], U32 pix_idx_h, U32 pix_idx_v, U8 taps)
{
  I32 start_v, start_h;
  U8 last_value[VD_MAX_COMPONENTS] = { 0 };

  start_v = pix_idx_v - (taps / 2 - 1);
  start_h = pix_idx_h;
  if (start_v < 0) {
    for (I32 v = (start_v + taps - 1); v >= start_v; v--) {
      for (U8 c = 0; c < VD_MAX_COMPONENTS; c++) {
        if (v >= 0) {
          pix_array[v - start_v][c] =
              in_buf[(v * width * VD_MAX_COMPONENTS) +
              (start_h * VD_MAX_COMPONENTS) + c];
          last_value[c] = pix_array[v - start_v][c];
        } else {
          pix_array[v - start_v][c] = last_value[c];
        }
      }
    }
  } else {
    for (I32 v = start_v; v < (start_v + taps); v++) {
      for (U8 c = 0; c < VD_MAX_COMPONENTS; c++) {
        if (v < height) {
          pix_array[v - start_v][c] =
              in_buf[(v * width * VD_MAX_COMPONENTS) +
              (start_h * VD_MAX_COMPONENTS) + c];
          last_value[c] = pix_array[v - start_v][c];
        } else {
          pix_array[v - start_v][c] = last_value[c];
        }
      }
    }
  }
}


void
vertical_scale_u8 (void *in_buf, U32 in_width, U32 in_height, U32 out_height,
    void **out_buf, I16 coeff[64][12], video_format_desc * fmt)
{
  U8 *in_buf_8 = (U8 *) in_buf;
  float scaling_factor = ((float) in_height / out_height);
  U32 coeff_idx = 0;
  U32 src_pix_idx_h = 0, src_pix_idx_v = 0;
  I32 sum[VD_MAX_COMPONENTS] = { 0 }, norm = 0;
  U8 *vs_out_buf = NULL;

  if (in_height == out_height) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, g_log_level,
        "Input and Output heights are same, no need to scale");
    *out_buf = in_buf;
    return;
  }
  vs_out_buf = (U8 *) mem_pool_get_free_mem (&fmt->pool);

  for (I32 h = 0; h < in_width; h++) {  /* Run for all input width */
    for (I32 v = 0; v < out_height; v++) {      /* Run for all output height */
      src_pix_idx_h = h;
      /* Get the scaled height co-ordinate */
      src_pix_idx_v = v * scaling_factor;
      /* Find out the coefficient precision index value to be used */
      coeff_idx = ((v * scaling_factor) - src_pix_idx_v) * 64;
      sum[0] = (1 << 12) >> 1;
      sum[1] = (1 << 12) >> 1;
      sum[2] = (1 << 12) >> 1;
      U8 pix_array[12][VD_MAX_COMPONENTS];
      populate_v8_array_for_scaling (in_buf_8, in_width, in_height, pix_array,
          src_pix_idx_h, src_pix_idx_v, 12);

      for (I32 i = 0; i < 12; i++) {    /* Run the loop for all 12 tap values. TODO: Tap size hardcoded */
        for (I32 k = 0; k < VD_MAX_COMPONENTS; k++) {
          sum[k] += pix_array[i][k] * coeff[coeff_idx][i];
        }
      }

      for (I32 k = 0; k < VD_MAX_COMPONENTS; k++) {
        /* Normalize and add to the output buffer value */
        norm = sum[k] >> 12;    /*  Coefficient Precision Shift */
        norm = CLAMP (norm, 0, (1 << 8) - 1);   /* Clamp the values between 0 and max of 8 bit values */
        vs_out_buf[(v * in_width * VD_MAX_COMPONENTS) + h * VD_MAX_COMPONENTS +
            k] = norm;
        sum[k] = 0;
      }
    }
  }
  *out_buf = vs_out_buf;
  mem_pool_release_mem (&fmt->pool, in_buf);
}

void
horizontal_scale_u16 (void *in_buf, U32 in_width, U32 in_height, U32 out_width,
    void **out_buf, I16 coeff[64][12], video_format_desc * fmt)
{
  U16 *in_buf_16 = (U16 *) in_buf;
  float scaling_factor = ((float) in_width / out_width);
  U32 coeff_idx = 0;
  U32 src_pix_idx_h = 0, src_pix_idx_v = 0;
  U16 *hs_out_buf = NULL;
  long long sum[VD_MAX_COMPONENTS] = { 0 };

  if (in_width == out_width) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, g_log_level,
        "Input and Output widths are same, no need to scale");
    *out_buf = in_buf;
    return;
  }

  hs_out_buf = (U16 *) mem_pool_get_free_mem (&fmt->pool);

  for (int v = 0; v < in_height; v++) { /* Run for all input height */
    for (int h = 0; h < out_width; h++) {       /* Run for all output width */
      /* Get the scaled width co-ordinate */
      src_pix_idx_h = h * scaling_factor;
      src_pix_idx_v = v;
      /* Find out the coefficient precision index value to be used */
      coeff_idx = ((h * scaling_factor) - src_pix_idx_h) * 64;
      for (int i = 0; i < 12; i++) {    /* Run the loop for all 12 tap values */
        /* Get byte idx from pix index */
        int byte_h = src_pix_idx_h * VD_MAX_COMPONENTS;
        int byte_v = src_pix_idx_v;
        for (int k = 0; k < VD_MAX_COMPONENTS; k++) {
          /* Get indexes of input source buffers to be reffered at sample level */
          int src_byte_v = byte_v;
          int src_byte_h =
              byte_h - (6 * VD_MAX_COMPONENTS) + (i * VD_MAX_COMPONENTS) + k;
          sum[k] +=
              (I32) (coeff[coeff_idx][i]) *
              (I32) (get_array_value_u16 (in_buf_16, src_byte_v, src_byte_h,
                  in_width, in_height, VD_MAX_COMPONENTS));
        }
      }
      for (int k = 0; k < VD_MAX_COMPONENTS; k++) {
        /* Normalize and add to the output buffer value */
        sum[k] /= 4096;
        hs_out_buf[(v * out_width * VD_MAX_COMPONENTS) + h * VD_MAX_COMPONENTS +
            k] = sum[k];
        sum[k] = 0;
      }
    }
  }
  *out_buf = hs_out_buf;
  mem_pool_release_mem (&fmt->pool, in_buf);
}


static void
populate_h8_array_for_scaling (U8 * in_buf, U32 width, U32 height,
    U8 pix_array[12][VD_MAX_COMPONENTS], U32 pix_idx_h, U32 pix_idx_v, U8 taps)
{
  I32 start_v, start_h;
  U8 last_value[VD_MAX_COMPONENTS] = { 0 };

  /* Locate the start end of the pixel on which filter has to be applied */
  start_v = pix_idx_v;
  start_h = pix_idx_h - (taps / 2 - 1);

  /* If we are going out of the boundary to start with, start taking values from other end
   * of the pixels, so that we can repeat the last boundary value to extend the filter window.
   */
  if (start_h < 0) {
    for (I32 h = (start_h + taps - 1); h >= start_h; h--) {
      for (U8 c = 0; c < VD_MAX_COMPONENTS; c++) {
        if (h >= 0) {
          pix_array[h - start_h][c] =
              in_buf[(start_v * (width * VD_MAX_COMPONENTS)) +
              (h * VD_MAX_COMPONENTS) + c];
          /* Keep updating the current pixel values into last value,
           * will use it when go out of boundary */
          last_value[c] = pix_array[h - start_h][c];
        } else {
          pix_array[h - start_h][c] = last_value[c];
        }
      }
    }
  } else {
    for (I32 h = start_h; h < (start_h + taps); h++) {
      for (U8 c = 0; c < VD_MAX_COMPONENTS; c++) {
        if (h < width) {
          pix_array[h - start_h][c] =
              in_buf[(start_v * (width * VD_MAX_COMPONENTS)) +
              (h * VD_MAX_COMPONENTS) + c];
          last_value[c] = pix_array[h - start_h][c];
        } else {
          pix_array[h - start_h][c] = last_value[c];
        }
      }
    }
  }
}


void
horizontal_scale_u8 (void *in_buf, U32 in_width, U32 in_height, U32 out_width,
    void **out_buf, I16 coeff[64][12], video_format_desc * fmt)
{
  U8 *in_buf_8 = (U8 *) in_buf;
  float scaling_factor = ((float) in_width / out_width);
  U32 coeff_idx = 0;
  U32 src_pix_idx_h = 0, src_pix_idx_v = 0;
  U8 *hs_out_buf = NULL;
  I32 sum[VD_MAX_COMPONENTS] = { 0 }, norm = 0;

  if (in_width == out_width) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, g_log_level,
        "Input and Output widths are same, no need to scale");
    *out_buf = in_buf;
    return;
  }

  hs_out_buf = (U8 *) mem_pool_get_free_mem (&fmt->pool);
  for (int v = 0; v < in_height; v++) { /* Run for all input height */
    for (int h = 0; h < out_width; h++) {       /* Run for all output width */
      /* Get the scaled width co-ordinate */
      src_pix_idx_h = h * scaling_factor;
      src_pix_idx_v = v;
      /* Find out the coefficient precision index value to be used */
      coeff_idx = ((h * scaling_factor) - src_pix_idx_h) * 64;
      sum[0] = (1 << 12) >> 1;
      sum[1] = (1 << 12) >> 1;
      sum[2] = (1 << 12) >> 1;
      U8 pix_array[12][VD_MAX_COMPONENTS];

      populate_h8_array_for_scaling (in_buf_8, in_width, in_height, pix_array,
          src_pix_idx_h, src_pix_idx_v, 12);

      for (I32 i = 0; i < 12; i++) {    /* Run the loop for all 12 tap values. TODO: Tap size hardcoded */
        for (I32 k = 0; k < VD_MAX_COMPONENTS; k++) {
          sum[k] += pix_array[i][k] * coeff[coeff_idx][i];
        }
      }

      for (int k = 0; k < VD_MAX_COMPONENTS; k++) {
        /* Normalize and add to the output buffer value */
        norm = sum[k] >> 12;
        norm = CLAMP (norm, 0, (1 << 8) - 1);
        hs_out_buf[(v * out_width * VD_MAX_COMPONENTS) + h * VD_MAX_COMPONENTS +
            k] = norm;
        sum[k] = 0;
      }
    }
  }
  *out_buf = hs_out_buf;
  mem_pool_release_mem (&fmt->pool, in_buf);
}

static I32
populate_video_format_info (U32 in_fmt, U32 out_fmt,
    video_format_desc * vd_fmt_dsc)
{

  switch (in_fmt) {
    case XV_MULTI_SCALER_RGB8: /* RGB */
      vd_fmt_dsc->in_pix_fmt = VD_PIX_FMT_RGB_RGB8;
      vd_fmt_dsc->out_pix_fmt = xv_fmt_to_pix_enum (out_fmt);
      vd_fmt_dsc->in_cs = map_cs (vd_fmt_dsc->in_pix_fmt);
      vd_fmt_dsc->out_cs = map_cs (vd_fmt_dsc->out_pix_fmt);
      vd_fmt_dsc->num_planes = 1;
      vd_fmt_dsc->num_components = 3;
      vd_fmt_dsc->bits_per_sample = 8;
      break;
    case XV_MULTI_SCALER_Y_UV8_420:    /* NV12 */
      vd_fmt_dsc->in_pix_fmt = VD_PIX_FMT_NV12_Y_UV8;
      vd_fmt_dsc->out_pix_fmt = xv_fmt_to_pix_enum (out_fmt);
      vd_fmt_dsc->in_cs = map_cs (vd_fmt_dsc->in_pix_fmt);
      vd_fmt_dsc->out_cs = map_cs (vd_fmt_dsc->out_pix_fmt);
      vd_fmt_dsc->num_planes = 2;
      vd_fmt_dsc->num_components = 1;
      vd_fmt_dsc->bits_per_sample = 8;
      break;
    case XV_MULTI_SCALER_I420: /* I420 */
      vd_fmt_dsc->in_pix_fmt = VD_PIX_FMT_I420_Y_U_V8;
      vd_fmt_dsc->out_pix_fmt = xv_fmt_to_pix_enum (out_fmt);
      vd_fmt_dsc->in_cs = map_cs (vd_fmt_dsc->in_pix_fmt);
      vd_fmt_dsc->out_cs = map_cs (vd_fmt_dsc->out_pix_fmt);
      vd_fmt_dsc->num_planes = 2;
      vd_fmt_dsc->num_components = 1;
      vd_fmt_dsc->bits_per_sample = 8;
      break;
    case XV_MULTI_SCALER_BGR8: /* BGR */
      vd_fmt_dsc->in_pix_fmt = VD_PIX_FMT_BGR_BGR8;
      vd_fmt_dsc->out_pix_fmt = xv_fmt_to_pix_enum (out_fmt);
      vd_fmt_dsc->in_cs = map_cs (vd_fmt_dsc->in_pix_fmt);
      vd_fmt_dsc->out_cs = map_cs (vd_fmt_dsc->out_pix_fmt);
      vd_fmt_dsc->num_planes = 1;
      vd_fmt_dsc->num_components = 3;
      vd_fmt_dsc->bits_per_sample = 8;
      break;
    case XV_MULTI_SCALER_Y8:   /* GRAY8 */
      vd_fmt_dsc->in_pix_fmt = VD_PIX_FMT_GRAY_GRAY8;
      vd_fmt_dsc->out_pix_fmt = xv_fmt_to_pix_enum (out_fmt);
      vd_fmt_dsc->in_cs = map_cs (vd_fmt_dsc->in_pix_fmt);
      vd_fmt_dsc->out_cs = map_cs (vd_fmt_dsc->out_pix_fmt);
      vd_fmt_dsc->num_planes = 1;
      vd_fmt_dsc->num_components = 1;
      vd_fmt_dsc->bits_per_sample = 8;
      break;
    case XV_MULTI_SCALER_Y_UV10_420:   /* NV12_10LE32 */
      vd_fmt_dsc->in_pix_fmt = VD_PIX_FMT_NV12_Y_UV10;
      vd_fmt_dsc->out_pix_fmt = xv_fmt_to_pix_enum (out_fmt);
      vd_fmt_dsc->in_cs = map_cs (vd_fmt_dsc->in_pix_fmt);
      vd_fmt_dsc->out_cs = map_cs (vd_fmt_dsc->out_pix_fmt);
      vd_fmt_dsc->num_planes = 2;
      vd_fmt_dsc->num_components = 1;
      vd_fmt_dsc->bits_per_sample = 10;
      break;
    default:
      LOG_MESSAGE (LOG_LEVEL_ERROR, g_log_level, "Unsupported format");
      return 1;
      break;
  }

  return 0;
}

static void
get_fuctions (video_format_desc * fmt)
{

  if (fmt->bits_per_sample == 8) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, g_log_level, "Assigning 8 bit functions");
    fmt->vc_resample_fn = vertical_chroma_resample_u8;
    fmt->hc_resample_fn = horizontal_chroma_resample_u8;
    fmt->pack_yuv444_fn = pack_into_3sample_pixel_u8;
    fmt->vertical_scale_fn = vertical_scale_u8;
    fmt->horizontal_scale_fn = horizontal_scale_u8;
    fmt->unpack_yuv444_fn = unpack_into_yuv444_u8;
    fmt->csc_fn = csc_u8;
  } else if (fmt->bits_per_sample == 10) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, g_log_level, "Assigning 10 bit functions");
    fmt->vc_resample_fn = vertical_chroma_resample_u16;
    fmt->hc_resample_fn = horizontal_chroma_resample_u16;
    fmt->pack_yuv444_fn = pack_into_3sample_pixel_u16;
    fmt->vertical_scale_fn = vertical_scale_u16;
    fmt->horizontal_scale_fn = horizontal_scale_u16;
    fmt->unpack_yuv444_fn = unpack_into_yuv444_u16;
    fmt->csc_fn = csc_u16;
  }
}

static void
horizontal_downsample_core_u16 (U16 * in_buf, U32 in_width, U32 in_height,
    U16 * out_buf)
{
  U32 out_width = in_width / 2;

  for (int v = 0; v < in_height; v++) {
    for (int h = 0; h < in_width; h += 2) {
      /* We are doing the average to downsample */
      out_buf[v * out_width + (h / 2)] =
          (in_buf[(v * in_width) + h] + in_buf[(v * in_width) + (h + 1)]) / 2;
    }
  }

}

static void
horizontal_downsample_core_u8 (U8 * in_buf, U32 in_width, U32 in_height,
    U8 * out_buf)
{
  U32 out_width = in_width / 2;

  for (int v = 0; v < in_height; v++) {
    for (int h = 0; h < in_width; h += 2) {
      /* Apply filter (1/4, 1/2, 1/4) to scale down */
      if (h == 0) {
        out_buf[v * out_width + (h / 2)] =
            (in_buf[(v * in_width) + h] + 2 * in_buf[(v * in_width) + h] +
            in_buf[(v * in_width) + (h + 1)] + 2) / 4;
      } else {
        out_buf[v * out_width + (h / 2)] =
            (in_buf[(v * in_width) + (h - 1)] + 2 * in_buf[(v * in_width) + h] +
            in_buf[(v * in_width) + (h + 1)] + 2) / 4;
      }
    }
  }
}

static void
horizontal_upsample_core_u16 (U16 * in_buf, U32 in_width, U32 in_height,
    U16 * out_buf)
{
  U32 out_width = in_width * 2;
  U32 out_height = in_height;

  /* Fill output values with same as input at the factor of 2x (even co-ordinates) co-ordinate */
  for (int v = 0; v < in_height; v++) {
    for (int h = 0; h < in_width; h++) {
      out_buf[(v * out_width) + (h * 2)] = in_buf[(v * in_width) + h];
    }
  }

  /* Fill the missing values in output buffer */
  for (int v = 0; v < out_height; v++) {
    for (int h = 0; h < out_width; h++) {
      /* Check for odd co-ordinate places and fill the value with average of neighbours */
      if (h & 1) {
        /* We have only one neighbour at the end, so just copy the same value */
        if (h == (out_width - 1)) {
          out_buf[(v * out_width) + h] = out_buf[(v * out_width) + (h - 1)];
        } else {
          out_buf[(v * out_width) + h] =
              (out_buf[(v * out_width) + (h - 1)] + out_buf[(v * out_width) +
                  (h + 1)]) / 2;
        }
      }
    }
  }
}

static void
horizontal_upsample_core_u8 (U8 * in_buf, U32 in_width, U32 in_height,
    U8 * out_buf)
{
  U32 out_width = in_width * 2;
  U32 out_height = in_height;

  /* Fill the missing values in output buffer */
  for (int v = 0; v < out_height; v++) {
    for (int h = 0; h < out_width; h++) {
      /* Check for odd co-ordinate places and fill the value with average of neighbours */
      if (h & 1) {
        /* We have only one neighbour at the end, so just copy the same value */
        if (h == (out_width - 1)) {
          out_buf[(v * out_width) + h] = in_buf[(v * in_width) + (h - 1) / 2];
        } else {
          out_buf[(v * out_width) + h] =
              (in_buf[(v * in_width) + (h - 1) / 2] + in_buf[(v * in_width) +
                  (((h - 1) / 2) + 1)] + 1) / 2;
        }
      } else {
        out_buf[(v * out_width) + h] = in_buf[(v * in_width) + h / 2];
      }
    }
  }
}

static void
vertical_downsample_core_u16 (U16 * in_buf, U32 in_width, U32 in_height,
    U16 * out_buf)
{

  for (int h = 0; h < in_width; h++) {
    for (int v = 0; v < in_height; v += 2) {
      /* We are doing the average to downsample */
      out_buf[((v / 2) * in_width) + h] =
          (in_buf[(v * in_width) + h] + in_buf[((v + 1) * in_width) + h]) / 2;
    }
  }
}

static void
vertical_downsample_core_u8 (U8 * in_buf, U32 in_width, U32 in_height,
    U8 * out_buf)
{

  for (int h = 0; h < in_width; h++) {
    for (int v = 0; v < in_height; v += 2) {
      /* Apply filter (1/4, 1/2, 1/4) to scale down */
      if (v == 0) {
        out_buf[((v / 2) * in_width) + h] =
            (in_buf[((v / 2) * in_width) + h] + 2 * in_buf[(v * in_width) + h] +
            in_buf[((v + 1) * in_width) + h] + 2) / 4;
      } else {
        out_buf[((v / 2) * in_width) + h] =
            (in_buf[((v - 1) * in_width) + h] + 2 * in_buf[(v * in_width) + h] +
            in_buf[((v + 1) * in_width) + h] + 2) / 4;
      }
    }
  }
}

static void
vertical_upsample_core_u16 (U16 * in_buf, U32 in_width, U32 in_height,
    U16 * out_buf)
{
  U32 out_width = in_width;
  U32 out_height = in_height * 2;

  /* Fill output values with same as input at the factor of 2x co-ordinate */
  for (int h = 0; h < in_width; h++) {
    for (int v = 0; v < in_height; v++) {
      out_buf[(v * 2 * out_width) + h] = in_buf[(v * in_width) + h];
    }
  }
  /* Fill the missing values in output buffer */
  for (int h = 0; h < out_width; h++) {
    for (int v = 0; v < out_height; v++) {
      if (v & 1) {
        if (v == (out_height - 1)) {
          out_buf[v * out_width + h] = out_buf[((v - 1) * out_width) + h];
        } else {
          out_buf[v * out_width + h] =
              (out_buf[((v - 1) * out_width) + h] + out_buf[((v +
                          1) * out_width) + h]) / 2;
        }
      }
    }
  }
}

static void
vertical_upsample_core_u8 (U8 * in_buf, U32 in_width, U32 in_height,
    U8 * out_buf)
{
  U32 out_width = in_width;
  U32 out_height = in_height * 2;

  for (int h = 0; h < out_width; h++) {
    for (int v = 0; v < out_height; v++) {
      if (v & 1) {
        if (v == (out_height - 1)) {
          out_buf[v * out_width + h] = in_buf[(((v - 1) / 2) * in_width) + h];
        } else {
          out_buf[v * out_width + h] =
              (in_buf[(((v - 1) / 2) * in_width) + h] +
              in_buf[(((v - 1) / 2 + 1) * in_width) + h] + 1) / 2;
        }
      } else {
        out_buf[v * out_width + h] = in_buf[(v / 2) * in_width + h];
      }
    }
  }
}

void
vertical_chroma_resample_u16 (void *in_buf, U32 in_width, U32 in_height,
    bool passthru, video_format_desc * fmt, void **out_buf,
    MULTI_SCALER_DESC_STRUCT * desc)
{
  U16 *in_buf_16 = (U16 *) in_buf;
  U16 *temp_buf = NULL, *temp_buf_u = NULL, *temp_buf_v = NULL;
  U16 *in_buf_uv = NULL;
  U32 offset_y = in_width * in_height;
  U32 offset_u;
  U16 *vc_out_buf = NULL;

  if (passthru) {
    *out_buf = in_buf;
    return;
  }
  if (VD_PIX_FMT_NV12_Y_UV10 == fmt->curr_pix_fmt) {    /* Input is of 420, converting it to 422 */
    LOG_MESSAGE (LOG_LEVEL_DEBUG, g_log_level, "Converting 420 to 422");
    offset_u = (in_width * in_height) / 4;
    /* Allocate one temporary buffer for storing frame with de-interleaved U and V */
    temp_buf = (U16 *) mem_pool_get_free_mem (&fmt->pool);
    I32 uv_width = in_width / 2;
    I32 uv_height = in_height / 2;

    temp_buf_u = temp_buf + offset_y;
    temp_buf_v = temp_buf + offset_y + offset_u;
    in_buf_uv = in_buf_16 + offset_y;

    /* Copy the luma part */
    memcpy (temp_buf, in_buf_16, (in_width * in_height * sizeof (U16)));


    for (I32 v = 0; v < uv_height; v++) {
      for (I32 h = 0; h < uv_width; h++) {
        /* Copy UV..UV...  to UU....VV.... */
        temp_buf_u[v * uv_width + h] = in_buf_uv[(v * in_width) + (h * 2)];
        temp_buf_v[v * uv_width + h] = in_buf_uv[(v * in_width) + (h * 2) + 1];
      }
    }

    /* New release the input buffer */
    mem_pool_release_mem (&fmt->pool, in_buf);

    vc_out_buf = (U16 *) mem_pool_get_free_mem (&fmt->pool);
    memcpy (vc_out_buf, temp_buf, (in_width * in_height * sizeof (U16)));

    /* Interpolate U by factor 2 */
    vertical_upsample_core_u16 (temp_buf_u, (in_width / 2), (in_height / 2),
        (vc_out_buf + offset_y));
    /* Interpolate v by factor 2 */
    offset_u = ((in_width / 2) * in_height);
    vertical_upsample_core_u16 (temp_buf_v, (in_width / 2), (in_height / 2),
        (vc_out_buf + offset_y + offset_u));

    mem_pool_release_mem (&fmt->pool, temp_buf);
    *out_buf = vc_out_buf;
    fmt->curr_pix_fmt = VD_PIX_FMT_YUV422;

  } else if (VD_PIX_FMT_YUV422 == fmt->curr_pix_fmt) {  /* Input is 422, converting it to 420 */
    U32 in_offset_u = ((in_width / 2) * in_height);
    U32 out_offset_u = ((in_width / 2) * (in_height / 2));
    I32 uv_width = in_width / 2;
    I32 uv_height = in_height / 2;
    U16 *temp_buf_u;
    U16 *temp_buf_v;
    U16 *out_buf_uv;
    LOG_MESSAGE (LOG_LEVEL_DEBUG, g_log_level, "Converting 422 to 420");

    /* Allocate one temporary buffer for storing de-interleaved U and V */
    temp_buf = (U16 *) mem_pool_get_free_mem (&fmt->pool);

    /* Copy the luma part */
    memcpy (temp_buf, in_buf_16, (in_width * in_height * sizeof (U16)));

    /* New release the input buffer */
    mem_pool_release_mem (&fmt->pool, in_buf);

    vc_out_buf = (U16 *) mem_pool_get_free_mem (&fmt->pool);

    memcpy (vc_out_buf, temp_buf, (in_width * in_height * sizeof (U16)));

    /* Downsample U by factor of 2 */
    vertical_downsample_core_u16 ((in_buf_16 + offset_y), (in_width / 2),
        in_height, (temp_buf + offset_y));

    /* Downsample V by factor of 2 */
    vertical_downsample_core_u16 ((in_buf_16 + offset_y + in_offset_u),
        (in_width / 2), in_height, (temp_buf + offset_y + out_offset_u));

    temp_buf_u = temp_buf + offset_y;
    temp_buf_v = temp_buf + offset_y + out_offset_u;
    out_buf_uv = vc_out_buf + offset_y;

    /* Interleave U and V to get NV12 */
    for (int v = 0; v < uv_height; v++) {
      for (int h = 0; h < uv_width; h++) {
        /* Copy U....V.... to UV...... */
        out_buf_uv[(v * in_width) + (h * 2)] = temp_buf_u[v * uv_width + h];
        out_buf_uv[(v * in_width) + (h * 2) + 1] = temp_buf_v[v * uv_width + h];
      }                         /* End of v */
    }                           /* End of h */
    *out_buf = vc_out_buf;
    fmt->curr_pix_fmt = VD_PIX_FMT_NV12_Y_UV8;
    mem_pool_release_mem (&fmt->pool, temp_buf);
  }
}


void
vertical_chroma_resample_u8 (void *in_buf, U32 in_width, U32 in_height,
    bool passthru, video_format_desc * fmt, void **out_buf,
    MULTI_SCALER_DESC_STRUCT * desc)
{
  U8 *in_buf_8 = (U8 *) in_buf;
  U8 *temp_buf = NULL, *temp_buf_u = NULL, *temp_buf_v = NULL;
  U8 *in_buf_uv = NULL;
  U32 offset_y = in_width * in_height;
  U32 offset_u;
  U8 *vc_out_buf = NULL;

  if (passthru) {
    *out_buf = in_buf;
    return;
  }

  if (VD_PIX_FMT_NV12_Y_UV8 == fmt->curr_pix_fmt || VD_PIX_FMT_I420_Y_U_V8 == fmt->curr_pix_fmt) {      /* Input is of 420, converting it to 422 */
    LOG_MESSAGE (LOG_LEVEL_DEBUG, g_log_level, "Converting 420(NV12) to 422");
    offset_u = (in_width * in_height) / 4;
    /* Allocate one temporary buffer for storing de-interleaved U and V */
    temp_buf = (U8 *) mem_pool_get_free_mem (&fmt->pool);
    I32 uv_width = in_width / 2;
    I32 uv_height = in_height / 2;

    temp_buf_u = temp_buf + offset_y;
    temp_buf_v = temp_buf + offset_y + offset_u;
    in_buf_uv = in_buf_8 + offset_y;

    /* Copy the luma part */
    memcpy (temp_buf, in_buf_8, (in_width * in_height));

    if (VD_PIX_FMT_NV12_Y_UV8 == fmt->curr_pix_fmt) {
      for (int v = 0; v < uv_height; v++) {
        for (int h = 0; h < uv_width; h++) {
          /* Copy UV..UV...  to UU....VV.... */
          temp_buf_u[v * uv_width + h] = in_buf_uv[(v * in_width) + (h * 2)];
          temp_buf_v[v * uv_width + h] =
              in_buf_uv[(v * in_width) + (h * 2) + 1];
        }
      }
    } else if (VD_PIX_FMT_I420_Y_U_V8 == fmt->curr_pix_fmt) {
      memcpy (temp_buf_u, in_buf_uv, (in_width * in_height * 0.5));
    }

    /* New release the input buffer */
    mem_pool_release_mem (&fmt->pool, in_buf);
    vc_out_buf = (U8 *) mem_pool_get_free_mem (&fmt->pool);

    memcpy (vc_out_buf, temp_buf, (in_width * in_height));

    /* Interpolate U by factor 2 */
    vertical_upsample_core_u8 (temp_buf_u, (in_width / 2), (in_height / 2),
        (vc_out_buf + offset_y));
    /* Interpolate v by factor 2 */
    offset_u = ((in_width / 2) * in_height);
    vertical_upsample_core_u8 (temp_buf_v, (in_width / 2), (in_height / 2),
        (vc_out_buf + offset_y + offset_u));

    mem_pool_release_mem (&fmt->pool, temp_buf);
    *out_buf = vc_out_buf;
    fmt->curr_pix_fmt = VD_PIX_FMT_YUV422;

  } else if (VD_PIX_FMT_YUV422 == fmt->curr_pix_fmt) {  /* Input is 422, converting it to 420 */
    U32 in_offset_u = ((in_width / 2) * in_height);
    U32 out_offset_u = ((in_width / 2) * (in_height / 2));
    I32 uv_width = in_width / 2;
    I32 uv_height = in_height / 2;
    U8 *temp_buf_u;
    U8 *temp_buf_v;
    U8 *out_buf_uv;
    LOG_MESSAGE (LOG_LEVEL_DEBUG, g_log_level, "Converting 422 to 420");

    /* Allocate one temporary buffer for storing de-interleaved U and V */
    temp_buf = (U8 *) mem_pool_get_free_mem (&fmt->pool);

    /* Copy the luma part */
    memcpy (temp_buf, in_buf_8, (in_width * in_height));

    /* New release the input buffer */
    mem_pool_release_mem (&fmt->pool, in_buf);

    vc_out_buf = (U8 *) mem_pool_get_free_mem (&fmt->pool);

    memcpy (vc_out_buf, temp_buf, (in_width * in_height));

    /* Downsample U by factor of 2 */
    vertical_downsample_core_u8 ((in_buf_8 + offset_y), (in_width / 2),
        in_height, (temp_buf + offset_y));

    /* Downsample V by factor of 2 */
    vertical_downsample_core_u8 ((in_buf_8 + offset_y + in_offset_u),
        (in_width / 2), in_height, (temp_buf + offset_y + out_offset_u));

    temp_buf_u = temp_buf + offset_y;
    temp_buf_v = temp_buf + offset_y + out_offset_u;
    out_buf_uv = vc_out_buf + offset_y;

    if (VD_PIX_FMT_NV12_Y_UV8 == fmt->out_pix_fmt) {
      /* Interleave U and V to get NV12 */
      for (int v = 0; v < uv_height; v++) {
        for (int h = 0; h < uv_width; h++) {
          /* Copy U....V.... to UV...... */
          out_buf_uv[(v * in_width) + (h * 2)] = temp_buf_u[v * uv_width + h];
          out_buf_uv[(v * in_width) + (h * 2) + 1] =
              temp_buf_v[v * uv_width + h];
        }                       /* End of v */
      }                         /* End of h */
      *out_buf = vc_out_buf;
      fmt->curr_pix_fmt = VD_PIX_FMT_NV12_Y_UV8;
      mem_pool_release_mem (&fmt->pool, temp_buf);
    } else if (VD_PIX_FMT_I420_Y_U_V8 == fmt->out_pix_fmt) {
      memcpy (out_buf_uv, temp_buf_u, (in_width * in_height * 0.5));
      *out_buf = vc_out_buf;
      fmt->curr_pix_fmt = VD_PIX_FMT_I420_Y_U_V8;
      mem_pool_release_mem (&fmt->pool, temp_buf);
    }
  }
}

void
horizontal_chroma_resample_u16 (void *in_buf, U32 in_width, U32 in_height,
    bool passthru, video_format_desc * fmt, void **out_buf)
{
  U16 *in_buf_16 = (U16 *) in_buf;
  U32 offset_y = in_width * in_height;
  U32 in_offset_u;
  U32 out_offset_u;
  U16 *hc_out_buf = NULL;

  if (passthru) {
    *out_buf = in_buf;
    return;
  }

  if (fmt->curr_pix_fmt == VD_PIX_FMT_YUV422) { /* Input is YUV422, upsampling to YUV444  */
    LOG_MESSAGE (LOG_LEVEL_DEBUG, g_log_level, "Converting 422 to 444");
    in_offset_u = (in_width / 2) * (in_height);
    out_offset_u = in_width * in_height;

    hc_out_buf = (U16 *) mem_pool_get_free_mem (&fmt->pool);

    /* Copy the lumar part */
    memcpy (hc_out_buf, in_buf_16, (in_width * in_height * sizeof (U16)));
    /* Upsample U horizontally */
    horizontal_upsample_core_u16 (in_buf_16 + offset_y, (in_width / 2),
        in_height, (hc_out_buf + offset_y));
    /* Upsample V horizontally */
    horizontal_upsample_core_u16 ((in_buf_16 + offset_y + in_offset_u),
        (in_width / 2), in_height, (hc_out_buf + offset_y + out_offset_u));

    *out_buf = hc_out_buf;
    fmt->curr_pix_fmt = VD_PIX_FMT_YUV444;
    mem_pool_release_mem (&fmt->pool, in_buf);

  } else if (VD_PIX_FMT_YUV444 == fmt->curr_pix_fmt) {  /* Input is YUV444, downsampling to YUV422  */
    in_offset_u = in_width * in_height;
    out_offset_u = (in_width / 2) * (in_height);
    LOG_MESSAGE (LOG_LEVEL_DEBUG, g_log_level, "Converting 444 to 422");
    hc_out_buf = (U16 *) mem_pool_get_free_mem (&fmt->pool);

    /* Copy the luma part */
    memcpy (hc_out_buf, in_buf_16, (in_width * in_height * sizeof (U16)));

    /* Upsample U horizontally */
    horizontal_downsample_core_u16 ((in_buf_16 + offset_y), in_width, in_height,
        (hc_out_buf + offset_y));
    /* Upsample V horizontally */
    horizontal_downsample_core_u16 ((in_buf_16 + offset_y + in_offset_u),
        in_width, in_height, (hc_out_buf + offset_y + out_offset_u));
    *out_buf = hc_out_buf;
    fmt->curr_pix_fmt = VD_PIX_FMT_YUV422;
    mem_pool_release_mem (&fmt->pool, in_buf);
  }
}

void
horizontal_chroma_resample_u8 (void *in_buf, U32 in_width, U32 in_height,
    bool passthru, video_format_desc * fmt, void **out_buf)
{
  U8 *in_buf_8 = (U8 *) in_buf;
  U32 offset_y = in_width * in_height;
  U32 in_offset_u;
  U32 out_offset_u;
  U8 *hc_out_buf = NULL;

  if (passthru) {
    *out_buf = in_buf;
    return;
  }

  if (fmt->curr_pix_fmt == VD_PIX_FMT_YUV422) { /* Input is YUV422, upsampling to YUV444  */
    LOG_MESSAGE (LOG_LEVEL_DEBUG, g_log_level, "Converting 422 to 444");
    in_offset_u = (in_width / 2) * (in_height);
    out_offset_u = in_width * in_height;

    hc_out_buf = (U8 *) mem_pool_get_free_mem (&fmt->pool);

    /* Copy the lumar part */
    memcpy (hc_out_buf, in_buf_8, (in_width * in_height));
    /* Upsample U horizontally */
    horizontal_upsample_core_u8 (in_buf_8 + offset_y, (in_width / 2), in_height,
        (hc_out_buf + offset_y));
    /* Upsample V horizontally */
    horizontal_upsample_core_u8 ((in_buf_8 + offset_y + in_offset_u),
        (in_width / 2), in_height, (hc_out_buf + offset_y + out_offset_u));

    *out_buf = hc_out_buf;
    fmt->curr_pix_fmt = VD_PIX_FMT_YUV444;
    mem_pool_release_mem (&fmt->pool, in_buf);

  } else if (VD_PIX_FMT_YUV444 == fmt->curr_pix_fmt) {  /* Input is YUV444, downsampling to YUV422  */
    in_offset_u = in_width * in_height;
    out_offset_u = (in_width / 2) * (in_height);
    LOG_MESSAGE (LOG_LEVEL_DEBUG, g_log_level, "Converting 444 to 422");
    hc_out_buf = (U8 *) mem_pool_get_free_mem (&fmt->pool);

    /* Copy the luma part */
    memcpy (hc_out_buf, in_buf_8, (in_width * in_height));

    /* Upsample U horizontally */
    horizontal_downsample_core_u8 ((in_buf_8 + offset_y), in_width, in_height,
        (hc_out_buf + offset_y));
    /* Upsample V horizontally */
    horizontal_downsample_core_u8 ((in_buf_8 + offset_y + in_offset_u),
        in_width, in_height, (hc_out_buf + offset_y + out_offset_u));
    *out_buf = hc_out_buf;
    fmt->curr_pix_fmt = VD_PIX_FMT_YUV422;
    mem_pool_release_mem (&fmt->pool, in_buf);
  }
}

void
unpack_into_yuv444_u16 (void *in_buf, U32 in_width, U32 in_height,
    bool passthru, video_format_desc * fmt, void **out_buf)
{
  U16 *in_buf_16 = (U16 *) in_buf;
  U32 offset_y = in_width * in_height;
  U32 offset_u = in_width * in_height * 2;
  U16 *out_buf_u;
  U16 *out_buf_v;
  U16 *unpack_out_buf = NULL;

  if (passthru) {
    *out_buf = in_buf;
    return;
  }
  LOG_MESSAGE (LOG_LEVEL_DEBUG, g_log_level, "Lets un-pack");
  unpack_out_buf = (U16 *) mem_pool_get_free_mem (&fmt->pool);

  out_buf_u = unpack_out_buf + offset_y;
  out_buf_v = unpack_out_buf + offset_u;


  for (I32 v = 0; v < in_height; v++) { /* Runs for height */
    for (I32 h = 0; h < in_width; h++) {        /* Runs for width */
      /* Copy Y, U and V into unpacked format */
      /* Copy from YUV... Y..U..V.. */
      unpack_out_buf[v * in_width + h] =
          in_buf_16[(v * in_width * 3) + (h * 3)];
      out_buf_u[v * in_width + h] = in_buf_16[(v * in_width * 3) + (h * 3 + 1)];
      out_buf_v[v * in_width + h] = in_buf_16[(v * in_width * 3) + (h * 3 + 2)];
    }
  }
  *out_buf = unpack_out_buf;
  fmt->curr_pix_fmt = VD_PIX_FMT_YUV444;
  mem_pool_release_mem (&fmt->pool, in_buf);
}

void
unpack_into_yuv444_u8 (void *in_buf, U32 in_width, U32 in_height, bool passthru,
    video_format_desc * fmt, void **out_buf)
{
  U8 *in_buf_8 = (U8 *) in_buf;
  U32 offset_y = in_width * in_height;
  U32 offset_u = in_width * in_height * 2;
  U8 *out_buf_u;
  U8 *out_buf_v;
  U8 *unpack_out_buf = NULL;

  if (passthru) {
    *out_buf = in_buf;
    return;
  }
  LOG_MESSAGE (LOG_LEVEL_DEBUG, g_log_level, "Lets un-pack");
  unpack_out_buf = (U8 *) mem_pool_get_free_mem (&fmt->pool);

  out_buf_u = unpack_out_buf + offset_y;
  out_buf_v = unpack_out_buf + offset_u;

  for (I32 v = 0; v < in_height; v++) { /* Runs for height */
    for (I32 h = 0; h < in_width; h++) {        /* Runs for width */
      /* Copy Y, U and V into unpacked format */
      /* Copy from YUV... Y..U..V.. */
      unpack_out_buf[v * in_width + h] = in_buf_8[(v * in_width * 3) + (h * 3)];
      out_buf_u[v * in_width + h] = in_buf_8[(v * in_width * 3) + (h * 3 + 1)];
      out_buf_v[v * in_width + h] = in_buf_8[(v * in_width * 3) + (h * 3 + 2)];
    }
  }
  *out_buf = unpack_out_buf;
  fmt->curr_pix_fmt = VD_PIX_FMT_YUV444;
  mem_pool_release_mem (&fmt->pool, in_buf);
}

void
pack_into_3sample_pixel_u16 (void *in_buf, U32 in_width, U32 in_height,
    bool passthru, video_format_desc * fmt, void **out_buf)
{
  U16 *in_buf_16 = (U16 *) in_buf;
  U32 offset_y = in_width * in_height;
  U32 offset_u = in_width * in_height * 2;
  U16 *in_buf_u = in_buf_16 + offset_y;
  U16 *in_buf_v = in_buf_16 + offset_u;
  U16 *pack_out_buf = NULL;

  if (passthru) {
    *out_buf = in_buf;
    return;
  }

  LOG_MESSAGE (LOG_LEVEL_DEBUG, g_log_level, "Lets pack");

  pack_out_buf = (U16 *) mem_pool_get_free_mem (&fmt->pool);

  for (I32 v = 0; v < in_height; v++) { /* Runs for height */
    for (I32 h = 0; h < in_width; h++) {        /* Runs for width which will be 3 times */
      /* Copy Y, U and V into packet format */
      /* Copy Y..U..V... to YUV.... */
      pack_out_buf[(v * in_width * 3) + (h * 3)] =
          in_buf_16[(v * in_width) + h];
      pack_out_buf[(v * in_width * 3) + (h * 3 + 1)] =
          in_buf_u[(v * in_width) + h];
      pack_out_buf[(v * in_width * 3) + (h * 3 + 2)] =
          in_buf_v[(v * in_width) + h];
    }
  }
  *out_buf = pack_out_buf;
  mem_pool_release_mem (&fmt->pool, in_buf);

}

void
pack_into_3sample_pixel_u8 (void *in_buf, U32 in_width, U32 in_height,
    bool passthru, video_format_desc * fmt, void **out_buf)
{
  U8 *in_buf_8 = (U8 *) in_buf;
  U32 offset_y = in_width * in_height;
  U32 offset_u = in_width * in_height * 2;
  U8 *in_buf_u = in_buf_8 + offset_y;
  U8 *in_buf_v = in_buf_8 + offset_u;
  U8 *pack_out_buf = NULL;

  if (passthru) {
    *out_buf = in_buf;
    return;
  }

  LOG_MESSAGE (LOG_LEVEL_DEBUG, g_log_level, "Lets pack");

  pack_out_buf = (U8 *) mem_pool_get_free_mem (&fmt->pool);

  for (I32 v = 0; v < in_height; v++) { /* Runs for height */
    for (I32 h = 0; h < in_width; h++) {        /* Runs for width which will be 3 times */
      /* Copy Y, U and V into packet format */
      /* Copy Y..U..V... to YUV.... */
      pack_out_buf[(v * in_width * 3) + (h * 3)] = in_buf_8[(v * in_width) + h];
      pack_out_buf[(v * in_width * 3) + (h * 3 + 1)] =
          in_buf_u[(v * in_width) + h];
      pack_out_buf[(v * in_width * 3) + (h * 3 + 2)] =
          in_buf_v[(v * in_width) + h];
    }
  }
  *out_buf = pack_out_buf;
  mem_pool_release_mem (&fmt->pool, in_buf);
}

#ifdef ENABLE_PPE_SUPPORT
static void
preprocess (U8 * in_buf, U32 width, U32 height, bool passthru,
    video_format_desc * fmt, U8 ** out_buf)
{
  U8 *temp_buf = NULL;
  if ((fmt->out_pix_fmt != VD_PIX_FMT_RGB_RGB8 &&
          fmt->out_pix_fmt != VD_PIX_FMT_BGR_BGR8) || passthru) {
    LOG_MESSAGE (LOG_LEVEL_INFO, g_log_level, "Returning without preprocess");
    *out_buf = in_buf;
    return;
  }
  temp_buf = (U8 *) mem_pool_get_free_mem (&fmt->pool);

  for (U32 v = 0; v < height; v++) {
    for (U32 h = 0; h < width; h++) {
      for (U32 c = 0; c < VD_MAX_COMPONENTS; c++) {
        I32 prod = 0, a, b;
        U8 x;

        if (VD_PIX_FMT_RGB_RGB8 == fmt->out_pix_fmt) {
          x = in_buf[v * (width * VD_MAX_COMPONENTS) + h * VD_MAX_COMPONENTS +
              c];
          a = fmt->alpha[c];
          b = fmt->beta[c];
          prod = (x - a) * b;
        } else if (VD_PIX_FMT_BGR_BGR8 == fmt->out_pix_fmt) {
          x = in_buf[v * (width * VD_MAX_COMPONENTS) + h * VD_MAX_COMPONENTS +
              c];
          a = fmt->alpha[2 - c];
          b = fmt->beta[2 - c];
          prod = (x - a) * b;
        }
        prod = prod >> 16;
        temp_buf[v * (width * VD_MAX_COMPONENTS) + h * VD_MAX_COMPONENTS + c] =
            prod;
      }
    }
  }

  *out_buf = temp_buf;
  mem_pool_release_mem (&fmt->pool, in_buf);
}
#endif

void
csc_u16 (void *in_buf, U32 width, U32 height, bool passthru,
    video_format_desc * fmt, void **out_buf)
{
  U16 *in_buf_16 = (U16 *) in_buf;
  U16 *cs_out_buf = NULL;
  if (passthru) {
    *out_buf = in_buf;
    return;
  }
  cs_out_buf = (U16 *) mem_pool_get_free_mem (&fmt->pool);
  for (I32 v = 0; v < height; v++) {
    for (I32 h = 0; h < width * 3; h += 3) {
      U16 R_Y, G_U, B_V;

      R_Y = in_buf_16[(v * width * 3) + h];
      G_U = in_buf_16[(v * width * 3) + h + 1];
      B_V = in_buf_16[(v * width * 3) + h + 2];

      if (fmt->in_cs != VD_CS_RGB) {
        I32 R, G, B;
        I32 Cr = B_V - (1 << (10 - 1)); // TODO : Fix for 10bit
        I32 Cb = G_U - (1 << (10 - 1));

        R = (I32) R_Y + (((I32) Cr * 1733) >> 10);
        G = (I32) R_Y - (((I32) Cb * 404 + (I32) Cr * 595) >> 10);
        B = (I32) R_Y + (((I32) Cb * 2081) >> 10);

        cs_out_buf[(v * width * 3) + h] = MAX (MIN (R, 255), 0);
        cs_out_buf[(v * width * 3) + h + 1] = MAX (MIN (G, 255), 0);
        cs_out_buf[(v * width * 3) + h + 2] = MAX (MIN (B, 255), 0);
      } else {
        I32 Y, U, V;
        Y = (306 * (I32) R_Y + 601 * (I32) G_U + 117 * (I32) B_V) >> 10;
        U = (1 << (10 - 1)) + ((((I32) B_V - (I32) Y) * 504) >> 10);
        V = (1 << (10 - 1)) + ((((I32) R_Y - (I32) Y) * 898) >> 10);

        cs_out_buf[(v * width * 3) + h] = MAX (MIN (Y, 255), 0);
        cs_out_buf[(v * width * 3) + h + 1] = MAX (MIN (U, 255), 0);
        cs_out_buf[(v * width * 3) + h + 2] = MAX (MIN (V, 255), 0);
      }
    }
  }
  *out_buf = cs_out_buf;
  mem_pool_release_mem (&fmt->pool, in_buf);
}

void
csc_u8 (void *in_buf, U32 width, U32 height, bool passthru,
    video_format_desc * fmt, void **out_buf)
{
  U8 *in_buf_8 = (U8 *) in_buf;
  U8 *cs_out_buf = NULL;
  if (passthru) {
    *out_buf = in_buf;
    return;
  }

  cs_out_buf = (U8 *) mem_pool_get_free_mem (&fmt->pool);
  for (I32 v = 0; v < height; v++) {
    for (I32 h = 0; h < width * 3; h += 3) {
      U8 R_Y, G_U, B_V;

      if (fmt->in_pix_fmt == VD_PIX_FMT_BGR_BGR8) {
        B_V = in_buf_8[(v * width * 3) + h];
        G_U = in_buf_8[(v * width * 3) + h + 1];
        R_Y = in_buf_8[(v * width * 3) + h + 2];
      } else {
        R_Y = in_buf_8[(v * width * 3) + h];
        G_U = in_buf_8[(v * width * 3) + h + 1];
        B_V = in_buf_8[(v * width * 3) + h + 2];
      }

      if (fmt->in_cs != VD_CS_RGB) {
        I32 R, G, B;
        I32 Cr = B_V - (1 << (8 - 1));  // TODO : Fix for 10bit
        I32 Cb = G_U - (1 << (8 - 1));

        R = (I32) R_Y + (((I32) Cr * 1733) >> 10);
        G = (I32) R_Y - (((I32) Cb * 404 + (I32) Cr * 595) >> 10);
        B = (I32) R_Y + (((I32) Cb * 2081) >> 10);

        if (fmt->out_pix_fmt == VD_PIX_FMT_RGB_RGB8) {
          cs_out_buf[(v * width * 3) + h] = MAX (MIN (R, 255), 0);
          cs_out_buf[(v * width * 3) + h + 1] = MAX (MIN (G, 255), 0);
          cs_out_buf[(v * width * 3) + h + 2] = MAX (MIN (B, 255), 0);
        } else if (fmt->out_pix_fmt == VD_PIX_FMT_BGR_BGR8) {
          cs_out_buf[(v * width * 3) + h] = MAX (MIN (B, 255), 0);
          cs_out_buf[(v * width * 3) + h + 1] = MAX (MIN (G, 255), 0);
          cs_out_buf[(v * width * 3) + h + 2] = MAX (MIN (R, 255), 0);
        }
      } else {
        I32 Y, U, V;
        Y = (306 * (I32) R_Y + 601 * (I32) G_U + 117 * (I32) B_V) >> 10;
        U = 128 + ((((I32) B_V - (I32) Y) * 504) >> 10);
        V = 128 + ((((I32) R_Y - (I32) Y) * 898) >> 10);

        cs_out_buf[(v * width * 3) + h] = MAX (MIN (Y, 255), 0);
        cs_out_buf[(v * width * 3) + h + 1] = MAX (MIN (U, 255), 0);
        cs_out_buf[(v * width * 3) + h + 2] = MAX (MIN (V, 255), 0);
      }
    }
  }
  *out_buf = cs_out_buf;
  mem_pool_release_mem (&fmt->pool, in_buf);
}

static void
convert_format (void *in_buf, U32 in_width, U32 in_height,
    video_format_desc * fmt, void **out_buf)
{

  if ((fmt->in_pix_fmt == VD_PIX_FMT_RGB_RGB8
          && fmt->out_pix_fmt == VD_PIX_FMT_BGR_BGR8)
      || (fmt->in_pix_fmt == VD_PIX_FMT_BGR_BGR8
          && fmt->out_pix_fmt == VD_PIX_FMT_RGB_RGB8)) {
    U8 *temp_in_buf = (U8 *) in_buf;
    U8 temp_value;
    for (U32 v = 0; v < in_height; v++) {
      for (U32 h = 0; h < in_width; h++) {
        temp_value = temp_in_buf[v * (in_width * 3) + h * 3];
        temp_in_buf[v * (in_width * 3) + h * 3] =
            temp_in_buf[v * (in_width * 3) + h * 3 + 2];
        temp_in_buf[v * (in_width * 3) + h * 3 + 2] = temp_value;
      }
    }
    *out_buf = in_buf;
  } else {
    *out_buf = in_buf;
  }
}

int
v_multi_scaler_sw (U32 num_outs, MULTI_SCALER_DESC_STRUCT * desc_start,
    bool need_preprocess, VvasLogLevel log_level)
{
  U32 in_width, in_height, out_width, out_height;
  U32 out_stride = 0;
  void *in_buf = NULL, *v_scale_out = NULL, *h_scale_out = NULL, *out_buf1 =
      NULL, *out_buf2 = NULL;
  void *vc_out_buf = NULL, *hc_out_buf = NULL, *unpack_out = NULL, *cs_out =
      NULL;
  void *pre_proc_out = NULL, *format_out = NULL;
#ifdef DUMP_OUTPUT
  I32 out_fd_f = 0;
  char out_filename[2048];
  ssize_t ret = 0;
#endif
  bool passthru_vcr_up = false, passthru_hcr_up = false, passthru_hcr_down =
      false;
#ifdef ENABLE_PPE_SUPPORT
  bool passthru_preproc = !need_preprocess;
#endif
  bool passthru_vcr_down = false, passthru_pack = false, passthru_unpack =
      false, passthru_csc = false;
  video_format_desc vd_fmt_dsc;
  U8 *scale_input_buf = NULL;
  void *in_pix_array;
  U8 num_outputs = num_outs;
  MULTI_SCALER_DESC_STRUCT *desc = desc_start;

  g_log_level = log_level;

  while (num_outputs) {
    U8 *out_buf_temp[3];
    out_buf_temp[0] = (U8 *) desc->msc_dstImgBuf0;
    out_buf_temp[1] = (U8 *) desc->msc_dstImgBuf1;
    out_buf_temp[2] = (U8 *) desc->msc_dstImgBuf2;

    in_width = desc->msc_widthIn;
    in_height = desc->msc_heightIn;
    out_width = desc->msc_widthOut;
    out_height = desc->msc_heightOut;
    out_stride = desc->msc_strideOut;

    /* Populate input and output formats based on input descriptor */
    if (populate_video_format_info (desc->msc_inPixelFmt, desc->msc_outPixelFmt,
            &vd_fmt_dsc)) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, g_log_level, "Unsupported video format");
      return -1;
    }

    LOG_MESSAGE (LOG_LEVEL_INFO, g_log_level,
        "In Pix format : %s Out Pix format : %s",
        format_enum_to_str (vd_fmt_dsc.in_pix_fmt),
        format_enum_to_str (vd_fmt_dsc.out_pix_fmt));

    /* Few stages of scaling may need to be skipped based on the input and 
     * output formats and their color spaces */
    /* Vertical chroma up sampling must only be done for YUV 420 formats */
    passthru_vcr_up = (vd_fmt_dsc.in_pix_fmt == VD_PIX_FMT_NV12_Y_UV8
        || vd_fmt_dsc.in_pix_fmt == VD_PIX_FMT_NV12_Y_UV10
        || vd_fmt_dsc.in_pix_fmt == VD_PIX_FMT_I420_Y_U_V8) ? false : true;

    /* Horizontal chroma up sampling must only be done for YUV 420 and 422 
     * formats */
    passthru_hcr_up = (vd_fmt_dsc.in_pix_fmt == VD_PIX_FMT_NV12_Y_UV8
        || vd_fmt_dsc.in_pix_fmt == VD_PIX_FMT_NV12_Y_UV10
        || vd_fmt_dsc.in_pix_fmt == VD_PIX_FMT_I420_Y_U_V8) ? false : true;

    passthru_hcr_down = (vd_fmt_dsc.out_pix_fmt == VD_PIX_FMT_NV12_Y_UV8
        || vd_fmt_dsc.out_pix_fmt == VD_PIX_FMT_NV12_Y_UV10
        || vd_fmt_dsc.out_pix_fmt == VD_PIX_FMT_I420_Y_U_V8) ? false : true;
    passthru_vcr_down = (vd_fmt_dsc.out_pix_fmt == VD_PIX_FMT_NV12_Y_UV8
        || vd_fmt_dsc.out_pix_fmt == VD_PIX_FMT_NV12_Y_UV10
        || vd_fmt_dsc.out_pix_fmt == VD_PIX_FMT_I420_Y_U_V8) ? false : true;

    passthru_pack = (vd_fmt_dsc.in_cs != VD_CS_RGB) ? false : true;
    passthru_unpack = (vd_fmt_dsc.out_cs != VD_CS_RGB) ? false : true;

    passthru_csc = (vd_fmt_dsc.in_cs == VD_CS_RGB
        && vd_fmt_dsc.out_cs != VD_CS_RGB) || (vd_fmt_dsc.in_cs != VD_CS_RGB
        && vd_fmt_dsc.out_cs == VD_CS_RGB) ? false : true;

    if ((vd_fmt_dsc.in_pix_fmt == vd_fmt_dsc.out_pix_fmt)
        && (in_width == out_width)
        && (in_height == out_height)) {
      passthru_vcr_up = true;
      passthru_hcr_up = true;
      passthru_hcr_down = true;
      passthru_vcr_down = true;
      passthru_pack = true;
      passthru_unpack = true;
      passthru_csc = true;
    }

    /* Create memory pool */
    U16 byte_per_comp = (vd_fmt_dsc.bits_per_sample + 7) / 8;
    U32 max_frame_size = MAX (in_width, out_stride) * MAX (in_height,
        out_height) * VD_MAX_COMPONENTS * byte_per_comp;
    LOG_MESSAGE (LOG_LEVEL_DEBUG, g_log_level,
        "Creating pool with each buffer size : %d\n", max_frame_size);
    mem_pool_create (&vd_fmt_dsc.pool, max_frame_size, 2);

    /* Calculate input file size */
    LOG_MESSAGE (LOG_LEVEL_INFO, g_log_level,
        "Input width : %d Input height : %d Stride : %d", in_width, in_height,
        desc->msc_strideIn);
    LOG_MESSAGE (LOG_LEVEL_INFO, g_log_level,
        "Output width : %d Output height : %d", out_width, out_height);

    /* Allocate memory for input buffer */
    in_buf = mem_pool_get_free_mem (&vd_fmt_dsc.pool);

    if (vd_fmt_dsc.in_pix_fmt == VD_PIX_FMT_NV12_Y_UV8) {
      U8 *src_buf = (U8 *) desc->msc_srcImgBuf0;
      U8 *src_buf_uv = (U8 *) desc->msc_srcImgBuf1;
      U8 *in_buf_cpy = in_buf;
      U8 *in_buf_uv_cpy = (U8 *) in_buf + (in_width * in_height);
      for (U32 v = 0; v < in_height; v++) {
        memcpy (in_buf_cpy, src_buf, in_width);
        in_buf_cpy += in_width;
        src_buf += desc->msc_strideIn;
        if (!(v & 1)) {
          memcpy (in_buf_uv_cpy, src_buf_uv, in_width);
          in_buf_uv_cpy += in_width;
          src_buf_uv += desc->msc_strideIn;
        }
      }
    } else if (vd_fmt_dsc.in_pix_fmt == VD_PIX_FMT_I420_Y_U_V8) {
      U8 *src_buf = (U8 *) desc->msc_srcImgBuf0;
      U8 *src_buf_u = (U8 *) desc->msc_srcImgBuf1;
      U8 *src_buf_v = (U8 *) desc->msc_srcImgBuf2;
      U8 *in_buf_cpy = in_buf;
      U8 *in_buf_cpu = (U8 *) in_buf + (in_width * in_height);
      U8 *in_buf_cpv = in_buf_cpu + (in_width / 2 * in_height / 2);
      for (U32 v = 0; v < in_height; v++) {
        memcpy (in_buf_cpy, src_buf, in_width);
        in_buf_cpy += in_width;
        src_buf += desc->msc_strideIn;
        if (!(v & 1)) {
          memcpy (in_buf_cpu, src_buf_u, in_width / 2);
          in_buf_cpu += in_width / 2;
          src_buf_u += desc->msc_strideIn / 2;
          memcpy (in_buf_cpv, src_buf_v, in_width / 2);
          in_buf_cpv += in_width / 2;
          src_buf_v += desc->msc_strideIn / 2;
        }
      }
    } else if (vd_fmt_dsc.in_pix_fmt == VD_PIX_FMT_RGB_RGB8 ||
        vd_fmt_dsc.in_pix_fmt == VD_PIX_FMT_BGR_BGR8) {
      U8 *src_buf = (U8 *) desc->msc_srcImgBuf0;
      U8 *in_buf_cpy = in_buf;
      for (U32 v = 0; v < in_height; v++) {
        memcpy (in_buf_cpy, src_buf, (in_width * 3));
        in_buf_cpy += (in_width * 3);
        src_buf += desc->msc_strideIn;
      }
    } else if (vd_fmt_dsc.in_pix_fmt == VD_PIX_FMT_GRAY_GRAY8) {
      U8 *src_buf = (U8 *) desc->msc_srcImgBuf0;
      U8 *in_buf_cpy = in_buf;
      for (U32 v = 0; v < in_height; v++) {
        memcpy (in_buf_cpy, src_buf, in_width);
        in_buf_cpy += in_width;
        src_buf += desc->msc_strideIn;
      }
    } else if (vd_fmt_dsc.in_pix_fmt == VD_PIX_FMT_NV12_Y_UV10) {
      U32 *src_buf = (U32 *) desc->msc_srcImgBuf0;
      U32 *src_buf_uv = (U32 *) desc->msc_srcImgBuf1;
      U32 *in_buf_cpy = in_buf;
      U32 *in_buf_uv_cpy =
          (U32 *) ((U8 *) in_buf + (((in_width + 2) / 3) * in_height * 4));
      for (U32 v = 0; v < in_height; v++) {
        memcpy (in_buf_cpy, (void *) src_buf, (((in_width + 2) / 3) * 4));
        in_buf_cpy += ((in_width + 2) / 3);
        src_buf += desc->msc_strideIn;
        if (!(v & 1)) {
          memcpy (in_buf_uv_cpy, src_buf_uv, (((in_width + 2) / 3) * 4));
          in_buf_uv_cpy += ((in_width + 2) / 3);
          src_buf_uv += desc->msc_strideIn;
        }
      }
    }

    /* In case of 10bit NV12 we need to pack 10bit samples values into
     * short integer array for further processing */
    bytes_to_pixel_array (in_buf, in_width, in_height, &vd_fmt_dsc,
        &in_pix_array);

    /* Chose 8bit or 10bit scaling functions based on the input format */
    get_fuctions (&vd_fmt_dsc);

    /* Lets start with input pix format as the current format */
    vd_fmt_dsc.curr_pix_fmt = vd_fmt_dsc.in_pix_fmt;

    vd_fmt_dsc.vc_resample_fn (in_pix_array, in_width, in_height,
        passthru_vcr_up, &vd_fmt_dsc, (void **) &vc_out_buf, desc);

    vd_fmt_dsc.hc_resample_fn (vc_out_buf, in_width, in_height, passthru_hcr_up,
        &vd_fmt_dsc, (void **) &hc_out_buf);

    /* Pack YUV, YY..UU..VV.. to YUV...YUV... */
    vd_fmt_dsc.pack_yuv444_fn (hc_out_buf, in_width, in_height, passthru_pack,
        &vd_fmt_dsc, (void **) &scale_input_buf);

    /* Scale the height alone */
    vd_fmt_dsc.vertical_scale_fn (scale_input_buf, in_width, in_height,
        out_height, (void **) &v_scale_out,
        (I16 (*)[12]) desc->msc_blkmm_vfltCoeff, &vd_fmt_dsc);
    /* Scale the width alone */
    vd_fmt_dsc.horizontal_scale_fn (v_scale_out, in_width, out_height,
        out_width, (void **) &h_scale_out,
        (I16 (*)[12]) desc->msc_blkmm_hfltCoeff, &vd_fmt_dsc);

    /* Lets convert color space here */
    vd_fmt_dsc.csc_fn (h_scale_out, out_width, out_height, passthru_csc,
        &vd_fmt_dsc, &cs_out);

    /* Unpack YUV, YUV...YUV... to YY..UU..VV.. */
    vd_fmt_dsc.unpack_yuv444_fn (cs_out, out_width, out_height, passthru_unpack,
        &vd_fmt_dsc, &unpack_out);

    vd_fmt_dsc.hc_resample_fn (unpack_out, out_width, out_height,
        passthru_hcr_down, &vd_fmt_dsc, (void **) &out_buf1);

    vd_fmt_dsc.vc_resample_fn (out_buf1, out_width, out_height,
        passthru_vcr_down, &vd_fmt_dsc, (void **) &out_buf2, desc);

    convert_format (out_buf2, out_width, out_height, &vd_fmt_dsc, &format_out);

#ifdef ENABLE_PPE_SUPPORT
    vd_fmt_dsc.alpha[0] = desc->msc_alpha_0;
    vd_fmt_dsc.alpha[1] = desc->msc_alpha_1;
    vd_fmt_dsc.alpha[2] = desc->msc_alpha_2;
    vd_fmt_dsc.beta[0] = desc->msc_beta_0;
    vd_fmt_dsc.beta[1] = desc->msc_beta_1;
    vd_fmt_dsc.beta[2] = desc->msc_beta_2;
    preprocess (format_out, out_width, out_height, passthru_preproc,
        &vd_fmt_dsc, (U8 **) & pre_proc_out);
#else
    pre_proc_out = format_out;
#endif
    pixel_array_to_bytes (pre_proc_out, out_width, out_height, out_stride,
        &vd_fmt_dsc, (void **) out_buf_temp);

#ifdef DUMP_OUTPUT
    if (vd_fmt_dsc.out_pix_fmt == VD_PIX_FMT_NV12_Y_UV10) {
      int hc_fd_f = 0;
      sprintf (out_filename, "output_NV1210LE.nv12");
      hc_fd_f = open (out_filename, O_WRONLY | O_CREAT | O_APPEND, 0777);
      ret =
          write (hc_fd_f, out_buf_temp[0],
          ((out_width + 2) / 3 * out_height * 4));
      if (ret == -1) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, g_log_level,
            "Write failed with errno : %d", errno);
      }
      ret =
          write (hc_fd_f, out_buf_temp[1],
          ((out_width + 2) / 3 * out_height * 4) / 2);
      if (ret == -1) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, g_log_level,
            "Write failed with errno : %d", errno);
      }
      close (hc_fd_f);
    }

    if (vd_fmt_dsc.out_pix_fmt == VD_PIX_FMT_NV12_Y_UV8) {
      sprintf (out_filename, "output_NV12_%d_%d.yuv", out_width, out_height);
      out_fd_f = open (out_filename, O_WRONLY | O_CREAT | O_APPEND, 0777);
      ret = write (out_fd_f, out_buf_temp[0], (out_width * out_height));
      if (ret == -1) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, g_log_level,
            "Write failed with errno : %d", errno);
      }
      ret = write (out_fd_f, out_buf_temp[1], (out_width * out_height) / 2);
      if (ret == -1) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, g_log_level,
            "Write failed with errno : %d", errno);
      }
      close (out_fd_f);
    } else if (vd_fmt_dsc.out_pix_fmt == VD_PIX_FMT_I420_Y_U_V8) {
      sprintf (out_filename, "output_I420_%d_%d.yuv", out_width, out_height);
      out_fd_f = open (out_filename, O_WRONLY | O_CREAT | O_APPEND, 0777);
      ret = write (out_fd_f, out_buf_temp[0], (out_width * out_height));
      if (ret == -1) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, g_log_level,
            "Write failed with errno : %d", errno);
      }
      ret = write (out_fd_f, out_buf_temp[1], (out_width * out_height) / 4);
      if (ret == -1) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, g_log_level,
            "Write failed with errno : %d", errno);
      }
      ret = write (out_fd_f, out_buf_temp[2], (out_width * out_height) / 4);
      if (ret == -1) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, g_log_level,
            "Write failed with errno : %d", errno);
      }
      close (out_fd_f);
    } else if (vd_fmt_dsc.out_pix_fmt == VD_PIX_FMT_RGB_RGB8) {
      sprintf (out_filename, "output_RGB_%d_%d.rgb", out_width, out_height);
      out_fd_f = open (out_filename, O_WRONLY | O_CREAT | O_APPEND, 0777);
      ret = write (out_fd_f, out_buf_temp[0], (out_width * out_height * 3));
      if (ret == -1) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, g_log_level,
            "Write failed with errno : %d", errno);
      }
      close (out_fd_f);
    } else if (vd_fmt_dsc.out_pix_fmt == VD_PIX_FMT_BGR_BGR8) {
      sprintf (out_filename, "output_BGR_%d_%d.rgb", out_width, out_height);
      out_fd_f = open (out_filename, O_WRONLY | O_CREAT | O_APPEND, 0777);
      ret = write (out_fd_f, out_buf_temp[0], (out_width * out_height * 3));
      if (ret == -1) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, g_log_level,
            "Write failed with errno : %d", errno);
      }
      close (out_fd_f);
    } else if (vd_fmt_dsc.out_pix_fmt == VD_PIX_FMT_GRAY_GRAY8) {
      sprintf (out_filename, "output_GRAY8_%d_%d.gray", out_width, out_height);
      out_fd_f = open (out_filename, O_WRONLY | O_CREAT | O_APPEND, 0777);
      ret = write (out_fd_f, out_buf_temp[0], (out_width * out_height));
      if (ret == -1) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, g_log_level,
            "Write failed with errno : %d", errno);
      }
      close (out_fd_f);
    }
#endif

    mem_pool_destroy (&vd_fmt_dsc.pool);
    num_outputs--;
    desc = (MULTI_SCALER_DESC_STRUCT *) desc->msc_nxtaddr;
  }
  return 0;
}
