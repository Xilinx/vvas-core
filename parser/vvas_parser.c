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

/** @file vvas_parser.c
 *  @brief Contains VVAS base parser APIs for parsing H254 and H265 stream to
 *  extract access-unit(au)
 */

#include "stdint.h"
#include "stdlib.h"
#include "string.h"
#include <vvas_core/vvas_common.h>
#include <vvas_core/vvas_memory.h>
#include <vvas_core/vvas_video.h>
#include <vvas_core/vvas_log.h>
#include <vvas_core/vvas_parser.h>
#include "vvas_parser_priv.h"
#include "parser_common.h"
#include "parser_h264.h"
#include "parser_h265.h"

/** @fn static void populate_obuf_db_fn(void *data, void* udata)
 *
 *  @param[in] data - list element data
 *
 *  @return On Success returns TRUE \n
 *          On Failure returns FALSE
 *
 *  @brief Internal function called for each list element to add new or update
 *  existing entry into the obuf_db
 */
/** @fn static void VvasParserMemoryDataFreeCB(void *data, void *user_data)
 *
 *  @param[in] data - Instance data pointer
 *  @param[in] user_data - User data pointer
 *
 *  @return Void
 *
 *  @brief Internal callback function to free the VvasMemory return by
 *  vvas_parser_get_au()
 */
static void VvasParserMemoryDataFreeCB(void *data, void *user_data) {

  if(data)
    free(data);

  //LOGD(self, "data=0x%p freeed", data);
}

/** @fn VvasParser *vvas_parser_create (VvasContext* vvas_ctx,
 *                                      VvasCodecType codec_type,
 *                                      VvasLogLevel log_level)
 *
 *  @param[in] vvas_ctx - Device context handle pointer
 *  @param[in] codec_type - Codec type for which stream required to be parsed
 *  @param[in] log_level - Log level to control the traces
 *
 *  @return On Success returns VvasParser handle pointer \n
 *          On Failure returns NULL
 *
 *  @brief Creates parser instance for processing the stream parsing.
 */
VvasParser *vvas_parser_create (VvasContext* vvas_ctx, VvasCodecType codec_type,
              VvasLogLevel log_level) {
  VvasParserPriv *self;

  if (log_level < LOG_LEVEL_ERROR || log_level > LOG_LEVEL_DEBUG) {
    LOG_MSG(LOG_LEVEL_ERROR, LOG_LEVEL_DEBUG, MODULE_NAME,
      "Invaid log_level(%d), valid range is [%d, %d]", log_level,
      LOG_LEVEL_ERROR, LOG_LEVEL_DEBUG);
    return NULL;
  }

  if (codec_type != VVAS_CODEC_H264 && codec_type != VVAS_CODEC_H265) {
    LOG_MSG(LOG_LEVEL_ERROR, LOG_LEVEL_DEBUG, MODULE_NAME,
      "Invalid codec_type=%d, valid range is [%d, %d]", codec_type,
      VVAS_CODEC_H264, VVAS_CODEC_H265)
    return NULL;
  }

  self = malloc(sizeof(VvasParserPriv));
  if (!self) {
    LOG_MSG(LOG_LEVEL_ERROR, LOG_LEVEL_DEBUG, MODULE_NAME,
      "Failed to allocate memory for instance");
    return NULL;
  }

  memset(self, 0, sizeof(VvasParserPriv));

  self->codec_type = codec_type;
  self->log_level = log_level;
  memcpy (self->module_name, (uint8_t *)MODULE_NAME, MODULE_NAME_SZ-1);
  self->vvas_ctx = vvas_ctx;

  self->parse_state = 0;
  init_parse_data(&self->s_info);

  /* Setting to the decoder default config - can be changes by user before
  invoking decoder_config() as needed */
  self->dec_cfg.entropy_buffers_count = 2;
  self->dec_cfg.splitbuff_mode = false;
  self->dec_cfg.low_latency = false;
  self->dec_cfg.bitdepth = 8;

  /* Following are the only spported */
  self->dec_cfg.chroma_mode = 420;
  self->dec_cfg.scan_type = 1;
  if (self->codec_type == VVAS_CODEC_H265)
    self->dec_cfg.codec_type = 1;

  self->dec_cfg_changed = true;
  self->has_slice = 0;
  self->handle = (VvasParser *)self;

  return (VvasParser *)self;
}

