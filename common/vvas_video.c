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

#include <vvas_core/vvas_video.h>
#include <vvas_core/vvas_video_priv.h>

static void
vvas_video_info_align (VvasVideoInfo * vinfo, VvasVideoFramePriv * priv)
{
  bool aligned;
  int32_t padded_width;
  int32_t idx;
  VvasVideoAlignment *alignment = &vinfo->alignment;

  padded_width =
      vinfo->width + alignment->padding_right + alignment->padding_left;

  do {
    vvas_fill_planes (vinfo, priv);

    /* check alignment */
    aligned = true;
    for (idx = 0; idx < priv->num_planes; idx++) {
      LOG_MESSAGE (LOG_LEVEL_DEBUG, DEFAULT_VVAS_LOG_LEVEL,
          "plane %d, stride %lu, alignment %u",
          idx, priv->planes[idx].stride, alignment->stride_align[idx]);
      aligned &= (priv->planes[idx].stride & alignment->stride_align[idx]) == 0;
    }
    if (aligned)
      break;

    LOG_MESSAGE (LOG_LEVEL_DEBUG, DEFAULT_VVAS_LOG_LEVEL,
        "unaligned strides, increasing dimension");
    /* increase padded_width */
    padded_width += padded_width & ~(padded_width - 1);
    alignment->padding_right =
        padded_width - vinfo->width - alignment->padding_left;
  } while (!aligned);

  alignment->padding_right =
      padded_width - vinfo->width - alignment->padding_left;
}

