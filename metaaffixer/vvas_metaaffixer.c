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

#include <vvas_core/vvas_metaaffixer.h>
#include "vvas_utils/vvas_utils.h"
#include <vvas_core/vvas_log.h>
#include <math.h>
#define LOG_LEVEL     (pHandle->loglevel)
#define DEFAULT_LOG_LEVEL LOG_LEVEL_WARNING

#define LOG_E(...)    (LOG_MESSAGE(LOG_LEVEL_ERROR, LOG_LEVEL,  __VA_ARGS__))
#define LOG_W(...)    (LOG_MESSAGE(LOG_LEVEL_WARNING, LOG_LEVEL,  __VA_ARGS__))
#define LOG_I(...)    (LOG_MESSAGE(LOG_LEVEL_INFO, LOG_LEVEL,  __VA_ARGS__))
#define LOG_D(...)    (LOG_MESSAGE(LOG_LEVEL_DEBUG, LOG_LEVEL,  __VA_ARGS__))

#define LOG(...)    (LOG_MESSAGE(LOG_LEVEL_INFO, LOG_LEVEL_INFO,  __VA_ARGS__))
#define INFER_META_BATCH_SIZE       (8u)

/** @struct VvasMetaAffixerInfo
 *  @brief Meta Affixer internal structure.           
 */
typedef struct
{
  VvasHashTable *map;           /* for storing infer frame data */
  uint64_t *cur_pts;
  uint64_t *near_pts;
  uint64_t inferframe_dur;
  uint32_t max_infer_size;
  VvasLogLevel loglevel;
} VvasMetaAffixerInfo;

/** @struct vvasinferscalefactor
 *  @brief  contains inference information of a frame
 */
typedef struct
{
  int32_t sw;
  int32_t sh;
  int32_t dw;
  int32_t dh;
  double hfactor;
  double vfactor;
} VvasInferScaleFactor;

/** @struct VvasMetaAffixerMapData
 *  @brief  contains information related to infer & frame info. 
 */
typedef struct
{
  int32_t width;
  int32_t height;
  uint64_t pts;
  uint64_t dur;
  VvasInferPrediction *meta;
  uint64_t seq_id;
} VvasMetaAffixerMapData;

static void
get_sequence_id (VvasMetaAffixerMapData * map)
{
  static uint64_t _id = 0ul;
  static VvasMutex _id_mutex;

  vvas_mutex_lock (&_id_mutex);
  map->seq_id = _id++;
  vvas_mutex_unlock (&_id_mutex);

  return;
}

/**
 *  @fn  bool vvas_metaaffixer_mapdata_remove_foreach (void * key, void * value, 
 *                                                             void * user_data)
 *  @param [in] key -  Key associated with the entry. 
 *  @param [in] value - Value associated with the key value
 *  @param [in] user_data - User data to be passed, can be NULL. 
 *  
 *  @return TRUE - entry will be removed and move to next entry.
 *          FALSE - will go to next element in table until end
 *          
 *  @brief This function is used to copy node data while performing deep-copy 
 *         of a tree node.
 */
static bool
vvas_metaaffixer_mapdata_remove_foreach (void *key, void *value,
    void *user_data)
{
  VvasMetaAffixerMapData *mp = (VvasMetaAffixerMapData *) value;
  if (NULL != mp) {
    vvas_inferprediction_free (mp->meta);
    free (mp);
  }
  return TRUE;
}

/**
 *  @fn  void* vvas_metaaffixer_node_scale(const void  *node, void *data)
 *  
 *  @param [in] node - Address of the node.
 *  @param [in] data - Additional data.
 *  
 *  @return On Success returns address of the new node.
 *          On Failure returns NULL 
 *          
 *  @brief This function will scale each node cored on scaling factor.
 */