/**
 *  @fn static uint32_t populate_dec_cfg(VvasParserPriv *self)
 *  @param [in] self - pointer to @ref VvasParserPriv
 *  @return returns 0 if success else returns -1
 *  @brief internal API to process the stream parameter and update dec_cfg
 */
static uint32_t populate_dec_cfg(VvasParserPriv *self)
{
  if(self->codec_type == VVAS_CODEC_H265) {
    LOGD(self, "s_info.latest_hevc_sps=%d\n", self->s_info.latest_hevc_sps);
    VvasHevcSeqParamSet  *sps
      = &self->s_info.hevc_seq_parameter_set[self->s_info.latest_hevc_sps];
    if(sps->valid) {
      LOGD(self, "profile_idc=%d, level_idc=%d\n", sps->profile_idc,
        sps->level_idc);

      if (self->dec_cfg.profile != sps->profile_idc) {
        self->dec_cfg.profile = sps->profile_idc;
        self->dec_cfg_changed = true;
      }

      if (self->dec_cfg.level != sps->level_idc) {
        self->dec_cfg.level = sps->level_idc;
        self->dec_cfg_changed = true;
      }

      if (self->dec_cfg.bitdepth != (sps->bit_depth_luma_minus8 + 8)) {
        self->dec_cfg.bitdepth = sps->bit_depth_luma_minus8 + 8;
        self->dec_cfg_changed = true;
      }
    }
  } else {
    VvasH264SeqParamSet *sps;
    uint32_t sps_id = 0, pps_id = self->s_info.current_h264_pps;
    if (self->s_info.h264_pic_parameter_set[pps_id].valid) {
      sps_id =
        self->s_info.h264_pic_parameter_set[pps_id].seq_parameter_set_id;
      if (self->s_info.h264_seq_parameter_set[sps_id].valid ) {
        sps = &self->s_info.h264_seq_parameter_set[sps_id];
      } else {
        printf("ERROR: sps_id=%d is not valid\n", sps_id);
        return -1;
      }
    } else {
      printf("ERROR: pps_id=%d is not valid\n", pps_id);
      return -1;
    }

    if (self->dec_cfg.profile != sps->profile_idc) {
      self->dec_cfg.profile = sps->profile_idc;
      self->dec_cfg_changed = true;
    }

    if (self->dec_cfg.level != sps->level_idc) {
      self->dec_cfg.level = sps->level_idc;
      self->dec_cfg_changed = true;
    }

    if (self->dec_cfg.bitdepth != (sps->bit_depth_luma_minus8 + 8)) {
      self->dec_cfg.bitdepth = sps->bit_depth_luma_minus8 + 8;
      self->dec_cfg_changed = true;
    }
  }

  if (self->dec_cfg.height != self->s_info.height) {
    self->dec_cfg.height = self->s_info.height;
  }

  if (self->dec_cfg.width != self->s_info.width) {
    self->dec_cfg.width = self->s_info.width;
  }

  if (self->dec_cfg.frame_rate != self->s_info.fr_num) {
    self->dec_cfg.frame_rate = self->s_info.fr_num;
  }

  if (self->dec_cfg.clk_ratio != self->s_info.fr_den) {
    self->dec_cfg.clk_ratio = self->s_info.fr_den;
  }

  return 0;
}

/** @fn VvasReturnType vvas_parser_get_au(VvasParser *handle,
 *                                        VvasMemory *inbuf,
 *                                        VvasMemory **outbuf,
 *                                        int32_t *offset,
 *                                        VvasDecoderInCfg *dec_cfg,
 *                                        bool is_eos)
 *
 *  @param[in] handle - VvasParser handle pointer
 *  @param[in] inbuf - input data blob pointer to be parsed
 *  @param[out] outbuf - output VvasMemory pointer containing access-unit(au)
 *    on successful find of the same, else returns NULL
 *  @param[in/out] offset - offset in data blob to parsed for input, whereas for
 *    out it returns offset till which data blob has been processed
 *  @param[out] dec_cfg - pointer to pointer of stream information, only valid
 *    if there is change else it returns NULL
 *  @param[in] is_eos - if this is last buffer being submitted for parsing
 *
 *  @return On Success returns VVAS_RET_SUCCESS \n
 *          If more data is needed returns VVAS_RET_NEED_MOREDATA \n
 *          On Failure returns VVAS_RET_ERROR
 *
 *  @brief Internal function called for each list element to add new or update
 *  existing entry into the obuf_db
 */
