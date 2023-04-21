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

#include <vvas_core/vvas_context.h>
#include <vvas_core/vvas_log.h>
#include <vvas_core/vvas_video.h>
#include <vvas_core/vvas_video_priv.h>
#include <vvas_core/vvas_infer_prediction.h>
#include "vvas_core/vvas_tracker.hpp"
#include "tracker_algo/tracker.hpp"

#include <glib-object.h>
#include <cmath>
#include <string>

#define LOG_LEVEL         (LOG_LEVEL_INFO)

#define LOG_E(...)        (LOG_MESSAGE(LOG_LEVEL_ERROR, LOG_LEVEL,  __VA_ARGS__))
#define LOG_W(...)        (LOG_MESSAGE(LOG_LEVEL_WARNING, LOG_LEVEL,  __VA_ARGS__))
#define LOG_I(...)        (LOG_MESSAGE(LOG_LEVEL_INFO, LOG_LEVEL,  __VA_ARGS__))
#define LOG_D(...)        (LOG_MESSAGE(LOG_LEVEL_DEBUG, LOG_LEVEL,  __VA_ARGS__))

#define MEM_BANK_IDX 0

/**
 * @struct VvasTrackerInfo
 * @brief Structure to store information related tracker
 */
typedef struct
{
  /** private tracker handle to store info related to object */
  void *tracker_priv;
  /** pointer to local copy of image data */
  VvasVideoFrame *img_data;
  /** Stores previously detected object tree structure */
  VvasInferPrediction *pr;
  /** @ref VvasContext structure to have global context */
  VvasContext *vvas_gctx;
} VvasTrackerInfo;

/**
 *  @fn VvasReturnType create_tracker_algo_config(VvasTrackerconfig *vvas_tconfig,
 *                                         track_config *tconfig)
 *
 *  @param [in] vvas_tconfig @ref VvasTrackerconfig structure.
 *  @param [in] tconfig @ref track_config structure.
 *  @return returns on success.
 *
 * @details Converts tracker config from user space to tracker algo config file.
 */

VvasReturnType
create_tracker_algo_config (VvasTrackerconfig * vvas_tconfig,
    track_config * tconfig)
{
  VvasReturnType vret = VVAS_RET_SUCCESS;

  tconfig->fixed_window = 1;
  tconfig->hog_feature = 1;
  if (vvas_tconfig->tracker_type == TRACKER_ALGO_IOU)
    tconfig->tracker_type = ALGO_IOU;
  else if (vvas_tconfig->tracker_type == TRACKER_ALGO_MOSSE)
    tconfig->tracker_type = ALGO_MOSSE;
  else if (vvas_tconfig->tracker_type == TRACKER_ALGO_KCF)
    tconfig->tracker_type = ALGO_KCF;
  if (vvas_tconfig->search_scales == SEARCH_SCALE_ALL)
    tconfig->multiscale = 1;
  else if (vvas_tconfig->search_scales == SEARCH_SCALE_UP)
    tconfig->multiscale = 2;
  else if (vvas_tconfig->search_scales == SEARCH_SCALE_DOWN)
    tconfig->multiscale = 3;
  if (vvas_tconfig->obj_match_color == TRACKER_USE_RGB)
    tconfig->obj_match_color = USE_RGB;
  else if (vvas_tconfig->obj_match_color == TRACKER_USE_HSV)
    tconfig->obj_match_color = USE_HSV;
  tconfig->iou_use_color = vvas_tconfig->iou_use_color;
  tconfig->fet_length = vvas_tconfig->fet_length;
  tconfig->min_width = vvas_tconfig->min_width;
  tconfig->min_height = vvas_tconfig->min_height;
  tconfig->max_width = vvas_tconfig->max_width;
  tconfig->max_height = vvas_tconfig->max_height;
  tconfig->num_inactive_frames = vvas_tconfig->num_inactive_frames;
  tconfig->num_frames_confidence = vvas_tconfig->num_frames_confidence;
  tconfig->obj_match_search_region = vvas_tconfig->obj_match_search_region;
  tconfig->padding = vvas_tconfig->padding;
  tconfig->dist_correlation_threshold =
      vvas_tconfig->dist_correlation_threshold;
  tconfig->dist_overlap_threshold = vvas_tconfig->dist_overlap_threshold;
  tconfig->dist_scale_change_threshold =
      vvas_tconfig->dist_scale_change_threshold;
  tconfig->dist_correlation_weight = vvas_tconfig->dist_correlation_weight;
  tconfig->dist_overlap_weight = vvas_tconfig->dist_overlap_weight;
  tconfig->dist_scale_change_weight = vvas_tconfig->dist_scale_change_weight;
  tconfig->occlusion_threshold = vvas_tconfig->occlusion_threshold;
  tconfig->confidence_score = vvas_tconfig->confidence_score;
  tconfig->skip_inactive_objs = vvas_tconfig->skip_inactive_objs;
  return vret;
}

