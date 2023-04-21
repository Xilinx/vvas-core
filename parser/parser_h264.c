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

/** @file parser_h264.c
 *  @brief Contains code to parse h264 stream
 */

#include <vvas_core/vvas_parser.h>
#include "parser_h264.h"
#include "vvas_parser_priv.h"

#define IS_VCL_NALU(nalutype) ((1 <= (nalutype)) && ((nalutype) <= 5))

typedef enum {
  VVAS_H264_NALU_UNSPECIFIED = 0,
  VVAS_H264_NALU_NON_IDR,
  VVAS_H264_NALU_PART_A,
  VVAS_H264_NALU_PART_B,
  VVAS_H264_NALU_PART_C,
  VVAS_H264_NALU_IDR,
  VVAS_H264_NALU_SEI,
  VVAS_H264_NALU_SPS,
  VVAS_H264_NALU_PPS,
  VVAS_H264_NALU_AUD,
  VVAS_H264_NALU_END_SEQ,
  VVAS_H264_NALU_END_STREAM,
  VVAS_H264_NALU_FILLER_DATA,
  VVAS_H264_NALU_SPS_EXT,
  VVAS_H264_NALU_PREFIX,
  VVAS_H264_NALU_SUB_SPS,
  VVAS_H264_NALU_RESERVED_1,
  VVAS_H264_NALU_RESERVED_2,
  VVAS_H264_NALU_RESERVED_3,
  VVAS_H264_NALU_AUX_WO_PART,
  VVAS_H264_NALU_EXT,
  VVAS_H264_NALU_EXT_1,
  VVAS_H264_NALU_NOT_HANDLED,
} VvasH264NALUType;

typedef enum {
  VVAS_H264_PARSER_HAVE_SPS = 1 << 0,
  VVAS_H264_PARSER_HAVE_PPS = 1 << 1,
  VVAS_H264_PARSER_HAVE_FRAME = 1 << 2,
} VvasH264ParserState;

//#define DBG_LOG
/**
 *  @fn parse_h264_scaling_list (int32_t sizeoflist,
 *                               VvasParserGetBits* getbits)
 *  @param [in] sizeoflist - size of the scaling list
 *  @param [in] getbits - VvasParserGetBits object to read from
 *  @return returns P_SUCCESS on success
 *  @brief Parse H.264 scaling list
 */
static int32_t parse_h264_scaling_list (int32_t sizeoflist,
        VvasParserGetBits* getbits)
{
  int32_t lastscale = 8;
  int32_t nextscale = 8;
  for (int32_t i = 0; i < sizeoflist; i++)
  {
    if (nextscale != 0)
    {
      int32_t deltascale = get_bits_signed_eg (getbits);
      nextscale = (lastscale + deltascale + 256) % 256;
    }
    int32_t scalinglist = (nextscale == 0) ? lastscale : nextscale;
    lastscale = scalinglist;
  }
  return P_SUCCESS;
}

/**
 *  @fn static int32_t parse_h264_sps (VvasParserPriv *self,
 *                                     VvasParserBuffer* in_buffer,
 *                                     VvasParserStreamInfo* parsedata,
 *                                     int32_t startoffset,
 *                                     int32_t end_offset)
 *  @param [in] self - Instance handle
 *  @param [in] in_buffer - input buffer
 *  @param [in] parsedata - structure for parsed data
 *  @param [in] startoffset - Current offset for the input buffer
 *  @param [in] end_offset - End offset for the input buffer
 *  @return returns P_SUCCESS on success, P_ERROR on error
 *  @brief Parse H.264 sps
 */
