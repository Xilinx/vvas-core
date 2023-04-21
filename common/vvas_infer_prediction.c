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

#include <vvas_core/vvas_infer_prediction.h>
#include <vvas_core/vvas_log.h>
#include <stdlib.h>
#include <string.h>
#include <vvas_utils/vvas_utils.h>

#define LOG_LEVEL     (LOG_LEVEL_WARNING)

#define LOG_E(...)    (LOG_MESSAGE(LOG_LEVEL_ERROR, LOG_LEVEL,  __VA_ARGS__))
#define LOG_W(...)    (LOG_MESSAGE(LOG_LEVEL_WARNING, LOG_LEVEL,  __VA_ARGS__))
#define LOG_I(...)    (LOG_MESSAGE(LOG_LEVEL_INFO, LOG_LEVEL,  __VA_ARGS__))
#define LOG_D(...)    (LOG_MESSAGE(LOG_LEVEL_DEBUG, LOG_LEVEL,  __VA_ARGS__))
#define MAX_LEN       2048

static char *prediction_to_string (VvasInferPrediction * self, int level);
static char *prediction_classes_to_string (VvasInferPrediction * self,
    int level);
static char *prediction_children_to_string (VvasInferPrediction * self,
    int level);
static char *bounding_box_to_string (VvasBoundingBox * bbox, int level);

/**
 *  @fn void vvas_inferprediction_free_node(void *data)
 *  \param [in]  self - Address of Prediction node
 *  \return  none 
 *  \brief This function deallocates memory of all child  nodes
 */
static void
vvas_inferprediction_free_node (void *data)
{
  VvasInferPrediction *self = (VvasInferPrediction *) data;

  if (NULL == self) {
    LOG_D ("Null received");
    return;
  }

  /* Free classification */
  if (self->classifications) {
    vvas_list_free_full (self->classifications,
        (vvas_list_free_notify) vvas_inferclassification_free);
    self->classifications = NULL;
  }

  /* free object label */
  if (self->obj_track_label) {
    free (self->obj_track_label);
  }

  if (self->model_name) {
    free (self->model_name);
  }

  if (self->reid.data && self->reid.free) {
    self->reid.free (&self->reid);
  }

  if (self->segmentation.data && self->segmentation.free) {
    self->segmentation.free (&self->segmentation);
  }

  if (self->tb) {
    self->tb->free ((void **)&self->tb);
  }

  if (self->node) {
    vvas_treenode_destroy (self->node);
    self->node = NULL;
  }

  free (self);
}

/**
 *  @fn void vvas_inferprediction_get_childnodes(VvasTreeNode * node, void* data)
 *  \param [in]  node - Address of the node
 *  \param [in]  data - user data to be passed
 *  \return  none 
 *  \brief This function appends all nodes into a usergiven list data structure
 */
static void
vvas_inferprediction_get_childnodes (VvasTreeNode * node, void *data)
{
  if ((NULL == node) || (NULL == data)) {
    LOG_D ("NULL received");
    return;
  }

  VvasList **children = (VvasList **) data;
  VvasInferPrediction *prediction;

  prediction = (VvasInferPrediction *) node->data;

  *children = vvas_list_append (*children, prediction);
}

/**
 *  @fn VvasList* vvas_inferprediction_get_nodes(VvasInferPrediction* self)
 *  \param [in]  self - Prediction instance
 *  \return  on Success list of all child nodes in the tree is returned. 
 *           on Failure NULL is returned.
 *  \brief This function appends all nodes into a usergiven list data structur
 */
static VvasList *
vvas_inferprediction_get_nodes (VvasInferPrediction * self)
{
  VvasList *children = NULL;

  if (NULL == self) {
    LOG_D ("NULL received");
    return NULL;
  }

  if (self->node) {
    vvas_treenode_traverse_child (self->node, TRAVERSE_ALL,
        vvas_inferprediction_get_childnodes, &children);
  }

  return children;
}