int8_t
vvas_fill_planes (VvasVideoInfo * info, VvasVideoFrame * vvas_frame)
{
  VvasVideoFramePriv *priv = (VvasVideoFramePriv *) vvas_frame;
  int32_t padded_width;
  int32_t padded_height;

  padded_width = info->width + info->alignment.padding_left +
      info->alignment.padding_right;
  padded_height = info->height + info->alignment.padding_top +
      info->alignment.padding_bottom;

  switch (info->fmt) {
    case VVAS_VIDEO_FORMAT_Y_UV8_420:  /* NV12 */
      priv->num_planes = 2;
      priv->planes[0].stride = VVAS_ROUND_UP_4 (padded_width);
      priv->planes[0].elevation = VVAS_ROUND_UP_2 (padded_height);
      priv->planes[0].offset = 0;
      priv->planes[0].size = priv->planes[0].stride * priv->planes[0].elevation;

      priv->planes[1].stride = priv->planes[0].stride;
      // TODO: need to support interlaced mode
      priv->planes[1].elevation = priv->planes[0].elevation / 2;
      priv->planes[1].offset = priv->planes[0].size;
      priv->planes[1].size = priv->planes[1].stride * priv->planes[1].elevation;
      priv->size = priv->planes[0].size + priv->planes[1].size;
      break;
    case VVAS_VIDEO_FORMAT_I420:
      priv->num_planes = 3;
      priv->planes[0].stride = VVAS_ROUND_UP_4 (padded_width);
      priv->planes[0].elevation = VVAS_ROUND_UP_2 (padded_height);
      priv->planes[0].offset = 0;
      priv->planes[0].size = priv->planes[0].stride * priv->planes[0].elevation;

      priv->planes[1].stride = VVAS_ROUND_UP_4 (VVAS_ROUND_UP_2 (padded_width) / 2);
      // TODO: need to support interlaced mode
      priv->planes[1].elevation = priv->planes[0].elevation / 2;
      priv->planes[1].offset = priv->planes[0].size;
      priv->planes[1].size = priv->planes[1].stride * priv->planes[1].elevation;

      priv->planes[2].stride = VVAS_ROUND_UP_4 (VVAS_ROUND_UP_2 (padded_width) / 2);
      // TODO: need to support interlaced mode
      priv->planes[2].elevation = priv->planes[0].elevation / 2;
      priv->planes[2].offset = priv->planes[0].size + priv->planes[1].size;
      priv->planes[2].size = priv->planes[2].stride * priv->planes[2].elevation;

      priv->size = priv->planes[0].size + priv->planes[1].size + priv->planes[2].size;
      break;
    case VVAS_VIDEO_FORMAT_RGBx:
    case VVAS_VIDEO_FORMAT_r210:
    case VVAS_VIDEO_FORMAT_Y410:
    case VVAS_VIDEO_FORMAT_BGRx:
    case VVAS_VIDEO_FORMAT_BGRA:
    case VVAS_VIDEO_FORMAT_RGBA:
      priv->num_planes = 1;
      priv->planes[0].stride = padded_width * 4;
      priv->planes[0].elevation = padded_height;
      priv->planes[0].offset = 0;
      priv->size = priv->planes[0].size =
          priv->planes[0].stride * priv->planes[0].elevation;
      break;
    case VVAS_VIDEO_FORMAT_YUY2:
      priv->num_planes = 1;
      priv->planes[0].stride = VVAS_ROUND_UP_4 (padded_width * 2);
      priv->planes[0].elevation = padded_height;
      priv->planes[0].offset = 0;
      priv->size = priv->planes[0].size =
          priv->planes[0].stride * priv->planes[0].elevation;
      break;
    case VVAS_VIDEO_FORMAT_NV16:
      priv->num_planes = 2;
      priv->planes[0].stride = VVAS_ROUND_UP_4 (padded_width);
      priv->planes[0].elevation = padded_height;
      priv->planes[0].offset = 0;
      priv->planes[0].size = priv->planes[0].stride * priv->planes[0].elevation;

      priv->planes[1].stride = priv->planes[0].stride;
      priv->planes[1].elevation = padded_height;
      priv->planes[1].offset = priv->planes[0].size;
      priv->planes[1].size = priv->planes[1].stride * priv->planes[1].elevation;
      priv->size = priv->planes[0].size + priv->planes[1].size;
      break;
    case VVAS_VIDEO_FORMAT_RGB:
    case VVAS_VIDEO_FORMAT_v308:
    case VVAS_VIDEO_FORMAT_BGR:
      priv->num_planes = 1;
      priv->planes[0].stride = VVAS_ROUND_UP_4 (padded_width * 3);
      priv->planes[0].elevation = padded_height;
      priv->planes[0].offset = 0;
      priv->size = priv->planes[0].size =
          priv->planes[0].stride * priv->planes[0].elevation;
      break;
    case VVAS_VIDEO_FORMAT_I422_10LE:
      priv->num_planes = 3;

      priv->planes[0].stride = VVAS_ROUND_UP_4 (padded_width * 2);
      priv->planes[0].elevation = VVAS_ROUND_UP_2 (padded_height);
      priv->planes[0].offset = 0;
      priv->planes[0].size = priv->planes[0].stride * priv->planes[0].elevation;

      priv->planes[1].stride = VVAS_ROUND_UP_4 (padded_width);
      priv->planes[1].elevation = VVAS_ROUND_UP_2 (padded_height);
      priv->planes[1].offset = priv->planes[0].size;
      priv->planes[1].size = priv->planes[1].stride * priv->planes[1].elevation;

      priv->planes[2].stride = priv->planes[1].stride;
      priv->planes[2].elevation = priv->planes[1].elevation;
      priv->planes[2].offset = priv->planes[0].size + priv->planes[1].size;
      priv->planes[2].size = priv->planes[2].stride * priv->planes[2].elevation;

      priv->size =
          priv->planes[0].size + priv->planes[1].size + priv->planes[2].size;
      break;
    case VVAS_VIDEO_FORMAT_NV12_10LE32:
      priv->num_planes = 2;

      priv->planes[0].stride = (padded_width + 2) / 3 * 4;
      priv->planes[0].elevation = VVAS_ROUND_UP_2 (padded_height);
      priv->planes[0].offset = 0;
      priv->planes[0].size = priv->planes[0].stride * priv->planes[0].elevation;
      // TODO: need to support interlaced mode
      priv->planes[1].stride = priv->planes[0].stride;
      priv->planes[1].elevation = priv->planes[0].elevation / 2;
      priv->planes[1].offset = priv->planes[0].size;
      priv->planes[1].size = priv->planes[1].stride * priv->planes[1].elevation;

      priv->size = priv->planes[0].size + priv->planes[1].size;
      break;
    case VVAS_VIDEO_FORMAT_GRAY8:
      priv->num_planes = 1;

      priv->planes[0].stride = VVAS_ROUND_UP_4 (padded_width);
      priv->planes[0].elevation = padded_height;
      priv->planes[0].offset = 0;
      priv->planes[0].size = priv->planes[0].stride * priv->planes[0].elevation;
      priv->size = priv->planes[0].size;
      break;

    case VVAS_VIDEO_FORMAT_GRAY10_LE32:
      priv->num_planes = 1;

      priv->planes[0].stride = (padded_width + 2) / 3 * 4;
      priv->planes[0].elevation = VVAS_ROUND_UP_2 (padded_height);
      priv->planes[0].offset = 0;
      priv->planes[0].size = priv->planes[0].stride * priv->planes[0].elevation;
      priv->size = priv->planes[0].size;
      break;

    default:
      LOG_MESSAGE (LOG_LEVEL_ERROR, priv->ctx->log_level,
          "%d format not supported", info->fmt);
      return -1;
  }

  if (priv->num_planes != info->n_planes) {
    LOG_MESSAGE (LOG_LEVEL_WARNING, priv->ctx->log_level,
        "Supplied input nplanes value %d wrong, should be %d! \
      using num planes %d instread of %d ", info->n_planes, priv->num_planes, priv->num_planes, info->n_planes);
  }

  return 0;
}