static int32_t parse_h264_sps (VvasParserPriv *self, VvasParserBuffer* in_buffer,
        VvasParserStreamInfo* parsedata, int32_t startoffset, int32_t end_offset)
{
  int32_t ret;
  VvasParserBuffer buffer;

  LOG_MESSAGE (LOG_LEVEL_DEBUG,  self->log_level, "start offset %d and end offset %d", startoffset, end_offset);

  ret = convert_to_rbsp (in_buffer, startoffset, end_offset, &buffer);
  if (ret == P_ERROR) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, self->log_level, "Failed in converting to rbsp in sps parsing");
    return P_ERROR;
  }

  unsigned char* pt = buffer.data + ((buffer.data[2] == 0) ? 5 : 4);
  unsigned char* end = buffer.data + buffer.size;
  VvasParserGetBits getbits;
  VvasH264SeqParamSet sps;
  int32_t i;

  memset (&sps, 0, sizeof (sps));

  init_get_bits (&getbits, pt, end);

  sps.profile_idc = get_bits_byte (&getbits, 8);
  /* constraint_set_flag */
  get_bits_byte (&getbits, 8);
  sps.level_idc = get_bits_byte (&getbits, 8);
  unsigned char seq_parameter_set_id = get_bits_unsigned_eg (&getbits);

  if ((sps.profile_idc == 100)  || (sps.profile_idc == 110) ||
      (sps.profile_idc == 122)  || (sps.profile_idc == 244) ||
      (sps.profile_idc == 44)   || (sps.profile_idc == 83)  ||
      (sps.profile_idc == 86)   || (sps.profile_idc == 118) ||
      (sps.profile_idc == 128)  || (sps.profile_idc == 138) ||
      (sps.profile_idc == 139)  || (sps.profile_idc == 134) ||
      (sps.profile_idc == 135))
  {
    sps.chroma_format_idc = get_bits_unsigned_eg (&getbits);
    if (sps.chroma_format_idc == 3)
    {
      /* residual_colour_transform_flag */
      get_bits_byte (&getbits, 1);
    }
    /* bit_depth_luma_minus8 */
    sps.bit_depth_luma_minus8 = get_bits_unsigned_eg (&getbits);
    /* bit_depth_chroma_minus8 */
    get_bits_unsigned_eg (&getbits);
    /* qpprime_y_zero_transform_bypass_flag */
    get_bits_byte (&getbits, 1);
    unsigned char seq_scaling_matrix_present_flag = get_bits_byte (&getbits, 1);

    unsigned char seq_scaling_list_present_flag[8];
    if (seq_scaling_matrix_present_flag)
    {
      for (i = 0; i < 8; i++)
      {
        /* seq_scaling_list_present_flag[8] */
        seq_scaling_list_present_flag[i] = get_bits_byte (&getbits, 1);
        if(seq_scaling_list_present_flag[i]) {
          if (i < 6)
            parse_h264_scaling_list (16, &getbits);
          else
            parse_h264_scaling_list (64, &getbits);
        }
      }
    }
  } else
    sps.chroma_format_idc = 1;

  sps.log2_max_frame_num_minus4 = get_bits_unsigned_eg (&getbits);
  sps.pic_order_cnt_type = get_bits_unsigned_eg (&getbits);

  if (sps.pic_order_cnt_type == 0)
    sps.log2_max_pic_order_cnt_lsb_minus4 = get_bits_unsigned_eg (&getbits);
  else if (sps.pic_order_cnt_type == 1)
  {
    sps.delta_pic_order_always_zero_flag = get_bits_byte (&getbits, 1);
    /* offset_for_non_ref_pic */
    get_bits_signed_eg (&getbits);
    /* offset_for_top_to_bottom_field */
    get_bits_signed_eg (&getbits);
    unsigned char num_ref_frames_in_pic_order_cnt_cycle =
      get_bits_unsigned_eg (&getbits);
    for (i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++)
      get_bits_signed_eg (&getbits);
  }

  /* num_ref_frames */
  get_bits_unsigned_eg (&getbits);
  /* gaps_in_frame_num_value_allowed_flag */
  get_bits_byte (&getbits, 1);
  sps.pic_width_in_mbs_minus1 = get_bits_unsigned_eg (&getbits);
  sps.pic_height_in_map_units_minus1 = get_bits_unsigned_eg (&getbits);
  sps.frame_mbs_only_flag = get_bits_byte (&getbits, 1);

  if (!sps.frame_mbs_only_flag)
  {
    /* mb_adaptive_frame_field_flag */
    get_bits_byte (&getbits, 1);
  }

  /* direct_8x8_inference_flag */
  get_bits_byte (&getbits, 1);
  sps.frame_cropping_flag = get_bits_byte (&getbits, 1);
  if (sps.frame_cropping_flag)
  {
    sps.frame_crop_left_offset = get_bits_unsigned_eg (&getbits);
    sps.frame_crop_right_offset = get_bits_unsigned_eg (&getbits);
    sps.frame_crop_top_offset = get_bits_unsigned_eg (&getbits);
    sps.frame_crop_bottom_offset = get_bits_unsigned_eg (&getbits);
  }

  unsigned char timing_info_present_flag = 0;
  unsigned long num_units_in_tick = 0;
  unsigned long time_scale = 0;

  unsigned char vui_parameters_present_flag = get_bits_byte (&getbits, 1);
  if (vui_parameters_present_flag)
  {
    unsigned char aspect_ratio_info_present_flag = get_bits_byte (&getbits, 1);
    if (aspect_ratio_info_present_flag)
    {
      unsigned char aspect_ratio_idc = get_bits_byte (&getbits, 8);
      if (aspect_ratio_idc == 255)
      {
        /* sar_width */
        get_bits_short (&getbits, 16);
        /* sar_height */
        get_bits_short (&getbits, 16);
      }
    }

    unsigned char overscan_info_present_flag = get_bits_byte (&getbits, 1);
    if (overscan_info_present_flag)
    {
      /* overscan_appropriate_flag */
      get_bits_byte (&getbits, 1);
    }

    unsigned char video_signal_type_present_flag = get_bits_byte (&getbits, 1);
    if (video_signal_type_present_flag)
    {
      /* video_format */
      get_bits_byte (&getbits, 3);
      /* video_full_range_flag */
      get_bits_byte (&getbits, 1);
      unsigned char colour_description_present_flag =
        get_bits_byte (&getbits, 1);
      if (colour_description_present_flag)
      {
        /* colour_primaries */
        get_bits_byte (&getbits, 8);
        /* transfer_characteristics */
        get_bits_byte (&getbits, 8);
        /* matrix_coefficients */
        get_bits_byte (&getbits, 8);
      }
    }

    unsigned char chroma_loc_info_present_flag = get_bits_byte (&getbits, 1);
    if (chroma_loc_info_present_flag)
    {
      /* chroma_sample_loc_type_top_field */
      get_bits_unsigned_eg (&getbits);
      /* chroma_sample_loc_type_bottom_field */
      get_bits_unsigned_eg (&getbits);
    }

    timing_info_present_flag = get_bits_byte (&getbits, 1);
    if (timing_info_present_flag)
    {
      num_units_in_tick = get_bits_long (&getbits, 32);
      time_scale = get_bits_long (&getbits, 32);
      /* fixed_frame_rate_flag */
      get_bits_byte (&getbits, 1);
    }
  }

  parsedata->height = ((2 - sps.frame_mbs_only_flag) *
      (sps.pic_height_in_map_units_minus1 + 1)) * 16;
  parsedata->width = (sps.pic_width_in_mbs_minus1 + 1) * 16;
  if (sps.frame_cropping_flag)
  {
    int32_t cropunitx = 0, cropunity = 0;
    if (sps.chroma_format_idc == 0)
    {
      // mono
      cropunitx = 1;
      cropunity = 2 - sps.frame_mbs_only_flag;
    } else if (sps.chroma_format_idc == 1)
    {
      // 4:2:0
      cropunitx = 2;
      cropunity = 2 * (2 - sps.frame_mbs_only_flag);
    } else if (sps.chroma_format_idc == 2)
    {
      // 4:2:2
      cropunitx = 2;
      cropunity = 2 - sps.frame_mbs_only_flag;
    } else if (sps.chroma_format_idc == 3)
    {
      // 4:4:4
      cropunitx = 1;
      cropunity = 2 - sps.frame_mbs_only_flag;
    }
    parsedata->width -= cropunitx * (sps.frame_crop_left_offset +
        sps.frame_crop_right_offset);
    parsedata->height -= cropunity * (sps.frame_crop_top_offset +
        sps.frame_crop_bottom_offset);
  }

  if (timing_info_present_flag)
  {
    parsedata->fr_num = time_scale;
    parsedata->fr_den = num_units_in_tick * 2;
    if (!parsedata->fr_den) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, LOG_LEVEL_ERROR, "Invalid framerate");
      free (buffer.data);
      return P_ERROR;
    }
    if (parsedata->fr_num) {
      unsigned long temp = gcd (parsedata->fr_num, parsedata->fr_den);
      parsedata->fr_num /= temp;
      parsedata->fr_den /= temp;
    }
  }

  sps.valid = 1;
  parsedata->h264_seq_parameter_set[seq_parameter_set_id] = sps;

  free (buffer.data);

  return P_SUCCESS;
}

