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

#include <vvas_core/vvas_memory.h>
#include <vvas_core/vvas_memory_priv.h>

/**
 * @fn VvasMemory* vvas_memory_alloc (VvasContext *vvas_ctx,
 *                                    VvasAllocationType mem_type,
 *                                    VvasAllocationFlags mem_flags,
 *                                    uint8_t mbank_idx,
 *                                    size_t size,
 *                                    VvasReturnType *ret)
 * @param[in] vvas_ctx - Address of VvasContext handle created using @ref vvas_context_create
 * @param[in] mem_type - Type of the memory need to be allocated
 * @param[in] mbank_idx - Index of the memory bank on which memory need to be allocated
 * @param[in] size - Size of the memory to be allocated on a device.
 * @param[out] ret - Address to store return value. In case of error, \p ret is useful in understanding the root cause
 * @brief Allocates memory cored on \p size
 * @return On Success returns VvasMemory handle\n
 *               On Failure returns NULL
 */
VvasMemory* 
vvas_memory_alloc (VvasContext *vvas_ctx, VvasAllocationType alloc_type, VvasAllocationFlags alloc_flags, uint8_t mbank_idx, size_t size, VvasReturnType *ret)
{
  VvasMemoryPrivate* priv = NULL;
  VvasReturnType vret = VVAS_RET_SUCCESS;

  /* check arguments validity */
  if (!vvas_ctx || !ALLOC_TYPE_IS_VALID(alloc_type) || !size) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid arguments");
    vret = VVAS_RET_INVALID_ARGS;
    goto error;
  }

  priv = (VvasMemoryPrivate*) calloc (1, sizeof (VvasMemoryPrivate));
  if (priv == NULL) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, vvas_ctx->log_level, "failed to allocate memory for VvasMemory");
    vret = VVAS_RET_ALLOC_ERROR;
    goto error;
  }

  if (alloc_type == VVAS_ALLOC_TYPE_CMA) { /* allocate XRT memory */
    if (!vvas_ctx->dev_handle) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, vvas_ctx->log_level, "Invalid device handle to allocate CMA memory");
      goto error;
    }

    /*allocate xrt BO and store in private handle */
    priv->boh = vvas_xrt_alloc_bo (vvas_ctx->dev_handle, size, alloc_flags, mbank_idx);
    if (priv->boh  == NULL) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, vvas_ctx->log_level, "failed to allocate memory with params : size = %zu, bank idx = %d", size, mbank_idx);
      vret = VVAS_RET_ALLOC_ERROR;
      goto error;
    }
  } else if (VVAS_ALLOC_TYPE_NON_CMA) { /* allocate SW memory */
    priv->data = (uint8_t *) malloc (size);
    if (priv == NULL) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, vvas_ctx->log_level, "failed to allocate non-cma memory");
      goto error;
    }
  }

  priv->size = size;
  priv->mbank_idx = mbank_idx;
  priv->free_cb = NULL;
  priv->own_alloc = 1;
  priv->ctx = vvas_ctx;
  priv->mem_info.alloc_type = alloc_type;
  priv->mem_info.alloc_flags = alloc_flags;
  priv->mem_info.mbank_idx = mbank_idx;
  priv->mem_info.sync_flags = VVAS_DATA_SYNC_NONE;
  priv->mem_info.map_flags = VVAS_DATA_MAP_NONE;

  if (ret)
    *ret = VVAS_RET_SUCCESS;

  return (VvasMemory *)priv;

error:
  if (priv) {
    if (priv->boh)
      vvas_xrt_free_bo (priv->boh);
    if (priv->data)
      free (priv->data);
    free(priv);
  }
  if (ret)
    *ret = vret;
  return NULL;
}

/**
 * @fn VvasMemory* vvas_memory_alloc_from_data (VvasContext *vvas_ctx,
 *                                              uint8_t *data,
 *                                              size_t size,
 *                                              VvasMemoryDataFreeCB free_cb,
 *                                              void *user_data,
 *                                              VvasReturnType *ret)
 * @param[in] vvas_ctx Address of VvasContext handle created using @ref vvas_context_create
 * @param[in] data Pointer to data which needs to encapsulated in @ref VvasMemory
 * @param[in] size Size of the memory to which \p data pointer is pointing
 * @param[in] free_cb Callback function to be called during @ref vvas_memory_free API.
 * @param[out] ret Address to store return value. Upon case of error, \p ret is useful in understanding the root cause
 * @return  On Success returns VvasMemory handle\n
 *                On Failure returns NULL
 * @brief Allocates VvasMemory handle from \p data pointer and \p size
 * @details When application needs to send its data pointer to VVAS core APIs, this API is useful to wrap user provided data pointer into Vvasmemory and get VvasMemory handle.
 */