/**
 * @fn VvasVideoFrame* vvas_video_frame_alloc (VvasContext *vvas_ctx,
 *                                                                         VvasAllocationType alloc_type,
 *                                                                         VvasAllocationFlags alloc_flags,
 *                                                                         uint8_t mbank_idx,
 *                                                                         VvasVideoInfo *vinfo,
 *                                                                         VvasReturnType *ret)
 * @param [in] vvas_ctx - Address of VvasContext handle created using @ref vvas_context_create
 * @param [in] alloc_type - Type of the memory need to be allocated
 * @param [in] alloc_flags - Allocation flags used to allocate video frame
 * @param [in] mbank_idx - Index of the memory bank on which memory need to be allocated
 * @param [in] vinfo - Address of VvasVideoInfo which contains video frame specific information
 * @param[out] ret - Address to store return value. Upon case of error, \p ret is useful in understanding the root cause
 * @return  On Success returns VvasVideoFrame handle\n
 *                On Failure returns NULL
 * @brief Allocates memory based on VvasVideoInfo structure
 */
VvasVideoFrame *
vvas_video_frame_alloc (VvasContext * vvas_ctx, VvasAllocationType alloc_type,
    VvasAllocationFlags alloc_flags, uint8_t mbank_idx, VvasVideoInfo * vinfo,
    VvasReturnType * ret)
{
  VvasVideoFramePriv *priv = NULL;
  VvasReturnType vret = VVAS_RET_SUCCESS;
  uint8_t pidx;

  /* check arguments validity */
  if (!vvas_ctx || !ALLOC_TYPE_IS_VALID (alloc_type)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid arguments");
    vret = VVAS_RET_INVALID_ARGS;
    goto error;
  }

  priv = (VvasVideoFramePriv *) calloc (1, sizeof (VvasVideoFramePriv));
  if (priv == NULL) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL,
        "failed to allocate memory for VvasVideoFrame");
    vret = VVAS_RET_ALLOC_ERROR;
    goto error;
  }

  priv->width = vinfo->width;
  priv->height = vinfo->height;
  priv->fmt = vinfo->fmt;
  priv->ctx = vvas_ctx;
  priv->mbank_idx = mbank_idx;

  /* Before allocating the memory lets do the alignment of stride and pixel */
  vvas_video_info_align (vinfo, priv);

  priv->alignment.padding_left = vinfo->alignment.padding_left;
  priv->alignment.padding_right = vinfo->alignment.padding_right;
  priv->alignment.padding_top = vinfo->alignment.padding_top;
  priv->alignment.padding_bottom = vinfo->alignment.padding_bottom;
  for (pidx = 0; pidx < priv->num_planes; pidx++) {
    priv->alignment.stride_align[pidx] = vinfo->alignment.stride_align[pidx];
  }

  if (alloc_type == VVAS_ALLOC_TYPE_CMA) {      /* allocate XRT memory */
    if (!vvas_ctx->dev_handle) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, vvas_ctx->log_level,
          "Invalid device handle to allocate CMA memory");
      goto error;
    }

    /*allocate xrt BO and store in private handle */
    priv->boh =
        vvas_xrt_alloc_bo (vvas_ctx->dev_handle, priv->size, alloc_flags,
        mbank_idx);
    if (priv->boh == NULL) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, priv->ctx->log_level,
          "failed to allocate memory with params : bank idx = %d", mbank_idx);
      vret = VVAS_RET_ALLOC_ERROR;
      goto error;
    }

    for (pidx = 0; pidx < priv->num_planes; pidx++) {
      priv->planes[pidx].boh =
          vvas_xrt_create_sub_bo (priv->boh, priv->planes[pidx].size,
          priv->planes[pidx].offset);
      if (priv->planes[pidx].boh == NULL) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, priv->ctx->log_level,
            "failed to allocate sub BO with size %zu and offset %zu",
            priv->planes[pidx].size, priv->planes[pidx].offset);
        vret = VVAS_RET_ALLOC_ERROR;
        goto error;
      }
    }
  } else if (alloc_type == VVAS_ALLOC_TYPE_NON_CMA) {   /* allocate SW memory */
    for (pidx = 0; pidx < priv->num_planes; pidx++) {
      priv->planes[pidx].data = (uint8_t *) malloc (priv->planes[pidx].size);
      if (priv->planes[pidx].data == NULL) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, priv->ctx->log_level,
            "failed to allocate non-cma memory of size %zu",
            priv->planes[pidx].size);
        vret = VVAS_RET_ALLOC_ERROR;
        goto error;
      }
    }
  }

  priv->mem_info.alloc_type = alloc_type;
  priv->mem_info.alloc_flags = alloc_flags;
  priv->mem_info.mbank_idx = mbank_idx;
  priv->mem_info.sync_flags = VVAS_DATA_SYNC_NONE;
  priv->mem_info.map_flags = VVAS_DATA_MAP_NONE;
  priv->own_alloc = 1;

  return (VvasVideoFrame *) priv;