/**
 *  @fn static int32_t parse_h264_pps (VvasParserPriv *self,
 *                                     VvasParserBuffer* in_buffer,
 *                                     VvasParserStreamInfo* parsedata,
 *                                     int32_t startoffset,
 *                                     int32_t end_offset)
 *  @param [in] self - Instance handle
 *  @param [in] in_buffer - input buffer
 *  @param [in] parsedata - structure for parsed data
 *  @param [in] startoffset - Current offset for the input buffer
 *  @param [in] end_offset - End offset for the input buffer
 *  @return returns P_SUCCESS on success, P_ERROR on error
 *  @brief Parse H.264 pps
 */
static int32_t parse_h264_pps (VvasParserPriv *self, VvasParserBuffer* in_buffer,
        VvasParserStreamInfo* parsedata, int32_t startoffset, int32_t end_offset)
{
  int32_t ret;
  VvasParserBuffer buffer;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "start offset %d and end offset %d", startoffset, end_offset);

  ret = convert_to_rbsp (in_buffer, startoffset, end_offset, &buffer);
  if (ret == P_ERROR)
    return P_ERROR;

  unsigned char* pt = buffer.data + ((buffer.data[2] == 0) ? 5 : 4);
  unsigned char* end = buffer.data + buffer.size;
  VvasParserGetBits getbits;
  VvasH264PicParamSet pps;

  memset (&pps, 0, sizeof (pps));

  init_get_bits (&getbits, pt, end);

  unsigned char pic_parameter_set_id = get_bits_unsigned_eg (&getbits);
  pps.seq_parameter_set_id = get_bits_unsigned_eg (&getbits);
  /* entropy_coding_mode_flag */
  get_bits_byte (&getbits, 1);
  pps.pic_order_present_flag = get_bits_byte (&getbits, 1);

  if (get_bits_eof (&getbits))
  {
    free (buffer.data);
    return P_ERROR;
  }

  pps.valid = 1;
  parsedata->h264_pic_parameter_set[pic_parameter_set_id] = pps;

  LOG_MESSAGE (LOG_LEVEL_INFO,  self->log_level, "adding PPS with id %d", pic_parameter_set_id);

  free (buffer.data);
  return P_SUCCESS;
}

