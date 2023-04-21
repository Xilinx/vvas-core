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

#include <vvas_core/vvas_metaconvert.h>
#include <vvas_core/vvas_infer_prediction.h>
#include <vvas_core/vvas_infer_classification.h>

#define NEED_TEXT_BG_COLOR 1    /* Text will have backgroup color */
#define MAX_LABEL_LEN 1024

typedef struct
{
  VvasContext *vvas_ctx;
  VvasLogLevel log_level;
  VvasFontType font_type;
  float font_size;
  int32_t line_thickness;
  int32_t radius;
  uint8_t level;
  uint8_t mask_tree_level;
  uint32_t y_offset;
  bool draw_above_bbox_flag;
  char **allowed_labels;
  uint32_t allowed_labels_count;
  VvasFilterObjectInfo **allowed_classes;
  uint32_t allowed_classes_count;
} VvasMetaConvertPriv;

bool vvas_metaconvert_consider_child (VvasMetaConvert * meta_convert,
    VvasTreeNode * child);

int vvas_metaconvert_set_color_for_allowed_classes (VvasMetaConvertPriv * priv,
    VvasInferClassification * classification, uint8_t * do_mask,
    VvasRGBColor * clr);

static inline char *
append_string (const char *str1, const char *str2)
{
  uint32_t new_size = strlen (str1) + strlen (str2) + 1;
  char *new_str = NULL;

  new_str = (char *) malloc (new_size);
  if (!new_str) {
    return NULL;
  }

  strcpy (new_str, str1);
  strcpy (new_str + strlen (str1), str2);
  new_str[new_size - 1] = '\0';
  return new_str;
}

static char *
prepare_label_string (VvasMetaConvertPriv * priv,
    VvasInferPrediction * prediction, VvasInferClassification * classification)
{
  int idx;
  char *cur_label_string = NULL;

  for (idx = 0; idx < priv->allowed_labels_count; idx++) {
    if (classification->class_label &&
        !strcmp (priv->allowed_labels[idx], "class")) {
      char *first_label = NULL;
      char *save_ptr = NULL;

      first_label = strtok_r (classification->class_label, ",", &save_ptr);
      if (!first_label)
        continue;

      if (!cur_label_string)
        cur_label_string = strdup (first_label);
      else {
        char *tmp_str;

        tmp_str = append_string (cur_label_string, " : ");
        free (cur_label_string);

        cur_label_string = append_string (tmp_str, first_label);
        free (tmp_str);
      }
    } else if (prediction && prediction->obj_track_label &&
        !strcmp (priv->allowed_labels[idx], "tracker-id")) {
      if (!cur_label_string)
        cur_label_string = strdup (prediction->obj_track_label);
      else {
        char *tmp_str;

        tmp_str = append_string (cur_label_string, " : tid - ");
        free (cur_label_string);

        cur_label_string = append_string (tmp_str, prediction->obj_track_label);
        free (tmp_str);
      }
    } else if (!strcmp (priv->allowed_labels[idx], "probability")) {
      char prob[128] = { 0, };

      snprintf (prob, 128, "%.2f", classification->class_prob);

      if (!cur_label_string)
        cur_label_string = strdup (prob);
      else {
        char *tmp_str;

        tmp_str = append_string (cur_label_string, " : prob - ");
        free (cur_label_string);

        cur_label_string = append_string (tmp_str, prob);
        free (tmp_str);
      }
    }
  }

  return cur_label_string;
}