/**
 *  @fn void* vvas_inferprediction_classification_copy (const void *classification, void *data)
 *  @param [in] classification - VvasInferClassification object.
 *  @param [in] data - user data to be passed.
 *  
 *  @return On Success returns address of the new list.
 *          On Failure returns NULL
 *          
 *  @brief This function is used to be passed as param to vvas_list_copy_deep for list copy.
 */
static void *
vvas_inferprediction_classification_copy (const void *classification,
    void *data)
{
  VvasInferClassification *self = (VvasInferClassification *) classification;

  return vvas_inferclassification_copy (self);
}


/**
 *  @fn void* vvas_inferprediction_node_copy(const void *infer, void *data)
 *  @param [in] infer - VvasInferPrediction object will be passed while traversing to child nodes.
 *  @param [in] data - user data to be passed.
 *  
 *  @return On Success returns address of the new node.
 *          On Failure returns NULL 
 *          
 *  @brief This function is used to be passed as param to traverse tree node for node copy. 
 */
void *
vvas_inferprediction_node_copy (const void *infer, void *data)
{
  VvasInferPrediction *dmeta = NULL;
  VvasInferPrediction *smeta = (VvasInferPrediction *) infer;
  if (NULL != smeta) {
    dmeta = vvas_inferprediction_new ();
  }

  if (NULL != dmeta) {
    dmeta->prediction_id = smeta->prediction_id;
    dmeta->enabled = smeta->enabled;
    if (smeta->model_name) {
      dmeta->model_name = strdup (smeta->model_name);
    }
    if (smeta->obj_track_label) {
      dmeta->obj_track_label = strdup (smeta->obj_track_label);
    }
    dmeta->model_class = smeta->model_class;
    dmeta->count = smeta->count;
    dmeta->bbox_scaled = smeta->bbox_scaled;
    dmeta->pose14pt = smeta->pose14pt;
    dmeta->feature = smeta->feature;
    if (smeta->reid.data && smeta->reid.copy) {
      smeta->reid.copy (&smeta->reid, &dmeta->reid);
    }
    if (smeta->segmentation.data && smeta->segmentation.copy) {
      smeta->segmentation.copy (&smeta->segmentation, &dmeta->segmentation);
    }
    if (smeta->tb) {
      smeta->tb->copy ((void **)&smeta->tb, (void **)&dmeta->tb);
    }
    memcpy (&dmeta->bbox, &smeta->bbox, sizeof (VvasBoundingBox));
    dmeta->classifications = vvas_list_copy_deep (smeta->classifications,
        vvas_inferprediction_classification_copy, NULL);
  }
  return (void *) dmeta;
}

/**
 *  @fn  VvasInferPrediction * vvas_inferprediction_new(void)
 *  \param [in]  none 
 *  \return  On Success returns address of the new object instance of VvasInferPrediction.  
 *           On Failure returns NULL
 *  \brief This function allocates new memory for VvasInferPrediction structure
 */
VvasInferPrediction *
vvas_inferprediction_new (void)
{
  VvasInferPrediction *infer =
      (VvasInferPrediction *) malloc (sizeof (VvasInferPrediction));

  if (NULL != infer) {

    infer->prediction_id = vvas_inferprediction_get_prediction_id();
    infer->enabled = true;

    infer->bbox.x = 0;
    infer->bbox.y = 0;
    infer->bbox.width = 0;
    infer->bbox.height = 0;

    infer->bbox.box_color.red = 0;
    infer->bbox.box_color.green = 0;
    infer->bbox.box_color.blue = 0;
    infer->bbox.box_color.alpha = 0;

    infer->bbox_scaled = false;
    infer->obj_track_label = NULL;
    infer->classifications = NULL;
    infer->model_name = NULL;
    infer->model_class = VVAS_XCLASS_NOTFOUND;
    infer->reid.data = NULL;
    infer->segmentation.data = NULL;
    infer->feature.type = UNKNOWN_FEATURE;
    infer->tb = NULL;

    infer->node = vvas_treenode_new (infer);
  } else {
    LOG_D (" NULL Received ");
  }
  return infer;
}