VvasMemory*
vvas_memory_alloc_from_data (VvasContext *vvas_ctx, uint8_t *data, uint64_t size, VvasMemoryDataFreeCB free_cb, void *user_data, VvasReturnType *vret)
{
  VvasMemoryPrivate* priv;

  if (!data) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, vvas_ctx->log_level, "invalid arguments");
    if (vret)
      *vret = VVAS_RET_INVALID_ARGS;
    return NULL;
  }

  priv = (VvasMemoryPrivate*) calloc (1, sizeof (VvasMemoryPrivate));
  if (priv == NULL) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, vvas_ctx->log_level, "failed to allocate memory for VvasMemory");
    if (vret)
      *vret = VVAS_RET_ALLOC_ERROR;
    return NULL;
  }

  priv->data = data;
  priv->size = size;
  priv->free_cb = free_cb;
  priv->user_data = user_data;
  priv->own_alloc = 0;
  priv->ctx = vvas_ctx;
  priv->mem_info.alloc_type = VVAS_ALLOC_TYPE_NON_CMA;

  if (vret)
    *vret = VVAS_RET_SUCCESS;

  return (VvasMemory *)priv;
}

/**
 * @fn VvasReturnType vvas_memory_map (VvasMemory* vvas_mem,
 *                                     VvasDataMapFlags flags,
 *                                     VvasMemoryMapInfo *info)
 * @param[in] vvas_mem - Address of @ref VvasMemory
 * @param[in] flags - Flags used to map \p vvas_mem
 * @param[out] info - Structure which gets populated after mapping is successful
 * @return  @ref VvasReturnType
 * @brief Maps \p vvas_mem to user space using \p flags. Based on VvasMemory::sync_flags, data will be synchronized between host and device.
 */
VvasReturnType
vvas_memory_map (VvasMemory* vvas_mem, VvasDataMapFlags map_flags, VvasMemoryMapInfo *info)
{
  VvasMemoryPrivate* priv = (VvasMemoryPrivate* )vvas_mem;

  if (!priv || !info) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid arguments");
    return VVAS_RET_INVALID_ARGS;
  }

  if (priv->mem_info.alloc_type == VVAS_ALLOC_TYPE_CMA) { /* map XRT memory */

    if (map_flags & VVAS_DATA_MAP_READ) {
      /* Sync Data from device before mapping in READ mode */
      vvas_memory_sync_data (vvas_mem, VVAS_BO_SYNC_BO_FROM_DEVICE);
    }

    if (map_flags & VVAS_DATA_MAP_WRITE) {
      /* Now user is requesting memory in write mode, we need to sync data to device after user writes */
      vvas_memory_set_sync_flag (&priv->mem_info, VVAS_DATA_SYNC_TO_DEVICE);
    }

    priv->data = vvas_xrt_map_bo (priv->boh, map_flags & VVAS_DATA_MAP_WRITE);
    if (!priv->data) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, priv->ctx->log_level, "failed to map memory");
      return VVAS_RET_ERROR;
    }
  }

  info->data = priv->data;
  info->size = priv->size;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->ctx->log_level, "mapped memory %p : data = %p, size = %zu", vvas_mem, info->data, info->size);

  return VVAS_RET_SUCCESS;
}

/**
 * @fn VvasReturnType vvas_memory_unmap (VvasMemory* vvas_mem,
 *                                       VvasMemoryMapInfo *info);
 * @param[in] vvas_mem - Address of @ref VvasMemory
 * @return  @ref VvasReturnType
 * @brief Unmaps \p vvas_mem from user space
 */
