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
 * DOC: brief Contains private data-structure and macros for parser.
 */

#ifndef __VVAS_PARSER_PRIV_H__
#define __VVAS_PARSER_PRIV_H__

#include "parser_common.h"

#define MODULE_NAME "parser"
#define MODULE_NAME_SZ 8

#define IS_NALU_HEADER(data) ((((data)[0] == 0x00) && ((data)[1] == 0x00) && ((data)[2] == 0x00) && ((data)[3] == 0x01)) || (((data)[0] == 0x00) && ((data)[1] == 0x00) && ((data)[2] == 0x01)))

typedef struct {
  VvasParser *handle;
  VvasLogLevel log_level;
  uint8_t module_name[MODULE_NAME_SZ];
  VvasContext *vvas_ctx;
  VvasCodecType codec_type;
  VvasParserBuffer partial_inbuf;
  VvasParserBuffer partial_outbuf;
  uint8_t nalu_start_found;
  uint32_t last_nalu_offset;
  VvasHevcSliceHdr slice_hdr;
  uint32_t parse_state;
  VvasParserStreamInfo s_info;
  VvasDecoderInCfg dec_cfg;
  bool dec_cfg_changed;
  uint8_t has_slice;
  int32_t last_ret;
} VvasParserPriv;

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : \
                        __FILE__)
#define LOG_MSG(level, set_level, mstr, ...) {\
  do {\
    if (level <= set_level) {\
      printf("[%s %s:%d][%s] : ",__FILENAME__, __func__, __LINE__, mstr);\
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
#endif /* #ifndef __VVAS_PARSER_PRIV_H__ */
