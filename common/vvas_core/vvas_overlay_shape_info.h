/*
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
* DOC: VVAS Overlay Shape Info APIs
* This file contains public methods and data structures related to VVAS Overlay Shape Info.
*/

#ifndef __VVAS_OVERLAY_SHAPE_INFO_H__
#define __VVAS_OVERLAY_SHAPE_INFO_H__

#include <vvas_utils/vvas_utils.h>

/**
 * enum VvasOverlayArrowDirection - Structure representing Arrow Direction information
 * @ARROW_DIRECTION_START: START
 * @ARROW_DIRECTION_END: END
 * @ARROW_DIRECTION_BOTH_ENDS: BOTH_ENDS
 */ 
typedef enum {
    /** Arrow direction start */
   ARROW_DIRECTION_START ,
    /** Arrow direction end */
   ARROW_DIRECTION_END,
    /** Arrow direction both ends */
   ARROW_DIRECTION_BOTH_ENDS
} VvasOverlayArrowDirection;

/**
 * struct VvasOverlayCoordinates - Structure representing Coordinate information
 * @x: x offset
 * @y: y offset
 */ 
typedef struct {
  int32_t x;
  int32_t y;
}VvasOverlayCoordinates;

/**
 * struct VvasOverlayColorData - Structure representing Color information
 * @red: red value
 * @green: green value
 * @blue: blue value
 * @alpha: alpha value
 */
typedef struct {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t alpha;
} VvasOverlayColorData;

/**
 * struct VvasOverlayFontData - Structure representing Font information
 * @font_num: font style number from openCV
 * @font_size: font size
 * @font_color: font color
 */
typedef struct {
  uint32_t font_num;
  float font_size;
  VvasOverlayColorData font_color;
}VvasOverlayFontData;

/**
 * struct VvasOverlayRectParams - Structure representing information to draw rectangle on frame.
 * @points: frame coordinate info
 * @width: width of the rectangle
 * @height: height of the rectangle
 * @thickness: thickness of rectangle
 * @apply_bg_color: flag to apply bg color
 * @rect_color: color information of rectangle
 * @bg_color: color information of background
 */
typedef struct {
  VvasOverlayCoordinates points;  
  uint32_t width;
  uint32_t height;
  uint32_t thickness;
  uint32_t apply_bg_color;
  VvasOverlayColorData rect_color;  
  VvasOverlayColorData bg_color;  
}VvasOverlayRectParams;

/**
 * struct VvasOverlayTextParams - Structure representing Text information
 * @points: text coordinate info
 * @disp_text: display text
 * @bottom_left_origin: display text position
 * @apply_bg_color: display text background color
 * @text_font: display text font
 * @bg_color: background color information
 */
typedef struct {
  VvasOverlayCoordinates points;  
  char *disp_text;
  uint32_t bottom_left_origin;  
  uint32_t apply_bg_color;
  VvasOverlayFontData text_font;
  VvasOverlayColorData bg_color;
}VvasOverlayTextParams;

/**
 * struct VvasOverlayLineParams - Structure representing Line information
 * @start_pt: line start coordinate info
 * @end_pt: line end coordinate info
 * @thickness: Thickness in units of Pixels
 * @line_color: color information
 */
typedef struct {
  VvasOverlayCoordinates start_pt;
  VvasOverlayCoordinates end_pt;
  uint32_t thickness;    
  VvasOverlayColorData line_color;
}VvasOverlayLineParams;

/**
 * struct VvasOverlayArrowParams - Structure representing Arrow information
 * @start_pt: arrow start coordinate info
 * @end_pt: arrow end coordinate info
 * @arrow_direction: arrow direction
 * @thickness: thickness in units of Pixels
 * @tipLength: tip length
 * @line_color: color information
 */
typedef struct {
  VvasOverlayCoordinates start_pt;
  VvasOverlayCoordinates end_pt;
  VvasOverlayArrowDirection arrow_direction;
  uint32_t thickness;
  float tipLength;
  VvasOverlayColorData line_color;  
} VvasOverlayArrowParams;

/**
 * struct VvasOverlayCircleParams - Structure representing Circle Information
 * @center_pt: circle coordinate info
 * @thickness: circle thickness
 * @radius: circle radius
 * @circle_color: color information
 */ 
typedef struct {
  VvasOverlayCoordinates center_pt;
  uint32_t thickness;
  uint32_t radius;
  VvasOverlayColorData circle_color;  
} VvasOverlayCircleParams;

/**
 * struct VvasOverlayPolygonParams - Structure representing Polygon information
 * @poly_pts: polygon coordinate info
 * @thickness: polygon thickness
 * @num_pts: number of points
 * @poly_color: polygon color information
 */ 
typedef struct {
  VvasList *poly_pts;
  uint32_t thickness;
  int32_t num_pts;
  VvasOverlayColorData poly_color; 
} VvasOverlayPolygonParams;

/**
 * struct VvasOverlayShapeInfo - Structure representing Overlay Shape information
 * @num_rects: number of rectangles to be displayed
 * @num_text: number of texts to be displayed
 * @num_lines: number of lines to be displayed
 * @num_arrows: number of arrows to be displayed
 * @num_circles: number of circles to be displayed
 * @num_polys: number of polygons to be displayed
 * @rect_params: rectangle information
 * @text_params: text meta information
 * @line_params: line meta information
 * @arrow_params: arrow meta information
 * @circle_params: circle meta information
 * @polygn_params: polygon meta information
 */ 
typedef struct {
  uint32_t num_rects;
  uint32_t num_text;
  uint32_t num_lines;
  uint32_t num_arrows;
  uint32_t num_circles;
  uint32_t num_polys;
  VvasList *rect_params;
  VvasList *text_params;
  VvasList *line_params;
  VvasList *arrow_params;
  VvasList *circle_params;
  VvasList *polygn_params;
} VvasOverlayShapeInfo;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * vvas_overlay_shape_info_init() - Initializes shape_info parameters
 * @shape_info: Pointer to shape info structure
 *
 * Return: none
 */
void vvas_overlay_shape_info_init (VvasOverlayShapeInfo *shape_info);

/**
 * vvas_overlay_shape_info_copy() - Copies shape information from src to dst
 * @dest_shape_info: Destination shape info structure
 * @src_shape_info: Source shape info structure
 *
 * Return: none
 */
void vvas_overlay_shape_info_copy (VvasOverlayShapeInfo *dest_shape_info, VvasOverlayShapeInfo *src_shape_info);

/**
 * vvas_overlay_shape_info_free() - Deinitializes shape_info parameters
 * @shape_info: Pointer to shape info structure
 *
 * Return: none
 */

void vvas_overlay_shape_info_free (VvasOverlayShapeInfo *shape_info);

#ifdef __cplusplus
}
#endif

#endif