static void *
vvas_metaaffixer_node_scale (const void *node, void *data)
{
  VvasInferPrediction *dmeta = NULL;
  VvasInferPrediction *smeta = (VvasInferPrediction *) node;
  VvasInferScaleFactor *scl_factor = (VvasInferScaleFactor *) data;

  if ((NULL != smeta) && (NULL != scl_factor)) {
    dmeta = vvas_inferprediction_node_copy (smeta, NULL);
    dmeta->bbox.x = smeta->bbox.x * scl_factor->hfactor;
    dmeta->bbox.y = smeta->bbox.y * scl_factor->vfactor;
    dmeta->bbox.width = nearbyintf (smeta->bbox.width * scl_factor->hfactor);
    dmeta->bbox.height = nearbyintf (smeta->bbox.height * scl_factor->vfactor);
  }

  return dmeta;
}

/**
 *  @fn void  vvas_metaaffixer_get_inferframe_pts (VvasMetaAffixerInfo *pHandle, 
 *                                                VvasVideoInfo *vinfo
 *                                                VvasMetadata *metadata)
 *  @param [in] handle - handle for metaaffixer instance
 *  @param [in] vinfo  - address of input frame info
 *  @param [in] metadata - address of input frame metadata
 *  @return none 
 *  @brief  this function will find nearest pts from infer frames.  
 */
static void
vvas_metaaffixer_get_inferframe_pts (VvasMetaAffixerInfo * pHandle,
    VvasVideoInfo * vinfo, VvasMetadata * metadata)
{
  if ((NULL == pHandle) ||
      (NULL == vinfo) || (NULL == metadata) || (NULL == pHandle->map)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_LOG_LEVEL,  "Invalid arguments");
    return;
  }

  VvasHashTableIter iter;
  uint32_t size = vvas_hash_table_size (pHandle->map);
  uint64_t min_seq_id = 0;
  VvasMetaAffixerMapData *mp;

  if (0 == size) {
    LOG_D ("No data available in Infer table ");
    return;
  }

  /* initialize iterator */
  vvas_hash_table_iter_init (pHandle->map, &iter);

  uint64_t inframe_spts = metadata->pts;
  uint64_t inframe_epts = metadata->pts + metadata->duration;

  uint64_t *near_pts = NULL;
  uint32_t overlap_percent = 0;

  VvasMetaAffixerMapData *frame = NULL;
  pHandle->near_pts = NULL;

  uint64_t infer_meta_dur = pHandle->inferframe_dur;
  bool first_itr = TRUE;

  while (vvas_hash_table_iter_next (&iter, (void **) &near_pts,
          (void **) &frame)) {
    uint64_t infer_meta_spts = *near_pts;
    uint64_t infer_meta_epts = *near_pts + infer_meta_dur;
    uint32_t ovl_per = 0;

    /* if infer frame is ahead  of input frame */
    if ((infer_meta_spts >= inframe_spts) &&
        (infer_meta_spts < inframe_epts) && (0 != infer_meta_dur)) {
      /* overlap present calculate overlap w.r.t to Infer meta frame */
      ovl_per =
          (uint32_t) round (((float) (inframe_epts -
                  infer_meta_spts) / infer_meta_dur) * 100);
      LOG_D
          ("Infer<Input: frame ovlerlaps %d  Nearest PTS:%ld to inframepts:%ld ",
          ovl_per, *near_pts, inframe_spts);

    }
    /* if infer frame  is behind of inframe */
    else if ((inframe_spts >= infer_meta_spts) &&
        (inframe_spts < infer_meta_epts) && (0 != infer_meta_dur)) {
      /* overlap present calculate overlap w.r.t to Infer meta frame */
      ovl_per =
          (uint32_t) round (((float) (infer_meta_epts -
                  inframe_spts) / infer_meta_dur) * 100);
      LOG_D
          ("Infer>Input: frame ovlerlaps %d  Nearest PTS:%ld to inframepts:%ld ",
          ovl_per, *near_pts, inframe_spts);
    }

    if (ovl_per > overlap_percent) {

      /* store near PTS */
      pHandle->near_pts = near_pts;

      /* update new overlap percent */
      overlap_percent = ovl_per;
      mp = (VvasMetaAffixerMapData *) vvas_hash_table_lookup (pHandle->map,
          pHandle->near_pts);
      if (mp) {
        min_seq_id = mp->seq_id;
        first_itr = FALSE;
      }
      LOG_I ("frame ovlerlaps %d  Nearest PTS:%ld to inframepts:%ld ", ovl_per,
          *near_pts, inframe_spts);
    } else if (ovl_per == overlap_percent) {
      mp = (VvasMetaAffixerMapData *) vvas_hash_table_lookup (pHandle->map,
          near_pts);
      if (mp) {
        if (first_itr) {
          min_seq_id = mp->seq_id;
          first_itr = FALSE;
        } else if (mp->seq_id < min_seq_id) {
          pHandle->near_pts = near_pts;
          min_seq_id = mp->seq_id;
        }
      }
    }
  }

  return;
}

 /**
 *  @fn  void vvas_metaaffixer_compute_scale_factor(VvasInferScaleFactor *scl_factor) 
 *  @param [in]  dmeta Address of context handle 
 *  @param [in]  smeta Stream ID of the input frame.
 *  *  
 *  @return  On Success returns VVAS_RET_SUCCESS 
 *           On Failure returns VVAS_RET_ERROR_* 
 *  @brief This function returns scaled metadata cored on input frame info
 */
