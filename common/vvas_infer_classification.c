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
 
#include <stdlib.h>
#include <string.h>
#include <vvas_core/vvas_infer_classification.h>
#include <vvas_core/vvas_log.h> 

#define LOG_LEVEL     (LOG_LEVEL_ERROR)

#define LOG_E(...)    (LOG_MESSAGE(LOG_LEVEL_ERROR, LOG_LEVEL,  __VA_ARGS__))
#define LOG_W(...)    (LOG_MESSAGE(LOG_LEVEL_WARNING, LOG_LEVEL,  __VA_ARGS__))
#define LOG_I(...)    (LOG_MESSAGE(LOG_LEVEL_INFO, LOG_LEVEL,  __VA_ARGS__))
#define LOG_D(...)    (LOG_MESSAGE(LOG_LEVEL_DEBUG, LOG_LEVEL,  __VA_ARGS__))

#define DEFAULT_CLASS_ID -1
#define DEFAULT_CLASS_PROB 0.0f
#define DEFAULT_CLASS_LABEL NULL
#define DEFAULT_NUM_CLASSES 0
#define MAX_LEN 2048

/**
 *  @fn char * vvas_inferclassification_copy_labels( char **labels) 
 *  @param [in]  labels - Address of labels to be copied
 *  @return  On Success returns address of the new object where labels is copied.  
 *           On Failure returns NULL
 *  @brief This function returns labels copied into a new memory. 
 */ 
static char** vvas_inferclassification_copy_labels(char **labels) 
{
  if (labels) {
      uint64_t i = 0;
      char **retval;

      while (labels[i]) {
        ++i;
      }

      retval = malloc (sizeof(char*) * (i + 1));

      i = 0;
      while (labels[i]) {
          retval[i] = strdup(labels[i]);
          ++i;
      }
      retval[i] = NULL;

      return retval;
    }
  else {
    return NULL;
  }
}

/**
 *  @fn double * vvas_inferclassification_copy_probabilities(const double *self, int32_t num_classes)
 *  @param [in]  self - Address of probabilities to be copied
 *  @param [in]  num_classes - represents number of classes. 
 *  @return  On Success returns address of the new instance where probabilites are copied.  
 *           On Failure returns NULL
 *  @brief This function returns class_label copied into a new memory. 
 */ 
static double* vvas_inferclassification_copy_probabilities(const double *self, int32_t num_classes)
{
  uint64_t size = 0;
  double *copy = NULL;

  if(NULL == self) {
    return NULL;
  }

  size = num_classes * sizeof (double);
  copy = (double *)malloc(size);
  if (!copy) {
    LOG_E("failed to allocate memory");
    return NULL;
  }
  memcpy (copy, self, size);

  return copy;
}

/**
 *  @fn void vvas_inferclassification_reset (VvasInferClassification * self)
 *  @param [in] self - Address of the VvasInferenceClassification to be reset 
 *  @return none
 *  @brief This function resets VvasInferClassification object
 */ 
static void vvas_inferclassification_reset (VvasInferClassification * self)
{
  if(NULL == self) {
    LOG_E("Null received");
    return;
  }

  self->classification_id = 0;
  self->class_id = DEFAULT_CLASS_ID;
  self->class_prob = DEFAULT_CLASS_PROB;
  self->num_classes = DEFAULT_NUM_CLASSES;
  
  self->label_color.red = 0;
  self->label_color.green = 0;
  self->label_color.blue = 0;
  self->label_color.alpha = 0;
  
  if (self->class_label) {
    free(self->class_label);
  }

  if (self->probabilities) {
    free(self->probabilities);
  }
  
  if(self->labels) {
    char **str_array  = self->labels;

    for (uint64_t i = 0; str_array[i] != NULL; i++) {
       free(str_array[i]);
    }
    free(str_array);
  }

  self->class_label = NULL;
  self->probabilities = NULL;
  self->labels = NULL;
}

