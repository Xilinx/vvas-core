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
/** @file vvas_overlay.h
 *  @brief Function prototypes for overlay operation.
 *
 *  This file contains  prototypes for overlay
 *  operation for requested frame and this API does not support 
 *  HW acceleration for Bounding box.
 */

/**
* DOC: VVAS Overlay APIs
* This file contains public methods and data structures related to VVAS Overlay.
*/

#ifndef __VVAS_OVERLAY_H__
#define __VVAS_OVERLAY_H__

#include <vvas_core/vvas_common.h>
#include <vvas_core/vvas_video.h>
#include <vvas_utils/vvas_utils.h>
#include <vvas_core/vvas_overlay_shape_info.h>

/**
 * struct VvasFrameInfo - Structure represents input frame information on
 * which overlay has to be performed.
 * @stride: stride information
 * @height: height of the frame
 * @width: width of the frame
 * @fmt: Specify input frame color format
 * @buf_addr: Address of frame on which overlay operation to be performed
 */ 
typedef struct {
  uint32_t stride;
  uint32_t height;
  uint32_t width;
  VvasVideoFormat fmt;
  uint8_t *buf_addr;
}VvasFrameInfo;

/**
 * struct VvasOverlayClockInfo - Structure represents Clock information to be
 * display on frame.
 * @display_clock: display clock true or false
 * @clock_font_name: font style name
 * @clock_font_scale: font scale
 * @clock_font_color: font color
 * @clock_x_offset: clock x offset
 * @clock_y_offset: clock y offset
 */
typedef struct {
  bool display_clock;            
  uint32_t clock_font_name; 
  float clock_font_scale;
  uint32_t clock_font_color;
  uint32_t clock_x_offset;
  uint32_t clock_y_offset;    
} VvasOverlayClockInfo;

/**
 * struct VvasOverlayFrameInfo - This is the main structure to be passed
 * for processing overlay onto a frame.
 * @frame_info: frame information
 * @clk_info: clock overlay information
 * @shape_info: Overlay information
 */
typedef struct {
  VvasVideoFrame *frame_info;
  VvasOverlayClockInfo clk_info;
  VvasOverlayShapeInfo shape_info;
} VvasOverlayFrameInfo;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * vvas_overlay_process_frame ()
 * @pFrameInfo: Address of VvasOverlayFrameInfo
 *
 * Context: Drawing is performed on the given frame.
 *
 * Return:
 * * On Success, returns VVAS_SUCCESS.
 * * On Failure, returns VVAS_ERROR_*
 */
VvasReturnType vvas_overlay_process_frame (VvasOverlayFrameInfo *pFrameInfo);

#ifdef __cplusplus
}
#endif


#endif
