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

#ifndef __VVAS_PARSER_H264_H__
#define __VVAS_PARSER_H264_H__

#include "vvas_parser_priv.h"
/**
 *  @fn static int32_t parse_h264_au (VvasParserPriv *self,
 *                                    VvasParserBuffer* in_buffer,
 *                                    VvasParserBuffer *out_buffer
 *                                    bool is_eos);
 *  @param [in] self Handle to parser object
 *  @param [in] in_buffer Input buffer
 *  @param [in] out_buffer Output buffer where access-unit has to be put into
 *  @param [in] is_eos Whether end of input buffer to be parsed
 *  @return returns P_SUCCESS on success, P_ERROR on error
 *  @brief Parse H.264 and returns h264 access-unit
 */
VvasReturnType
parse_h264_au (VvasParserPriv *self, VvasParserBuffer *in_buffer,
    VvasParserBuffer *out_buffer, bool is_eos);

#endif