/**
 *  @fn void vvas_inferprediction_append(VvasInferPrediction *self, VvasInferPrediction *child);
 *  @param [in] self - Instance of the parent node to which child node will be appended.
 *  @param [in] child - instance of the child node to be appended.
 *  @return none 
 *  @brief This function will append child node to parent node. 
 */
void
vvas_inferprediction_append (VvasInferPrediction * self,
    VvasInferPrediction * child)
{
  vvas_treenode_append (self->node, child->node);
}

/**
 *  @fn bool vvas_inferprediction_node_assign(const VvasTreeNode  *node, void *data)
 *  @param [in] node - Address of the node .
 *  @param [in] data - user data to be passed
 *  @return TRUE - To stop traversing
 *          FALSE - To continue traversing
 *  @brief This function is passed as parameter to traver all nodes in a tree.
 *
 */
static bool
vvas_inferprediction_node_assign (const VvasTreeNode * node, void *data)
{
  VvasInferPrediction *dmeta = NULL;
  if (NULL == node) {
    return false;
  }

  dmeta = (VvasInferPrediction *) node->data;
  if (dmeta->node) {
    vvas_treenode_destroy (dmeta->node);
  }
  dmeta->node = (VvasTreeNode *) node;

  return false;
}

/**
 *  @fn VvasInferPrediction* vvas_inferprediction_copy(VvasInferPrediction *smeta)   
 *  @param [in] smeta - Address of the source Prediction structure to be copied.
 *  @return On Success returns address of the new node in which data is copied.
 *          On Failure returns NULL 
 *          
 *  @brief This function is used to copy node data while performing deep-copy 
 *         of a tree node.
 */
VvasInferPrediction *
vvas_inferprediction_copy (VvasInferPrediction * smeta)
{
  VvasTreeNode *Node = NULL;

  if (NULL == smeta) {
    return NULL;
  }

  Node =
      vvas_treenode_copy_deep (smeta->node, vvas_inferprediction_node_copy,
      NULL);

  if (NULL == Node) {
    return NULL;
  }

  vvas_treenode_traverse (Node, IN_ORDER,
      TRAVERSE_ALL, -1, vvas_inferprediction_node_assign, NULL);

  return Node->data;
}

/**
 *  @fn  void vvas_inferprediction_free(VvasInferPrediction *self);
 *  @param [in] self - Address of VvasInferPrediction
 *  @return none
 *  @brief This function deallocates memory for VvasInferPrediction
 */
void
vvas_inferprediction_free (VvasInferPrediction * self)
{
  if (NULL == self) {
    LOG_D ("Null received");
    return;
  }

  VvasList *pred_nodes = vvas_inferprediction_get_nodes (self);

  if (NULL == pred_nodes) {
    /** Leaf node */
    vvas_inferprediction_free_node (self);
    return;
  }

  vvas_list_free_full (pred_nodes,
      (void (*)(void *)) vvas_inferprediction_free);

  /*Free parent node */
  vvas_inferprediction_free_node (self);
}

static char *
bounding_box_to_string (VvasBoundingBox * bbox, int level)
{
  int indent = level * 2;
  char tmp[MAX_LEN] = { 0, };
  char *box = NULL;

  if (!bbox) {
    return NULL;
  }

  snprintf (tmp, (MAX_LEN - 1), "{\n"
      "%*s  x : %d\n"
      "%*s  y : %d\n"
      "%*s  width : %u\n"
      "%*s  height : %u\n"
      "%*s}",
      indent, "", bbox->x,
      indent, "", bbox->y,
      indent, "", bbox->width, indent, "", bbox->height, indent, "");

  box = strdup(tmp);

  return box;
}

static char *
prediction_children_to_string (VvasInferPrediction * self, int level)
{
  VvasList *subpreds = NULL;
  VvasList *iter = NULL;
  char *string = NULL;

  if (!self) {
    return NULL;
  }

  subpreds = vvas_inferprediction_get_nodes (self);

  for (iter = subpreds; iter != NULL; iter = iter->next) {
    VvasInferPrediction *pred = (VvasInferPrediction *) iter->data;
    char *child = prediction_to_string (pred, level + 1);

    int old_len = 0;
    if (string) {
      old_len = strlen (string);
    }
    if (!string) {
      string = (char *) malloc (strlen (child) + 1);
    } else {
      string = realloc (string, strlen (child) + old_len + 1);
    }

    if (string) {
      memcpy (string + old_len, child, strlen (child));
      string[old_len + strlen (child)] = '\0';
    }
    free (child);
  }

  vvas_list_free (subpreds);

  return string;
}

