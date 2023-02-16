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

#include <vvas_core/vvas_parser.h>
#include "parser_common.h"

/**
 *  @fn uint32_t gcd (uint32_t a, uint32_t b);
 *  @param [in] a - first number
 *  @param [in] b - second number
 *  @return a number which is gcd of a and b
 *  @brief This API find the GCD of 2 number
 */
uint32_t gcd (uint32_t a, uint32_t b)
{
  while(b != 0) {
    uint32_t t = b;
    b = a % b;
    a = t;
  }
  return a;
}

/**
 *  @fn uint32_t min(uint32_t a, uint32_t b);
 *  @param [in] a - first number
 *  @param [in] b - second number
 *  @return minimum of both operand
 *  @brief This API find min of the 2 number
 */
uint32_t min(uint32_t a, uint32_t b) {
  return (a>b ? b : a);
}

/**
 *  @fn void init_get_bits (VvasParserGetBits* hpgb,
 *                          const uint8_t* start,
 *                          const uint8_t* end);
 *  @param [in] hpgb - pointer of handle to VvasParserGetBits object
 *  @param [in] start - start pointer in the buffer to be parsed
 *  @param [in] end - end pointer in the buffer to be parsed
 *  @return void
 *  @brief This API creates the bit parser object
 */
void init_get_bits (VvasParserGetBits* hpgb, const uint8_t* start,
  const uint8_t* end)
{
  hpgb->start = start;
  hpgb->end = end;
  hpgb->offset_bytes = 0;
  hpgb->offset_bits = 0;
}

/**
 *  @fn int32_t get_bits_eof (VvasParserGetBits* hpgb);
 *  @param [in] hpgb - handle pointer to VvasParserGetBits object
 *  @return returns 1 if EOF has been reached otherwise returns 0
 *  @brief checks if EOF has been reached.
 */
int32_t get_bits_eof (VvasParserGetBits* hpgb)
{
  if (hpgb->start + hpgb->offset_bytes >= hpgb->end)
    return 1;
  return 0;
}

/**
 *  @fn uint8_t get_bits_byte (VvasParserGetBits* hpgb, uint8_t nbits);
 *  @param [in] hpgb - handle pointer to VvasParserGetBits object
 *  @param [in] nbits - number of bits to read
 *  @return bits read
 *  @brief reads up to 8 bits from the VvasParserGetBits object
 */
uint8_t get_bits_byte (VvasParserGetBits* hpgb, uint8_t nbits)
{
  uint8_t ret = 0;
  while (nbits > 0)
  {
    if (get_bits_eof (hpgb))
      return ret << nbits;
    while (hpgb->offset_bits < 8)
    {
      ret = (ret << 1) | ((hpgb->start[hpgb->offset_bytes] >>
              (7 - hpgb->offset_bits)) & 0x01);
      hpgb->offset_bits++;
      nbits--;
      if (nbits == 0)
      {
        if (hpgb->offset_bits == 8)
        {
          hpgb->offset_bytes++;
          hpgb->offset_bits = 0;
        }
        return ret;
      }
    }
    hpgb->offset_bytes++;
    hpgb->offset_bits = 0;
  }
  return ret;
}

/**
 *  @fn uint16_t get_bits_short (VvasParserGetBits* hpgb, uint8_t nbits);
 *  @param [in] hpgb - handle pointer to VvasParserGetBits object
 *  @param [in] nbits - number of bits to read
 *  @return bits read
 *  @brief reads up to 16 bits from the VvasParserGetBits object
 */
uint16_t get_bits_short (VvasParserGetBits* hpgb, uint8_t nbits)
{
  uint16_t ret = 0;
  while (nbits > 0)
  {
    if (get_bits_eof (hpgb))
      return ret << nbits;
    while (hpgb->offset_bits < 8)
    {
      ret = (ret << 1) | ((hpgb->start[hpgb->offset_bytes] >>
              (7 - hpgb->offset_bits)) & 0x01);
      hpgb->offset_bits++;
      nbits--;
      if (nbits == 0)
      {
        if (hpgb->offset_bits == 8)
        {
          hpgb->offset_bytes++;
          hpgb->offset_bits = 0;
        }
        return ret;
      }
    }
    hpgb->offset_bytes++;
    hpgb->offset_bits = 0;
  }
  return ret;
}

