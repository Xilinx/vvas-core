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

/** @file parser_h265.c
 *  @brief Contains code to parse h265 stream
 */

#include <vvas_core/vvas_parser.h>
#include "vvas_parser_priv.h"
#include "parser_h265.h"

typedef enum {
  VVAS_H265_NALU_TRAIL_N = 0,
  VVAS_H265_NALU_TRAIL_R = 1,
  VVAS_H265_NALU_TSA_N = 2,
  VVAS_H265_NALU_TSA_R = 3,
  VVAS_H265_NALU_STSA_N = 4,
  VVAS_H265_NALU_STSA_R = 5,
  VVAS_H265_NALU_RADL_N = 6,
  VVAS_H265_NALU_RADL_R = 7,
  VVAS_H265_NALU_RASL_N = 8,
  VVAS_H265_NALU_RASL_R = 9,
  VVAS_H265_NALU_BLA_W_LP = 16,
  VVAS_H265_NALU_BLA_W_RADL = 17,
  VVAS_H265_NALU_BLA_N_LP = 18,
  VVAS_H265_NALU_IDR_W_RADL = 19,
  VVAS_H265_NALU_IDR_N_LP = 20,
  VVAS_H265_NALU_CRA_NUT  = 21,
  VVAS_H265_NALU_RESERVED_VCL22 = 22,
  VVAS_H265_NALU_RESERVED_VCL23 = 23,
  /** Video parameter set */
  VVAS_H265_NALU_VPS = 32,
  /** Sequence parameter set */
  VVAS_H265_NALU_SPS = 33,
  /** Picture parameter set */
  VVAS_H265_NALU_PPS = 34,
  /** Access unit delimiter */
  VVAS_H265_NALU_AUD = 35,
  /** End of sequence */
  VVAS_H265_NALU_EOS_NUT = 36,
  /** End of bitstream */
  VVAS_H265_NALU_EOB_NUT = 37,
  /** Filler data */
  VVAS_H265_NALU_FD_NUT = 38,
  /** Supplemental enhancement information prefix */
  VVAS_H265_NALU_SEI_PREFIX = 39,
  /** Supplemental enhancement information suffix */
  VVAS_H265_NALU_SEI_SUFFIX = 40,
  VVAS_H265_NALU_RESERVED_MIN = 41,
  VVAS_H265_NALU_RESERVED_MAX = 47,
  VVAS_H265_NALU_UNSPECIFIED_MIN = 48,
  VVAS_H265_NALU_UNSPECIFIED_MAX = 63,
} VvasH265NALUType;

#define IS_H265_VCL_NALU(nalu_type) ((nalu_type) <= VVAS_H265_NALU_RESERVED_VCL23)
#define IS_H265_NONVCL_NALU(nalu_type) (((nalu_type) >= VVAS_H265_NALU_VPS) && ((nalu_type) <= VVAS_H265_NALU_UNSPECIFIED_MAX))
#define IS_H265_SUPPORTED_VCL_NALU(nalu_type) (((nalu_type) <= VVAS_H265_NALU_RASL_R) || (((nalu_type) >= VVAS_H265_NALU_BLA_W_LP) && ((nalu_type) <= VVAS_H265_NALU_CRA_NUT)))

typedef enum {
  VVAS_H265_PARSER_HAVE_SPS = 1 << 0,
  VVAS_H265_PARSER_HAVE_PPS = 1 << 1,
  VVAS_H265_PARSER_HAVE_FRAME = 1 << 2,
} VvasH265ParserState;

//#define DBG_LOG
/**
 *  @fn static int32_t decode_hevc_profile_tier_level (
 *                                          VvasParserGetBits* getbits,
 *                                          VvasHevcSeqParamSet* sps);
 *  @param [in] getbits - VvasParserGetBits object to read from
 *  @param [in] sps - structure pointer of sps
 *  @return returns P_SUCCESS on success
 *  @brief Get H.265/HEVC profile from the input
 */
static int32_t decode_hevc_profile_tier_level (VvasParserGetBits* getbits,
        VvasHevcSeqParamSet* sps)
{
  /* profile_space */
  get_bits_byte (getbits, 2);
  /* tier_flag */
  get_bits_byte (getbits, 1);
  unsigned char profile_idc = get_bits_byte (getbits, 5);

  /* profile_compatibility_flags */
  get_bits_long (getbits, 32);

  /* progressive_source_flag */
  get_bits_byte (getbits, 1);
  /* interlaced_source_flag */
  get_bits_byte (getbits, 1);
  /* non_packed_constraint_flag */
  get_bits_byte (getbits, 1);
  /* frame_only_constraint_flag */
  get_bits_byte (getbits, 1);

  get_bits_long (getbits, 32);
  get_bits_short (getbits, 12);

  if (sps)
    sps->profile_idc = profile_idc;

  return P_SUCCESS;
}