/**
 *  @fn static int32_t parse_h264_slice_header (VvasParserPriv *self,
 *                                          VvasParserBuffer* in_buffer,
 *                                          VvasParserStreamInfo* parsedata,
 *                                          VvasH264SliceHeader *sliceheader,
 *                                          int32_t startoffset,
 *                                          int32_t end_offset)
 *  @param [in] self - Instance handle
 *  @param [in] in_buffer - input buffer
 *  @param [in] parsedata - structure for parsed data
 *  @param [in] sliceheader - Slice header structure
 *  @param [in] startoffset - Current offset for the input buffer
 *  @param [in] end_offset - End offset for the input buffer
 *  @return returns P_SUCCESS on success, P_ERROR on error
 *  @brief Parse H.264 slice header
 */
static int32_t parse_h264_slice_header (VvasParserPriv *self, VvasParserBuffer* in_buffer,
                                        VvasParserStreamInfo* parsedata,
                                        VvasH264SliceHeader *sliceheader,
                                        int32_t startoffset, int32_t end_offset)
{
  int32_t ret;
  VvasParserBuffer buffer;

  LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "start offset %d and end offset %d", startoffset, end_offset);

  ret = convert_to_rbsp (in_buffer, startoffset, end_offset, &buffer);
  if (ret == P_ERROR) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, self->log_level, "Got Error in slice header");
    return P_ERROR;
  }

  unsigned char* pt = buffer.data + ((buffer.data[2] == 0) ? 4 : 3);
  unsigned char* end = buffer.data + buffer.size;
  VvasParserGetBits getbits;

  init_get_bits (&getbits, pt, end);

  get_bits_byte (&getbits, 1); // skip forbidden zero bit
  sliceheader->nal_ref_idc = get_bits_byte (&getbits, 2);
  sliceheader->nal_unit_type = get_bits_byte (&getbits, 5);

  /* first_mb_in_slice */
  get_bits_unsigned_eg (&getbits);
  /* slice_type */
  get_bits_unsigned_eg (&getbits);
  sliceheader->pic_parameter_set_id = get_bits_unsigned_eg (&getbits);
  parsedata->current_h264_pps = sliceheader->pic_parameter_set_id;

  VvasH264PicParamSet pps = parsedata->h264_pic_parameter_set[
    sliceheader->pic_parameter_set_id];
  if (!pps.valid)
  {
    free (buffer.data);
    LOG_MESSAGE (LOG_LEVEL_ERROR, self->log_level, "Got Error in slice header pps corresponding to pps_id=%d"
      " is not valid ", sliceheader->pic_parameter_set_id);
    return P_ERROR;
  }
  VvasH264SeqParamSet sps = parsedata->h264_seq_parameter_set[
    pps.seq_parameter_set_id];
  if (!sps.valid)
  {
    free (buffer.data);
    LOG_MESSAGE (LOG_LEVEL_ERROR, self->log_level, "Got Error in slice header");
    return P_ERROR;
  }

  sliceheader->frame_num = get_bits_short (&getbits,
      sps.log2_max_frame_num_minus4 + 4);

  if (!sps.frame_mbs_only_flag)
  {
    sliceheader->field_pic_flag = get_bits_byte (&getbits, 1);
    if (sliceheader->field_pic_flag)
      sliceheader->bottom_field_flag = get_bits_byte (&getbits, 1);
  }

  if (sliceheader->nal_unit_type == 5)
    sliceheader->idr_pic_id = get_bits_unsigned_eg (&getbits);

  if (sps.pic_order_cnt_type == 0)
  {
    sliceheader->pic_order_cnt_lsb = get_bits_short (&getbits,
        sps.log2_max_pic_order_cnt_lsb_minus4 + 4);

    if (pps.pic_order_present_flag && !sliceheader->field_pic_flag)
      sliceheader->delta_pic_order_cnt_bottom =
        get_bits_signed_eg (&getbits);
  }
  if (sps.pic_order_cnt_type == 1 && !sps.delta_pic_order_always_zero_flag)
  {
    sliceheader->delta_pic_order_cnt[0] = get_bits_signed_eg (&getbits);
    if (pps.pic_order_present_flag && !sliceheader->field_pic_flag)
      sliceheader->delta_pic_order_cnt[1] = get_bits_signed_eg (&getbits);
  }

  if (get_bits_eof (&getbits))
  {
    free (buffer.data);
    LOG_MESSAGE (LOG_LEVEL_ERROR, self->log_level, "Got error in slice header parsing");
    return P_ERROR;
  }

  free (buffer.data);

  return P_SUCCESS;
}