static VvasReturnType
convert_pose_detection_meta (VvasMetaConvertPriv * priv, VvasTreeNode * node,
    VvasOverlayShapeInfo * shape_info)
{
  VvasInferPrediction *prediction = (VvasInferPrediction *) node->data;
  Pose14Pt *pose_ptr = &prediction->pose14pt;
  Pointf *pt_ptr = (Pointf *) & prediction->pose14pt;
  int num_circles = shape_info->num_circles;
  int num_lines = shape_info->num_lines;
  int num, idx;
  int level = vvas_treenode_get_depth (node) - 1;
  VvasOverlayLineParams *line_params;
  VvasList *head;

  LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->log_level, "parsing pose detection meta");

  /* Add circles for each point */
  for (num = 0; num < sizeof (Pose14Pt) / sizeof (Pointf); num++) {
    VvasOverlayCircleParams *circle_params =
        (VvasOverlayCircleParams *) calloc (1,
        sizeof (VvasOverlayCircleParams));
    VvasOverlayColorData *circle_color = &(circle_params->circle_color);

    circle_params->center_pt.x = pt_ptr->x;
    circle_params->center_pt.y = pt_ptr->y;
    circle_params->radius = 3;
    circle_params->thickness = 3;
    if (level == 1) {
      circle_color->blue = 255; /*blue */
      circle_color->green = 0;
      circle_color->red = 0;
    } else if (level == 2) {
      circle_color->blue = 0;
      circle_color->green = 255;        /*green */
      circle_color->red = 0;
    } else if (level == 3) {
      circle_color->blue = 0;
      circle_color->green = 0;
      circle_color->red = 255;  /*red */
    } else {
      circle_color->blue = 225;
      circle_color->green = 225;
      circle_color->red = 0;    /*aqua */
    }

    /* append circle */
    shape_info->circle_params =
        vvas_list_append (shape_info->circle_params, circle_params);
    num_circles++;
    pt_ptr++;
  }
  shape_info->num_circles = num_circles;

  /* Add lines */
  line_params =
      (VvasOverlayLineParams *) calloc (1, sizeof (VvasOverlayLineParams));
  if (!line_params) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, priv->log_level, "failed to allocate memory");
    return VVAS_RET_ALLOC_ERROR;
  }
  line_params->start_pt.x = pose_ptr->right_shoulder.x;
  line_params->start_pt.y = pose_ptr->right_shoulder.y;
  line_params->end_pt.x = pose_ptr->right_elbow.x;
  line_params->end_pt.y = pose_ptr->right_elbow.y;
  shape_info->line_params =
      vvas_list_append (shape_info->line_params, line_params);
  num_lines++;

  line_params =
      (VvasOverlayLineParams *) calloc (1, sizeof (VvasOverlayLineParams));
  if (!line_params) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, priv->log_level, "failed to allocate memory");
    return VVAS_RET_ALLOC_ERROR;
  }
  line_params->start_pt.x = pose_ptr->right_elbow.x;
  line_params->start_pt.y = pose_ptr->right_elbow.y;
  line_params->end_pt.x = pose_ptr->right_wrist.x;
  line_params->end_pt.y = pose_ptr->right_wrist.y;
  shape_info->line_params =
      vvas_list_append (shape_info->line_params, line_params);
  num_lines++;

  line_params =
      (VvasOverlayLineParams *) calloc (1, sizeof (VvasOverlayLineParams));
  if (!line_params) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, priv->log_level, "failed to allocate memory");
    return VVAS_RET_ALLOC_ERROR;
  }
  line_params->start_pt.x = pose_ptr->right_hip.x;
  line_params->start_pt.y = pose_ptr->right_hip.y;
  line_params->end_pt.x = pose_ptr->right_knee.x;
  line_params->end_pt.y = pose_ptr->right_knee.y;
  shape_info->line_params =
      vvas_list_append (shape_info->line_params, line_params);
  num_lines++;

  line_params =
      (VvasOverlayLineParams *) calloc (1, sizeof (VvasOverlayLineParams));
  if (!line_params) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, priv->log_level, "failed to allocate memory");
    return VVAS_RET_ALLOC_ERROR;
  }
  line_params->start_pt.x = pose_ptr->right_knee.x;
  line_params->start_pt.y = pose_ptr->right_knee.y;
  line_params->end_pt.x = pose_ptr->right_ankle.x;
  line_params->end_pt.y = pose_ptr->right_ankle.y;
  shape_info->line_params =
      vvas_list_append (shape_info->line_params, line_params);
  num_lines++;

  line_params =
      (VvasOverlayLineParams *) calloc (1, sizeof (VvasOverlayLineParams));
  if (!line_params) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, priv->log_level, "failed to allocate memory");
    return VVAS_RET_ALLOC_ERROR;
  }
  line_params->start_pt.x = pose_ptr->left_shoulder.x;
  line_params->start_pt.y = pose_ptr->left_shoulder.y;
  line_params->end_pt.x = pose_ptr->left_elbow.x;
  line_params->end_pt.y = pose_ptr->left_elbow.y;
  shape_info->line_params =
      vvas_list_append (shape_info->line_params, line_params);
  num_lines++;

  line_params =
      (VvasOverlayLineParams *) calloc (1, sizeof (VvasOverlayLineParams));
  if (!line_params) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, priv->log_level, "failed to allocate memory");
    return VVAS_RET_ALLOC_ERROR;
  }
  line_params->start_pt.x = pose_ptr->left_elbow.x;
  line_params->start_pt.y = pose_ptr->left_elbow.y;
  line_params->end_pt.x = pose_ptr->left_wrist.x;
  line_params->end_pt.y = pose_ptr->left_wrist.y;
  line_params =
      (VvasOverlayLineParams *) vvas_list_append (shape_info->line_params,
      line_params);
  num_lines++;

  line_params =
      (VvasOverlayLineParams *) calloc (1, sizeof (VvasOverlayLineParams));
  if (!line_params) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, priv->log_level, "failed to allocate memory");
    return VVAS_RET_ALLOC_ERROR;
  }
  line_params->start_pt.x = pose_ptr->left_hip.x;
  line_params->start_pt.y = pose_ptr->left_hip.y;
  line_params->end_pt.x = pose_ptr->left_knee.x;
  line_params->end_pt.y = pose_ptr->left_knee.y;
  shape_info->line_params =
      vvas_list_append (shape_info->line_params, line_params);
  num_lines++;

  line_params =
      (VvasOverlayLineParams *) calloc (1, sizeof (VvasOverlayLineParams));
  if (!line_params) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, priv->log_level, "failed to allocate memory");
    return VVAS_RET_ALLOC_ERROR;
  }
  line_params->start_pt.x = pose_ptr->left_knee.x;
  line_params->start_pt.y = pose_ptr->left_knee.y;
  line_params->end_pt.x = pose_ptr->left_ankle.x;
  line_params->end_pt.y = pose_ptr->left_ankle.y;
  shape_info->line_params =
      vvas_list_append (shape_info->line_params, line_params);
  num_lines++;

  line_params =
      (VvasOverlayLineParams *) calloc (1, sizeof (VvasOverlayLineParams));
  if (!line_params) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, priv->log_level, "failed to allocate memory");
    return VVAS_RET_ALLOC_ERROR;
  }
  line_params->start_pt.x = pose_ptr->head.x;
  line_params->start_pt.y = pose_ptr->head.y;
  line_params->end_pt.x = pose_ptr->neck.x;
  line_params->end_pt.y = pose_ptr->neck.y;
  shape_info->line_params =
      vvas_list_append (shape_info->line_params, line_params);
  num_lines++;

  line_params =
      (VvasOverlayLineParams *) calloc (1, sizeof (VvasOverlayLineParams));
  if (!line_params) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, priv->log_level, "failed to allocate memory");
    return VVAS_RET_ALLOC_ERROR;
  }
  line_params->start_pt.x = pose_ptr->right_shoulder.x;
  line_params->start_pt.y = pose_ptr->right_shoulder.y;
  line_params->end_pt.x = pose_ptr->neck.x;
  line_params->end_pt.y = pose_ptr->neck.y;
  shape_info->line_params =
      vvas_list_append (shape_info->line_params, line_params);
  num_lines++;

  line_params =
      (VvasOverlayLineParams *) calloc (1, sizeof (VvasOverlayLineParams));
  if (!line_params) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, priv->log_level, "failed to allocate memory");
    return VVAS_RET_ALLOC_ERROR;
  }
  line_params->start_pt.x = pose_ptr->left_shoulder.x;
  line_params->start_pt.y = pose_ptr->left_shoulder.y;
  line_params->end_pt.x = pose_ptr->neck.x;
  line_params->end_pt.y = pose_ptr->neck.y;
  shape_info->line_params =
      vvas_list_append (shape_info->line_params, line_params);
  num_lines++;

  line_params =
      (VvasOverlayLineParams *) calloc (1, sizeof (VvasOverlayLineParams));
  if (!line_params) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, priv->log_level, "failed to allocate memory");
    return VVAS_RET_ALLOC_ERROR;
  }
  line_params->start_pt.x = pose_ptr->right_shoulder.x;
  line_params->start_pt.y = pose_ptr->right_shoulder.y;
  line_params->end_pt.x = pose_ptr->right_hip.x;
  line_params->end_pt.y = pose_ptr->right_hip.y;
  shape_info->line_params =
      vvas_list_append (shape_info->line_params, line_params);
  num_lines++;

  line_params =
      (VvasOverlayLineParams *) calloc (1, sizeof (VvasOverlayLineParams));
  if (!line_params) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, priv->log_level, "failed to allocate memory");
    return VVAS_RET_ALLOC_ERROR;
  }
  line_params->start_pt.x = pose_ptr->left_shoulder.x;
  line_params->start_pt.y = pose_ptr->left_shoulder.y;
  line_params->end_pt.x = pose_ptr->left_hip.x;
  line_params->end_pt.y = pose_ptr->left_hip.y;
  shape_info->line_params =
      vvas_list_append (shape_info->line_params, line_params);
  num_lines++;

  line_params =
      (VvasOverlayLineParams *) calloc (1, sizeof (VvasOverlayLineParams));
  if (!line_params) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, priv->log_level, "failed to allocate memory");
    return VVAS_RET_ALLOC_ERROR;
  }
  line_params->start_pt.x = pose_ptr->right_hip.x;
  line_params->start_pt.y = pose_ptr->right_hip.y;
  line_params->end_pt.x = pose_ptr->left_hip.x;
  line_params->end_pt.y = pose_ptr->left_hip.y;
  shape_info->line_params =
      vvas_list_append (shape_info->line_params, line_params);
  num_lines++;

  /* Add line colour in loop */
  idx = 0;
  head = shape_info->line_params;
  /* Updating the head ptr to start of the newly predicted lines
   * if already previous predicted lines are present */
  while (shape_info->num_lines && idx != shape_info->num_lines && head) {
    idx++;
    head = head->next;
  }

  for (num = shape_info->num_lines;
      head
      && num < ((sizeof (Pose14Pt) / sizeof (Pointf)) + shape_info->num_lines);
      num++) {
    VvasOverlayColorData *line_color;
    line_params = (VvasOverlayLineParams *) head->data;
    line_color = &(line_params->line_color);

    line_params->thickness = 3;
    if (level == 1) {
      line_color->blue = 255;   /*blue */
      line_color->green = 0;
      line_color->red = 0;
    } else if (level == 2) {
      line_color->blue = 0;
      line_color->green = 255;  /*green */
      line_color->red = 0;
    } else if (level == 3) {
      line_color->blue = 0;
      line_color->green = 0;
      line_color->red = 255;    /*red */
    } else {
      line_color->blue = 225;
      line_color->green = 225;
      line_color->red = 0;      /*aqua */
    }
    head = head->next;
  }

  shape_info->num_lines = num_lines;
  return VVAS_RET_SUCCESS;
}