static char *
prediction_classes_to_string (VvasInferPrediction * self, int level)
{
  VvasList *iter = NULL;
  char *string = NULL;

  if (!self) {
    return NULL;
  }

  for (iter = (VvasList *) self->classifications; iter != NULL;
      iter = iter->next) {
    VvasInferClassification *c = (VvasInferClassification *) iter->data;
    char *sclass = vvas_inferclassification_to_string (c, level + 1);

    int old_len = 0;
    if (string) {
      old_len = strlen (string);
    }
    if (!string) {
      string = (char *) malloc (strlen (sclass) + 1);
    } else {
      string = realloc (string, strlen (sclass) + old_len + 1);
    }

    memcpy (string + old_len, sclass, strlen (sclass));
    string[old_len + strlen (sclass)] = '\0';

    free (sclass);
  }

  return string;
}

static char *
prediction_to_string (VvasInferPrediction * self, int level)
{
  int indent = level * 2;
  char *bbox = NULL;
  char *children = NULL;
  char *classes = NULL;
  char *prediction = NULL;

  bbox = bounding_box_to_string (&self->bbox, level + 1);
  classes = prediction_classes_to_string (self, level + 1);
  children = prediction_children_to_string (self, level + 1);

  int len = 0;

  if (bbox != NULL) {
    len += strlen (bbox);
  }

  if (classes != NULL) {
    len += strlen (classes);
  }

  if (children != NULL) {
    len += strlen (children);
  }

  if(self->obj_track_label) {
    len += strlen(self->obj_track_label);
  }

  prediction = (char *) calloc (len + MAX_LEN, sizeof (char));
  if (prediction == NULL) {
    LOG_E("Failed to allocate memory of size=%d\n", len + MAX_LEN);
    goto error;
  }

  snprintf (prediction, (len + MAX_LEN - 1), "{\n"
      "%*s  id : %" "lu" ",\n"
      "%*s  enabled : %s,\n"
      "%*s  bbox : %s,\n"
      "%*s  track label : %s,\n"
      "%*s  classes : [\n"
      "%*s    %s\n"
      "%*s  ],\n"
      "%*s  predictions : [\n"
      "%*s    %s\n"
      "%*s  ]\n"
      "%*s}",
      indent, "", self->prediction_id,
      indent, "", self->enabled ? "True" : "False",
      indent, "", bbox,
      indent, "", self->obj_track_label,
      indent, "", indent, "", classes, indent, "",
      indent, "", indent, "", children, indent, "", indent, "");

error:
  free (bbox);
  free (children);
  free (classes);

  return prediction;
}

/**
 *  @fn char *vvas_inferprediction_to_string (VvasInferPrediction * self)
 *  @param [in]  self- Address of VvasInferPrediction
 *  @return  Returns a string with all predictions serialized.
 *  @brief This function creates a string of predictions.
 *  @note User has to free this memory.
 */
char *
vvas_inferprediction_to_string (VvasInferPrediction * self)
{
  char *serial = NULL;
  if (!self) {
    return NULL;
  }
  serial = prediction_to_string (self, 0);
  return serial;
}

/**
 *  @fn uint64_t vvas_inferprediction_get_prediction_id(void)
 *  @param [in]  none
 *  @return returns unique prediction id.
 *  @brief This function generates unique prediction id.
 */
uint64_t vvas_inferprediction_get_prediction_id(void)
{

  static uint64_t _id = 0ul;
  static VvasMutex _id_mutex;
  uint64_t ret = 0ul;

  vvas_mutex_lock (&_id_mutex);
  ret = _id++;
  vvas_mutex_unlock (&_id_mutex);

  return ret;
}
