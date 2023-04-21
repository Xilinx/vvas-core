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
 * DOC: VVAS Infer Classification APIs
 * This file contains data type and API declarations for Infer classification operations.
 */

#ifndef __VVAS_INFER_CLASSIFICATION_H__
#define __VVAS_INFER_CLASSIFICATION_H__

#include <stdint.h>
#include <vvas_utils/vvas_utils.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * struct VvasColorInfo - Contains information for color of the detected object
 * @red: R color component
 * @green: G color component
 * @blue: B color component
 * @alpha: Transparency
 */
typedef struct {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t alpha;
}VvasColorInfo;

/**
 * struct VvasInferClassification - Contains information on classification for each object
 * @classification_id: A unique id associated to this classification
 * @class_id: The numerical id associated to the assigned class
 * @class_prob: The resulting probability of the assigned class. Typically ranges between 0 and 1
 * @class_label: The label associated to this class or NULL if not available
 * @num_classes: The total number of classes of the entire prediction
 * @probabilities: The entire array of probabilities of the prediction
 * @labels: The entire array of labels of the prediction. NULL if not available
 * @label_color: The color of labels
 */
typedef struct {
  uint64_t classification_id;
  int32_t class_id;
  double class_prob;
  char* class_label;
  int32_t num_classes;
  double* probabilities;
  char** labels;
  VvasColorInfo label_color;
}VvasInferClassification;

/**
 *  vvas_inferclassification_new () - This function  allocates new memory for @VvasInferClassification
 *
 *  Return:
 *  * On Success returns address of the new object of @VvasInferClassification.
 *  * On Failure returns NULL
 */
VvasInferClassification * vvas_inferclassification_new(void);

/**
 *  vvas_inferclassification_free () - This function deallocates memory associated with VvasInferClassification object
 *
 *  @self: Address of the object handle to be freed
 *
 *  Return: none.
 */
void vvas_inferclassification_free(VvasInferClassification *self);

/**
 *  vvas_inferclassification_copy () - This function creates a new copy of @VvasInferClassification object
 *
 *  @self: Address of context handle
 *
 *  Return: 
 *  * On Success returns address of the new object of @VvasInferClassification.
 *  * On Failure returns NULL
 */
VvasInferClassification* vvas_inferclassification_copy(const VvasInferClassification *self);

/**
 *  vvas_inferclassification_to_string () - This function creates a string of classifications
 *
 *  @self: Address of @VvasInferenceClassification
 *  @level: Level of inference prediction
 *
 *  User has to free this memory
 *
 *  Return: Returns a string with all classifications serialized.
 */
char *vvas_inferclassification_to_string (VvasInferClassification * self, int level);
#ifdef __cplusplus
}
#endif
#endif

