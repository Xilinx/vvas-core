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

#ifndef __VVAS_METACONVERT_H__
#define __VVAS_METACONVERT_H__

#include <vvas_core/vvas_context.h>
#include <vvas_core/vvas_common.h>
#include <vvas_core/vvas_overlay.h>
#include <vvas_utils/vvas_utils.h>

/**
 * DOC: VVAS Metaconvert APIs
 * This file contains functions to covert different type of infer metadata to overlay metadata.
 */

#define META_CONVERT_MAX_STR_LENGTH 2048

/**
 * struct VvasRGBColor - Holds RGB color values
 * @red: Red component value
 * @green: Green component value
 * @blue: Blue component value
 */
typedef struct {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
} VvasRGBColor;

/**
 * struct VvasFilterObjectInfo - Information about objects to be filtered from a list
 * @name: Name of object to be considered for processing by metaconvert
 * @color: Color components to be applied on the object
 * @do_mask: If set, masking will be done on this object
 */
typedef struct {
  char name[META_CONVERT_MAX_STR_LENGTH];
  VvasRGBColor color;
  uint8_t do_mask;
} VvasFilterObjectInfo;

/**
 * struct VvasMetaConvertConfig - Configuration to be supplied by the user
 * @font_type: Font type &enum VvasFontType
 * @font_size: Font size
 * @line_thickness: Line thickness while drawing rectangles or lines
 * @radius: Circle/point radius
 * @level: Prepare VvasOverlayShapeInfo from nodes present in specific level only.
 *              With level = 0, displays all nodes information
 * @mask_level: Apply masking at specific level
 * @y_offset: Y-axis offset while displaying text
 * @draw_above_bbox_flag: Flag to draw text above or inside bounding box.
 *                        In case x and y position are zero draws inside frame left top corner
 * @text_color: Color values to be used to display text
 * @allowed_labels: List of labels from VvasInferPrediction to be considered while creating text.
 *                              If allowed_labels is NULL, then all possible VvasInferPrediction labels are allowed
 * @allowed_labels_count: Count of the filter_labels array
 * @allowed_classes: Consider only specific classes and respective color while preparing &struct VvasOverlayShapeInfo
 * @allowed_classes_count: Count of the @allowed_classes array
 */
typedef struct {
  VvasFontType font_type;
  float font_size;
  int32_t line_thickness;
  int32_t radius;
  uint8_t level;
  uint8_t mask_level;
  uint32_t y_offset;
  bool draw_above_bbox_flag;
  VvasRGBColor text_color;
  char **allowed_labels;
  uint32_t allowed_labels_count;
  VvasFilterObjectInfo **allowed_classes;
  uint32_t allowed_classes_count;
} VvasMetaConvertConfig;

typedef void VvasMetaConvert;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * vvas_metaconvert_create() - Creates VvasMetaConvert handle based on @cfg
 * @vvas_ctx: Handle to VVAS context
 * @cfg: Handle to &struct VvasMetaConvertConfig
 * @log_level: Log level to be used to dump metaconvert logs
 * @ret: Address to store return value. In case of error, @ret is useful in understanding the root cause
 *
 * Return: Handle to VvasMetaConvert
 */
VvasMetaConvert *vvas_metaconvert_create (VvasContext *vvas_ctx, VvasMetaConvertConfig *cfg,
    VvasLogLevel log_level, VvasReturnType *ret);

/**
 * vvas_metaconvert_prepare_overlay_metadata() - Converts Inference prediction tree to structure which can be understood by overlay module
 * @meta_convert: Handle to VVAS Meta convert
 * @parent: Handle to parent node of Inference prediction tree
 * @shape_info: Handle to overlay information which will be used overlay module to draw bounding box
 *
 * Return: &enum VvasReturnType
 */
VvasReturnType vvas_metaconvert_prepare_overlay_metadata (VvasMetaConvert *meta_convert, VvasTreeNode *parent, VvasOverlayShapeInfo *shape_info);

/**
 * vvas_metaconvert_destroy() - Destorys &struct VvasMetaConvert handle
 * @meta_convert: Handle to VVAS Meta convert
 *
 * Return: None
 */
void vvas_metaconvert_destroy (VvasMetaConvert *meta_convert);

#ifdef __cplusplus
}
#endif

#endif