static void
vvas_metaaffixer_compute_scale_factor (VvasInferScaleFactor * scl_factor)
{
  if (NULL != scl_factor) {
    scl_factor->hfactor = ((scl_factor->dw * 1.0) / scl_factor->sw);
    scl_factor->vfactor = ((scl_factor->dh * 1.0) / scl_factor->sh);
  }
}


/**
 *  @fn bool vvas_metaaffixer_print( const VvasTreeNode  *node, void *data)
 *  @param [in]  node address of the node. 
 *  @param [in]  data to be passed to each node.  
 *  *  
 *  @return  On Success returns VVAS_RET_SUCCESS 
 *           On Failure returns VVAS_RET_ERROR_* 
 *  @brief This function is called for printing all nodes data. 
 */
static bool
vvas_metaaffixer_print (const VvasTreeNode * node, void *data)
{
  VvasInferPrediction *p = (VvasInferPrediction *) node->data;
  VvasMetaAffixerInfo *pHandle = (VvasMetaAffixerInfo *) data;
  LOG_I ("x=%d,y=%d, w=%d, h=%d", p->bbox.x, p->bbox.y, p->bbox.width,
      p->bbox.height);

  return FALSE;
}

/**
 *  @fn bool vvas_metaaffixer_node_assign(const VvasTreeNode  *node, void *data)
 *  @param [in] node - Address of the node .
 *  @param [in] data - user data to be passed
 *  @return TRUE - To stop traversing
 *          FALSE - To continue traversing
 *  @brief This function is passed as parameter to traver all nodes in a tree.
 *
 */
static bool
vvas_metaaffixer_node_assign (const VvasTreeNode * node, void *data)
{
  VvasInferPrediction *dmeta = NULL;

  if (NULL != node) {
    dmeta = (VvasInferPrediction *) node->data;
    if (dmeta->node) {
      vvas_treenode_destroy (dmeta->node);
    }
    dmeta->node = (VvasTreeNode *) node;
  }

  return false;
}

/**
 *  @fn  VvasInferPrediction* vvas_metaaffixer_get_scaled_meta(VvasInferPrediction *dmeta, VvasInferPrediction *smeta)
 *  @param [in]  dmeta Address of context handle 
 *  @param [in]  smeta Stream ID of the input frame.
 *  *  
 *  @return  On Success returns VVAS_RET_SUCCESS 
 *           On Failure returns VVAS_RET_ERROR_* 
 *  @brief This function returns scaled metadata cored on input frame info
 */