VvasReturnType vvas_parser_get_au(VvasParser *handle, VvasMemory *inbuf,
    int32_t valid_insize, VvasMemory **outbuf, int32_t *offset,
    VvasDecoderInCfg **dec_cfg, bool is_eos)
{
  VvasReturnType vret = VVAS_RET_SUCCESS;
  VvasMemoryMapInfo inbuf_info;
  VvasParserPriv *self = (VvasParserPriv *) handle;
  VvasParserBuffer buffer, out_buffer;

  if (self->codec_type != VVAS_CODEC_H265 &&
      self->codec_type != VVAS_CODEC_H264) {
    LOGE(self, "codec_type = %d, cann't parse, supported range [%d, %d]",
      self->codec_type, VVAS_CODEC_H264, VVAS_CODEC_H265);
    return VVAS_RET_ERROR;
  }

  if (inbuf) {
    vvas_memory_map(inbuf, VVAS_DATA_MAP_READ, &inbuf_info);
    buffer.data = inbuf_info.data;
  } else {
    buffer.data = NULL;
  }
  buffer.size = valid_insize;
  if (offset)
    buffer.offset = *offset;
  else
    buffer.offset = 0;

  if (self->codec_type == VVAS_CODEC_H265) {
    vret = parse_h265_au(self, &buffer, &out_buffer, is_eos);
  } else {
    vret = parse_h264_au(self, &buffer, &out_buffer, is_eos);
  }

  if (inbuf)
    vvas_memory_unmap(inbuf, &inbuf_info);

  if (vret == VVAS_RET_SUCCESS) {
    populate_dec_cfg(self);
    if (self->dec_cfg_changed) {
      *dec_cfg = malloc(sizeof(VvasDecoderInCfg));
      memcpy(*dec_cfg, &self->dec_cfg, sizeof(VvasDecoderInCfg));
      self->dec_cfg_changed = false;
    } else {
      *dec_cfg = NULL;
    }

    /* Wrap the buffer into VvasMemory and return */
    *outbuf = vvas_memory_alloc_from_data(self->vvas_ctx, out_buffer.data,
                out_buffer.size, VvasParserMemoryDataFreeCB, self, &vret);
    if (vret != VVAS_RET_SUCCESS) {
      LOGE(self, "Failed to wrap data(0x%p) into VvasMemory",
        out_buffer.data);
      return VVAS_RET_ERROR;
    }
    if (offset)
      *offset = buffer.offset;
    return vret;
  } else if (vret == VVAS_RET_NEED_MOREDATA) {
    /* Update the out offset till which parsing is completed */
    if (offset)
      *offset = buffer.offset;
  } else if (vret == VVAS_RET_EOS) {
    /* Wrap the buffer into VvasMemory and return */
    *outbuf = vvas_memory_alloc_from_data(self->vvas_ctx, out_buffer.data,
                out_buffer.size, VvasParserMemoryDataFreeCB, self, &vret);
    if (vret != VVAS_RET_SUCCESS) {
      LOGE(self, "Failed to wrap data(0x%p) into VvasMemory",
        out_buffer.data);
      return VVAS_RET_ERROR;
    }

    if (offset)
      *offset = buffer.offset;
    return VVAS_RET_EOS;
  }

  return vret;
}

/** @fn VvasReturnType vvas_parser_destroy (VvasParser *handle)
 *
 *  @param[in] handle - Parser handle pointer
 *
 *  @return On Success returns VVAS_RET_SUCCESS \n
 *          On Failure returns VVAS_RET_ERROR
 *
 *  @brief Destroys parser instance
 */
VvasReturnType vvas_parser_destroy (VvasParser *handle) {
  VvasParserPriv *self = (VvasParserPriv *) handle;

  free(self);

  return VVAS_RET_SUCCESS;
}
