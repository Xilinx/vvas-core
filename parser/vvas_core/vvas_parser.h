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
 * DOC: VVAS Parser API's
 * This file contains prototypes for parsining and extracting access-unit(au)
 * from avc/h264 or hevc/h265 elementary stream.
 */

#ifndef __VVAS_PARSE_H__
#ifdef __cplusplus
extern "C" {
#endif
#define __VVAS_PARSE_H__

#include <vvas_core/vvas_video.h>
#include <vvas_core/vvas_context.h>
#include <vvas_core/vvas_common.h>
#include <vvas_core/vvas_memory.h>
#include <vvas_core/vvas_decoder.h>

/**
 * typedef VvasParser - Opaque handle to reference Parser instance.
 **/
typedef void VvasParser;

/**
 * struct VvasParserFrameInfo - Holds the parsed frame information.
 * @bitdepth: Number of bits each pixel is coded for say 8 bit or 10 bit.
 * @codec_type: AVC(H264)/HEVC(H265) codec type.
 * @profile: AVC(H264)/HEVC(H265) profile.
 * @level: AVC(H264)/HEVC(H265) level.
 * @height: Frame height in pixel.
 * @width: Frame width in pixel.
 * @chroma_mode: Chroma sampling mode 444, 420 etc.
 * @scan_type: Scan type interlaced or progressive.
 * @frame_rate: Frame rate numerator.
 * @clk_ratio: Frame rate denominator.
 **/
typedef struct {
  uint32_t bitdepth;
  uint32_t codec_type;
  uint32_t profile;
  uint32_t level;
  uint32_t height;
  uint32_t width;
  uint32_t chroma_mode;
  uint32_t scan_type;
  uint32_t frame_rate;
  uint32_t clk_ratio;
} VvasParserFrameInfo;

/**
 * vvas_parser_create - Creates parser instance for processing the stream
 * parsing.
 * @vvas_ctx: Device context handle pointer.
 * @codec_type: Codec type for which stream required to be parsed.
 * @log_level: Log level to control the traces.
 *
 * Context: This function will allocate internal resources and return the
 *          handle.
 * Return:
 * * VvasParser handle pointer on success.
 * * NULL on failure.
 */
VvasParser* vvas_parser_create (VvasContext* vvas_ctx, VvasCodecType codec_type,
  VvasLogLevel log_level);

/**
 * vvas_parser_get_au - Internal function called for each list element to add
 * new or update existing entry into the obuf_db.
 * @handle: VvasParser handle pointer.
 * @inbuf: Input data blob pointer to be parsed.
 * @valid_insize: Valid input size.
 * @outbuf: output VvasMemory pointer containing access-unit(au) on successful
 * find of the same, else returns NULL.
 * @offset: offset in data blob to parsed for input, whereas for out it returns
 * offset till which data blob has been processed.
 * @dec_cfg: pointer to pointer of stream information, only valid if there is
 * change else it returns NULL.
 * @is_eos: whether end of stream has happened. All of the input data blob
 * should be consumed by the parser before setting this argument to TRUE. It
 * should be sent as TRUE if earlier invocation of this API returned
 * VVAS_RET_NEED_MOREDATA and there is no further data to be sent.
 *
 * Context: This function returns access-unit in outbuf if found and dec_cfg
 * if there is any change in stream properties. outbuf VvasMemory is allocated
 * by this function and its resposiblity of application to free this memory
 * once it has been consumed.
 *
 * Return:
 * * VVAS_RET_SUCCESS on Success.
 * * VVAS_RET_NEED_MOREDATA, If more data is needed.
 * * VVAS_RET_ERROR on any other Failure.
 */
VvasReturnType vvas_parser_get_au(VvasParser *handle, VvasMemory *inbuf,
    int32_t valid_insize, VvasMemory **outbuf, int32_t *offset,
    VvasDecoderInCfg **dec_cfg, bool is_eos);

/**
 * vvas_parser_destroy - Destroys parser instance
 * @handle: Parser handle pointer.
 *
 * Context: This function will free internal resources and destroy handle.
 *
 * Return:
 * * VVAS_RET_SUCCESS on Success.
 * * VVAS_RET_ERROR on Failure.
 */
VvasReturnType vvas_parser_destroy (VvasParser *handle);
#ifdef __cplusplus
}
#endif
#endif
