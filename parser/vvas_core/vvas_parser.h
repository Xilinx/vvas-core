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
 * DOC: VVAS Parser APIs
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
 * vvas_parser_get_au - This API is called to extract one complete encoded Access Unit/Frame 
 * from the input elementary stream buffer. It may be possible that there is partial AU in 
 * current input buffer. In this case this function will return VVAS_RET_NEED_MOREDATA indicating 
 * that parser need more data to extract the complete AU. In this case user needs to call this 
 * API several times with new elementary stream data to get complete AU.
 * It is also possible that there is more than one AUs (access-units) in current input buffer. 
 * In this case this function will return the first complete AU in this buffer and the "offset" 
 * parameter will represent the amount of data consumed from the beginning of the current input 
 * buffer. To get remaining AUs in this buffer, this APIs must be called with same input buffer 
 * and "offset" received in previous call to this function, until this function returns either 
 * a new AU, VVAS_RET_NEED_MOREDATA or VVAS_RET_ERROR.
 * @handle: VvasParser handle pointer.
 * @inbuf: Input data blob pointer (pointer to elementary stream buffer) to be parsed.
 * @valid_insize: Valid input data buffer size. The "inbuf" is allocated initially once
 * based on the application requirements, like stream resolution, bitrate etc. 
 * Later on, this same buffer is re-sent with new data. For example, let's assume 
 * "inbuf" size is 4K bytes at the beginning. As long as this buffer is completely 
 * carrying new data of 4K size, then "valid_insize" will be 4K. But towards the end 
 * of stream when the available new data is less than 4K bytes, say, it is 3K only, 
 * then "valid_insize" will be 3K even though the buffer size is 4K.
 * @outbuf: output VvasMemory pointer containing one encoded access-unit(au)/frame on 
 * success, else returns NULL.
 * @offset: This is input as well as output parameter to this function. As an input 
 * to this function, this parameter indicates from which offset, from the beginning 
 * of the input buffer, the parser needs to start parsing. This is required when there
 * are multiple AUs/Frames in the current input buffer. As an output, this parameter 
 * represents how much data, from the beginning of the current input buffer, has been 
 * consumed.
 * @dec_cfg: pointer to pointer of decoder configuration. Valid if there is
 * change in stream properties with respect to previous properties else its value will be NULL.
 * @is_eos: whether end of stream is reached. All of the input data blob
 * should be consumed by the parser before setting this argument to TRUE. It
 * should be sent as TRUE if earlier invocation of this API returned
 * VVAS_RET_NEED_MOREDATA and there is no new data has been passed in this call.
 *
 * Context: This function returns one encoded access-unit/frame in "outbuf", if found. 
 * The "outbuf" is allocated by this function and it is the responsibility of application
 * to free this memory once it has been consumed.
 * This function also returns decoder configuration information in "dec_cfg" parameter along 
 * with first AU. In case the stream properties remain same, then "dec_cfg" will be NULL for 
 * the sub-sequent AUs. In case there is change in the stream properties which requires decoder
 * re-configuration then decoder configuration information, "dec_cfg", parameter will have new 
 * decoder configuration.
 *
 * Return:
 * * VVAS_RET_SUCCESS on Success.
 * * VVAS_RET_NEED_MOREDATA, If more data is needed to extract a complete Access Unit/frame.
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