static VvasReturnType
convert_face_landmark_meta (VvasMetaConvertPriv * priv, VvasTreeNode * node,
    VvasOverlayShapeInfo * shape_info)
{
  VvasInferPrediction *prediction = (VvasInferPrediction *) node->data;
  int idx;
  int level = vvas_treenode_get_depth (node) - 1;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->log_level, "parsing pose detection meta");

  /* Add circles for each point */
  for (idx = 0; idx < NUM_LANDMARK_POINT; idx++) {
    Pointf *pt_ptr = (Pointf *) & (prediction->feature.landmark[idx].x);
    VvasOverlayCircleParams *circle_params =
        (VvasOverlayCircleParams *) calloc (1,
        sizeof (VvasOverlayCircleParams));
    if (!circle_params) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, priv->log_level,
          "failed to allocate memory");
      return VVAS_RET_ALLOC_ERROR;
    }
    VvasOverlayColorData *circle_color = &(circle_params->circle_color);

    circle_params->center_pt.x = pt_ptr->x;
    circle_params->center_pt.y = pt_ptr->y;
    circle_params->radius = priv->radius;
    circle_params->thickness = priv->line_thickness;

    if (level == 1) {
      circle_color->blue = 255; /*blue */
      circle_color->green = 0;
      circle_color->red = 0;
    } else if (level == 2) {
      circle_color->blue = 0;
      circle_color->green = 255;        /*green */
      circle_color->red = 0;
    } else if (level == 3) {
      circle_color->blue = 0;
      circle_color->green = 0;
      circle_color->red = 255;  /*red */
    } else {
      circle_color->blue = 225;
      circle_color->green = 225;
      circle_color->red = 0;    /*aqua */
    }

    shape_info->circle_params =
        vvas_list_append (shape_info->circle_params, circle_params);
    shape_info->num_circles++;
  }

  return VVAS_RET_SUCCESS;
}

