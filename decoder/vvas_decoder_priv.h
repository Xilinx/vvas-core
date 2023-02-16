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
 * DOC: Contains VVAS core decoder internal APIs and data-structure.
 */

#ifndef __VVAS_DECODER_PRIV_H__
#define __VVAS_DECODER_PRIV_H__

#define HDR_DATA_SUPPORT

#ifdef HDR_DATA_SUPPORT
#include "video_hdr.h"
#endif
#include <vvas_core/vvas_log.h>

#define KERNEL_NAME_SZ     100

#define MAX_IBUFFS 2

#define MODULE_NAME "videodec"
#define MODULE_NAME_SZ 8

#define FRM_BUF_POOL_SIZE 150
#define MAX_ERR_STRING 1024

/* TODO: retrive memory bank dynamically */
#define VIDEODEC_DEFAULT_MEM_BANK 3

#define ERT_CMD_DATA_LEN 1024
#define ERT_CMD_SIZE 4096

#define DEC_ENT_BUF_CNT_MIN 2
#define DEC_ENT_BUF_CNT_MAX 10
#define DECCFG_SCANTYPE_P   1
#define DECCFG_CODEC_AVC    0
#define DECCFG_CODEC_HEVC   1

#define AVC_FRAME_WIDTH_MIN   80
#define AVC_FRAME_HEIGHT_MIN  96
#define HEVC_FRAME_WIDTH_MIN  128
#define HEVC_FRAME_HEIGHT_MIN 128
#define FRAME_WIDTH_MAX       3840
#define FRAME_HEIGHT_MAX      2160

/* AVC Profiles */
#define AVC_PROFILE_IDC_BASELINE  66
#define AVC_PROFILE_IDC_MAIN      77
#define AVC_PROFILE_IDC_HIGH      100
#define AVC_PROFILE_IDC_HIGH10    110
#define AVC_PROFILE_IDC_CONSTRAINED_BASELINE (AVC_PROFILE_IDC_BASELINE | (1<<9))
#define AVC_PROFILE_IDC_HIGH10_INTRA (AVC_PROFILE_IDC_HIGH10 | (1<<11))

/* HEVC Profiles */
#define HEVC_PROFILE_IDC_MAIN     1
#define HEVC_PROFILE_IDC_MAIN10   2
#define HEVC_PROFILE_IDC_RExt     4

typedef struct dec_params
{
  uint32_t bitdepth;
  uint32_t codec_type;
  uint32_t low_latency;
  uint32_t entropy_buffers_count;
  uint32_t frame_rate;
  uint32_t clk_ratio;
  uint32_t profile;
  uint32_t level;
  uint32_t height;
  uint32_t width;
  uint32_t chroma_mode;
  uint32_t scan_type;
  uint32_t splitbuff_mode;
  uint32_t instance_id;
  uint32_t i_frame_only;
} dec_params_t;

typedef struct _video_dec_in_usermeta
{
  int64_t pts;
} video_dec_in_usermeta;

typedef struct _video_dec_out_usermeta
{
  int64_t pts;
#ifdef HDR_DATA_SUPPORT
  bool is_hdr_present;
#endif
} video_dec_out_usermeta;

typedef struct _out_buf_info
{
  uint64_t freed_obuf_paddr;
  size_t freed_obuf_size;
  uint32_t freed_obuf_index;
} out_buf_info;

typedef struct host_dev_data
{
  uint32_t cmd_id;
  uint32_t cmd_rsp;
  uint32_t obuff_size;
  uint32_t obuff_num;
  uint32_t obuff_index[FRM_BUF_POOL_SIZE];
  uint32_t ibuff_valid_size;
  uint32_t host_to_dev_ibuf_idx;
  uint32_t dev_to_host_ibuf_idx;
  bool last_ibuf_copied;
  bool resolution_found;
  video_dec_in_usermeta ibuff_meta;
  video_dec_out_usermeta obuff_meta[FRM_BUF_POOL_SIZE];
  bool end_decoding;
  uint32_t free_index_cnt;
  int valid_oidxs;
  out_buf_info obuf_info[FRM_BUF_POOL_SIZE];
#ifdef HDR_DATA_SUPPORT
  out_buf_info hdrbuf_info[FRM_BUF_POOL_SIZE];
#endif
  char dev_err[MAX_ERR_STRING];
} sk_payload_data;

typedef struct {
  VvasVideoFrame  *vframe;
  uint64_t        paddr;
  size_t          size;
  bool            is_free;
} VideodecOutBufDb;