/**
 *  @fn VvasTracker *vvas_tracker_create (VvasContext *vvas_ctx, VvasTrackerconfig *config)
 *
 *  @param [in] vvas_ctx @ref VvasContext structure.
 *  @param [in] config @ref VvasTrackerConfig structure.
 *  @return On Success returns @ref VvasTracker handle. \n
 *           On Failure returns NULL.
 *
 * @details Upon successful initializes tracker with config parameters and allocates required memory.
 */
VvasTracker *
vvas_tracker_create (VvasContext * vvas_ctx, VvasTrackerconfig * vvas_tconfig)
{
  VvasTracker *trk_hndl = NULL;
  VvasTrackerInfo *trackers_data = NULL;
  VvasReturnType vret;
  tracker_handle *tracker_priv;

  trackers_data = (VvasTrackerInfo *) calloc (1, sizeof (VvasTrackerInfo));
  if (trackers_data == NULL)
    return trk_hndl;

  trackers_data->img_data = NULL;
  trackers_data->pr = NULL;
  trackers_data->vvas_gctx = vvas_ctx;

  trackers_data->tracker_priv =
      (tracker_handle *) calloc (1, sizeof (tracker_handle));

  if (trackers_data->tracker_priv == NULL) {
    free (trackers_data);
    return trk_hndl;
  }

  tracker_priv = (tracker_handle *) trackers_data->tracker_priv;
  vret = create_tracker_algo_config (vvas_tconfig, &tracker_priv->tconfig);
  if (VVAS_IS_ERROR (vret)) {
    free (trackers_data->tracker_priv);
    free (trackers_data);
    return trk_hndl;
  }

  /* Call tracker algo function to initalize tracker instances */
  init_tracker (tracker_priv);

  trk_hndl = (VvasTracker *) trackers_data;

  return trk_hndl;
}

/**
 *  @fn static bool input_each_node_to_tracker (const VvasTreeNode *node, void *new_objs_ptr) 
 *
 *  @param [in] node Pointer to infer tree stored as VvasTreeNode.
 *  @param[in] new_objs_ptr Pointer to the @ref objs_data.
 *
 *  @return FALSE to continue tree traversal\n
 *          TRUE to stop tree traversal.
 *
 *  @brief  Access node to get object info and stores as @ref objs_data. Called
 *          when detection info available.
 */

static bool
input_each_node_to_tracker (const VvasTreeNode * node, void *new_objs_ptr)
{
  objs_data *ptr = (objs_data *) new_objs_ptr;
  VvasInferPrediction *prediction = (VvasInferPrediction *) node->data;

  /* check for number of objects more than MAX_OBJ_TRACK.
     If more ignores the object data. */
  if (ptr->num_objs >= MAX_OBJ_TRACK)
    return FALSE;
  else if (!node->parent)       /* Ignores parent node data */
    return FALSE;
  else {
    /* accessing and storing object info */
    int i = ptr->num_objs;
    ptr->objs[i].x = prediction->bbox.x;
    ptr->objs[i].y = prediction->bbox.y;
    ptr->objs[i].width = prediction->bbox.width;
    ptr->objs[i].height = prediction->bbox.height;
    ptr->objs[i].map_id = prediction->prediction_id;
    ptr->num_objs = i + 1;
  }

  return FALSE;
}

/**
 *  @fn static bool update_each_node_with_results (VvasTreeNode *node, gpointer kpriv_ptr)
 *  @param [in] node Pointer to infer tree stored as VvasTreeNode.
 *  @param [out] kpriv_ptr Pointer to the @ref tracker_handle.
 *
 *  @return FALSE to continue tree traversal\n
 *          TRUE to stop tree traversal.
 *
 *  @brief  Updates node of inference tree with tracked data.
 */