/**
 *  @fn static int32_t hevc_scaling_list_data (VvasParserGetBits* getbits)
 *  @param [in] getbits - VvasParserGetBits object to read from
 *  @return returns P_SUCCESS on success
 *  @brief Parses H.265/HEVC scaling list
 */
static int32_t hevc_scaling_list_data (VvasParserGetBits* getbits)
{
  int32_t size_id;
  for (size_id = 0; size_id < 4; size_id++)
  {
    int32_t matrix_id;
    for (matrix_id = 0; matrix_id < 6; matrix_id += ((size_id == 3) ? 3 : 1))
    {
      unsigned char scaling_list_pred_mode_flag = get_bits_byte (getbits, 1);
      if (!scaling_list_pred_mode_flag)
      {
        /* delta */
        get_bits_unsigned_eg (getbits);
      } else {
        int32_t coef_num = min (64, 1 << (4 + (size_id << 1)));
        if (size_id > 1)
        {
          /* scaling_list_dc_coef */
          get_bits_unsigned_eg (getbits);
        }
        int32_t i;
        for (i = 0; i < coef_num; i++)
        {
          /* scaling_list_delta_coef */
          get_bits_unsigned_eg (getbits);
        }
      }
    }
  }
  return P_SUCCESS;
}

/**
 *  @fn static int32_t hevc_decode_short_term_rps (VvasParserGetBits* getbits,
 *                                                 HEVCShortTermRPS* rps,
 *                                                 VvasHevcSeqParamSet* sps);
 *  @param [in] getbits - VvasParserGetBits object to read from
 *  @param [in] rps - RPS structure
 *  @param [in] sps - SPS structure
 *  @return P_SUCCESS on success
 *  @brief Parses H.265/HEVC rps header
 */
static int32_t hevc_decode_short_term_rps (VvasParserGetBits* getbits,
        HEVCShortTermRPS* rps, VvasHevcSeqParamSet* sps)
{
  int32_t i;
  int32_t k = 0;
  int32_t k0 = 0;
  int32_t delta_poc;
  unsigned char rps_predict = 0;
  if (rps != sps->st_rps && sps->nb_st_rps)
    rps_predict = get_bits_byte (getbits, 1);

  if (rps_predict)
  {
    HEVCShortTermRPS* rps_ridx = &sps->st_rps[rps - sps->st_rps - 1];
    unsigned char delta_rps_sign = get_bits_byte (getbits, 1);
    unsigned long abs_delta_rps = get_bits_unsigned_eg (getbits);
    unsigned char use_delta_flag = 0;
    int32_t delta_rps = (1 - (delta_rps_sign << 1)) * abs_delta_rps;
    for (i = 0; i <= rps_ridx->num_delta_pocs; i++)
    {
      int32_t used = rps->used[k] = get_bits_byte (getbits, 1);
      if (!used)
        use_delta_flag = get_bits_byte (getbits, 1);
      if (used || use_delta_flag)
      {
        if (i < rps_ridx->num_delta_pocs)
          delta_poc = delta_rps + rps_ridx->delta_poc[i];
        else
          delta_poc = delta_rps;
        rps->delta_poc[k] = delta_poc;
        if (delta_poc < 0)
          k0++;
        k++;
      }
    }
    rps->num_delta_pocs = k;
    rps->num_negative_pics = k0;
    if (rps->num_delta_pocs != 0)
    {
      int32_t used, tmp;
      for (i = 1; i < rps->num_delta_pocs; i++)
      {
        delta_poc = rps->delta_poc[i];
        used = rps->used[i];
        for (k = i - 1; k >= 0; k--)
        {
          tmp = rps->delta_poc[k];
          if (delta_poc < tmp)
          {
            rps->delta_poc[k + 1] = tmp;
            rps->used[k + 1] = rps->used[k];
            rps->delta_poc[k] = delta_poc;
            rps->used[k] = used;
          }
        }
      }
    }

    if ((rps->num_negative_pics >> 1) != 0)
    {
      int32_t used;
      k = rps->num_negative_pics - 1;
      for (i = 0; i < rps->num_negative_pics >> 1; i++)
      {
        delta_poc = rps->delta_poc[i];
        used = rps->used[i];
        rps->delta_poc[i] = rps->delta_poc[k];
        rps->used[i] = rps->used[k];
        rps->delta_poc[k] = delta_poc;
        rps->used[k] = used;
        k--;
      }
    }

  } else {
    unsigned long prev;
    rps->num_negative_pics = get_bits_unsigned_eg (getbits);
    unsigned long nb_positive_pics = get_bits_unsigned_eg (getbits);
    rps->num_delta_pocs = rps->num_negative_pics + nb_positive_pics;
    if (rps->num_delta_pocs)
    {
      prev = 0;
      for (i = 0; i < rps->num_negative_pics; i++)
      {
        delta_poc = get_bits_unsigned_eg (getbits) + 1;
        prev -= delta_poc;
        rps->delta_poc[i] = prev;
        rps->used[i] = get_bits_byte (getbits, 1);
      }
      prev = 0;
      for (i = 0; i < nb_positive_pics; i++) {
        delta_poc = get_bits_unsigned_eg (getbits) + 1;
        prev += delta_poc;
        rps->delta_poc[rps->num_negative_pics + i] = prev;
        rps->used[rps->num_negative_pics + i]
            = get_bits_byte (getbits, 1);
      }
    }
  }

  return P_SUCCESS;
}