error:
  if (priv)
    free (priv);
  if (ret)
    *ret = vret;
  return NULL;
}

/**
 * @fn VvasVideoFrame* vvas_video_frame_alloc_from_data (VvasContext *vvas_ctx,
 *                                      VvasVideoInfo *vinfo,
 *                                      void *data[VVAS_VIDEO_MAX_PLANES],
 *                                      VvasVideoFrameDataFreeCB free_cb,
 *                                      void *user_data,
 *                                      VvasReturnType *ret)
 *
 * @param [in] vvas_ctx - Address of VvasContext handle created using @ref vvas_context_create
 * @param [in] vinfo - Video information related a frame
 * @param [in] data Array of data pointers to each plane
 * @param [in] free_cb - Pointer to callback function to be called when VvasVideoFrame is freed
 * @param [in] user_data - User data to be passed to callback function \p free_cb
 * @param[out] ret - Address to store return value. Upon case of error, \p ret is useful in understanding the root cause
 *
 * @return  On Success returns VvasVideoFrame handle\n
 *                On Failure returns NULL
 * @brief Allocates memory based on data pointers provided by user
 */
VvasVideoFrame *
vvas_video_frame_alloc_from_data (VvasContext * vvas_ctx,
    VvasVideoInfo * vinfo, void *data[VVAS_VIDEO_MAX_PLANES],
    VvasVideoFrameDataFreeCB free_cb, void *user_data, VvasReturnType * ret)
{
  VvasVideoFramePriv *priv = NULL;
  VvasReturnType vret = VVAS_RET_SUCCESS;
  int8_t iret = 0;
  uint8_t pidx;

  /* check arguments validity */
  if (!vvas_ctx || !vinfo || !data) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid arguments");
    vret = VVAS_RET_INVALID_ARGS;
    goto error;
  }

  priv = (VvasVideoFramePriv *) calloc (1, sizeof (VvasVideoFramePriv));
  if (priv == NULL) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL,
        "failed to allocate memory for VvasVideoFrame");
    vret = VVAS_RET_ALLOC_ERROR;
    goto error;
  }

  priv->ctx = vvas_ctx;
  priv->log_level = vvas_ctx->log_level;
  priv->width = vinfo->width;
  priv->height = vinfo->height;
  priv->fmt = vinfo->fmt;
  priv->num_planes = vinfo->n_planes;
  priv->alignment.padding_left = vinfo->alignment.padding_left;
  priv->alignment.padding_right = vinfo->alignment.padding_right;
  priv->alignment.padding_top = vinfo->alignment.padding_top;
  priv->alignment.padding_bottom = vinfo->alignment.padding_bottom;
  priv->boh = NULL;
  priv->mbank_idx = -1;
  priv->free_cb = free_cb;
  priv->user_data = user_data;
  priv->own_alloc = 0;
  for (pidx = 0; pidx < priv->num_planes; pidx++) {
    priv->alignment.stride_align[pidx] = vinfo->alignment.stride_align[pidx];
  }

  iret = vvas_fill_planes (vinfo, priv);
  if (iret < 0) {
    vret = VVAS_RET_INVALID_ARGS;
    goto error;
  }

  for (pidx = 0; pidx < priv->num_planes; pidx++) {
    priv->planes[pidx].boh = NULL;
    priv->planes[pidx].data = data[pidx];
  }

  priv->mem_info.alloc_type = VVAS_ALLOC_TYPE_NON_CMA;
  priv->mem_info.alloc_flags = VVAS_ALLOC_FLAG_UNKNOWN;
  priv->mem_info.mbank_idx = -1;
  priv->mem_info.sync_flags = VVAS_DATA_SYNC_NONE;
  priv->mem_info.map_flags = VVAS_DATA_MAP_NONE;

  return (VvasVideoFrame *) priv;

error:
  if (priv)
    free (priv);
  if (ret)
    *ret = vret;
  return NULL;
}


/**
 * @fn VvasReturnType vvas_video_frame_map (VvasVideoFrame* vvas_vframe,
 *                                                                       VvasDataMapFlags map_flags,
 *                                                                       VvasVideoFrameMapInfo *info)
 * @param[in] vvas_vframe - Address of @ref VvasVideoFrame
 * @param[in] map_flags - Flags used to map \p vvas_vframe
 * @param[out] info - Structure which gets populated after mapping is successful
 * @return @ref VvasReturnType
 * @brief Maps \p vvas_vframe to user space using \p map_flags.
 *             Based on VvasMemory::sync_flags, data will synchronized between host and device.
 */