static bool
update_each_node_with_results (const VvasTreeNode * node, void *kpriv_ptr)
{
  tracker_handle *trackers_data = (tracker_handle *) kpriv_ptr;
  VvasInferPrediction *prediction = (VvasInferPrediction *) node->data;
  bool flag = true;

  /* Ignores parent node data */
  if (!node->parent)
    return FALSE;

  /* Loop runs for all tracker objects and updates node with
     object data having same prediction_id and object tracking
     status is active (status = 1) */
  for (int i = 0; i < trackers_data->trk_objs.num_objs && flag; i++) {
    if (prediction->prediction_id == trackers_data->trk_objs.objs[i].map_id) {
      prediction->bbox.x = round (trackers_data->trk_objs.objs[i].x);
      prediction->bbox.y = round (trackers_data->trk_objs.objs[i].y);
      prediction->bbox.width = round (trackers_data->trk_objs.objs[i].width);
      prediction->bbox.height = round (trackers_data->trk_objs.objs[i].height);

      if (trackers_data->trk_objs.objs[i].status == 1) {
        std::string str =
            std::to_string (trackers_data->trk_objs.objs[i].trk_id);
        prediction->obj_track_label = g_strdup (str.c_str ());
        flag = false;
      } else if (trackers_data->trk_objs.objs[i].status == 0 &&
          !trackers_data->tconfig.skip_inactive_objs) {
        std::string str = std::to_string (-1);
        prediction->obj_track_label = g_strdup (str.c_str ());
        flag = false;
      }
    }
  }

  /* If no object found set the tracker_id as -1 */
  if (flag == true) {
    std::string str = std::to_string (-1);
    prediction->obj_track_label = g_strdup (str.c_str ());
    if (trackers_data->tconfig.skip_inactive_objs) {
      prediction->enabled = false;
    }
  }

  return FALSE;
}

/**
 * @fn VvasReturnType vvas_tracker_process (VvasTracker *vvas_tracker_hndl,
 *                                   VvasVideoFrame *pFrame,
 *                                   VvasInferPrediction *infer_meta)
 *
 *  @param[inout] vvas_tracker_hndl @ref VvasTracker with newly detected objects in VvasTracker:new_objs. \n
 *                  Upon tracking updates @ref VvasTracker:trk_objs with tracked objects info.
 *  @param [in] img @ref VvasMatImg structure of input frame.
 *  @param [in] infer_meta @ref VvasInferPrediction contains detection tree if detection info available
 *              else NULL
 *  @return Returns @ref VvasReturnType.
 *
 * @details Tracks the objects and updates the VvasTracker's trk_objs with objects news position and their status.
 */
