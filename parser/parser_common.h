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

#ifndef __COMMON_PARSER_H__
#define __COMMON_PARSER_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#define P_SUCCESS   0
#define P_ERROR    -1
#define P_MOREDATA  1

/** @struct VvasParserBuffer
 *  @brief Parser buffer object
 */
typedef struct {
  /** buffer pointer */
  uint8_t *data;
  /** buffer size */
  uint32_t size;
  /** offset inside the buffer */
  uint32_t offset;
} VvasParserBuffer;

/** @struct VvasParserGetBits
 *  @brief Parser bit parser object
 */
typedef struct {
  /** start pointer */
  const uint8_t* start;
  /** end pointer */
  const uint8_t* end;
  /** byte offset */
  uint32_t offset_bytes;
  /** bit offset */
  uint8_t offset_bits;
} VvasParserGetBits;


typedef struct {
  const uint8_t* start;
  const uint8_t* end;
  uint32_t offset_bytes;
  uint8_t offset_bits;
} VVasDecGetBits;

/** @struct VvasH264SeqParamSet
 *   @brief This structure contains information related to H264 Sequence
 *   Parameter Set
 */
typedef struct {
  uint8_t profile_idc;
  uint8_t level_idc;
  uint8_t bit_depth_luma_minus8;
  uint64_t pic_width_in_mbs_minus1;
  uint64_t pic_height_in_map_units_minus1;
  uint64_t frame_crop_left_offset;
  uint64_t frame_crop_right_offset;
  uint64_t frame_crop_top_offset;
  uint64_t frame_crop_bottom_offset;
  uint8_t chroma_format_idc;
  uint8_t log2_max_frame_num_minus4;
  uint8_t pic_order_cnt_type;
  uint8_t log2_max_pic_order_cnt_lsb_minus4;
  uint8_t delta_pic_order_always_zero_flag;
  uint8_t frame_mbs_only_flag;
  uint8_t frame_cropping_flag;
  uint8_t valid;
} VvasH264SeqParamSet;

typedef struct _HEVCShortTermRPS {
  unsigned int num_negative_pics;
  int num_delta_pocs;
  int32_t delta_poc[32];
  uint8_t used[32];
} HEVCShortTermRPS;

typedef struct {
  uint8_t profile_idc;
  uint8_t level_idc;
  uint8_t bit_depth_luma_minus8;
  uint8_t nb_st_rps;
  HEVCShortTermRPS st_rps[64];
  uint8_t valid;
} VvasHevcSeqParamSet;

/** @struct VvasH264PicParamSet
 *   @brief This structure contains information related to H264 Picture
 *   Parameter Set
 */
typedef struct {
  uint8_t seq_parameter_set_id;
  uint8_t pic_order_present_flag;
  uint8_t valid;
} VvasH264PicParamSet;

/** @struct VvasH264SliceHeader
 *   @brief This structure contains information related to H264 Slice
 */
typedef struct {
  int32_t delta_pic_order_cnt_bottom;
  int32_t delta_pic_order_cnt[2];
  uint16_t frame_num;
  uint16_t idr_pic_id;
  uint16_t pic_order_cnt_lsb;
  uint8_t pic_parameter_set_id;
  uint8_t field_pic_flag;
  uint8_t bottom_field_flag;
  uint8_t nal_ref_idc;
  uint8_t nal_unit_type;
} VvasH264SliceHeader;

/** @struct VvasH264SliceHeader
 *   @brief This structure contains information related to HEVC/H265 Slice
 */
typedef struct
{
  uint8_t first_slice_segment_in_pic_flag;
  uint8_t no_output_of_prior_pics_flag;
  uint32_t pps_id;
} VvasHevcSliceHdr;

typedef struct {
  /**< width of the video */
  int32_t width;
  /**< height of video frame */
  int32_t height;
  /**< framerate's numerator */
  int32_t fr_num;
  /**< framerate's denominator */
  int32_t fr_den;
  /**< current PPS(Picture Parameter Set) index */
  uint8_t current_h264_pps;
  /**< latest HEVC SPC index  */
  uint8_t latest_hevc_sps;
  /**< array to store SPS (Sequence Parameter Set) info */
  VvasH264SeqParamSet h264_seq_parameter_set[32];
  /**< array to store PPS (Picture Parameter Set) info */
  VvasH264PicParamSet h264_pic_parameter_set[256];
  /**< array to store SPS (Sequence Parameter Set) info  for HEVC/H265 */
  VvasHevcSeqParamSet hevc_seq_parameter_set[32];
  /**< last slice header to prepare complete NALU */
  VvasH264SliceHeader last_h264_slice_header;
} VvasParserStreamInfo;

/**
 *  @fn uint32_t gcd (uint32_t a, uint32_t b);
 *  @param [in] a first number
 *  @param [in] b second number
 *  @return a number which is gcd of a and b
 *  @brief This API find the GCD of 2 number
 */
uint32_t gcd (uint32_t a, uint32_t b);