/**
 *  @fn static int32_t parse_hevc_slice_header(VvasParserBuffer* in_buffer,
 *                                             VvasParserStreamInfo* parsedata,
 *                                             int32_t startoffset,
 *                                             int32_t endoffset
 *                                             VvasHevcSliceHdr *slice_hdr);
 *  @param [in] in_buffer - input buffer
 *  @param [in] parsedata - structure for parsed data
 *  @param [in] startoffset - Current offset for the input buffer
 *  @param [in] endoffset - End offset for the input buffer
 *  @return P_SUCCESS on success
 *  @brief Parses H.265/HEVC slice header
 */
static int32_t parse_hevc_slice_header(VvasParserBuffer* in_buffer,
                  VvasParserStreamInfo* parsedata, int32_t startoffset, int32_t endoffset,
                  VvasHevcSliceHdr *slice_hdr) {
  VvasParserBuffer buffer;
  int32_t ret;

  uint8_t nalhdroff = (in_buffer->data[startoffset+2] == 0 ? 4 : 3);
  uint8_t nalutype = (in_buffer->data[startoffset + nalhdroff + 1] & 0x7E) >> 1;

  memset(slice_hdr, 0, sizeof(VvasHevcSliceHdr));

  ret = convert_to_rbsp (in_buffer, startoffset, endoffset, &buffer);
  if (ret == P_ERROR)
    return P_ERROR;

  unsigned char* pt = buffer.data + nalhdroff + 2;
  unsigned char* end = buffer.data + buffer.size;
  VvasParserGetBits getbits;

  init_get_bits (&getbits, pt, end);
  slice_hdr->first_slice_segment_in_pic_flag = get_bits_byte(&getbits, 1);

  if (nalutype >= VVAS_H265_NALU_BLA_W_LP  && nalutype <= VVAS_H265_NALU_RESERVED_VCL23) {
    slice_hdr->no_output_of_prior_pics_flag = get_bits_byte(&getbits, 1);
  }

  slice_hdr->pps_id = get_bits_unsigned_eg (&getbits);

  #ifdef DBG_LOG
  printf("parse_hevc_slice_header :first_slice_segment_in_pic_flag=%d,"
    "pps_id=%d\n", slice_hdr->first_slice_segment_in_pic_flag,
    slice_hdr->pps_id);
  #endif

  return P_SUCCESS;
}

/**
 *  @fn static int32_t parse_hevc_sps (VvasParserBuffer* in_buffer,
 *                                     VvasParserStreamInfo* parsedata,
 *                                     int32_t startoffset,
 *                                     int32_t endoffset)
 *  @param [in] in_buffer - input buffer
 *  @param [in] parsedata - structure for parsed data
 *  @param [in] startoffset - Current offset for the input buffer
 *  @param [in] end_offset - End offset for the input buffer
 *  @return returns P_SUCCESS on success
 *  @brief Parses H.265/HEVC sps
 */