typedef enum {
  VVAS_DEC_STATE_CREATED,
  VVAS_DEC_STATE_CONFIGURED,
  VVAS_DEC_STATE_INITED,
  VVAS_DEC_STATE_STARTED,
  VVAS_DEC_STATE_FINISHED,
} VvasDecoderState;

typedef struct {
  uint32_t            insize;
  video_dec_in_usermeta meta;
}VvasDecoderIbuffParam;

typedef struct {
  VvasDecoder* handle;
  VvasLogLevel log_level;
  uint8_t module_name[MODULE_NAME_SZ];
  uint8_t kernel_name[KERNEL_NAME_SZ];
  VvasContext *vvas_ctx;

  uint32_t  in_mem_bank;
  uint32_t  out_mem_bank;

  vvasDeviceHandle  hskd;
  /** Kernel handle */
  vvasKernelHandle kernel_handle;

  VvasDecoderIbuffParam ibuff_param;

  xrt_buffer *cmd_buf;
  xrt_buffer *sk_payload_buf;
  xrt_buffer *dec_cfg_buf;

  xrt_buffer *xrt_bufs_in[MAX_IBUFFS];

  /* XRT Buffer which contains the phy addresses of XRT out buffers
   * out put buffers are allocated by the application */
  xrt_buffer  *dec_out_bufs_handle;

#ifdef HDR_DATA_SUPPORT
  xrt_buffer  *hdr_out_bufs_handle;
  xrt_buffer  *hdr_out_bufs_arr;
#endif

  VvasHashTable  *oidx_hash;

  VvasCodecType dec_type;
  VvasDecoderInCfg  *icfg;
  VvasDecoderOutCfg  *ocfg;

  VvasDecoderState  state;
  VideodecOutBufDb obuf_db[FRM_BUF_POOL_SIZE];

  uint32_t max_ibuf_size;

  sk_payload_data last_rcvd_payload;
  uint32_t last_rcvd_oidx;

  uint32_t host_to_dev_ibuf_idx;
  VvasList *free_buf_list;
  uint32_t hw_instance_id;
  bool vf_max_error;
} VvasDecoderPrivate;

typedef enum
{
  VCU_PREINIT = 0,
  VCU_INIT,
  VCU_PUSH,
  VCU_RECEIVE,
  VCU_FLUSH,
  VCU_DEINIT,
} VcuCmdType;

uint8_t VcuCmdId2Str[][16] = {
  "VCU_PREINIT",
  "VCU_INIT",
  "VCU_PUSH",
  "VCU_RECEIVE",
  "VCU_FLUSH",
  "VCU_DEINIT",
};

uint8_t VvasLogLevelStr[][8] = {
  "ERR",
  "WRN",
  "INF",
  "DBG"
};

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : \
                        __FILE__)
#if 0
#define LOG_MSG(level, set_level, mstr, ...) {\
  do {\
    if (level <= set_level) {\
      printf("[%s %s:%d][%s] %s: ",__FILENAME__, __func__, __LINE__, mstr, \
          VvasLogLevelStr[level]);\
      printf(__VA_ARGS__);\
      printf("\n");\
    }\
  } while (0); \
}

#define LOGE(dec_inst, ...) LOG_MSG(LOG_LEVEL_ERROR, dec_inst->log_level,\
                              dec_inst->module_name, __VA_ARGS__)
#define LOGW(dec_inst, ...) LOG_MSG(LOG_LEVEL_WARNING, dec_inst->log_level,\
                              dec_inst->module_name, __VA_ARGS__)
#define LOGI(dec_inst, ...) LOG_MSG(LOG_LEVEL_INFO, dec_inst->log_level,\
                              dec_inst->module_name, __VA_ARGS__)
#define LOGD(dec_inst, ...) LOG_MSG(LOG_LEVEL_DEBUG, dec_inst->log_level,\
                              dec_inst->module_name, __VA_ARGS__)
#endif
#define LOGE(dec_inst, ...) LOG_ERROR(dec_inst->log_level, __VA_ARGS__)
#define LOGW(dec_inst, ...) LOG_WARNING(dec_inst->log_level, __VA_ARGS__)
#define LOGI(dec_inst, ...) LOG_INFO(dec_inst->log_level, __VA_ARGS__)
#define LOGD(dec_inst, ...) LOG_DEBUG(dec_inst->log_level, __VA_ARGS__)
#endif /* #ifndef __VVAS_DECODER_PRIV_H__ */