/**
 *  @fn uint32_t get_bits_long (VvasParserGetBits* hpgb, uint8_t nbits);
 *  @param [in] hpgb - handle pointer to VvasParserGetBits object
 *  @param [in] nbits - number of bits to read
 *  @return bits read
 *  @brief reads up to 32 bits from the VvasParserGetBits object
 */
uint32_t get_bits_long (VvasParserGetBits* hpgb, uint8_t nbits)
{
  uint32_t ret = 0;
  while (nbits > 0)
  {
    if (get_bits_eof (hpgb))
      return ret << nbits;
    while (hpgb->offset_bits < 8)
    {
      ret = (ret << 1) | ((hpgb->start[hpgb->offset_bytes] >>
              (7 - hpgb->offset_bits)) & 0x01);
      hpgb->offset_bits++;
      nbits--;
      if (nbits == 0)
      {
        if (hpgb->offset_bits == 8)
        {
          hpgb->offset_bytes++;
          hpgb->offset_bits = 0;
        }
        return ret;
      }
    }
    hpgb->offset_bytes++;
    hpgb->offset_bits = 0;
  }
  return ret;
}

/**
 *  @fn uint32_t get_bits_unsigned_eg (VvasParserGetBits* hpgb);
 *  @param [in] hpgb - handle pointer to VvasParserGetBits object
 *  @param [in] nbits - number of bits to read
 *  @return value read
 *  @brief reads unsigned Exp-Golomb code from VvasParserGetBits object
 */
uint32_t get_bits_unsigned_eg (VvasParserGetBits* hpgb)
{
  uint32_t leadingzerobits = -1;
  uint8_t b = 0;
  while (!b)
  {
    b = get_bits_byte(hpgb, 1);
    leadingzerobits++;
    if (get_bits_eof (hpgb))
      return 0;
  }
  if (leadingzerobits == 0)
    return 0;
  uint32_t result = 1, i;
  for (i = 0; i < leadingzerobits; i++)
    result *= 2;
  uint32_t remain = 0;
  while (leadingzerobits != 0)
  {
    if (leadingzerobits > 8)
    {
      remain = (remain << 8) + get_bits_byte (hpgb, 8);
      leadingzerobits -= 8;
    } else {
      remain = (remain << leadingzerobits) +
                get_bits_byte(hpgb, leadingzerobits);
      leadingzerobits = 0;
    }
  }

  return (result - 1 + remain);
}

/**
 *  @fn int32_t get_bits_signed_eg (VvasParserGetBits* hpgb);
 *  @param [in] hpgb - handle pointer to VvasParserGetBits object
 *  @param [in] nbits - number of bits to read
 *  @return value read
 *  @brief reads signed Exp-Golomb code from VvasParserGetBits object
 */
int32_t get_bits_signed_eg (VvasParserGetBits* hpgb)
{
  int32_t codenum = get_bits_unsigned_eg (hpgb);
  if (codenum % 2 == 0)
    return ((codenum / 2) * -1);
  else
    return ((codenum + 1) / 2);
}

/**
 *  @fn int32_t find_next_start_code (VvasParserBuffer* buffer,
 *                                    int offset,
 *                                    int* ret_offset);
 *  @param [in] hpgb - handle pointer to VvasParserGetBits object
 *  @param [in] nbits - number of bits to read
 *  @return return offset if match found, otherwise return offset till which
      search has been completed
 *  @brief searches for the next start code ("0x000001") or ("0x00000001")
 */
