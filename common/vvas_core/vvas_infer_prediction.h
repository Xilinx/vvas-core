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
 * DOC: VVAS Infer Prediction APIs
 * This file contains data type and API declarations for Inference operations.
 */

#ifndef __VVAS_INFER_PREDICTION_H__
#define __VVAS_INFER_PREDICTION_H__

#include <math.h>
#include <vvas_core/vvas_infer_classification.h>
#include <vvas_core/vvas_dpucommon.h>
#include <vvas_utils/vvas_utils.h>

#ifdef __cplusplus
#include <atomic>
using namespace std;
#else
#include <stdatomic.h>
#endif

#define NUM_POSE_POINT 14
#define NUM_LANDMARK_POINT 5
#define MAX_SEGOUTFMT_LEN 6
#define VVAS_MAX_FEATURES 512

#ifdef __cplusplus
extern "C" {
#endif

/**
 * struct VvasBoundingBox - Contains information for box data for detected object
 * @x: horizontal coordinate of the upper position in pixels
 * @y: vertical coordinate of the upper position in pixels
 * @width: width of bounding box in pixels
 * @height: height of bounding box in pixels
 * @box_color: bounding box color
 */
typedef struct {
  int32_t x;
  int32_t y;
  uint32_t width;
  uint32_t height;
  VvasColorInfo box_color;
}VvasBoundingBox;

/**
 * struct Pointf - coordinate of point
 * @x: horizontal coordinate of the upper position in pixels
 * @y: vertical coordinate of the upper position in pixels
 */
typedef struct {
  float x;
  float y;
}Pointf;

/**
 * struct Pose14Pt - 14 coordinate points to represented pose
 * @right_shoulder: R_shoulder coordinate
 * @right_elbow: R_elbow coordinate
 * @right_wrist: R_wrist coordinate
 * @left_shoulder: L_shoulder coordinate
 * @left_elbow: L_elbow coordinate
 * @left_wrist: L_wrist coordinate
 * @right_hip: R_hip coordinate
 * @right_knee: R_knee coordinate
 * @right_ankle: R_ankle coordinate
 * @left_hip: L_hip coordinate
 * @left_knee: L_knee coordinate
 * @left_ankle: L_ankle coordinate
 * @head: Head coordinate
 * @neck: Neck coordinate
 */
typedef struct {
  Pointf right_shoulder;
  Pointf right_elbow;
  Pointf right_wrist;
  Pointf left_shoulder;
  Pointf left_elbow;
  Pointf left_wrist;
  Pointf right_hip;
  Pointf right_knee;
  Pointf right_ankle;
  Pointf left_hip;
  Pointf left_knee;
  Pointf left_ankle;
  Pointf head;
  Pointf neck;
}Pose14Pt;

/** 
 * enum feature_type - Enum for holding type of feature
 * @UNKNOWN_FEATURE: Unknown feature
 * @FLOAT_FEATURE: Float features
 * @FIXED_FEATURE: Fixed point features
 * @LANDMARK: Landmark
 * @ROADLINE: Roadlines
 * @ULTRAFAST: Points from Ultrafast model
 */
enum feature_type {
  UNKNOWN_FEATURE = 0,
  FLOAT_FEATURE,
  FIXED_FEATURE,
  LANDMARK,
  ROADLINE,
  ULTRAFAST
};

/**
 * enum road_line_type - Enum for holding type of road line
 * @BACKGROUND: Background
 * @WHITE_DOTTED_LINE: White dotted line
 * @WHITE_SOLID_LINE:  White solid line
 * @YELLOW_LINE: Yellow line
 */
enum road_line_type {
  BACKGROUND = 0,
  WHITE_DOTTED_LINE,
  WHITE_SOLID_LINE,
  YELLOW_LINE,
};

/**
 * struct Feature - The features of a road/person
 * @float_feature: float features
 * @fixed_feature: fixed features
 * @road_line: points for drawing road lanes
 * @landmark: five key points on a human face
 * @line_size: Number of points in road_line
 * @type: enum to hold type of feature 
 * @line_type: enum to hold type of road lane
 */
typedef struct {
  union {
    float float_feature[VVAS_MAX_FEATURES];
    int8_t fixed_feature[VVAS_MAX_FEATURES];
    Pointf road_line[VVAS_MAX_FEATURES];
    Pointf landmark[NUM_LANDMARK_POINT];
  };
  uint32_t line_size;
  enum feature_type type;
  enum road_line_type line_type;
}Feature;

/**
 * struct Reid - Structure to gold reid model results
 * @width: Width of output image
 * @height: Height of output image
 * @size: Size of output
 * @type: Type of Reid
 * @data: Reid output data
 * @free: function pointer to free data
 * @copy: function pointer to copy data
 */
typedef struct {
  uint32_t width;
  uint32_t height;
  uint64_t size;
  uint64_t type;
  void *data;
  bool (*free) (void *);
  bool (*copy) (const void *, void *);
}Reid;

/**
 * enum seg_type - Enum for holding type of segmentation
 * @SEMANTIC: Semantic
 * @MEDICAL: Medical
 * @SEG3D: 3D Segmentation
 */
enum seg_type
{
  SEMANTIC = 0,
  MEDICAL,
  SEG3D,
};

/**
 * struct Segmentation - Structure for storing segmentation related information
 * @type: enum to hold type of segmentation
 * @width: Width of output image
 * @height: Height of output image
 * @fmt: Segmentation output format
 * @data: Segmentation output data
 * @free: function pointer to free data
 * @copy: function pointer to copy data
 */
typedef struct {
  enum seg_type type;
  uint32_t width;
  uint32_t height;
  char fmt[MAX_SEGOUTFMT_LEN];
  void *data;
  bool (*free) (void *);
  bool (*copy) (const void *, void *);
}Segmentation;

/**
 * struct TensorBuf - Structure for storing Tensor related information
 * @size: Size of output Tensors
 * @ptr: Pointers to output Tensors
 * @priv: Private structure
 * @free: function pointer to free data
 * @copy: function pointer to copy data
 * @height: Height of output image
 * @width: Width of output image
 * @fmt: Format of output image
 * @ref_count: Reference count
 */
typedef struct {
  int size;
  void *ptr[20];
  void *priv;
  void (*free) (void **);
  void (*copy) (void **, void **);
  unsigned long int height;
  unsigned long int width;
  unsigned long int fmt;
  atomic_int ref_count;
}TensorBuf;

/** 
 * struct VvasInferPrediction - Contains Inference  meta data information of a frame
 * @prediction_id: A unique id for this specific prediction
 * @enabled: This flag indicates whether or not this prediction should be used for further inference
 * @bbox: Bouding box for this specific prediction
 * @classifications: linked list to classifications
 * @node: Address to tree data structure node
 * @bbox_scaled: bbox co-ordinates scaled to root node resolution or not
 * @obj_track_label: Track Label for the object
 * @model_class: Model class defined in vvas-core
 * @model_name: Model name
 * @count: A number element, used by model which give output a number
 * @pose14pt: Struct of the result returned by the posedetect/openpose network
 * @feature: Features of a face/road
 * @reid: Getting feature from an image
 * @segmentation: Segmentation data
 * @tb: Rawtensor data
 */
typedef struct {
  uint64_t prediction_id;
  bool enabled;
  VvasBoundingBox bbox;
  VvasList* classifications;
  VvasTreeNode *node;
  bool bbox_scaled;
  char *obj_track_label;
  VvasClass model_class;
  char *model_name;
  int count;
  Pose14Pt pose14pt;
  Feature feature;
  Reid reid;
  Segmentation segmentation;
  TensorBuf *tb;
}VvasInferPrediction;

/**
 *  vvas_inferprediction_new () - Allocate new memory for @VvasInferPrediction
 *  
 *  Return:
 *  * On Success returns address of the new object instance of @VvasInferPrediction.
 *  * On Failure returns NULL
 */ 
VvasInferPrediction* vvas_inferprediction_new(void);

/**
 *  vvas_inferprediction_append () - Appends child node to parent node
 *
 *  @self: Instance of the parent node to which child node will be appended.
 *  @child: Instance of the child node to be appended.
 *
 *  Return: none 
 */
 void vvas_inferprediction_append(VvasInferPrediction *self, VvasInferPrediction *child);

/**
 *  vvas_inferprediction_copy () - This function will perform a deep copy of the given node
 *
 *  @smeta: Address of @VvasInferPrediction instance to be copied
 *
 *  Return:
 *  * On Success returns address of the new copied node.
 *  * On Failure returns NULL 
 */
VvasInferPrediction* vvas_inferprediction_copy(VvasInferPrediction *smeta);

/**
 *  vvas_inferprediction_node_copy () - This function is used to copy single node and also passed as param to node deep copy
 *
 *  @infer: VvasInferPrediction object will be passed while traversing to child nodes.
 *  @data: user data to be passed.
 *
 *  Return:
 *  * On Success returns address of the new node.
 *  * On Failure returns NULL 
 */
 void* vvas_inferprediction_node_copy(const void *infer, void *data);


/**
 *  vvas_inferprediction_free () - This function deallocates memory for @VvasInferPrediction
 *
 *  @self: Address of the object handle to be freed
 *
 *  Return: none 
 */ 
void vvas_inferprediction_free(VvasInferPrediction *self);

/**
 *  vvas_inferprediction_to_string () - This function creates a string of predictions
 *
 *  @self: Address of @VvasInferPrediction
 *
 *  User has to free this memory.
 *
 *  Return: Returns a string with all predictions serialized.
 */
char *vvas_inferprediction_to_string (VvasInferPrediction * self);

/**
 *  vvas_inferprediction_get_prediction_id () - This function generates unique prediction id
 *
 *  Return: Returns unique prediction id.
 */
uint64_t vvas_inferprediction_get_prediction_id(void);


#ifdef __cplusplus
}
#endif

#endif