VvasReturnType
vvas_video_frame_map (VvasVideoFrame * vvas_vframe, VvasDataMapFlags map_flags,
    VvasVideoFrameMapInfo * info)
{
  VvasVideoFramePriv *priv = (VvasVideoFramePriv *) vvas_vframe;
  uint8_t pidx;

  if (!priv || !info) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid arguments");
    return VVAS_RET_INVALID_ARGS;
  }

  if (priv->mem_info.alloc_type == VVAS_ALLOC_TYPE_CMA) {       /* map XRT memory */

    if (map_flags & VVAS_DATA_MAP_READ) {
      /* Sync Data from device before mapping in READ mode */
      vvas_video_frame_sync_data (vvas_vframe, priv->mem_info.sync_flags);
    }

    if (map_flags & VVAS_DATA_MAP_WRITE) {
      /* Now user is requesting memory in write mode, we need to sync data to device after user writes */
      vvas_video_frame_set_sync_flag (&priv->mem_info,
          VVAS_DATA_SYNC_TO_DEVICE);
    }

    for (pidx = 0; pidx < priv->num_planes; pidx++) {
      /* map each plane BO to user space */
      priv->planes[pidx].data =
          vvas_xrt_map_bo (priv->planes[pidx].boh,
          map_flags & VVAS_DATA_MAP_WRITE);
      if (!priv->planes[pidx].data) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, priv->ctx->log_level,
            "failed to map memory");
        return VVAS_RET_ERROR;
      }
    }
  }

  info->nplanes = priv->num_planes;
  info->size = priv->size;
  info->width = priv->width;
  info->height = priv->height;
  info->fmt = priv->fmt;
  info->alignment.padding_left = priv->alignment.padding_left;
  info->alignment.padding_right = priv->alignment.padding_right;
  info->alignment.padding_top = priv->alignment.padding_top;
  info->alignment.padding_bottom = priv->alignment.padding_bottom;

  for (pidx = 0; pidx < priv->num_planes; pidx++) {
    info->planes[pidx].data = priv->planes[pidx].data;
    info->planes[pidx].offset = priv->planes[pidx].offset;
    info->planes[pidx].size = priv->planes[pidx].size;
    info->planes[pidx].stride = priv->planes[pidx].stride;
    info->planes[pidx].elevation = priv->planes[pidx].elevation;
    info->alignment.stride_align[pidx] = priv->alignment.stride_align[pidx];
    LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->ctx->log_level,
        "mapped video frame plane[%u] : data = %p, size = %zu", pidx,
        info->planes[pidx].data, info->planes[pidx].size);
  }

  return VVAS_RET_SUCCESS;
}

/**
 * @fn VvasReturnType vvas_video_frame_unmap (VvasVideoFrame* vvas_vframe,
 *                                                                           VvasVideoFrameMapInfo *info)
 * @param[in] vvas_vframe - Address of @ref VvasVideoFrame
 * @param[in] info - Pointer to information which was populated during vvas_video_frame_map () API
 * @return @ref VvasReturnType
 * @brief Unmaps \p vvas_vframe which was mapped earlier
 */
VvasReturnType
vvas_video_frame_unmap (VvasVideoFrame * vvas_vframe,
    VvasVideoFrameMapInfo * info)
{
  VvasVideoFramePriv *priv = (VvasVideoFramePriv *) vvas_vframe;

  if (!priv || !info) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid arguments");
    return VVAS_RET_INVALID_ARGS;
  }

  if (priv->mem_info.alloc_type == VVAS_ALLOC_TYPE_CMA) {       /* map XRT memory */
    uint8_t pidx;

    for (pidx = 0; pidx < priv->num_planes; pidx++) {
      vvas_xrt_unmap_bo (priv->planes[pidx].boh, info->planes[pidx].data);
    }
  }
  return VVAS_RET_SUCCESS;
}

/**
 * @fn void vvas_video_frame_free (VvasVideoFrame* vvas_vframe)
 * @param[in] vvas_vframe - Address of @ref VvasVideoFrame
 * @return  None
 * @brief frees the video frame allocated during @ref vvas_video_frame_alloc API
 */
void
vvas_video_frame_free (VvasVideoFrame * vvas_vframe)
{
  VvasVideoFramePriv *priv = (VvasVideoFramePriv *) vvas_vframe;
  uint8_t pidx;

  if (!priv) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid arguments");
    return;
  }

  if (priv->mem_info.alloc_type == VVAS_ALLOC_TYPE_CMA) {
    for (pidx = 0; pidx < priv->num_planes; pidx++) {
      vvas_xrt_free_bo (priv->planes[pidx].boh);
    }
    vvas_xrt_free_bo (priv->boh);
  } else {
    if (priv->own_alloc) {
      for (pidx = 0; pidx < priv->num_planes; pidx++)
        free (priv->planes[pidx].data);
    } else {
      void *data[VVAS_VIDEO_MAX_PLANES];

      memset (data, 0x0, VVAS_VIDEO_MAX_PLANES * sizeof (uint8_t *));

      for (pidx = 0; pidx < priv->num_planes; pidx++)
        data[pidx] = priv->planes[pidx].data;

      if (NULL != priv->free_cb) {
        priv->free_cb (data, priv->user_data);
      }
    }
  }

  LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->ctx->log_level, "freeing memory %p",
      priv);
  free (priv);
}

