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

/** @file vvas_tracker.hpp
 *  @brief Contains structures, configuration and interfaces related to tracker
 *
 *  This file contains common structures and declarations for using tracker algorithm
 */

/**
* DOC: VVAS Tracker APIs
* This file contains public methods and data structures related to VVAS Tracker.
*/

#ifndef __VVAS_TRACKER_H__
#define __VVAS_TRACKER_H__

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vvas_core/vvas_context.h>
#include <vvas_core/vvas_common.h>
#include <vvas_core/vvas_video.h>

/**
 * enum VvasTrackerAlgoType - Enum representing tracker algorithm type
 * @TRACKER_ALGO_IOU: Intersection-Over-Union algorithm
 * @TRACKER_ALGO_MOSSE: Minimum Output Sum of Squared Error algorithm
 * @TRACKER_ALGO_KCF: Kernelized Correlation Filter algorithm
 * @TRACKER_ALGO_NONE: No Algorithm is specified. TRACKER_ALGO_KCF will be
 * set as default algorithm.
 */
typedef enum
{
  TRACKER_ALGO_IOU,
  TRACKER_ALGO_MOSSE,
  TRACKER_ALGO_KCF,
  TRACKER_ALGO_NONE,
} VvasTrackerAlgoType;

/**
 * enum VvasTrackerMatchColorSpace - Enum representing color space used for object matching
 * @TRACKER_USE_RGB: Use RGB color space for object matching
 * @TRACKER_USE_HSV: Use HSV (Hue-Saturation-Value) color space for object matching.
 */
typedef enum
{
  TRACKER_USE_RGB,
  TRACKER_USE_HSV,
} VvasTrackerMatchColorSpace;

/**
 * enum VvasTrackerSearchScale - Enum representing search scales to be used for tracking
 * @SEARCH_SCALE_ALL: Search for object both in up, same and down scale
 * @SEARCH_SCALE_UP: Search for object in up and same scale only
 * @SEARCH_SCALE_DOWN: Search for object in down and same scale only
 * @SEARCH_SCALE_NONE: Search for in same scale
 */
typedef enum
{
  SEARCH_SCALE_ALL,
  SEARCH_SCALE_UP,
  SEARCH_SCALE_DOWN,
  SEARCH_SCALE_NONE,
} VvasTrackerSearchScale;

/**
 * struct VvasTrackerconfig - Structure to hold tracker configuration
 * @tracker_type: Tracker algorithm to be used 0:IOU 1:KCF Tracker 2:MOSSE Tracker
 * @iou_use_color: To use color information for matching or not duirng IOU tracking
 * @obj_match_color: Color space to be used for object matching
 * @search_scales: Search scales of object during tracking
 * @fet_length: Feature length to be used during KCF based tracking
 * @min_width: Minimum width for considering as noise
 * @min_height: Minimum height for considering as noise
 * @max_width: Maximum width for considering as noise
 * @max_height: Maximum height for considering as noise
 * @num_inactive_frames: Number of frames wait for object reappearing before
 * consider as inactive
 * @num_frames_confidence: Number of frames of continuous detection before
 * considering for tracking and assiging an ID
 * @padding: Extra area surrounding the target to search in tracking
 * @obj_match_search_region: Search for nearest object to match
 * @dist_correlation_threshold: Objects correlation threshold
 * @dist_overlap_threshold: Objects overlap threshold
 * @dist_scale_change_threshold: Ojbects scale change threshold
 * @dist_correlation_weight: Weightage for correlation in distance function
 * @dist_overlap_weight: Weightage for overlap in distance function
 * @dist_scale_change_weight: Weightage for scale change in distance function
 * @occlusion_threshold: Occlusion threshold to ignore objects for tracking
 * @confidence_score: Tracker confidence threshold for tracking
 * @skip_inactive_objs: Flag to enable skipping of inactive object
 */
typedef struct {
  VvasTrackerAlgoType tracker_type;
  bool iou_use_color;
  VvasTrackerMatchColorSpace obj_match_color;
  VvasTrackerSearchScale search_scales;
  unsigned int fet_length;
  unsigned int min_width;
  unsigned int min_height;
  unsigned int max_width;
  unsigned int max_height;
  int num_inactive_frames;
  int num_frames_confidence;
  float padding;
  float obj_match_search_region;
  float dist_correlation_threshold;
  float dist_overlap_threshold;
  float dist_scale_change_threshold;
  float dist_correlation_weight;
  float dist_overlap_weight;
  float dist_scale_change_weight;
  float occlusion_threshold;
  float confidence_score; //confidence score
  bool skip_inactive_objs;
} VvasTrackerconfig;

typedef void VvasTracker; 

/**
 *  vvas_tracker_create () - initializes tracker with config parameters and allocates required memory
 *  
 *  @vvas_ctx: Pointer VvasContext handle.
 *  @config: Pointer to VvasTrackerConfig structure.
 *
 *  Return: On Success returns @ref VvasTracker handle. On Failure returns NULL.
 */
VvasTracker *vvas_tracker_create (VvasContext *vvas_ctx, VvasTrackerconfig *config);

/**
 *  vvas_tracker_process () - Called for every frames with or without detection information
 *                            for tracking objects in a frame.
 *
 *  @vvas_tracker_hndl: @ref VvasTracker with newly detected objects in VvasTracker:new_objs.
 *                  Upon tracking updates @ref VvasTracker:trk_objs with tracked objects info.
 *  @pFrame: @ref VvasVideoFrame structure of input frame.
 *  @infer_meta: @ref VvasInferPrediction contains detection tree if detection info available
 *              else NULL.
 *
 *  Return: @ref VvasReturnType.
 */
VvasReturnType vvas_tracker_process (VvasTracker *vvas_tracker_hndl, 
                                     VvasVideoFrame *pFrame, 
                                     VvasInferPrediction **infer_meta);

/**
 *  vvas_tracker_destroy () - free memory allocated during creating the tracker and
 *                          resets parameters to default values
 *  @vvas_tracker_hndl: Pointer to @ref VvasTracker
 *
 *  Return: True on success. False on failure.
 */
bool vvas_tracker_destroy (VvasTracker *vvas_tracker_hndl);

#endif /* __VVAS_TRACKER_H__ */
