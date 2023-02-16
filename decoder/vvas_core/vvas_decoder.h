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
 * DOC: VVAS Decoder API's
 * This file contains the public methods related to VVAS decoder. These
 * functions can be used to implement decode of avc/h264 and hevc/h265 encoded
 * stream(s). User need to pass one access-unit(au) at a time for decoding.
 */

#ifndef __VVAS_DECODER_H__
#ifdef __cplusplus
extern "C" {
#endif
#define __VVAS_DECODER_H__

#include <vvas_core/vvas_video.h>
#include <vvas_core/vvas_context.h>
#include <vvas_core/vvas_common.h>
#include <vvas_core/vvas_memory.h>
#include <vvas_utils/vvas_utils.h>

/* Maximum softkernel frame buffer pool size as set in device (150). */
#define FRM_BUF_POOL_SIZE 150

/**
 * typedef VvasDecoder - Holds the reference to decoder instance.
 **/
typedef void VvasDecoder;

/**
 * struct VvasDecoderInCfg - Holds decoder input configuration
 * @width: width of frame in pixel
 * @height: Height of frame in pixel
 * @frame_rate: Frame rate
 * @clk_ratio: Clock Ratio
 * @codec_type: Codec Type, AVC - 0 or HEVC - 1
 * @profile: Encoding profile
 * @level: Encoding level
 * @bitdepth: Bit depth of each pixel 8 or 10 bit
 * @chroma_mode: Chroma Mode
 * @scan_type: Scan Type, Interlaced(0) or Progressive(1)
 * @splitbuff_mode: Split Buffer Mode
 * @low_latency: Low Latency, Disabled(0) or Enabled(1)
 * @entropy_buffers_count: Number of Entropy Buffers
 * @i_frame_only: I frame only decode, Disabled(0) or Enabled(1)
 **/
typedef struct {
  uint32_t width;
  uint32_t height;
  uint32_t frame_rate;
  uint32_t clk_ratio;
  uint32_t codec_type;
  uint32_t profile;
  uint32_t level;
  uint32_t bitdepth;
  uint32_t chroma_mode;
  uint32_t scan_type;
  uint32_t splitbuff_mode;
  uint32_t low_latency;
  uint32_t entropy_buffers_count;
  uint32_t i_frame_only;
} VvasDecoderInCfg;

/**
 * struct VvasDecoderOutCfg - Holds the decoder configuration output
 * @vinfo: VvasVideoInfo for the video output frame required by decoder
 * @mem_bank_id: Memory bank on which video frame should be allocated
 * @min_out_buf: Minimum number of output buffers required for decoder
 **/
typedef struct {
  VvasVideoInfo vinfo;
  uint32_t      mem_bank_id;
  uint32_t      min_out_buf;
} VvasDecoderOutCfg;

/**
 * vvas_decoder_create() - Creates decoder's instance handle
 * @vvas_ctx: Address of VvasContext handle created using vvas_context_create
 * @dec_name: Name of the decoder to be used for decoding
 * @dec_type: Type of the decoder to be used (i.e. H264/H265)
 * @hw_instance_id: Decoder instance index in a multi-instance decoder
 *             Incase of V70, this represents HW instance index and can have
 *             any value from the range 0..3.
 * @log_level: Logging level
 *
 * Context: This function will allocate internal resources and return the
 *          handle.
 * Return:
 * * VvasDecoder handle pointer on success.
 * * NULL on failure.
 */
VvasDecoder* vvas_decoder_create (VvasContext *vvas_ctx, uint8_t *dec_name,
                VvasCodecType dec_type, uint8_t hw_instance_id,
                VvasLogLevel log_level);

/**
 * vvas_decoder_config() - Configures decoder with VvasDecoderInCfg and produces
 * VvasDecoderOutCfg. Applications can populate VvasDecoderInCfg
 * @dec_handle: Decoder handle pointer
 * @icfg: Decoder input configuration
 * @ocfg: Decoder output configuration
 *
 * Context: This fucntion configures the decoder hardware for stream to be
 * decoded. User need to allocate the alteast suggested min_out_buf each with
 * properties as in vinfo of ocfg. List if these allocated frames are required
 * to be passed to decoder in the fist invocation of function
 * vvas_decoder_submit_frames along with the first access-unit.
 *
 * Return:
 * * &enum VvasReturnType
 */
VvasReturnType vvas_decoder_config(VvasDecoder* dec_handle,
                    VvasDecoderInCfg *icfg, VvasDecoderOutCfg *ocfg);

/**
 * vvas_decoder_submit_frames - Submits one NALU frame and free output frames
 * to decoder for decoding
 * @dec_handle: Decoder handle pointer
 * @nalu: Complete NALU frame. send NULL pointer on End of stream
 * @loutframes: List of free output frames for decoding process
 *
 * Context: This function submits the encoded access-unit(au) and list of
 * output frames created or unref'ed. The passed output frames are used by
 * decoder to output decoded frame. User need to consume the output frame and
 * send it back to decoder in successive invocation. In first invocation of
 * this function user need to pass the list of atleast as many outout frames as
 * suggested in ocfg output parameter of vvas_decoder_config API. At end of
 * stream user need to pass nalu=NULL and drain the decoder by calling
 * vvas_decoder_get_decoded_frame. Each consumed(unref'ed) output buffers
 * required to be passed back to decoder even during the draining of remaining
 * decoded frame.
 *
 * Return:
 * * VVAS_RET_SUCCESS if success.
 * * VVAS_RET_INVALID_ARGS if parameter is not valid or not in  expeceted range.
 * * VVAS_RET_SEND_AGAIN if nalu is not consumed completely. In this case, send
 * same NALU again.
 * * VVAS_RET_ERROR if any other errors.
 */

VvasReturnType vvas_decoder_submit_frames(VvasDecoder* dec_handle,
                  VvasMemory *nalu, VvasList *loutframes);

/**
 * vvas_decoder_get_decoded_frame() - This API gets decoded frame from decoder
 * @dec_handle: Decoder handle pointer
 * @output: Video frame which contains decoded data
 *
 * Context: This function gives decoded frame if available, one at a time in
 * each invocation. User need to keep calling this function until VVAS_RET_EOS
 * is returned.
 *
 * Return:
 * * VVAS_RET_SUCCESS on success.
 * * VVAS_RET_EOS on End of stream.
 * * VVAS_RET_NEED_MOREDATA if decoder does not produce any decoded buffers.
 * * VVAS_RET_INVALID_ARGS if parameter is not valid or not in expeceted range.
 * * VVAS_RET_ERROR any other error.
 */
VvasReturnType vvas_decoder_get_decoded_frame(VvasDecoder* dec_handle,
                  VvasVideoFrame **output);

/**
 * vvas_decoder_destroy() - Destroys decoded handle
 * @dec_handle: Decoder handle pointer
 *
 * Context: This function will free internal resources and destroy handle.
 *
 * Return:
 * * VVAS_RET_SUCCESS on success.
 * * VVAS_RET_ERROR on failure.
 */
VvasReturnType vvas_decoder_destroy (VvasDecoder* dec_handle);

#ifdef __cplusplus
}
#endif
#endif /* __VVAS_DECODER_H__ */