VvasReturnType
vvas_memory_unmap (VvasMemory* vvas_mem, VvasMemoryMapInfo *info)
{
  VvasMemoryPrivate* priv = (VvasMemoryPrivate* )vvas_mem;

  if (!priv) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid arguments");
    return VVAS_RET_INVALID_ARGS;
  }

  if (priv->mem_info.alloc_type == VVAS_ALLOC_TYPE_CMA) { /* unmap XRT memory */
    vvas_xrt_unmap_bo  (priv->boh, priv->data);
  }

  LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->ctx->log_level, "unmapped memory %p : data = %p, size = %zu", vvas_mem, info->data, info->size);
  return VVAS_RET_SUCCESS;
}

/**
 * @fn void vvas_memory_free (VvasMemory* vvas_mem)
 * @param[in] vvas_mem - Address of @ref VvasMemory
 * @brief frees the memory allocated during @ref vvas_memory_alloc API
 * @return  None
 */
void
vvas_memory_free (VvasMemory* vvas_mem)
{
  VvasMemoryPrivate* priv = (VvasMemoryPrivate* )vvas_mem;

  if (!priv) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid arguments");
    return;
  }

  if (priv->mem_info.alloc_type == VVAS_ALLOC_TYPE_CMA) {
    vvas_xrt_free_bo (priv->boh);
  } else {
    if (priv->free_cb)
      priv->free_cb (priv->data, priv->user_data);
    else if (priv->data && priv->own_alloc)
      free (priv->data);
  }

  LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->ctx->log_level, "freeing memory %p", priv);
  free (priv);
}

/**
 * @fn void vvas_memory_set_metadata (VvasMemory* vvas_mem,
 *                                    VvasMetadata *meta_data);
 * @param[in] vvas_mem - Address of @ref VvasMemory
 * @param[in] meta_data - Address of @ref VvasMetadata
 * @return None
 * @brief Sets metadata on VvasMemory object
 */
void
vvas_memory_set_metadata (VvasMemory* vvas_mem, VvasMetadata *meta_data)
{
  VvasMemoryPrivate* priv = (VvasMemoryPrivate* )vvas_mem;

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
 * @fn void vvas_memory_get_metadata (VvasMemory* vvas_mem,
 *                                    VvasMetadata *meta_data)
 * @param[in] vvas_mem Address of @ref VvasMemory
 * @param[in] meta_data Address of @ref VvasMetadata
 * @return None
 * @brief Gets metadata from VvasMemory object
 */
void
vvas_memory_get_metadata (VvasMemory* vvas_mem, VvasMetadata *meta_data)
{
  VvasMemoryPrivate* priv = (VvasMemoryPrivate* )vvas_mem;

  if (!priv || !meta_data) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid arguments");
    return;
  }

  meta_data->pts = priv->meta_data.pts;
  meta_data->dts = priv->meta_data.dts;
  meta_data->duration = priv->meta_data.duration;

  return;
}


/****************************************************************************
 ***********  Private API for VVAS Base library implementation  *********
 ****************************************************************************/
/**
 * @fn void vvas_memory_sync_data (VvasMemory* vvas_mem, VvasDataSyncFlags flag)
 * @param[in] vvas_mem - Address of @ref VvasMemory
 * @return None
 * @brief Data will be synchronized between device and host cored on VvaseMemory::sync_flags.
 */
void
vvas_memory_sync_data (VvasMemory* vvas_mem, VvasDataSyncFlags sync_flag)
{
  VvasMemoryPrivate* priv = (VvasMemoryPrivate* )vvas_mem;
  int ret = 0;

  if (sync_flag & VVAS_DATA_SYNC_TO_DEVICE) {
    ret = vvas_xrt_sync_bo (priv->boh, VVAS_BO_SYNC_BO_TO_DEVICE, priv->size, 0);
    if (ret != 0) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, priv->ctx->log_level, "failed to sync memory");
      return;
    }
    vvas_memory_unset_sync_flag (vvas_mem, VVAS_BO_SYNC_BO_TO_DEVICE);
  } else if (sync_flag & VVAS_DATA_SYNC_FROM_DEVICE) {
    ret = vvas_xrt_sync_bo (priv->boh, VVAS_BO_SYNC_BO_FROM_DEVICE, priv->size, 0);
    if (ret != 0) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, priv->ctx->log_level, "failed to sync memory");
      return;
    }
    vvas_memory_unset_sync_flag (vvas_mem, VVAS_BO_SYNC_BO_FROM_DEVICE);
  } else {
    return;
  }
  LOG_MESSAGE (LOG_LEVEL_DEBUG, priv->ctx->log_level, "memory %p sync %s device is completed", vvas_mem, sync_flag == VVAS_DATA_SYNC_TO_DEVICE ? "to":"from");
}