static VvasInferPrediction *
vvas_metaaffixer_get_scaled_meta (VvasInferPrediction * smeta,
    VvasInferScaleFactor * scl_factor, VvasMetaAffixerInfo * pHandle)
{
  VvasTreeNode *node = NULL;

  if ((NULL != smeta) && (NULL != scl_factor) && (NULL != pHandle)) {
    node =
        vvas_treenode_copy_deep (smeta->node, vvas_metaaffixer_node_scale,
        scl_factor);

    vvas_treenode_traverse (node, IN_ORDER,
        TRAVERSE_ALL, -1, vvas_metaaffixer_node_assign, NULL);
    if (pHandle->loglevel == LOG_LEVEL_INFO) {
      vvas_treenode_traverse (node, IN_ORDER,
          TRAVERSE_ALL, -1, vvas_metaaffixer_print, pHandle);
    }
    return node->data;
  }

  return NULL;
}

/**
 *  @fn uint64_t vvas_metaaffixer_remove_infer_meta (VvasMetaAffixerInfo *pHandle)  
 *  @param [in]  pHandle  MetaAffixer handle. 
 *  @return  void 
 *  @brief This function removes old infer & meta data from table. 
 */
static void
vvas_metaaffixer_remove_infer_meta (VvasMetaAffixerInfo * pHandle)
{
  uint32_t *pts = NULL;
  VvasMetaAffixerMapData *vmeta = NULL;
  VvasMetaAffixerMapData *mp = NULL;
  bool bbreak = FALSE;

  if (NULL == pHandle) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_LOG_LEVEL,  "Invalid arguments");
    return;
  }

  VvasHashTableIter iter;

  /* initialize iterator */
  vvas_hash_table_iter_init (pHandle->map, &iter);

  uint64_t tmp_pts = 0u;
  bool first_itr = TRUE;
  uint32_t *key = NULL;
  uint32_t *tmp_key;
  uint64_t min_seq_id;

  while ((vvas_hash_table_iter_next (&iter, (void **) &pts, (void **) &vmeta) &&
          (bbreak == FALSE))) {

    if (first_itr) {
      tmp_pts = *pts;
      key = pts;
      mp = (VvasMetaAffixerMapData *) vvas_hash_table_lookup (pHandle->map,
          key);
      if (mp) {
        min_seq_id = mp->seq_id;
      }
      first_itr = FALSE;
    } else if (*pts < tmp_pts) {
      tmp_pts = *pts;
      key = pts;
      mp = (VvasMetaAffixerMapData *) vvas_hash_table_lookup (pHandle->map,
          key);
      if (mp) {
        min_seq_id = mp->seq_id;
      }
    } else if (*pts == tmp_pts) {
      LOG_W ("Duplicate timestamp %ld found", tmp_pts);
      tmp_key = pts;
      mp = (VvasMetaAffixerMapData *) vvas_hash_table_lookup (pHandle->map,
          tmp_key);
      if (mp) {
        if (mp->seq_id < min_seq_id) {
          min_seq_id = mp->seq_id;
          key = pts;
        }
      }
    }
  }

  if (NULL != key) {
    LOG_I ("Removing PTS %ld", tmp_pts);
    mp = (VvasMetaAffixerMapData *) vvas_hash_table_lookup (pHandle->map, key);
    vvas_metaaffixer_mapdata_remove_foreach (NULL, mp, pHandle);
    if (!vvas_hash_table_remove (pHandle->map, key)) {
      LOG_E ("Failed to delete infer frame data");
    }
  }
}

 /**
 *  @fn   VvasMetaAffixer* vvas_metaaffixer_create (uint64_t inferframe_dur,
 *                                                 uint32_t infer_queue_size , 
 *                                                 VvasLogLevel loglevel)
 *  @param [in] inferframe_dur - Duration of the infer frame.                                                  
 *  @param [in] infer_queue_size  - Represents Max infer frame queue size.
 *  @param [in] loglevel - indicates log level
 *  @return On Sucess returns handle of Metaaffixer handle
 *          On Failure returns NULL
 *  @brief  this function will allocate internal resources and  
 *          return the handle.
 */