/**
 *  @fn VvasInferClassification * vvas_inferclassification_new(void)
 *  @param [in] none 
 *  @return  On Success returns address of the new object instance of VvasInference.  
 *           On Failure returns NULL
 *  @brief This function returns allocates new memory for infer meta
 */ 
VvasInferClassification * vvas_inferclassification_new(void)
{
 VvasInferClassification  *self = (VvasInferClassification *) malloc(sizeof(VvasInferClassification));
  
  if(NULL != self) {

    self->num_classes = 0;
    self->label_color.red = 0;
    self->label_color.green = 0;
    self->label_color.blue = 0;
    self->label_color.alpha = 0;

    self->class_id = DEFAULT_CLASS_ID;
    self->class_prob = DEFAULT_CLASS_PROB;
    self->num_classes = DEFAULT_NUM_CLASSES;

    self->class_label = NULL; 
    self->probabilities = NULL; 
    self->labels = NULL; 
  }
  else {
    LOG_E(" Failed to allocate memory");
  }
  
  return self;
}


/**
 *  @fn void vvas_inferclassification_free(VvasInferClassification *self)
 *  @param [in]  self - Address of the object handle to be freed
 *  @return  none.
 *  @brief This function deallocates VvasInferClassification instance
 */
void vvas_inferclassification_free(VvasInferClassification *self)
{  
  if(NULL != self) {
    vvas_inferclassification_reset(self);
    free(self);
  }
  else {
    LOG_E("Null recevied");
  }
}

/**
 *  @fn VvasInferClassification* vvas_inferclassification_copy(const VvasInferClassification *self)
 *  @param [in]  self- Address of context handle
 *  @return  On Success returns address of the new object instance of VvasInferClassification.
 *           On Failure returns NULL
 *  @brief This function creates a new copy of VvasInferClassification object
 */
VvasInferClassification* vvas_inferclassification_copy(const VvasInferClassification *self)
{ 
  VvasInferClassification *copy = NULL;

  if (self) {
    copy = vvas_inferclassification_new ();
    if (!copy) {
      LOG_E(" Failed to allocate memory");
      return copy;
    }
    copy->classification_id = self->classification_id;
    copy->class_id = self->class_id;
    copy->class_prob = self->class_prob;
    copy->num_classes = self->num_classes;
    copy->label_color.red = self->label_color.red;
    copy->label_color.green = self->label_color.green;
    copy->label_color.blue = self->label_color.blue;
    copy->label_color.alpha = self->label_color.alpha;
    if (self->class_label)
      copy->class_label = strdup(self->class_label);
    copy->labels = vvas_inferclassification_copy_labels(self->labels);
    copy->probabilities = vvas_inferclassification_copy_probabilities(self->probabilities, self->num_classes);
  }
  
  return copy;
}

/**
 *  @fn char *vvas_inferclassification_to_string (VvasInferClassification * self, int level)
 *  @param [in]  self- Address of VvasInferenceClassification
 *  @param [in]  level - Level of inference predicition
 *  @return  Returns a string with all classifications serialized.
 *  @brief This function creates a string of classifications.
 *  @note User has to free this memory
 */
char *
vvas_inferclassification_to_string (VvasInferClassification * self, int level)
{
  int indent = level * 2;
  char *serial = NULL;
  char tmp [MAX_LEN] = {0, };

  if (!self) {
    return NULL;
  }

  snprintf (tmp, MAX_LEN - 1, "{\n"
      "%*s  Id : %" "lu" "\n"
      "%*s  Class : %d\n"
      "%*s  Label : %s\n"
      "%*s  Probability : %f\n"
      "%*s  Classes : %d\n"
      "%*s}",
      indent, "", self->classification_id,
      indent, "", self->class_id,
      indent, "", self->class_label,
      indent, "", self->class_prob, indent, "",
      self->num_classes, indent, "");

  serial = strdup(tmp);
  return serial;
}
 