static VvasReturnType
convert_road_line_meta (VvasMetaConvertPriv * priv, VvasTreeNode * node,
    VvasOverlayShapeInfo * shape_info)
{
  VvasInferPrediction *prediction = (VvasInferPrediction *) node->data;
  int idx;
  int type = prediction->feature.line_type;
  int line_size = prediction->feature.line_size;
  VvasOverlayPolygonParams *polygn_params = NULL;
  VvasOverlayColorData *line_color = NULL;

  LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->log_level, "parsing road line meta");

  polygn_params =
      (VvasOverlayPolygonParams *) calloc (1,
      sizeof (VvasOverlayPolygonParams));
  if (!polygn_params) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, priv->log_level, "failed to allocate memory");
    return VVAS_RET_ALLOC_ERROR;
  }

  line_color = &(polygn_params->poly_color);

  for (idx = 0; idx < line_size; idx++) {
    Pointf *pt_ptr = (Pointf *) & (prediction->feature.road_line[idx].x);
    VvasOverlayCoordinates *poly_pts =
        (VvasOverlayCoordinates *) calloc (1, sizeof (VvasOverlayCoordinates));
    if (!poly_pts) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, priv->log_level,
          "failed to allocate memory");
      free (polygn_params);
      return VVAS_RET_ALLOC_ERROR;
    }
    poly_pts->x = pt_ptr->x;
    poly_pts->y = pt_ptr->y;
    polygn_params->poly_pts =
        vvas_list_append (polygn_params->poly_pts, poly_pts);
  }

  polygn_params->thickness = 3;
  polygn_params->num_pts = line_size;
  if (type == 0) {
    line_color->blue = 255;
    line_color->green = 255;
    line_color->red = 0;        /* aqua */
  } else if (type == 1) {
    line_color->blue = 255;
    line_color->green = 0;
    line_color->red = 0;        /* blue */
  } else if (type == 2) {
    line_color->blue = 0;
    line_color->green = 255;    /* green */
    line_color->red = 0;
  } else if (type == 3) {
    line_color->blue = 0;
    line_color->green = 0;
    line_color->red = 255;      /* red */
  }

  shape_info->polygn_params =
      vvas_list_append (shape_info->polygn_params, polygn_params);
  shape_info->num_polys++;
  return VVAS_RET_SUCCESS;
}

static VvasReturnType
convert_ultrafast_meta (VvasMetaConvertPriv * priv, VvasTreeNode * node,
    VvasOverlayShapeInfo * shape_info)
{
  VvasInferPrediction *prediction = (VvasInferPrediction *) node->data;
  int num;
  int level = prediction->feature.line_type;
  int line_size = prediction->feature.line_size;

  LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->log_level, "parsing ultrafast meta");

  for (num = 0; num < line_size; num++) {
    Pointf *pt_ptr = (Pointf *) & (prediction->feature.road_line[num].x);
    VvasOverlayCircleParams *circle_params = NULL;
    VvasOverlayColorData *circle_color = NULL;

    if (pt_ptr->x < 0)
      continue;

    circle_params =
        (VvasOverlayCircleParams *) calloc (1,
        sizeof (VvasOverlayCircleParams));
    if (!circle_params) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, priv->log_level,
          "failed to allocate memory");
      return VVAS_RET_ALLOC_ERROR;
    }

    circle_color = &(circle_params->circle_color);

    circle_params->center_pt.x = pt_ptr->x;
    circle_params->center_pt.y = pt_ptr->y;
    circle_params->radius = priv->radius;
    circle_params->thickness = priv->line_thickness;

    if (level == 0) {
      circle_color->blue = 255;
      circle_color->green = 0;
      circle_color->red = 0;    /* blue */
    } else if (level == 1) {
      circle_color->blue = 0;
      circle_color->green = 255;        /* green */
      circle_color->red = 0;
    } else if (level == 2) {
      circle_color->blue = 255;
      circle_color->green = 255;
      circle_color->red = 0;    /* aqua */
    } else {
      circle_color->blue = 0;
      circle_color->green = 0;
      circle_color->red = 255;  /* red */
    }
    shape_info->num_circles++;
    shape_info->circle_params =
        vvas_list_append (shape_info->circle_params, circle_params);
  }

  return VVAS_RET_SUCCESS;
}

