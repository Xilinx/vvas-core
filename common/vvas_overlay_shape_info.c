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

#include <vvas_core/vvas_overlay_shape_info.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static VvasOverlayRectParams *
rects_copy (VvasOverlayRectParams * src, void * data)
{
  VvasOverlayRectParams *dest = (VvasOverlayRectParams *) calloc (1, sizeof (VvasOverlayRectParams));
  if (!dest)
    return NULL;

  memcpy (dest, src, sizeof (VvasOverlayRectParams));
  return dest;
}

static VvasOverlayTextParams *
text_copy (VvasOverlayTextParams * src, void * data)
{
  VvasOverlayTextParams *dest = (VvasOverlayTextParams *) calloc (1, sizeof (VvasOverlayTextParams));
  if (!dest)
    return NULL;

  memcpy (dest, src, sizeof (VvasOverlayTextParams));
  dest->disp_text = strdup(src->disp_text);
  return dest;
}

static VvasOverlayLineParams *
lines_copy (VvasOverlayLineParams * src, void * data)
{
  VvasOverlayLineParams *dest = (VvasOverlayLineParams *) calloc (1, sizeof (VvasOverlayLineParams));
  if (!dest)
    return NULL;

  memcpy (dest, src, sizeof (VvasOverlayLineParams));
  return dest;
}

static VvasOverlayArrowParams *
arrows_copy (VvasOverlayArrowParams * src, void * data)
{
  VvasOverlayArrowParams *dest = (VvasOverlayArrowParams *) calloc (1, sizeof (VvasOverlayArrowParams));
  if (!dest)
    return NULL;

  memcpy (dest, src, sizeof (VvasOverlayArrowParams));
  return dest;
}

static VvasOverlayCircleParams *
circles_copy (VvasOverlayCircleParams * src, void * data)
{
  VvasOverlayCircleParams *dest = (VvasOverlayCircleParams *) calloc (1, sizeof (VvasOverlayCircleParams));
  if (!dest)
    return NULL;

  memcpy (dest, src, sizeof (VvasOverlayCircleParams));
  return dest;
}

static VvasOverlayCoordinates *
points_copy (VvasOverlayCoordinates * src, void * data)
{
  VvasOverlayCoordinates *dest = (VvasOverlayCoordinates *) calloc (1, sizeof (VvasOverlayCoordinates));
  if (!dest)
    return NULL;

  memcpy (dest, src, sizeof (VvasOverlayCoordinates));
  return dest;
}

static VvasOverlayPolygonParams *
polygons_copy (VvasOverlayPolygonParams * src, void * data)
{
  VvasOverlayPolygonParams *dest = (VvasOverlayPolygonParams *) calloc (1, sizeof (VvasOverlayPolygonParams));
  if (!dest)
    return NULL;

  memcpy (dest, src, sizeof (VvasOverlayPolygonParams));
  dest->poly_pts =
      vvas_list_copy_deep (src->poly_pts, (void *) points_copy, NULL);
  return dest;
}

static void
text_free (void *data)
{
  VvasOverlayTextParams *text = (VvasOverlayTextParams *) data;
  if (text->disp_text) {
    free (text->disp_text);
  }
  free (data);
}

static void
rect_free (void *data)
{
  free (data);
}


static void
polygons_free (void * data)
{
  VvasOverlayPolygonParams *polygons = (VvasOverlayPolygonParams *) data;
  if (polygons->poly_pts) {
    vvas_list_free_full (polygons->poly_pts, free);
    polygons->poly_pts = NULL;
  }
  free (data);
}

void
vvas_overlay_shape_info_init (VvasOverlayShapeInfo *shape_info)
{
  shape_info->num_rects = 0;
  shape_info->num_text = 0;
  shape_info->num_lines = 0;
  shape_info->num_arrows = 0;
  shape_info->num_circles = 0;
  shape_info->num_polys = 0;

  shape_info->rect_params = NULL;
  shape_info->text_params = NULL;
  shape_info->line_params = NULL;
  shape_info->arrow_params = NULL;
  shape_info->circle_params = NULL;
  shape_info->polygn_params = NULL;
}

void
vvas_overlay_shape_info_copy (VvasOverlayShapeInfo *dest_shape_info, VvasOverlayShapeInfo *src_shape_info)
{
  dest_shape_info->num_rects = src_shape_info->num_rects;
  dest_shape_info->num_text = src_shape_info->num_text;
  dest_shape_info->num_lines = src_shape_info->num_lines;
  dest_shape_info->num_arrows = src_shape_info->num_arrows;
  dest_shape_info->num_circles = src_shape_info->num_circles;
  dest_shape_info->num_polys = src_shape_info->num_polys;

  dest_shape_info->rect_params =
      vvas_list_copy_deep (src_shape_info->rect_params, (void *) rects_copy, NULL);
  dest_shape_info->text_params =
      vvas_list_copy_deep (src_shape_info->text_params, (void *) text_copy, NULL);
  dest_shape_info->line_params =
      vvas_list_copy_deep (src_shape_info->line_params, (void *) lines_copy, NULL);
  dest_shape_info->arrow_params =
      vvas_list_copy_deep (src_shape_info->arrow_params, (void *) arrows_copy, NULL);
  dest_shape_info->circle_params =
      vvas_list_copy_deep (src_shape_info->circle_params, (void *) circles_copy, NULL);
  dest_shape_info->polygn_params =
      vvas_list_copy_deep (src_shape_info->polygn_params, (void *) polygons_copy, NULL);
}

void
vvas_overlay_shape_info_free (VvasOverlayShapeInfo *shape_info)
{
  if (shape_info->rect_params) {
    vvas_list_free_full (shape_info->rect_params, rect_free);
    shape_info->rect_params = NULL;
  }

  if (shape_info->text_params) {
    vvas_list_free_full (shape_info->text_params, text_free);
    shape_info->text_params = NULL;
  }

  if (shape_info->line_params) {
    vvas_list_free_full (shape_info->line_params, free);
    shape_info->line_params = NULL;
  }

  if (shape_info->arrow_params) {
    vvas_list_free_full (shape_info->arrow_params, free);
    shape_info->arrow_params = NULL;
  }

  if (shape_info->circle_params) {
    vvas_list_free_full (shape_info->circle_params, free);
    shape_info->circle_params = NULL;
  }

  if (shape_info->polygn_params) {
    vvas_list_free_full (shape_info->polygn_params, polygons_free);
    shape_info->polygn_params = NULL;
  }
}