/**
 *  @fn uint32_t min(uint32_t a, uint32_t b);
 *  @param [in] a first number
 *  @param [in] b second number
 *  @return minimum of both operand
 *  @brief This API find min of the 2 number
 */
uint32_t min(uint32_t a, uint32_t b);

/**
 *  @fn void init_get_bits (VvasParserGetBits* hpgb,
 *                          const uint8_t* start,
 *                          const uint8_t* end);
 *  @param [in] hpgb pointer of handle to VvasParserGetBits object
 *  @param [in] start start pointer in the buffer to be parsed
 *  @param [in] end end pointer in the buffer to be parsed
 *  @return void
 *  @brief This API creates the bit parser object
 */
void init_get_bits (VvasParserGetBits* hpgb, const uint8_t* start,
  const uint8_t* end);

/**
 *  @fn int32_t get_bits_eof (VvasParserGetBits* hpgb);
 *  @param [in] hpgb handle pointer to VvasParserGetBits object
 *  @return returns 1 if EOF has been reached otherwise returns 0
 *  @brief checks if EOF has been reached.
 */
int32_t get_bits_eof (VvasParserGetBits* hpgb);

/**
 *  @fn uint8_t get_bits_byte (VvasParserGetBits* hpgb, uint8_t nbits);
 *  @param [in] hpgb handle pointer to VvasParserGetBits object
 *  @param [in] nbits number of bits to read
 *  @return bits read
 *  @brief reads up to 8 bits from the VvasParserGetBits object
 */
uint8_t get_bits_byte (VvasParserGetBits* hpgb, uint8_t nbits);

/**
 *  @fn uint16_t get_bits_short (VvasParserGetBits* hpgb, uint8_t nbits);
 *  @param [in] hpgb handle pointer to VvasParserGetBits object
 *  @param [in] nbits number of bits to read
 *  @return bits read
 *  @brief reads up to 16 bits from the VvasParserGetBits object
 */
uint16_t get_bits_short (VvasParserGetBits* hpgb, uint8_t nbits);

/**
 *  @fn uint32_t get_bits_long (VvasParserGetBits* hpgb, uint8_t nbits);
 *  @param [in] hpgb handle pointer to VvasParserGetBits object
 *  @param [in] nbits number of bits to read
 *  @return bits read
 *  @brief reads up to 32 bits from the VvasParserGetBits object
 */
uint32_t get_bits_long (VvasParserGetBits* hpgb, uint8_t nbits);

/**
 *  @fn uint32_t get_bits_unsigned_eg (VvasParserGetBits* hpgb);
 *  @param [in] hpgb handle pointer to VvasParserGetBits object
 *  @param [in] nbits number of bits to read
 *  @return value read
 *  @brief reads unsigned Exp-Golomb code from VvasParserGetBits object
 */
uint32_t get_bits_unsigned_eg (VvasParserGetBits* hpgb);

/**
 *  @fn int32_t get_bits_signed_eg (VvasParserGetBits* hpgb);
 *  @param [in] hpgb handle pointer to VvasParserGetBits object
 *  @param [in] nbits number of bits to read
 *  @return value read
 *  @brief reads signed Exp-Golomb code from VvasParserGetBits object
 */
int32_t get_bits_signed_eg (VvasParserGetBits* hpgb);

/**
 *  @fn int32_t find_next_start_code (VvasParserBuffer* buffer,
 *                                    int offset,
 *                                    int* ret_offset);
 *  @param [in] hpgb handle pointer to VvasParserGetBits object
 *  @param [in] nbits number of bits to read
 *  @return return offset if match found, otherwise return offset till which
      search has been completed
 *  @brief searches for the next start code ("0x000001") or ("0x00000001")
 */
int32_t find_next_start_code (VvasParserBuffer* buffer, int offset,
          int* ret_offset);

/**
 *  @fn int32_t convert_to_rbsp (VvasParserBuffer* buffer,
 *                               int start_offset,
 *                               int end_offset,
 *                               VvasParserBuffer* new_buffer);
 *  @param [in] buffer buffer containing escaped bit-stream payload
 *  @param [in] start_offset starting offset in buffer to convert
 *  @param [in] end_offset ending offset in buffer to convert
 *  @param [in] new_buffer buffer to place raw bit-stream payload into
 *  @return return P_SUCCESS on success else P_ERROR
 *  @brief convert escaped bit stream payload to raw bit-stream payload
 */
int32_t convert_to_rbsp (VvasParserBuffer* buffer, int start_offset,
          int end_offset, VvasParserBuffer* new_buffer);

/**
 *  @fn int32_t init_parse_data (VvasParserStreamInfo* parsedata);
 *  @param [in] parsedata Pointer to @ref VvasParserStreamInfo
 *  @return returns P_SUCCESS on success
 *  @brief internal API Initializes VvasParserStreamInfo
 */
int32_t init_parse_data (VvasParserStreamInfo* parsedata);

#endif // __COMMON_PARSER_H__