/**
 *  @fn static uint8_t is_new_frame (VvasParserStreamInfo *parsedata,
 *                                   VvasH264SliceHeader *sliceheader,
 *                                   VvasH264SeqParamSet *sps)
 *  @param [in] parsedata - Handle to stream info
 *  @param [in] sliceheader - Handle to cvurrent slice
 *  @param [in] sps - Handle to current SPS
 *  @return returns 1 if current slice belongs to a new frame, else 0
 */
static uint8_t
is_new_frame (VvasParserStreamInfo *parsedata, VvasH264SliceHeader *sliceheader, VvasH264SeqParamSet *sps)
{
  uint8_t isnewpic = 0;

  if (parsedata->last_h264_slice_header.frame_num != sliceheader->frame_num)
    isnewpic = 1;

  if (parsedata->last_h264_slice_header.pic_parameter_set_id !=
      sliceheader->pic_parameter_set_id)
    isnewpic = 1;

  if (parsedata->last_h264_slice_header.field_pic_flag !=
      sliceheader->field_pic_flag)
    isnewpic = 1;

  if ((sps->frame_mbs_only_flag) &&
      (parsedata->last_h264_slice_header.field_pic_flag) &&
      (sliceheader->field_pic_flag) &&
      (parsedata->last_h264_slice_header.bottom_field_flag !=
       sliceheader->bottom_field_flag))
    isnewpic = 1;

  if ((parsedata->last_h264_slice_header.nal_ref_idc !=
       sliceheader->nal_ref_idc) &&
      ((parsedata->last_h264_slice_header.nal_ref_idc == 0) ||
       (sliceheader->nal_ref_idc == 0)))
    isnewpic = 1;

  if ((sps->pic_order_cnt_type == 0) &&
      ((parsedata->last_h264_slice_header.pic_order_cnt_lsb !=
        sliceheader->pic_order_cnt_lsb) ||
       (parsedata->last_h264_slice_header.delta_pic_order_cnt_bottom !=
        sliceheader->delta_pic_order_cnt_bottom)))
    isnewpic = 1;

  if ((sps->pic_order_cnt_type == 1) &&
      ((parsedata->last_h264_slice_header.delta_pic_order_cnt[0] !=
        sliceheader->delta_pic_order_cnt[0]) ||
       (parsedata->last_h264_slice_header.delta_pic_order_cnt[1] !=
        sliceheader->delta_pic_order_cnt[1])))
    isnewpic = 1;

  if ((parsedata->last_h264_slice_header.nal_unit_type !=
       sliceheader->nal_unit_type) &&
      ((parsedata->last_h264_slice_header.nal_unit_type == VVAS_H264_NALU_IDR) ||
      (sliceheader->nal_unit_type == VVAS_H264_NALU_IDR)))
    isnewpic = 1;

  if ((parsedata->last_h264_slice_header.nal_unit_type == VVAS_H264_NALU_IDR) &&
      (sliceheader->nal_unit_type == VVAS_H264_NALU_IDR) &&
      (parsedata->last_h264_slice_header.idr_pic_id !=
       sliceheader->idr_pic_id))
    isnewpic = 1;

  return isnewpic;

}

/**
 *  @fn static VvasReturnType copy_to_partial_outbuff(VvasParserPriv *self,
 *                                        VvasParserBuffer *cur_inbuf,
 *                                        uint32_t end_offset)
 *  @param [in] self - handle to parser object
 *  @param [in] cur_inbuf - input buffer
 *  @param [in] end_offset - end offset into the above input buffer
 *  @return returns VVAS_RET_SUCCESS on success, VVAS_RET_ERROR on error
 *  @brief copy current buffer into the partial output buffer
 */
static VvasReturnType copy_to_partial_outbuff(VvasParserPriv *self,
            VvasParserBuffer *cur_inbuf, uint32_t end_offset)
{
  /* copy current nalu to partial output buffer */
  self->partial_outbuf.data
   = realloc (self->partial_outbuf.data,
              self->partial_outbuf.size + end_offset - self->last_nalu_offset);
  if (!self->partial_outbuf.data) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, self->log_level, "failed to allocate memory");
    return VVAS_RET_ERROR;
  }
  memcpy (self->partial_outbuf.data + self->partial_outbuf.size,
          cur_inbuf->data + self->last_nalu_offset,
          end_offset - self->last_nalu_offset);

  self->partial_outbuf.size += end_offset - self->last_nalu_offset;

  LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level,
      "copied %d bytes to partial output buffer, size=%d",
      end_offset - self->last_nalu_offset, self->partial_outbuf.size);

  return VVAS_RET_SUCCESS;
}

/**
 *  @fn static int32_t parse_h264_au (VvasParserPriv *self,
 *                                    VvasParserBuffer* in_buffer,
 *                                    VvasParserBuffer *out_buffer
 *                                    bool is_eos);
 *  @param [in] self - handle to parser object
 *  @param [in] in_buffer - input buffer
 *  @param [in] out_buffer - output buffer where access-unit has to be put into
 *  @param [in] is_eos - whether end of input buffer to be parsed
 *  @return returns P_SUCCESS on success, P_ERROR on error
 *  @brief Parse H.264 and returns h264 access-unit
 */