/**
 * @fn void vvas_video_frame_set_metadata (VvasVideoFrame* vvas_mem,
 *                                         VvasMetadata *meta_data)
 * @param[in] vvas_mem - Address of @ref VvasVideoFrame
 * @param[in] meta_data - Address of @ref VvasMetadata to be set on \p vvas_mem
 * @return  None
 * @brief Sets metadata on VvasVideoFrame
 */
void
vvas_video_frame_set_metadata (VvasVideoFrame * vvas_mem,
    VvasMetadata * meta_data)
{
  VvasVideoFramePriv *priv = (VvasVideoFramePriv *) vvas_mem;

  if (!priv || !meta_data) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid arguments");
    return;
  }

  priv->meta_data.pts = meta_data->pts;
  priv->meta_data.dts = meta_data->dts;
  priv->meta_data.duration = meta_data->duration;

  return;
}

/**
 * @fn void vvas_video_frame_get_metadata (VvasVideoFrame* vvas_mem,
                                           VvasMetadata *meta_data)
 * @param[in] vvas_mem - Address of @ref VvasVideoFrame
 * @param[in] meta_data - Address of @ref VvasMetadata to store metadata coming from \p vvas_mem
 * @return  None
 * @brief Gets metadata on VvasVideoFrame
 */
void
vvas_video_frame_get_metadata (VvasVideoFrame * vvas_mem,
    VvasMetadata * meta_data)
{
  VvasVideoFramePriv *priv = (VvasVideoFramePriv *) vvas_mem;

  if (!priv || !meta_data) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid arguments");
    return;
  }

  meta_data->pts = priv->meta_data.pts;
  meta_data->dts = priv->meta_data.dts;
  meta_data->duration = priv->meta_data.duration;

  return;
}

/**
 * @fn void vvas_video_frame_get_videoinfo(VvasVideoFrame* vvas_mem,
 *                                         VvasVideoInfo *vinfo)
 * @param[in] vvas_mem - Address of @ref VvasVideoFrame
 * @param[out] meta_data - Pointer to store information from \p vvas_mem
 * @return None
 * @brief Gets video frame information from VvasVideoFrame
 */
void
vvas_video_frame_get_videoinfo (VvasVideoFrame * vvas_mem,
    VvasVideoInfo * vinfo)
{
  uint32_t idx;
  VvasVideoFramePriv *priv = (VvasVideoFramePriv *) vvas_mem;

  if (!priv || !vinfo) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid arguments");
    return;
  }

  vinfo->width = priv->width;
  vinfo->height = priv->height;
  vinfo->fmt = priv->fmt;
  vinfo->alignment.padding_left = priv->alignment.padding_left;
  vinfo->alignment.padding_right = priv->alignment.padding_right;
  vinfo->alignment.padding_top = priv->alignment.padding_top;
  vinfo->alignment.padding_bottom = priv->alignment.padding_bottom;
  vinfo->n_planes = priv->num_planes;
  for (idx = 0; idx < vinfo->n_planes; idx++) {
    vinfo->stride[idx] = priv->planes[idx].stride;
    vinfo->elevation[idx] = priv->planes[idx].elevation;
    vinfo->alignment.stride_align[idx] = priv->alignment.stride_align[idx];
  }
  return;
}

/****************************************************************************
 ***********  Private API for VVAS Base library implementation  *********
 ****************************************************************************/
/**
 * @fn void vvas_video_frame_sync_data (VvasVideoFrame* vvas_mem, VvasDataSyncFlags sync_flag)
 * @param[in] vvas_mem Address of @ref VvasVideoFrame
 * @return None
 * @brief Data will be synchronized between device and host based on VvaseMemory::sync_flags.
 */