VvasMetaAffixer *
vvas_metaaffixer_create (uint64_t inferframe_dur,
    uint32_t infer_queue_size, VvasLogLevel loglevel)
{
  VvasMetaAffixerInfo *pHandle = NULL;

  if ((0 == inferframe_dur) ||
      (0xFFFFFFFFFFFFFFFF == inferframe_dur) || (0 == infer_queue_size)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_LOG_LEVEL,  "Invalid input param received");
    return NULL;
  }

  int size = sizeof (VvasMetaAffixerInfo);
  pHandle = (VvasMetaAffixerInfo *) calloc (1, size);

  if (NULL != pHandle) {
    /* update loglevel */
    pHandle->loglevel = loglevel;

    pHandle->max_infer_size = infer_queue_size;

    pHandle->inferframe_dur = inferframe_dur;

    pHandle->map = vvas_hash_table_new (vvas_direct_hash, vvas_direct_equal);

    if (NULL == pHandle->map) {
      LOG_E ("fatal error: hashmap new returns NULL");

      /* free allocated mem */
      free (pHandle);

      return NULL;
    }
  }

  return (VvasMetaAffixer *) pHandle;
}

/**
 *  @fn void vvas_metaaffixer_destroy(VvasMetaAffixer* handle)
 *  @param [in] handle - MetaAffixer handle to be destroyed 
 *  @return void
 *  @brief  this function will destroy all memory allocated for handle instanced passed 
 */
void
vvas_metaaffixer_destroy (VvasMetaAffixer * handle)
{
  VvasMetaAffixerInfo *pHandle = (VvasMetaAffixerInfo *) handle;

  if (NULL != pHandle) {

    vvas_hash_table_foreach_remove (pHandle->map,
        vvas_metaaffixer_mapdata_remove_foreach, NULL);
    vvas_hash_table_destroy (pHandle->map);

    free (pHandle);
  } else {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_LOG_LEVEL,  "Invalid arguments");
  }
}

/**
 *  @fn  VvasReturnType vvas_metaaffixer_submit_infer_meta(VvasMetaAffixer *handle,
 *                                                         VvasVideoInfo *vinfo,
 *                                                         VvasMetadata *metadata,      
 *                                                         VvasInferPrediction *infer)
 *  @param [in] handle - context handle
 *  @param [in] metadata - metadata of frame
 *  @param [in] vinfo  - address of frame info
 *  @param [in] infer - infer metadata associated with infer frame
 *  
 *  @return On Sucess returns VVAS_RET_SUCCESS\n
 *          On Failure returns VVAS_RET_ERROR
 *  @brief  this function will submit meta data information into Queue 
 */
VvasReturnType
vvas_metaaffixer_submit_infer_meta (VvasMetaAffixer * handle,
    VvasVideoInfo * vinfo, VvasMetadata * metadata, VvasInferPrediction * infer)
{
  VvasReturnType ret = VVAS_RET_SUCCESS;
  VvasMetaAffixerInfo *pHandle = (VvasMetaAffixerInfo *) handle;

  if ((NULL == pHandle) ||
      (NULL == metadata) || (NULL == vinfo) || (NULL == infer)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_LOG_LEVEL,  "Invalid arguments");
    return VVAS_RET_ERROR;
  }

  uint32_t size = vvas_hash_table_size (pHandle->map);

  if (size >= pHandle->max_infer_size) {
    vvas_metaaffixer_remove_infer_meta (pHandle);
  }

  size = sizeof (VvasMetaAffixerMapData);
  VvasMetaAffixerMapData *map = (VvasMetaAffixerMapData *) calloc (1, size);

  if (NULL != map) {
    map->pts = metadata->pts;
    map->dur = metadata->duration;
    map->height = vinfo->height;
    map->width = vinfo->width;
    map->meta = vvas_inferprediction_copy (infer);
    get_sequence_id (map);
    vvas_hash_table_insert (pHandle->map, &map->pts, map);

    pHandle->cur_pts = (uint64_t *) & map->pts;
    ret = VVAS_RET_SUCCESS;
  }

  return ret;
}