VvasReturnType
vvas_tracker_process (VvasTracker * vvas_tracker_hndl,
    VvasVideoFrame * pFrame, VvasInferPrediction ** infer_meta)
{
  Mat_img img;
  VvasVideoFrameMapInfo map_info;
  VvasVideoFrameMapInfo mem_info;
  VvasVideoInfo vinfo;
  VvasReturnType vret;
  VvasTrackerInfo *tracker_data;
  tracker_data = (VvasTrackerInfo *) vvas_tracker_hndl;
  tracker_handle *tracker_priv = (tracker_handle *) tracker_data->tracker_priv;
  int buf_copy_flag = 1;

  memset (&map_info, 0x0, sizeof (VvasVideoFrameMapInfo));


  /* map memory to user space to get video frame information */
  vret = vvas_video_frame_map (pFrame, VVAS_DATA_MAP_READ, &map_info);
  if (VVAS_IS_ERROR (vret)) {
    LOG_E ("failed to map memory\n");
    return vret;
  }

  /* Supports only NV12 format. Else retruns false */
  if (map_info.fmt != VVAS_VIDEO_FORMAT_Y_UV8_420) {
    LOG_E ("Tracker supports only NV12 format");
    return VVAS_RET_INVALID_ARGS;
  }

  img.width = map_info.planes[0].stride;
  img.height = map_info.height;
  img.channels = 1;
  img.img_width = map_info.width;
  img.img_height = map_info.height;
  img.data[0] = NULL;
  img.data[1] = NULL;

  /* If algorithm of type ALGO_IOU and metadata is NULL return as
     error since ALGO_IOU requires every frame detection data */
  if (*infer_meta == NULL && tracker_priv->tconfig.tracker_type == ALGO_IOU) {
    LOG_E ("Metada should not be null if algorithm type is ALGO_IOU");
    return VVAS_RET_INVALID_ARGS;
  }

  /* If algo type is IOU and no color based matching is set then no need
     of accessing image data */
  if (tracker_priv->tconfig.tracker_type == ALGO_IOU
      && !tracker_priv->tconfig.iou_use_color) {
    buf_copy_flag = 0;
  } else {
    /* Check if in_mem of type vvas_memory. If vvas_memory data
       need to be copied locally as data is from uncached buffer */
    if (vvas_video_frame_get_bo (pFrame) != NULL) {
      /* Memory will be allocated to img_data if not allocated */
      if (tracker_data->img_data == NULL) {
        vvas_video_frame_get_videoinfo (pFrame, &vinfo);
        tracker_data->img_data =
            vvas_video_frame_alloc (tracker_data->vvas_gctx,
            VVAS_ALLOC_TYPE_NON_CMA,
            VVAS_ALLOC_FLAG_NONE, MEM_BANK_IDX, &vinfo, &vret);
        if (!tracker_data->img_data || VVAS_IS_ERROR (vret)) {
          LOG_E ("failed to allocate CMA memory of size %d",
              (int) (map_info.planes[0].size + map_info.planes[1].size));
          return VVAS_RET_ALLOC_ERROR;
        }
      }

      /* map memory to user space to get video frame information */
      vret =
          vvas_video_frame_map (tracker_data->img_data, VVAS_DATA_MAP_WRITE,
          &mem_info);
      if (VVAS_IS_ERROR (vret)) {
        LOG_E ("failed to map memory\n");
        return vret;
      }

      /* Copy Intensity plane of NV12 frame data */
      memcpy (mem_info.planes[0].data, map_info.planes[0].data,
          mem_info.planes[0].size);
      img.data[0] = (unsigned char *) mem_info.planes[0].data;

      /* If metadata available copy croma values also as they required
         for object matching */
      if (*infer_meta != NULL) {
        memcpy (mem_info.planes[1].data, map_info.planes[1].data,
            mem_info.planes[1].size);
        img.channels = 2;
        img.data[1] = (unsigned char *) mem_info.planes[1].data;
      }

      vret = vvas_video_frame_unmap (tracker_data->img_data, &mem_info);
      if (VVAS_IS_ERROR (vret)) {
        LOG_E ("failed to unmap memory of img_data\n");
        return vret;
      }

      vret =
          vvas_video_frame_map (tracker_data->img_data, VVAS_DATA_MAP_READ,
          &mem_info);
      if (VVAS_IS_ERROR (vret)) {
        LOG_E ("failed to map memory\n");
        return vret;
      }
    } else {
      img.data[0] = (unsigned char *) map_info.planes[0].data;
      img.data[1] = (unsigned char *) map_info.planes[1].data;
    }
  }
  /* Check to call tracker in detection mode or tracking mode based on
     metadata availability */
  if (*infer_meta != NULL) {
    tracker_priv->new_objs.num_objs = 0;
    objs_data *ptr = &tracker_priv->new_objs;

    /* free previous prediction tree pointer */
    if (tracker_data->pr != NULL) {
      vvas_inferprediction_free (tracker_data->pr);
      tracker_data->pr = NULL;
    }

    tracker_data->pr = vvas_inferprediction_copy (*infer_meta);
    /* Copy new prediction tree required updation during tracking */
    vvas_treenode_traverse ((*infer_meta)->node, PRE_ORDER,
        TRAVERSE_LEAFS, -1, input_each_node_to_tracker, ptr);

    /*  Call tracker in detection mode with flag true */
    run_tracker (img, tracker_priv, true);
  } else {
    /* In tracking mode remove if any infer metadata is attached */
    if (*infer_meta != NULL) {
      vvas_inferprediction_free (*infer_meta);
      *infer_meta = NULL;
    }

    /* Copy last detection infermetata */
    if (tracker_data->pr != NULL)
      *infer_meta = vvas_inferprediction_copy (tracker_data->pr);

    /*  Call tracker in tracking mode with flag false */
    run_tracker (img, tracker_priv, false);
  }

  if (*infer_meta != NULL) {
    vvas_treenode_traverse ((*infer_meta)->node, PRE_ORDER,
        TRAVERSE_LEAFS, -1, update_each_node_with_results, tracker_priv);
  }

  if (buf_copy_flag == 1) {
    /* Unmap the input frame if used */
    vret = vvas_video_frame_unmap (pFrame, &map_info);
    if (VVAS_IS_ERROR (vret)) {
      LOG_E ("failed to unmap memory of pFrame\n");
      return vret;
    }

    /* If input frame is of vvas frame then unmap the image data buffer */
    if (vvas_video_frame_get_bo (pFrame) != NULL) {
      vret = vvas_video_frame_unmap (tracker_data->img_data, &mem_info);
      if (VVAS_IS_ERROR (vret)) {
        LOG_E ("failed to unmap memory of img_data\n");
        return vret;
      }
    }
  }

  return VVAS_RET_SUCCESS;
}

/**
 *  @fn vvas_tracker_destroy (VvasTracker *vvas_tracker_hndl)
 *
 *  @param[in] vvas_tracker_hndl Address of @ref VvasTracker
 *
 *  @return Returns true on success.
 *
 * @details Free memory allocated during creating the tracker and resets parameters to default values.
 */
bool
vvas_tracker_destroy (VvasTracker * vvas_tracker_hndl)
{
  bool vret = TRUE;
  VvasTrackerInfo *trackers_data;

  trackers_data = (VvasTrackerInfo *) vvas_tracker_hndl;

  if (trackers_data->img_data)
    vvas_video_frame_free (trackers_data->img_data);

  trackers_data->img_data = NULL;

  vvas_inferprediction_free (trackers_data->pr);
  trackers_data->pr = NULL;

  /* Free the memory allocated in tracker algo */
  deinit_tracker ((tracker_handle *) trackers_data->tracker_priv);
  free (trackers_data->tracker_priv);

  free (trackers_data);
  return vret;
}