void
vvas_video_frame_sync_data (VvasVideoFrame * vvas_mem,
    VvasDataSyncFlags sync_flag)
{
  VvasVideoFramePriv *priv = (VvasVideoFramePriv *) vvas_mem;
  int32_t iret;

  if (!priv || (VVAS_ALLOC_TYPE_NON_CMA == priv->mem_info.alloc_type)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid arguments");
    return;
  }

  if ((sync_flag & VVAS_DATA_SYNC_NONE) ||
      ((sync_flag & VVAS_DATA_SYNC_FROM_DEVICE)
          && (sync_flag & VVAS_DATA_SYNC_TO_DEVICE))) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, priv->ctx->log_level, "Invalid sync flag");
    return;
  }

  if (priv->mem_info.sync_flags & sync_flag) {
    vvas_bo_sync_direction sync_dir;
    sync_dir =
        (sync_flag & VVAS_DATA_SYNC_TO_DEVICE) ? VVAS_BO_SYNC_BO_TO_DEVICE :
        VVAS_BO_SYNC_BO_FROM_DEVICE;
    iret = vvas_xrt_sync_bo (priv->boh, sync_dir, priv->size, 0);
    if (iret != 0) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, priv->ctx->log_level, "syncbo failed -%d, reason : %s", iret, strerror (errno));
      return;
    }
    vvas_video_frame_unset_sync_flag (&priv->mem_info, sync_flag);
  }

  LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->ctx->log_level,
      "memory %p sync %s device is completed", vvas_mem,
      sync_flag == VVAS_DATA_SYNC_TO_DEVICE ? "to" : "from");
}

/**
 * @fn void vvas_video_frame_set_sync_flag (VvasVideoFrame* vvas_mem, VvasDataSyncFlags flag)
 * @param[in] vvas_mem - Address of @ref VvasVideoFrame
 * @param[in] flag - Flag to be set on \p vvas_mem
 * @return  None
 * @brief Enables VvasMemorySyncFlags on memory
 */
void
vvas_video_frame_set_sync_flag (VvasVideoFrame * vvas_mem,
    VvasDataSyncFlags flag)
{
  VvasVideoFramePriv *priv = (VvasVideoFramePriv *) vvas_mem;

  if (!priv) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid arguments");
    return;
  }

  priv->mem_info.sync_flags |= flag;
}

/**
 * @fn void vvas_video_frame_unset_sync_flag (VvasVideoFrame* vvas_mem, VvasDataSyncFlags flag)
 * @param[in] vvas_mem - Address of @ref VvasVideoFrame
 * @param[in] flag - Flag to be cleared on \p vvas_mem
 * @return  None
 * @brief Disables VvasMemorySyncFlags on memory
 */
void
vvas_video_frame_unset_sync_flag (VvasVideoFrame * vvas_mem,
    VvasDataSyncFlags flag)
{
  VvasVideoFramePriv *priv = (VvasVideoFramePriv *) vvas_mem;

  if (!priv) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid arguments");
    return;
  }

  priv->mem_info.sync_flags &= ~flag;
}

/**
 * @fn uint64_t vvas_video_frame_get_frame_paddr (VvasVideoFrame* vvas_mem)
 * @param[in] vvas_mem - Address of @ref VvasMemory
 * @return Valid physical address or on error, returns (uint64_t)-1
 * @brief API to get physical address corresponding to VvasVideoFrame
 */
uint64_t
vvas_video_frame_get_frame_paddr (VvasVideoFrame * vvas_mem)
{
  VvasVideoFramePriv *priv = (VvasVideoFramePriv *) vvas_mem;

  if (!priv) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid arguments");
    return (uint64_t) - 1;
  }

  if (priv->mem_info.alloc_type != VVAS_ALLOC_TYPE_CMA) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, priv->ctx->log_level,
        "Not a CMA memory to get physical address");
    return (uint64_t) - 1;
  }

  return vvas_xrt_get_bo_phy_addres (priv->boh);
}

/**
 * @fn uint64_t vvas_video_frame_get_plane_paddr (VvasVideoFrame* vvas_mem, uint8_t plane_idx)
 * @param[in] vvas_mem - Address of @ref VvasMemory
 * @param[in] plane_idx - Plane index in a video frame
 * @return  Valid physical address or on error, returns (uint64_t)-1
 * @brief API to get physical address corresponding to VvasVideoFrame
 */
uint64_t
vvas_video_frame_get_plane_paddr (VvasVideoFrame * vvas_mem, uint8_t plane_idx)
{
  VvasVideoFramePriv *priv = (VvasVideoFramePriv *) vvas_mem;

  if (!priv || plane_idx >= priv->num_planes) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid arguments");
    return (uint64_t) - 1;
  }

  if (priv->mem_info.alloc_type != VVAS_ALLOC_TYPE_CMA) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, priv->ctx->log_level,
        "Not a CMA memory to get physical address");
    return (uint64_t) - 1;
  }

  return vvas_xrt_get_bo_phy_addres (priv->planes[plane_idx].boh);
}

/**
 * @fn void* vvas_video_frame_get_bo (VvasVideoFrame *vvas_mem)
 * @param[in] vvas_mem - Address of @ref VvasVideoFrame
 * @return  Valid BO address upon success \n On failure, NULL will be returned
 * @brief API to get XRT BO corresponding to VvasVideoFrame
 */