/**
 *  @fn bool vvas_metaconvert_consider_child (VvasMetaConvert *meta_convert,  VvasTreeNode *child);
 *  @param [in] meta_convert - Handle to VVAS Meta convert
 *  @param [in] child - Handle to child node of Inference prediction tree
 *  @return TRUE in SUCCESS
 *          FALSE in FAILURE
 *  @brief Consider a child whether to call in recursive mode
 */
bool
vvas_metaconvert_consider_child (VvasMetaConvert * meta_convert,
    VvasTreeNode * child)
{
  VvasInferPrediction *child_pred = (VvasInferPrediction *) child->data;
  bool bret = FALSE;
  /* consider a child if it contains bboxes */
  if (child_pred->bbox.width && child_pred->bbox.height) {
    bret = TRUE;
  }
  /* consider child if it belongs to below  model class */
  else if ((child_pred->model_class == VVAS_XCLASS_FACELANDMARK) ||
      (child_pred->model_class == VVAS_XCLASS_ROADLINE) ||
      (child_pred->model_class == VVAS_XCLASS_POSEDETECT) ||
      (child_pred->model_class == VVAS_XCLASS_BCC) ||
      (child_pred->model_class == VVAS_XCLASS_ULTRAFAST)) {

    bret = TRUE;
  } else {
    bret = FALSE;
  }
  return bret;
}

/**
 *  @fn VvasMetaConvert *vvas_metaconvert_create (VvasContext *vvas_ctx,
 *                                                                               VvasMetaConvertConfig *cfg)
 *  @param [in] vvas_ctx - Handle to VVAS context
 *  @param [in] cfg - Handle to VvasMetaConvertConfig
 *  @return Handle to VvasMetaConvert
 *  @brief Creates VvasMetaConvert handle based on \p cfg
 */
VvasMetaConvert *
vvas_metaconvert_create (VvasContext * vvas_ctx, VvasMetaConvertConfig * cfg,
    VvasLogLevel log_level, VvasReturnType * ret)
{
  VvasMetaConvertPriv *priv = NULL;
  int i;

  priv = (VvasMetaConvertPriv *) calloc (1, sizeof (VvasMetaConvertPriv));
  if (!priv) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, log_level, "failed to allocate memory");
    if (ret)
      *ret = VVAS_RET_ALLOC_ERROR;
    goto error;
  }

  priv->vvas_ctx = vvas_ctx;
  priv->log_level = log_level;
  priv->font_type = cfg->font_type;
  priv->font_size = cfg->font_size;
  priv->line_thickness = cfg->line_thickness;
  priv->radius = cfg->radius;
  priv->level = cfg->level;
  priv->mask_tree_level = cfg->mask_level + 1;  /* inference level and tree level are different */
  priv->y_offset = cfg->y_offset;
  priv->draw_above_bbox_flag = cfg->draw_above_bbox_flag;

  if (cfg->allowed_labels_count) {
    /* copy filter labels if it exists */
    priv->allowed_labels =
        (char **) calloc (cfg->allowed_labels_count, sizeof (char *));
    if (!priv->allowed_labels) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, log_level, "failed to allocate memory");
      if (ret)
        *ret = VVAS_RET_ALLOC_ERROR;
      goto error;
    }

    for (i = 0; i < cfg->allowed_labels_count; i++)
      priv->allowed_labels[i] = strdup (cfg->allowed_labels[i]);

    priv->allowed_labels_count = cfg->allowed_labels_count;
  }

  if (cfg->allowed_classes_count) {
    /* copy filter objects if it exists */
    priv->allowed_classes =
        (VvasFilterObjectInfo **) calloc (cfg->allowed_classes_count,
        sizeof (VvasFilterObjectInfo *));
    if (!priv->allowed_classes) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, log_level, "failed to allocate memory");
      if (ret)
        *ret = VVAS_RET_ALLOC_ERROR;
      goto error;
    }

    for (i = 0; i < cfg->allowed_classes_count; i++) {
      priv->allowed_classes[i] =
          (VvasFilterObjectInfo *) calloc (1, sizeof (VvasFilterObjectInfo));

      memcpy (&priv->allowed_classes[i]->name,
          &cfg->allowed_classes[i]->name, META_CONVERT_MAX_STR_LENGTH - 1);
      priv->allowed_classes[i]->name[META_CONVERT_MAX_STR_LENGTH - 1] = '\0';
      memcpy (&priv->allowed_classes[i]->color,
          &cfg->allowed_classes[i]->color, sizeof (VvasRGBColor));
      priv->allowed_classes[i]->do_mask = cfg->allowed_classes[i]->do_mask;
    }

    priv->allowed_classes_count = cfg->allowed_classes_count;
  }

  return (VvasMetaConvert *) priv;

error:
  if (priv)
    free (priv);
  return NULL;
}

/**
 *  @fn vvas_metaconvert_set_color_for_allowed_classes (VvasMetaConvertPriv * priv,
 *                               VvasInferClassification * classification, uint8_t * do_mask,
 *                               VvasRGBColor * clr)
 *  @param [in] priv - Meta convert private handler
 *  @param [in] classification - classification information of a node
 *  @param [out] do_mask - Returns whether to mask the object of the given node or not
 *  @param [out] clr - Returns the label color information of the given node
 *  @return int
 *  @brief Returns the label color information of the given node if the node class is part of allowed classes
 */