VvasReturnType
parse_h264_au (VvasParserPriv *self, VvasParserBuffer *in_buffer,
    VvasParserBuffer *out_buffer, bool is_eos)
{
  int32_t end_offset = 0;
  int32_t cur_sz = 0;
  int32_t ret = P_ERROR;
  VvasParserBuffer cur_inbuf = {0, };
  uint8_t input_taken = 1;
  uint8_t has_nalu_header = 0;

  if (self->partial_inbuf.data) {
    if (!IS_NALU_HEADER (self->partial_inbuf.data)) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, self->log_level, "partial in-buffer does not start with NALU header..");
      return VVAS_RET_ERROR;
    }

    if (self->last_ret == P_MOREDATA && !is_eos && in_buffer->data) {
      self->last_nalu_offset = 0; /* partial inbuf is starting with NALU header */
      /* we already having one NALU header and looking for next */
      has_nalu_header = 1;

      /* append input buffer to partial buffer from previous iteration */
      cur_sz = self->partial_inbuf.size + in_buffer->size - in_buffer->offset;

      self->partial_inbuf.data = realloc (self->partial_inbuf.data, cur_sz);
      if (!self->partial_inbuf.data) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, self->log_level, "failed to allocate memory");
        return VVAS_RET_ALLOC_ERROR;
      }

      memcpy (self->partial_inbuf.data + self->partial_inbuf.size,
          in_buffer->data + in_buffer->offset, in_buffer->size-in_buffer->offset);

      self->partial_inbuf.size += in_buffer->size - in_buffer->offset;

      in_buffer->offset = in_buffer->size; /* consumed entire input buffer */

      LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level,
          "appended input buffer to partial in-buffer. total size = %d and read offset = %d",
          self->partial_inbuf.size, self->partial_inbuf.offset);

      cur_inbuf.data = self->partial_inbuf.data;
      cur_inbuf.size = self->partial_inbuf.size;
      cur_inbuf.offset = self->partial_inbuf.offset;
    } else {
      /* don't consume new input buffer, as we have pending partial buffer */
      cur_inbuf.data = self->partial_inbuf.data;
      cur_inbuf.size = self->partial_inbuf.size;
      cur_inbuf.offset = self->last_nalu_offset; //self->partial_inbuf.data[2] == 0 ? self->last_nalu_offset + 2 : self->last_nalu_offset + 1;
      /* we already having one NALU header and looking for next */
      has_nalu_header = 1;
      input_taken = 0;

      LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level,
          "cached partial in-buffer is available to parser at offset %d and size %d", cur_inbuf.offset, cur_inbuf.size);
    }
  } else {
    if (!is_eos) {
      cur_inbuf.data = in_buffer->data;
      cur_inbuf.size = in_buffer->size;
      cur_inbuf.offset = in_buffer->offset;
      LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level,
          "received input buffer with size = %d and offset = %d", cur_inbuf.size, cur_inbuf.offset);
    } else {
      if (self->partial_outbuf.data) {
        LOG_MESSAGE (LOG_LEVEL_INFO, self->log_level,
            "send out pending partial output buffer");
        out_buffer->data = self->partial_outbuf.data;
        out_buffer->size = self->partial_outbuf.size;
      }

      if (self->partial_inbuf.data)
        free (self->partial_inbuf.data);

      memset (&self->partial_inbuf, 0x0, sizeof (VvasParserBuffer));
      memset (&self->partial_outbuf, 0x0, sizeof (VvasParserBuffer));

      return VVAS_RET_EOS;
    }
  }

  while (true) {
    VvasH264SliceHeader sliceheader = {0, };
    VvasParserStreamInfo *parsedata = NULL;

    if (has_nalu_header)
      cur_inbuf.offset += 2;

    LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "start offset %d for next nalu search", cur_inbuf.offset);

    /* find NALU start code */
    self->last_ret = find_next_start_code (&cur_inbuf, cur_inbuf.offset, &end_offset);
    if (self->last_ret == P_MOREDATA) {
      if (!is_eos) {
        uint8_t* tmp_partial_inbuf = NULL;

        if (!input_taken) {
          in_buffer->offset = 0;
          LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "input not consumed");
        } else {
          in_buffer->offset = in_buffer->size;
        }
        tmp_partial_inbuf = malloc (end_offset - self->last_nalu_offset);
        if (!tmp_partial_inbuf) {
          LOG_MESSAGE (LOG_LEVEL_ERROR, self->log_level, "failed to allocate memory");
          return VVAS_RET_ERROR;
        }
        memcpy (tmp_partial_inbuf, cur_inbuf.data+self->last_nalu_offset, end_offset - self->last_nalu_offset);

        if (self->partial_inbuf.data)
          free (self->partial_inbuf.data);

        self->partial_inbuf.data = tmp_partial_inbuf;
        self->partial_inbuf.size = end_offset - self->last_nalu_offset;
        self->partial_inbuf.offset = self->partial_inbuf.size - 4;//2;//3;
        self->last_nalu_offset = 0; /* partial buffer starting will be a NALU header */

        LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "need more data to prepare NALU. partial in-buffer size %d", self->partial_inbuf.size);

        return VVAS_RET_NEED_MOREDATA;
      } else {
        /* on EOS and we have a pending slice */
        LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "received last NALU : EOS");

        if (self->partial_outbuf.data == NULL) {
          out_buffer->data = malloc (end_offset - self->last_nalu_offset);
          if (!out_buffer->data) {
            LOG_MESSAGE (LOG_LEVEL_ERROR, self->log_level, "failed to allocate memory");
            return VVAS_RET_ERROR;
          }

          memcpy (out_buffer->data, cur_inbuf.data + self->last_nalu_offset, end_offset - self->last_nalu_offset);

          out_buffer->size = end_offset - self->last_nalu_offset;
          out_buffer->offset = 0;

          self->has_slice = 0;
          self->partial_outbuf.size = 0;

          if (self->partial_inbuf.data)
            free (self->partial_inbuf.data);

          LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "sending buffer out with size %d", out_buffer->size);

          memset (&self->partial_inbuf, 0x0, sizeof (VvasParserBuffer));
          memset (&self->partial_outbuf, 0x0, sizeof (VvasParserBuffer));

          return VVAS_RET_EOS;
        }
      }
    } else if (self->last_ret == P_SUCCESS ) {
      LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "NALU start code found at offset %d", end_offset);

      if (!has_nalu_header) {
        LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "This first NALU in current iteration");
        has_nalu_header = 1;
        cur_inbuf.offset = end_offset;
        self->last_nalu_offset = end_offset;
        continue; /* search for another NALU start code to prepare current NALU */
      }
    } else {
      LOG_MESSAGE (LOG_LEVEL_ERROR, self->log_level, "unhandled return value from API");
      return VVAS_RET_ERROR;
    }

    uint8_t nalhdroff
        = (cur_inbuf.data[self->last_nalu_offset + 2] == 0 ? 4 : 3);
    uint8_t nalutype
        = cur_inbuf.data[self->last_nalu_offset + nalhdroff] & 0x1F;

    LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "received NALU type %u", nalutype);

    if (nalutype == VVAS_H264_NALU_SPS) {
      /* received SPS nalu */
      ret = parse_h264_sps (self, &cur_inbuf, &self->s_info, self->last_nalu_offset, end_offset);
      if(ret == P_SUCCESS) {
        /* received SPS */
        LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "received SPS");
        self->parse_state |= VVAS_H264_PARSER_HAVE_SPS;
      } else {
        LOG_MESSAGE (LOG_LEVEL_ERROR, self->log_level, "SPS parsing failed");
        return VVAS_RET_ERROR;
      }
    } else if (nalutype == VVAS_H264_NALU_PPS) {
      /* received PPS nalu */
      ret = parse_h264_pps (self, &cur_inbuf, &self->s_info, self->last_nalu_offset, end_offset);
      if(ret == P_SUCCESS) {
        /* received PPS */
        LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "received PPS");
        self->parse_state |= VVAS_H264_PARSER_HAVE_PPS;
      }
    } else {
      if (!(self->parse_state & VVAS_H264_PARSER_HAVE_SPS)) {
        LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "frame received without SPS. ignore current NALU");
        if (self->partial_outbuf.data) {
          free(self->partial_outbuf.data);
          self->partial_outbuf.data = NULL;
        }
        self->partial_outbuf.size = 0;
        self->partial_outbuf.offset = 0;

        cur_inbuf.offset = end_offset;
        LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "moving offset to %d", cur_inbuf.offset);

        self->last_nalu_offset = end_offset;
        continue;
      }
    }

    if (IS_VCL_NALU (nalutype)) {
      ret = parse_h264_slice_header (self, &cur_inbuf, &self->s_info,
              &sliceheader, self->last_nalu_offset, end_offset);
      if (ret == P_ERROR) {
        LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "ERROR: Error parsing H.264 slice header");
        return VVAS_RET_ERROR;
      }
    }

    LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "has slice = %d", self->has_slice);

    if (((nalutype == VVAS_H264_NALU_SEI) || (nalutype == VVAS_H264_NALU_SPS) ||
        (nalutype == VVAS_H264_NALU_PPS) || (nalutype == VVAS_H264_NALU_AUD) || (nalutype == VVAS_H264_NALU_PREFIX) ||
        (nalutype == VVAS_H264_NALU_SUB_SPS) || (nalutype == VVAS_H264_NALU_RESERVED_1) || (nalutype == VVAS_H264_NALU_RESERVED_2) ||
        (nalutype == VVAS_H264_NALU_RESERVED_3) || (nalutype >= VVAS_H264_NALU_NOT_HANDLED))) {
      if (self->has_slice == 1) {
        out_buffer->data = self->partial_outbuf.data;
        out_buffer->size = self->partial_outbuf.size;
        out_buffer->offset = 0;

        if (cur_inbuf.data == self->partial_inbuf.data)
          self->partial_inbuf.offset = end_offset;
        else
          in_buffer->offset = end_offset;

        /* copy current NALU to partial input buffer to use in next iteration */
        self->partial_outbuf.size = end_offset - self->last_nalu_offset;
        self->partial_outbuf.data = malloc (self->partial_outbuf.size);
        memcpy(self->partial_outbuf.data, cur_inbuf.data+self->last_nalu_offset, self->partial_outbuf.size);

        LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "copy %d bytes to partial output buffer, size=%d", self->partial_outbuf.size, self->partial_outbuf.size);

        self->last_nalu_offset = end_offset;
        self->has_slice = 0;

        /* clear HAVE_FRAME */
        self->parse_state &= ~VVAS_H264_PARSER_HAVE_FRAME;
        return VVAS_RET_SUCCESS;
      } else {
        copy_to_partial_outbuff(self, &cur_inbuf, end_offset);
      }
    }

    if (IS_VCL_NALU (nalutype)) {
      parsedata = &self->s_info;

      if (!self->has_slice) {
        copy_to_partial_outbuff(self, &cur_inbuf, end_offset);

        self->has_slice = 1;
        cur_inbuf.offset = end_offset;
        self->last_nalu_offset = end_offset;
        memcpy(&self->s_info.last_h264_slice_header, &sliceheader,
          sizeof(VvasH264SliceHeader));

        if (!is_eos || self->last_ret == P_SUCCESS ) {
          continue;
        } else {
          out_buffer->data = self->partial_outbuf.data;
          out_buffer->size = self->partial_outbuf.size;
          out_buffer->offset = 0;

          if (self->partial_inbuf.data)
            free (self->partial_inbuf.data);

          memset (&self->partial_outbuf, 0x0, sizeof (VvasParserBuffer));
          memset (&self->partial_inbuf, 0x0, sizeof (VvasParserBuffer));

          return VVAS_RET_EOS;
        }
      }

      if (!parsedata->h264_pic_parameter_set [
          sliceheader.pic_parameter_set_id].valid) {
        cur_inbuf.offset = end_offset;
        self->last_nalu_offset = end_offset;
        continue;
      }

      if (!parsedata->h264_seq_parameter_set [
          parsedata->h264_pic_parameter_set [
          sliceheader.pic_parameter_set_id].
          seq_parameter_set_id].valid) {
        cur_inbuf.offset = end_offset;
        self->last_nalu_offset = end_offset;
        continue;
      }

      VvasH264SeqParamSet sps
        = parsedata->h264_seq_parameter_set[parsedata->h264_pic_parameter_set[
            sliceheader.pic_parameter_set_id].seq_parameter_set_id];

      unsigned char isnewpic = 0;

      isnewpic = is_new_frame (parsedata, &sliceheader, &sps);
      parsedata->last_h264_slice_header = sliceheader;

      if (isnewpic) {
        LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "received new frame, send out cached frame");

        out_buffer->data = self->partial_outbuf.data;
        out_buffer->size = self->partial_outbuf.size;
        out_buffer->offset = 0;

        if (cur_inbuf.data == self->partial_inbuf.data)
          self->partial_inbuf.offset = end_offset;
        else
          in_buffer->offset = end_offset;

        /* copy current NALU to partial input buffer to use in next iteration */
        self->partial_outbuf.size = end_offset - self->last_nalu_offset;
        self->partial_outbuf.data = malloc (self->partial_outbuf.size);
        memcpy(self->partial_outbuf.data, cur_inbuf.data+self->last_nalu_offset, self->partial_outbuf.size);

        self->last_nalu_offset = end_offset;
        self->parse_state &= ~VVAS_H264_PARSER_HAVE_FRAME;
        if (is_eos && self->partial_inbuf.offset == self->partial_inbuf.size) {
          memset (&self->partial_inbuf, 0x0, sizeof (VvasParserBuffer));
        }

        return VVAS_RET_SUCCESS;
      } else {
        copy_to_partial_outbuff(self, &cur_inbuf, end_offset);

        self->has_slice = 1;
        cur_inbuf.offset = end_offset;
        self->last_nalu_offset = end_offset;

        if (is_eos && self->last_ret == P_MOREDATA) {
          out_buffer->data = self->partial_outbuf.data;
          out_buffer->size = self->partial_outbuf.size;
          out_buffer->offset = 0;

          if (self->partial_inbuf.data)
            free (self->partial_inbuf.data);

          memset (&self->partial_inbuf, 0x0, sizeof (VvasParserBuffer));
          memset (&self->partial_outbuf, 0x0, sizeof (VvasParserBuffer));

          return VVAS_RET_EOS;
        }
      }
    }

    self->last_nalu_offset = cur_inbuf.offset  = end_offset;
  }

  return VVAS_RET_SUCCESS;
}