static int32_t parse_hevc_sps (VvasParserBuffer* in_buffer,
        VvasParserStreamInfo* parsedata, int32_t startoffset, int32_t endoffset)
{
  VvasParserBuffer buffer;
  int32_t ret;
  int32_t rem = 0;

  ret = convert_to_rbsp (in_buffer, startoffset, endoffset, &buffer);
  if (ret == P_ERROR)
    return P_ERROR;

  unsigned char* pt = buffer.data + ((buffer.data[2] == 0) ? 6 : 5);
  unsigned char* end = buffer.data + buffer.size;
  VvasParserGetBits getbits;

  init_get_bits (&getbits, pt, end);

  VvasHevcSeqParamSet sps;
  memset (&sps, 0, sizeof (sps));

  /* vps_id */
  get_bits_byte (&getbits, 4);
  unsigned char max_sub_layers = get_bits_byte (&getbits, 3) + 1;
  /* temporal_id_nesting_flag */
  get_bits_byte (&getbits, 1);

  decode_hevc_profile_tier_level (&getbits, &sps);

  sps.level_idc = get_bits_byte (&getbits, 8);

  /* Normalize the level_idc as per VDU */
  rem = sps.level_idc % 30;

  if (rem) {
    sps.level_idc = ((sps.level_idc / 3) + rem/3);
  } else {
    sps.level_idc = sps.level_idc / 3;
  }

  unsigned char sub_layer_profile_present_flag[8];
  unsigned char sub_layer_level_present_flag[8];
  int32_t i;
  for (i = 0; i < max_sub_layers - 1; i++)
  {
    sub_layer_profile_present_flag[i] = get_bits_byte (&getbits, 1);
    sub_layer_level_present_flag[i] = get_bits_byte (&getbits, 1);
  }

  if (max_sub_layers > 1)
  {
    for (i = max_sub_layers - 1; i < 8; i++)
      get_bits_byte (&getbits, 2);
  }
  for (i = 0; i < max_sub_layers - 1; i++)
  {
    if (sub_layer_profile_present_flag[i])
      decode_hevc_profile_tier_level(&getbits, NULL);
    if (sub_layer_level_present_flag[i])
    {
      /* sub_layer_ptl_level_idc */
      get_bits_byte (&getbits, 8);
    }
  }

  unsigned long sps_id = get_bits_unsigned_eg (&getbits);
  unsigned long chroma_format_idc = get_bits_unsigned_eg (&getbits);
  if (chroma_format_idc == 3)
  {
    unsigned char separate_colour_plane_flag = get_bits_byte (&getbits, 1);
    if (separate_colour_plane_flag)
      chroma_format_idc = 0;
  }

  unsigned long coded_width = get_bits_unsigned_eg (&getbits);
  unsigned long coded_height = get_bits_unsigned_eg (&getbits);
  parsedata->width = coded_width;
  parsedata->height = coded_height;

  unsigned char pic_conformance_flag = get_bits_byte (&getbits, 1);
  if (pic_conformance_flag)
  {
    int32_t vert_mult = 1 + (chroma_format_idc < 2);
    int32_t horiz_mult = 1 + (chroma_format_idc < 3);
    unsigned long output_window_left_offset, output_window_right_offset,
    output_window_top_offset, output_window_bottom_offset;
    /* output_window_left_offset */
    output_window_left_offset = get_bits_unsigned_eg (&getbits) * horiz_mult;
    /* output_window_right_offset */
    output_window_right_offset = get_bits_unsigned_eg (&getbits) * horiz_mult;
    /* output_window_top_offset */
    output_window_top_offset = get_bits_unsigned_eg (&getbits) * vert_mult;
    /* output_window_bottom_offset */
    output_window_bottom_offset = get_bits_unsigned_eg (&getbits) * vert_mult;
    parsedata->width  = coded_width - (output_window_left_offset +
                     output_window_right_offset);
    parsedata->height = coded_height - (output_window_top_offset +
                     output_window_bottom_offset);
  }

  /* bit_depth = value + 8 */
  sps.bit_depth_luma_minus8 = get_bits_unsigned_eg (&getbits);

  /* bit_depth_chroma = value + 8 */
  get_bits_unsigned_eg (&getbits);
  unsigned long log2_max_poc_lsb = get_bits_unsigned_eg (&getbits) + 4;

  unsigned char sublayer_ordering_info = get_bits_byte (&getbits, 1);
  unsigned char start = sublayer_ordering_info ? 0 : max_sub_layers - 1;
  for (i = start; i < max_sub_layers; i++)
  {
    /* temporal_layer_max_dec_pic_buffering = value + 1 */
    get_bits_unsigned_eg (&getbits);
    /* temporal_layer_num_reorder_pics */
    get_bits_unsigned_eg (&getbits);
    /* temporal_layer_max_latency_increase = value - 1 */
    get_bits_unsigned_eg (&getbits);
  }

  /* log2_min_cb_size = value + 3 */
  get_bits_unsigned_eg (&getbits);
  /* log2_diff_max_min_coding_block_size */
  get_bits_unsigned_eg (&getbits);
  /* log2_min_tb_size = value + 2 */
  get_bits_unsigned_eg (&getbits);
  /* log2_diff_max_min_transform_block_size */
  get_bits_unsigned_eg (&getbits);

  /* max_transform_hierarchy_depth_inter */
  get_bits_unsigned_eg (&getbits);
  /* max_transform_hierarchy_depth_intra */
  get_bits_unsigned_eg (&getbits);

  unsigned char scaling_list_enable_flag = get_bits_byte (&getbits, 1);
  if (scaling_list_enable_flag)
  {
    if (get_bits_byte (&getbits, 1))
      hevc_scaling_list_data (&getbits);
  }

  /* amp_enabled_flag */
  get_bits_byte (&getbits, 1);
  /* sao_enabled */
  get_bits_byte (&getbits, 1);
  unsigned char pcm_enabled_flag = get_bits_byte (&getbits, 1);
  if (pcm_enabled_flag)
  {
    /* pcm_bit_depth = value + 1 */
    get_bits_byte (&getbits, 4);
    /* pcm_bit_depth_chroma = value + 1 */
    get_bits_byte (&getbits, 4);
    /* pcm_log2_min_pcm_cb_size = value + 3 */
    get_bits_unsigned_eg (&getbits);
    /* pcm_log2_max_pcm_cb_size = pcm_log2_min_pcm_cb_size +
       get_bits_unsigned_eg (&getbits); */
    get_bits_unsigned_eg (&getbits);
    /* pcm_loop_filter_disable_flag */
    get_bits_byte (&getbits, 1);
  }

  sps.nb_st_rps = get_bits_unsigned_eg (&getbits);
  for (i = 0; i < sps.nb_st_rps; i++)
    hevc_decode_short_term_rps (&getbits, &sps.st_rps[i], &sps);

  unsigned char long_term_ref_pics_present_flag = get_bits_byte (&getbits, 1);
  if (long_term_ref_pics_present_flag)
  {
    unsigned long num_long_term_ref_pics_sps = get_bits_unsigned_eg (&getbits);
    for (i = 0; i < num_long_term_ref_pics_sps; i++)
    {
      /* lt_ref_pic_poc_lsb_sps */
      get_bits_byte (&getbits, log2_max_poc_lsb);
      /* used_by_curr_pic_lt_sps_flag */
      get_bits_byte (&getbits, 1);
    }
  }

  /* sps_temporal_mvp_enabled_flag */
  get_bits_byte (&getbits, 1);
  /* sps_strong_intra_smoothing_enable_flag */
  get_bits_byte (&getbits, 1);

  unsigned long def_disp_win_left_offset = 0;
  unsigned long def_disp_win_right_offset = 0;
  unsigned long def_disp_win_top_offset = 0;
  unsigned long def_disp_win_bottom_offset = 0;
  unsigned long vui_num_units_in_tick = 0;
  unsigned long vui_time_scale = 0;

  unsigned char vui_present = get_bits_byte (&getbits, 1);
  if (vui_present)
  {
    unsigned char sar_present = get_bits_byte (&getbits, 1);
    if (sar_present)
    {
      unsigned char sar_idx = get_bits_byte (&getbits, 8);
      if (sar_idx == 255)
      {
        /* sar_num */
        get_bits_short (&getbits, 16);
        /* sar_den */
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
        /* transfer_characteristic */
        get_bits_byte (&getbits, 8);
        /* matrix_coeffs */
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

    /* neutra_chroma_indication_flag */
    get_bits_byte (&getbits, 1);
    /* field_seq_flag */
    get_bits_byte (&getbits, 1);
    /* frame_field_info_present_flag */
    get_bits_byte (&getbits, 1);

    unsigned char default_display_window_flag = get_bits_byte (&getbits, 1);
    if (default_display_window_flag)
    {
      int32_t vert_mult = 1 + (chroma_format_idc < 2);
      int32_t horiz_mult = 1 + (chroma_format_idc < 3);
      def_disp_win_left_offset
          = get_bits_unsigned_eg (&getbits) * horiz_mult;
      def_disp_win_right_offset
          = get_bits_unsigned_eg (&getbits) * horiz_mult;
      def_disp_win_top_offset
          = get_bits_unsigned_eg (&getbits) * vert_mult;
      def_disp_win_bottom_offset
          = get_bits_unsigned_eg (&getbits) * vert_mult;
      parsedata->width = coded_width -
              (def_disp_win_left_offset + def_disp_win_right_offset);
      parsedata->height = coded_height -
              (def_disp_win_top_offset + def_disp_win_bottom_offset);
    }

    unsigned char vui_timing_info_present_flag = get_bits_byte (&getbits, 1);
    if (vui_timing_info_present_flag)
    {
      vui_num_units_in_tick = get_bits_long (&getbits, 32);
      vui_time_scale = get_bits_long (&getbits, 32);
      parsedata->fr_num = vui_time_scale;
      parsedata->fr_den = vui_num_units_in_tick;
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
  }

  if (get_bits_eof (&getbits))
  {
    free (buffer.data);
    return P_ERROR;
  }

  sps.valid = 1;

  #ifdef DBG_LOG
  printf("parse_hevc_sps: sps_id=%lu\n", sps_id);
  #endif

  memcpy(&parsedata->hevc_seq_parameter_set[sps_id], &sps,
    sizeof(VvasHevcSeqParamSet));

  free (buffer.data);

  return P_SUCCESS;
}

/**
 *  @fn static int32_t parse_h265_au (VvasParserPriv *self,
 *                                    VvasParserBuffer* in_buffer,
 *                                    VvasParserBuffer *out_buffer
 *                                    bool is_eos);
 *  @param [in] self - handle to parser object
 *  @param [in] in_buffer - input buffer
 *  @param [in] out_buffer - output buffer where access-unit has to be put into
 *  @param [in] is_eos - whether end of input buffer to be parsed
 *  @return returns P_SUCCESS on success, P_ERROR on error
 *  @brief Parse H.265/HEVC stream and returns h264 access-unit
 */
VvasReturnType
parse_h265_au (VvasParserPriv *self, VvasParserBuffer *in_buffer,
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
      LOG_MESSAGE (LOG_LEVEL_ERROR, self->log_level, "partial buffer does not start with NALU header..");
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
          "cached partial input buffer is available to parser at offset %d and size %d", cur_inbuf.offset, cur_inbuf.size);
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

    if (has_nalu_header)
      cur_inbuf.offset += 2;

    LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "start offset %d for next nalu search, size=%d", cur_inbuf.offset, cur_inbuf.size);

    /* find NALU start code */
    if (cur_inbuf.offset < cur_inbuf.size) {
      self->last_ret = find_next_start_code (&cur_inbuf, cur_inbuf.offset, &end_offset);
    } else {
      self->last_ret = P_MOREDATA;
    }

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
        self->partial_inbuf.offset = self->partial_inbuf.size - 4;
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

    uint8_t nalhdroff = (cur_inbuf.data[self->last_nalu_offset + 2] == 0 ? 4 : 3);
    uint8_t nalutype = (cur_inbuf.data[self->last_nalu_offset + nalhdroff] & 0x7E) >> 1;

    LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "received NALU type %u", nalutype);

    if (nalutype == VVAS_H265_NALU_SPS) {
      ret = parse_hevc_sps ( &cur_inbuf, &self->s_info, self->last_nalu_offset, end_offset);
      if(ret == P_SUCCESS) {
        /* received SPS */
        LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "received SPS");
        self->parse_state |= VVAS_H265_PARSER_HAVE_SPS;
      } else {
        LOG_MESSAGE (LOG_LEVEL_ERROR, self->log_level, "SPS parsing failed");
        return VVAS_RET_ERROR;
      }
    }

    /* Drop data if SPS is not received yet */
    if (!(self->parse_state & VVAS_H265_PARSER_HAVE_SPS) && IS_H265_VCL_NALU(nalutype)) {
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

    if (IS_H265_NONVCL_NALU(nalutype)) {
      if (self->has_slice && (nalutype == VVAS_H265_NALU_VPS || nalutype == VVAS_H265_NALU_SEI_PREFIX)) {
        /* received Non-VCL NAL unit, push cached frame */
        out_buffer->data = self->partial_outbuf.data;
        out_buffer->size = self->partial_outbuf.size;
        out_buffer->offset = 0;
        LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "sending out frame of size %d", self->partial_outbuf.size);

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
        self->parse_state &= ~VVAS_H265_PARSER_HAVE_FRAME;
        return VVAS_RET_SUCCESS;
      } else {
        /* copy current non-VCL NALU unit to partial output buffer */
        self->partial_outbuf.data = realloc (self->partial_outbuf.data, self->partial_outbuf.size + end_offset - self->last_nalu_offset);
        if (!self->partial_outbuf.data) {
          LOG_MESSAGE (LOG_LEVEL_ERROR, self->log_level, "failed to allocate memory");
          return VVAS_RET_ERROR;
        }
        memcpy (self->partial_outbuf.data + self->partial_outbuf.size,
            cur_inbuf.data + self->last_nalu_offset, end_offset - self->last_nalu_offset);
        self->partial_outbuf.size += end_offset - self->last_nalu_offset;

        LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "copied %d bytes to partial output buffer", end_offset - self->last_nalu_offset);
      }
    } else {
      if (IS_H265_SUPPORTED_VCL_NALU(nalutype)) {
        /* received supported nalu which can be parsed */
        ret = parse_hevc_slice_header(&cur_inbuf, &self->s_info, self->last_nalu_offset, end_offset, &self->slice_hdr);
        if (ret == P_ERROR) {
          LOG_MESSAGE (LOG_LEVEL_ERROR, self->log_level, "failed to parse slice header");
          return VVAS_RET_ERROR;
        }

        if (self->has_slice && self->slice_hdr.first_slice_segment_in_pic_flag) {
          LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "received new frame, send out cached frame");

          out_buffer->data = self->partial_outbuf.data;
          out_buffer->size = self->partial_outbuf.size;
          out_buffer->offset = 0;

          /* copy current NALU to partial input buffer to use in next iteration */
          self->partial_outbuf.size = end_offset - self->last_nalu_offset;
          self->partial_outbuf.data = malloc (self->partial_outbuf.size);
          memcpy(self->partial_outbuf.data, cur_inbuf.data+self->last_nalu_offset, self->partial_outbuf.size);
          self->last_nalu_offset = end_offset;
          if (cur_inbuf.data == self->partial_inbuf.data)
            self->partial_inbuf.offset = end_offset;
          else
            in_buffer->offset = end_offset;
          self->parse_state &= ~VVAS_H265_PARSER_HAVE_FRAME;
          if (is_eos && self->partial_inbuf.offset == self->partial_inbuf.size) {
            memset (&self->partial_inbuf, 0x0, sizeof (VvasParserBuffer));
          }

          return VVAS_RET_SUCCESS;
        } else {
          /* copy current nalu to partial output buffer */
          self->partial_outbuf.data = realloc (self->partial_outbuf.data, self->partial_outbuf.size + end_offset - self->last_nalu_offset);
          if (!self->partial_outbuf.data) {
            LOG_MESSAGE (LOG_LEVEL_ERROR, self->log_level, "failed to allocate memory");
            return VVAS_RET_ERROR;
          }
          memcpy (self->partial_outbuf.data + self->partial_outbuf.size,
              cur_inbuf.data + self->last_nalu_offset, end_offset - self->last_nalu_offset);
          self->partial_outbuf.size += end_offset - self->last_nalu_offset;

          LOG_MESSAGE (LOG_LEVEL_DEBUG, self->log_level, "copied %d bytes to partial output buffer", end_offset - self->last_nalu_offset);
        }
        self->has_slice = 1;
      }
    }
    if(is_eos && self->last_ret == P_MOREDATA) {
      out_buffer->data = self->partial_outbuf.data;
      out_buffer->size = self->partial_outbuf.size;
      out_buffer->offset = 0;

      if (self->partial_inbuf.data)
        free (self->partial_inbuf.data);

      memset (&self->partial_outbuf, 0x0, sizeof (VvasParserBuffer));
      memset (&self->partial_inbuf, 0x0, sizeof (VvasParserBuffer));

      return VVAS_RET_EOS;
    }

    self->last_nalu_offset = cur_inbuf.offset  = end_offset;
  }
  return VVAS_RET_SUCCESS;
}