int
vvas_metaconvert_set_color_for_allowed_classes (VvasMetaConvertPriv * priv,
    VvasInferClassification * classification, uint8_t * do_mask,
    VvasRGBColor * clr)
{
  int allowed_class_idx = -1;
  int i;

  for (i = 0; i < priv->allowed_classes_count; i++) {
    if (!strncmp (classification->class_label, priv->allowed_classes[i]->name,
            META_CONVERT_MAX_STR_LENGTH)) {
      LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->log_level, "class %s in allowed list",
          classification->class_label);
      allowed_class_idx = i;
      break;
    }
  }

  if (priv->allowed_classes && allowed_class_idx >= 0) {
    clr->blue = priv->allowed_classes[allowed_class_idx]->color.blue;
    clr->red = priv->allowed_classes[allowed_class_idx]->color.red;
    clr->green = priv->allowed_classes[allowed_class_idx]->color.green;
    *do_mask = priv->allowed_classes[allowed_class_idx]->do_mask;
  }

  return allowed_class_idx;
}

/**
 *  @fn void vvas_metaconvert_prepare_overlay_metadata (VvasMetaConvert *meta_convert,
 *                                                                  VvasTreeNode *parent,
 *                                                                  VvasOverlayShapeInfo *shape_info));
 *  @param [in] meta_convert - Handle to VVAS Meta convert
 *  @param [in] parent - Handle to parent node of Inference prediction tree
 *  @param [out] shape_info - Handle to overlay information which will be used overlay module to draw bounding box
 *  @return VvasReturnType
 *  @brief Converts Inference prediction tree to structure which can be understood by overlay module
 */