/**
 *  @fn  VvasReturnType vvas_metaaffixer_get_frame_meta(VvasMetaAffixer handle,
 *                                                   uint32_t stream_id,
 *                                                   bool sync_pts,
 *                                                   VvasVideoInfo *vinfo,                                         
 *                                                   VvasMetadata *metadata,
 *                                                   VvasMetaAffixerRespCode  *respcode,
 *                                                   VvasInferPrediction *ScaledMetadata)
 *  @param [in] handle -  Address of context handle 
 *  @param [in] sync_pts - if FALSE then last received infer meta data
 *                               is used for scaling else reference infer metadata is 
 *                               chosen cored on PTS of input frame.
 *  @param [in] vinfo - Input Frame Information
 *  @param [in] metadata - metadata of input frame
 *  @param [out] respcode - metaaffixer response code.
 *  @param [out] ScaledMetaData - Scaled meta data is udpated here.
 *  
 *  @return  On Success returns VVAS_RET_SUCCESS\n 
 *           On Failure returns VVAS_RET_ERROR_* 
 *  @brief This function returns scaled metadata cored on input frame info
 */
VvasReturnType
vvas_metaaffixer_get_frame_meta (VvasMetaAffixer * handle,
    bool sync_pts,
    VvasVideoInfo * vinfo,
    VvasMetadata * metadata,
    VvasMetaAffixerRespCode * respcode, VvasInferPrediction ** ScaledMetaData)
{
  VvasReturnType ret = VVAS_RET_ERROR;
  VvasMetaAffixerInfo *pHandle = (VvasMetaAffixerInfo *) handle;

  if ((NULL == pHandle) ||
      (NULL == metadata) || (NULL == ScaledMetaData) || (NULL == vinfo)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_LOG_LEVEL,  "Invalid arguments");
    return ret;
  }

  *respcode = VVAS_METAAFFIXER_PASS;

  if (!sync_pts) {

    /* sync PTS with last infer frame meta data */
    pHandle->near_pts = pHandle->cur_pts;

  } else {
    vvas_metaaffixer_get_inferframe_pts (pHandle, vinfo, metadata);

    if (NULL == pHandle->near_pts) {

      LOG_D ("No Frame Overlap");

      /* No overlap found */
      *respcode = VVAS_METAAFFIXER_NO_FRAME_OVERLAP;

      return VVAS_RET_SUCCESS;
    }
  }

  if (NULL != pHandle->near_pts) {

    LOG_D ("PTS: %ld", *pHandle->near_pts);
    VvasMetaAffixerMapData *mp = (VvasMetaAffixerMapData *)
        vvas_hash_table_lookup (pHandle->map, pHandle->near_pts);
    if (NULL == mp) {
      LOG_E ("Null received from table lookup");
      return ret;
    }

    VvasInferScaleFactor scl_factor;
    memset (&scl_factor, 0x0, sizeof (scl_factor));

    /* Source frame height & width */
    scl_factor.sh = mp->height;
    scl_factor.sw = mp->width;

    /* destination frame height and width */
    scl_factor.dh = vinfo->height;
    scl_factor.dw = vinfo->width;

    /* Compute scale factor */
    vvas_metaaffixer_compute_scale_factor (&scl_factor);
    *ScaledMetaData =
        vvas_metaaffixer_get_scaled_meta (mp->meta, &scl_factor, pHandle);

    /* 
       LOG_I("x=%d,y=%d, w=%d, h=%d", *ScaledMetaData->bbox.x, *ScaledMetaData->bbox.y,
       *ScaledMetaData->bbox.width,  *ScaledMetaData->bbox.height);
     */

    ret = VVAS_RET_SUCCESS;
  } else {
    LOG_E ("Near PTS is NULL ");
    *respcode = VVAS_METAAFFIXER_NULL_VALUE;
  }

  return ret;
}