/**
 * @fn void vvas_memory_set_sync_flag (VvasMemory* vvas_mem, VvasDataSyncFlags flag)
 * @param[in] vvas_mem - Address of @ref VvasMemory
 * @param[in] flag - Flag to be set on \p vvas_mem
 * @return  None
 * @brief Enables VvasDataSyncFlags on memory
 */
void
vvas_memory_set_sync_flag (VvasMemory* vvas_mem, VvasDataSyncFlags flag)
{
  VvasMemoryPrivate* priv = (VvasMemoryPrivate* )vvas_mem;
  priv->mem_info.sync_flags |= flag;
}

/**
 * @fn void vvas_memory_unset_sync_flag (VvasMemory* vvas_mem, VvasDataSyncFlags flag)
 * @param[in] vvas_mem - Address of @ref VvasMemory
 * @param[in] flag - Flag to be cleared on \p vvas_mem
 * @return  None
 * @brief Disables VvasDataSyncFlags on memory
 */
void
vvas_memory_unset_sync_flag (VvasMemory* vvas_mem, VvasDataSyncFlags flag)
{
  VvasMemoryPrivate* priv = (VvasMemoryPrivate* )vvas_mem;
  priv->mem_info.sync_flags &= ~flag;
}

/**
 * @fn uint64_t vvas_memory_get_paddr (VvasMemory* vvas_mem)
 * @param[in] vvas_mem - Address of @ref VvasMemory
 * @return  Valid physical address or on error, returns (uint64_t)-1
 * @brief API to get physical address corresponding to VvasMemory
 */
uint64_t
vvas_memory_get_paddr (VvasMemory* vvas_mem)
{
  VvasMemoryPrivate* priv = (VvasMemoryPrivate* )vvas_mem;

  if (!priv) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid memory address");
    return (uint64_t)-1;
  }

  if (priv->mem_info.alloc_type != VVAS_ALLOC_TYPE_CMA) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, priv->ctx->log_level, "Not a CMA memory to get physical address");
    return (uint64_t)-1;
  }

  return vvas_xrt_get_bo_phy_addres (priv->boh);
}

/**
 * @fn void* vvas_memory_get_bo (VvasMemory *vvas_mem)
 * @param[in] vvas_mem - Address of @ref VvasMemory
 * @return Valid BO address upon success \n On failure, NULL will be returned
 * @brief API to get XRT BO corresponding to VvasMemory
 */
void*
vvas_memory_get_bo (VvasMemory *vvas_mem)
{
  VvasMemoryPrivate* priv = (VvasMemoryPrivate* )vvas_mem;

  if (!priv) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid memory address");
    return NULL;
  }

  return (void *)priv->boh;
}

/**
 * @fn int32_t vvas_memory_get_device_index (VvasMemory *vvas_mem)
 * @param[in] vvas_mem - Address of @ref VvasMemory
 * @return  Valid device index or -1 on failure
 * @brief API to get device index on which VvasMemory was allocated
 */
int32_t
vvas_memory_get_device_index (VvasMemory *vvas_mem)
{
  VvasMemoryPrivate* priv = (VvasMemoryPrivate* )vvas_mem;

  if (!priv) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid memory address");
    return -1;
  }

  return priv->ctx->dev_idx;
}

/**
 * @fn int32_t vvas_memory_get_bank_index (VvasMemory *vvas_mem)
 * @param[in] vvas_mem - Address of @ref VvasMemory
 * @return Valid bank index or -1 on failure
 * @brief API to get bank index on which VvasMemory was allocated
 */
int32_t
vvas_memory_get_bank_index (VvasMemory *vvas_mem)
{
  VvasMemoryPrivate* priv = (VvasMemoryPrivate* )vvas_mem;

  if (!priv) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid memory address");
    return -1;
  }

  return priv->mbank_idx;
}