VvasReturnType
vvas_metaconvert_prepare_overlay_metadata (VvasMetaConvert * meta_convert,
    VvasTreeNode * parent, VvasOverlayShapeInfo * shape_info)
{
  VvasMetaConvertPriv *priv = (VvasMetaConvertPriv *) meta_convert;
  VvasInferPrediction *parent_pred = (VvasInferPrediction *) parent->data;
  VvasList *parent_classes;
  VvasInferClassification *classification;
  char *label_string = NULL;
  VvasRGBColor clr = { 0, };
  int level = vvas_treenode_get_depth (parent);
  VvasTreeNode *child = parent->children;
  uint8_t do_mask = 0;
  uint8_t rectangle_attached = 0;
  VvasReturnType vret = VVAS_RET_SUCCESS;

  LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->log_level, "node %p at depth %d",
      parent, level);

  if (level == 1) {
    clr.blue = 255;             /* blue */
    clr.green = 0;
    clr.red = 0;
  } else if (level == 2) {
    clr.blue = 0;
    clr.green = 255;            /* green */
    clr.red = 0;
  } else if (level == 3) {
    clr.blue = 0;
    clr.green = 0;
    clr.red = 255;              /* red */
  } else {
    clr.blue = 255;
    clr.green = 255;
    clr.red = 0;                /* aqua */
  }

  if (!parent_pred->enabled) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->log_level,
        "Object is inactive. Not displaying");
    return VVAS_RET_SUCCESS;
  }

  for (parent_classes = parent_pred->classifications; parent_classes;
      parent_classes = parent_classes->next) {
    int allowed_class_idx = -1;

    classification = (VvasInferClassification *) parent_classes->data;

    LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->log_level,
        "class label to bbox node : %s", classification->class_label);
    if (priv->allowed_classes && classification->class_label) {
      allowed_class_idx =
          vvas_metaconvert_set_color_for_allowed_classes (priv, classification,
          &do_mask, &clr);
    }

    if (!priv->allowed_classes || (priv->allowed_classes
            && allowed_class_idx >= 0)) {
      /* Prepare label_string only for infer level which is same as display_level */
      if (priv->level == 0 || (level - 1) == priv->level) {
        if (!label_string) {
          label_string =
              prepare_label_string (priv, parent_pred, classification);
        } else {
          char *cur_label_string =
              prepare_label_string (priv, parent_pred, classification);
          char *tmp_str;

          tmp_str = append_string (label_string, ", ");
          free (label_string);

          label_string = append_string (tmp_str, cur_label_string);
          free (tmp_str);
          free (cur_label_string);
        }
      }
    }
  }

  LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->log_level,
      "mode class %d", parent_pred->model_class);

  if (parent_pred->model_class == VVAS_XCLASS_POSEDETECT) {
    /* Add posedetect model coordinates in overlay meta */
    vret = convert_pose_detection_meta (priv, parent, shape_info);
  } else if (parent_pred->model_class == VVAS_XCLASS_FACELANDMARK) {
    /* Add posedetect model coordinates in overlay meta */
    vret = convert_face_landmark_meta (priv, parent, shape_info);
  } else if (parent_pred->model_class == VVAS_XCLASS_BCC) {
    /* get count value and convert to text to print as level */
    char bcc_text[MAX_LABEL_LEN];
    VvasOverlayTextParams *text_params =
        (VvasOverlayTextParams *) calloc (1, sizeof (VvasOverlayTextParams));
    if (!text_params) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, priv->log_level,
          "failed to allocate memory");
      if (label_string)
        free (label_string);

      return VVAS_RET_ALLOC_ERROR;
    }
    LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->log_level, "parsing BCC meta");

    memset (bcc_text, 0, MAX_LABEL_LEN);
    sprintf (bcc_text, "Crowd = %d", parent_pred->count);
    /* Update the label string information */
    shape_info->num_text++;
    /* default text will be drawn  inside the image since there is no bbox */
    text_params->bottom_left_origin = 0;

    if (priv->draw_above_bbox_flag)
      text_params->bottom_left_origin = 1;

    /* TODO: fix x and y location of text */
    text_params->points.x = 0;
    text_params->points.y = priv->y_offset;

    /* If y is zero bottom_left_origin will be set zero for drawing
       text inside image */
    if (!text_params->points.y)
      text_params->bottom_left_origin = 0;

    text_params->text_font.font_size = priv->font_size;
    text_params->text_font.font_num = priv->font_type;
    text_params->disp_text = strdup (bcc_text);
    text_params->apply_bg_color = 1;
    text_params->bg_color.blue = 0;
    text_params->bg_color.green = 255;
    text_params->bg_color.red = 255;
    shape_info->text_params =
        vvas_list_append (shape_info->text_params, text_params);
  } else if (parent_pred->model_class == VVAS_XCLASS_ROADLINE) {
    vret = convert_road_line_meta (priv, parent, shape_info);
  } else if (parent_pred->model_class == VVAS_XCLASS_ULTRAFAST) {
    vret = convert_ultrafast_meta (priv, parent, shape_info);
  }

  if (VVAS_IS_ERROR (vret)) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->log_level,
        "failed to create overlay meta");
    if (label_string)
      free (label_string);
    return vret;
  }

  LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->log_level, "child : %p", child);

  while (child) {
    VvasInferPrediction *child_pred = (VvasInferPrediction *) child->data;
    VvasInferClassification *classification = NULL;
    VvasList *classes;
    uint8_t append_slash = TRUE;
    uint8_t child_level = level + 1;

    LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->log_level,
        "child node %p at level %d, display_level %d, width = %d and height = %d",
        child, child_level, priv->level, child_pred->bbox.width,
        child_pred->bbox.height);

    if (vvas_metaconvert_consider_child (meta_convert, child) == TRUE) {
      /* ignore detection child node as it will be parsed as parent node */
      vvas_metaconvert_prepare_overlay_metadata (meta_convert, child,
          shape_info);
      child = child->next;
      continue;
    }

    for (classes = child_pred->classifications; classes;
        classes = classes->next) {
      char *class_label_str = NULL;

      classification = (VvasInferClassification *) classes->data;

      /* Prepare label_string only for child_level which is same as display_level
         and exclude all other child_level label_string */
      if (priv->level != 0 && (child_level - 1) != priv->level)
        continue;

      class_label_str = prepare_label_string (priv, NULL, classification);
      if (!class_label_str)
        continue;

      if (priv->allowed_classes && classification->class_label) {
        vvas_metaconvert_set_color_for_allowed_classes (priv, classification,
            &do_mask, &clr);
      }

      if (append_slash) {
        if (label_string) {
          char *tmp_str;

          tmp_str = append_string (label_string, "\n");
          free (label_string);

          label_string = append_string (tmp_str, class_label_str);
          free (tmp_str);
          free (class_label_str);
        } else {
          label_string = class_label_str;
        }
        append_slash = FALSE;
      } else {
        if (label_string) {
          if (label_string[strlen (label_string) - 1] == ',') { /* has "," as suffix */
            char *tmp_str;

            tmp_str = append_string (label_string, class_label_str);
            free (label_string);
            label_string = tmp_str;
          } else {
            char *tmp_str;

            tmp_str = append_string (label_string, ", ");       /* add comma as separator */
            free (label_string);

            label_string = append_string (tmp_str, class_label_str);
            free (tmp_str);
          }
          free (class_label_str);
        } else {
          label_string = class_label_str;
        }
      }
    }
    child = child->next;
  }

  if (level != 1 && (priv->level == 0 || (level - 1) == priv->level)) {
    if (parent_pred->bbox.width && parent_pred->bbox.height) {
      VvasOverlayRectParams *rect_params =
          (VvasOverlayRectParams *) calloc (1, sizeof (VvasOverlayRectParams));
      if (!rect_params) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, priv->log_level,
            "failed to allocate memory...");
        
        if (label_string)
          free (label_string);

        return VVAS_RET_ALLOC_ERROR;
      }
      rect_params->points.x = parent_pred->bbox.x;
      rect_params->points.y = parent_pred->bbox.y;
      rect_params->width = parent_pred->bbox.width;
      rect_params->height = parent_pred->bbox.height;
      rect_params->thickness = priv->line_thickness;
      rect_params->rect_color.red = clr.red;
      rect_params->rect_color.green = clr.green;
      rect_params->rect_color.blue = clr.blue;
      rect_params->apply_bg_color = 0;

      if (do_mask || ((priv->mask_tree_level)
              && (level == priv->mask_tree_level))) {
        /* Apply masking when class string matches or level matches */
        rect_params->apply_bg_color = 1;
        rect_params->bg_color.red = 0;
        rect_params->bg_color.green = 0;
        rect_params->bg_color.blue = 0;
      }

      LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->log_level,
          "appending rectangle [%u] : x = %u, y = %u, width = %u, height = %u"
          "Color : B = %u, G = %u, R = %u", shape_info->num_rects,
          parent_pred->bbox.x, parent_pred->bbox.y, parent_pred->bbox.width,
          parent_pred->bbox.height, rect_params->rect_color.blue,
          rect_params->rect_color.green, rect_params->rect_color.red);
      shape_info->rect_params =
          vvas_list_append (shape_info->rect_params, rect_params);
      shape_info->num_rects++;
      rectangle_attached = 1;
    }
  }

  if (label_string) {
    /* Add bounding box if label_string exists, with this approach we always have label with
       bounding box. Here 3 possible cases cover as per display_level
       case 1 - display only parent label, always have bounding box so add from same node
       case 2 - display only child label, do not have bounding box so consider parent bounding box
       case 3 - display child and parent label, have parent bounding box so add parent bounding box
       case 4 - display only labels, do not have bounding box so do not add bounding box */
    if ((level != 1) && (rectangle_attached == 0) &&
        parent_pred->bbox.width && parent_pred->bbox.height) {
      VvasOverlayRectParams *rect_params = (VvasOverlayRectParams *) calloc (1,
          sizeof (VvasOverlayRectParams));
      if (!rect_params) {
        free (label_string);
        LOG_MESSAGE (LOG_LEVEL_ERROR, priv->log_level,
            "failed to allocate memory...");
        return VVAS_RET_ALLOC_ERROR;
      }
      rect_params->points.x = parent_pred->bbox.x;
      rect_params->points.y = parent_pred->bbox.y;
      rect_params->width = parent_pred->bbox.width;
      rect_params->height = parent_pred->bbox.height;
      rect_params->thickness = priv->line_thickness;
      rect_params->rect_color.red = clr.red;
      rect_params->rect_color.green = clr.green;
      rect_params->rect_color.blue = clr.blue;
      rect_params->apply_bg_color = 0;

      if (do_mask || ((priv->mask_tree_level)
              && (level == priv->mask_tree_level))) {
        /* Apply masking when class string matches or level matches */
        rect_params->apply_bg_color = 1;
        rect_params->bg_color.red = 0;
        rect_params->bg_color.green = 0;
        rect_params->bg_color.blue = 0;
      }

      LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->log_level,
          "appending rectangle [%u] : x = %u, y = %u, width = %u, height = %u"
          "Color : B = %u, G = %u, R = %u", shape_info->num_rects,
          parent_pred->bbox.x, parent_pred->bbox.y, parent_pred->bbox.width,
          parent_pred->bbox.height, rect_params->rect_color.blue,
          rect_params->rect_color.green, rect_params->rect_color.red);
      shape_info->rect_params =
          vvas_list_append (shape_info->rect_params, rect_params);
      shape_info->num_rects++;
    }

    uint32_t y_offset;
    VvasOverlayTextParams *text_params =
        (VvasOverlayTextParams *) calloc (1, sizeof (VvasOverlayTextParams));
    if (!text_params) {
      free (label_string);
      LOG_MESSAGE (LOG_LEVEL_ERROR, priv->log_level,
          "failed to allocate memory...");
      return VVAS_RET_ALLOC_ERROR;
    }

    y_offset = priv->y_offset;

    text_params->bottom_left_origin = 1;
    if (!priv->draw_above_bbox_flag)
      text_params->bottom_left_origin = 0;

    text_params->points.x = parent_pred->bbox.x;
    text_params->points.y = parent_pred->bbox.y + y_offset;
    /* If y is zero bottom_left_origin will be set zero for drawing
       text inside image */
    if (!text_params->points.y)
      text_params->bottom_left_origin = 0;

    text_params->text_font.font_size = priv->font_size;
    text_params->text_font.font_num = priv->font_type;
    /* Setting black color for text */
    text_params->text_font.font_color.blue = 0;
    text_params->text_font.font_color.green = 0;
    text_params->text_font.font_color.red = 0;

    /* copy only what overlay can hold */
    text_params->disp_text = strdup (label_string);

    text_params->apply_bg_color = NEED_TEXT_BG_COLOR;
    text_params->bg_color.blue = clr.blue;
    text_params->bg_color.green = clr.green;
    text_params->bg_color.red = clr.red;
    LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->log_level, "appending text [%u] : "
        "x = %u, y = %u, font size = %f, font number = %u, "
        "need background color = %u Color : B = %u, G = %u, R = %u",
        shape_info->num_text, parent_pred->bbox.x,
        parent_pred->bbox.y + y_offset, priv->font_size, priv->font_type,
        NEED_TEXT_BG_COLOR, clr.blue, clr.green, clr.red);

    LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->log_level,
        "appending string %s", text_params->disp_text);
    shape_info->text_params =
        vvas_list_append (shape_info->text_params, text_params);

    shape_info->num_text++;
    free (label_string);
  }

  return VVAS_RET_SUCCESS;
}

/**
 *  @fn void vvas_metaconvert_destroy (VvasMetaConvert *meta_convert)
 *  @param [in] meta_convert - Handle to VVAS Meta convert
 *  @return None
 *  @brief Destorys VvasMetaConvert handle \p meta_convert
 */
void
vvas_metaconvert_destroy (VvasMetaConvert * meta_convert)
{
  VvasMetaConvertPriv *priv = (VvasMetaConvertPriv *) meta_convert;
  int idx;

  if (priv->allowed_labels_count) {
    for (idx = 0; idx < priv->allowed_labels_count; idx++)
      free (priv->allowed_labels[idx]);
    free (priv->allowed_labels);
  }

  if (priv->allowed_classes_count) {
    for (idx = 0; idx < priv->allowed_classes_count; idx++)
      free (priv->allowed_classes[idx]);
    free (priv->allowed_classes);
  }

  free (priv);
}