int32_t find_next_start_code (VvasParserBuffer* buffer, int32_t offset,
        int32_t* ret_offset)
{
  uint8_t* start = buffer->data + offset;
  uint8_t* end = buffer->data + buffer->size - 3;
  while (start <= end)  {
    if ((start[0] == 0x00) && (start[1] == 0x00) && (start[2] == 0x01))
    {
      if ((start - buffer->data > 0) && (*(start - 1) == 0x00)) {
        *ret_offset = start - buffer->data - 1;
      } else {
        *ret_offset = start - buffer->data;
      }
      return P_SUCCESS;
    }
    start++;
  }

  *ret_offset = buffer->size;
  return P_MOREDATA;
}

/**
 *  @fn int32_t convert_to_rbsp (VvasParserBuffer* buffer,
 *                               int start_offset,
 *                               int end_offset,
 *                               VvasParserBuffer* new_buffer);
 *  @param [in] buffer - buffer containing escaped bit-stream payload
 *  @param [in] start_offset - starting offset in buffer to convert
 *  @param [in] end_offset - ending offset in buffer to convert
 *  @param [in] new_buffer - buffer to place raw bit-stream payload into
 *  @return return P_SUCCESS on success else P_ERROR
 *  @brief convert escaped bit stream payload to raw bit-stream payload
 */
int32_t convert_to_rbsp (VvasParserBuffer* buffer, int32_t start_offset,
  int32_t end_offset, VvasParserBuffer* newbuffer)
{
  newbuffer->size = end_offset - start_offset;
  newbuffer->data = malloc (newbuffer->size);
  if (!newbuffer->data)
    return P_ERROR;
  newbuffer->size = 0;
  int32_t state = 0;
  uint8_t* pt = buffer->data + start_offset;
  uint8_t* end = buffer->data + end_offset;
  uint8_t* dst = newbuffer->data;
  while (pt != end)
  {
    if (state == 0 && *pt == 0)
      state = 1;
    else if (state == 1 && *pt == 0)
      state = 2;
    else if (state == 2 && *pt == 3)
    {
      state = 3;
      ++pt;
      continue;
    } else if (state == 3 && *pt == 0)
      state = 1;
    else if (state == 3 && ((*pt == 1) || (*pt == 2) || (*pt == 3)))
      state = 0;
    else if (state == 3)
    {
      *dst++ = 3;
      ++newbuffer->size;
      state = 0;
    }
    else
      state = 0;
    *dst++ = *pt++;
    ++newbuffer->size;
  }
  return P_SUCCESS;
}

/**
 *  @fn int32_t init_parse_data (VvasParserStreamInfo* parsedata);
 *  @param [in] parsedata - pointer to @ref VvasParserStreamInfo
 *  @return returns P_SUCCESS on success
 *  @brief internal API Initializes VvasParserStreamInfo
 */
int32_t init_parse_data (VvasParserStreamInfo* parsedata)
{
  memset (parsedata, 0, sizeof (VvasParserStreamInfo));

  int32_t i;

  for (i = 0; i < 32; i++)
    parsedata->h264_seq_parameter_set[i].valid = 0;

  for (i = 0; i < 256; i++)
    parsedata->h264_pic_parameter_set[i].valid = 0;

  parsedata->last_h264_slice_header.delta_pic_order_cnt_bottom = -1;
  parsedata->last_h264_slice_header.delta_pic_order_cnt[0] = -1;
  parsedata->last_h264_slice_header.delta_pic_order_cnt[1] = -1;
  parsedata->last_h264_slice_header.frame_num = 0;
  parsedata->last_h264_slice_header.idr_pic_id = 0;
  parsedata->last_h264_slice_header.pic_order_cnt_lsb = 0;
  parsedata->last_h264_slice_header.pic_parameter_set_id = 0;
  parsedata->last_h264_slice_header.field_pic_flag = 0;
  parsedata->last_h264_slice_header.bottom_field_flag = 0;
  parsedata->last_h264_slice_header.nal_ref_idc = 0;
  parsedata->last_h264_slice_header.nal_unit_type = 0;

  return P_SUCCESS;
}