void *
vvas_video_frame_get_bo (VvasVideoFrame * vvas_mem)
{
  VvasVideoFramePriv *priv = (VvasVideoFramePriv *) vvas_mem;

  if (!priv) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL,
        "invalid memory address");
    return NULL;
  }

  return (void *) priv->boh;
}

/**
 * @fn void* vvas_video_frame_get_plane_bo (VvasVideoFrame *vvas_mem, uint8_t plane_idx)
 * @param[in] vvas_mem - Address of @ref VvasVideoFrame
 * @param[in] plane_idx Video plane index
 * @return  Valid BO address upon success \n On failure, NULL will be returned
 * @brief API to get XRT BO corresponding to VvasVideoFrame
 */
void *
vvas_video_frame_get_plane_bo (VvasVideoFrame * vvas_mem, uint8_t plane_idx)
{
  VvasVideoFramePriv *priv = (VvasVideoFramePriv *) vvas_mem;

  if (!priv || plane_idx >= priv->num_planes) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL,
        "invalid memory address");
    return NULL;
  }

  return (void *) priv->planes[plane_idx].boh;
}

/**
 * @fn int32_t vvas_video_frame_get_device_index (VvasVideoFrame *vvas_mem)
 * @param[in] vvas_mem Address of @ref VvasVideoFrame
 * @return  Valid device index or -1 on failure
 * @brief API to get device index on which VvasVideoFrame was allocated
 */
int32_t
vvas_video_frame_get_device_index (VvasVideoFrame * vvas_mem)
{
  VvasVideoFramePriv *priv = (VvasVideoFramePriv *) vvas_mem;
  int32_t dev_idx;

  if (!priv) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid argument");
    return -1;
  }

  if (!priv->ctx && priv->mem_info.alloc_type == VVAS_ALLOC_TYPE_CMA) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid context");
    return -1;
  }

  if (priv->ctx) {
    dev_idx = priv->ctx->dev_idx;
  } else {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL,
        "Non CMA memory will not have device index");
    dev_idx = -1;
  }
  return dev_idx;
}

/**
 * @fn int32_t vvas_video_frame_get_bank_index (VvasVideoFrame *vvas_mem)
 * @param[in] vvas_mem Address of @ref VvasVideoFrame
 * @return  Valid memory index or -1 on failure
 * @brief API to get memory bank index on which VvasVideoFrame was allocated
 */
int32_t
vvas_video_frame_get_bank_index (VvasVideoFrame * vvas_mem)
{
  VvasVideoFramePriv *priv = (VvasVideoFramePriv *) vvas_mem;
  int32_t mbank_idx;

  if (!priv) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid argument");
    return -1;
  }

  if (!priv->ctx && priv->mem_info.alloc_type == VVAS_ALLOC_TYPE_CMA) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid context");
    return -1;
  }

  if (priv->ctx) {
    mbank_idx = priv->mbank_idx;
  } else {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL,
        "Non CMA memory will not have memory bank index");
    mbank_idx = -1;
  }
  return mbank_idx;
}

/**
 * @fn size_t vvas_video_frame_get_size (VvasVideoFrame* vvas_mem)
 * @param[in] vvas_mem - Address of @ref VvasVideoFrame
 * @brief API to get the video frame size
 * @return  Size of the vvas_mem
 */
size_t
vvas_video_frame_get_size (VvasVideoFrame * vvas_mem)
{
  VvasVideoFramePriv *priv = (VvasVideoFramePriv *) vvas_mem;

  if (!priv) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid arguments");
    return (uint64_t) - 1;
  }

  return priv->size;
}

/**
 * @fn VvasAllocationType vvas_video_frame_get_allocation_type (const VvasVideoFrame* vvas_frame)
 * @param[in] vvas_frame Address of @ref VvasVideoFrame
 * @brief   API to get the allocation type
 * @return  VvasAllocationType
 */
VvasAllocationType
vvas_video_frame_get_allocation_type (const VvasVideoFrame * vvas_frame)
{
  const VvasVideoFramePriv *priv = (VvasVideoFramePriv *) vvas_frame;

  if (!priv) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid argument");
    return VVAS_ALLOC_TYPE_UNKNOWN;
  }

  return priv->mem_info.alloc_type;
}

/**
 * @fn VvasAllocationFlags vvas_video_frame_get_allocation_flag (const VvasVideoFrame* vvas_frame)
 * @param[in] vvas_frame Address of @ref VvasVideoFrame
 * @brief   API to get the allocation flag
 * @return  VvasAllocationFlags
 */
VvasAllocationFlags
vvas_video_frame_get_allocation_flag (const VvasVideoFrame * vvas_frame)
{
  const VvasVideoFramePriv *priv = (VvasVideoFramePriv *) vvas_frame;

  if (!priv) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid argument");
    return -1;
  }

  return priv->mem_info.alloc_flags;
}
