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

#include <vvas_core/vvas_context.h>
#include <vvas_core/vvas_log.h>

/**
 * @fn VvasContext* vvas_context_create (int32_t dev_idx, uint8_t * xclbin_loc, VvasReturnType *vret)
 * @param[in] dev_idx - Index of the FPGA device. This can be -1 if no FPGA is present
 * @param[in] xclbin_loc - Location of xclbin to be downloaded on device index \p dev_idx. This can be NULL as well in case user does not want to access FPGA device
 * @param[in] vret - Address to store return value. In case of error, \p ret is useful in understanding the root cause
 * @brief Creates device handle by opening specified device index and download xclbin image on the same
 * @details User can create multiple contexts to a device with same xclbin.
 *               If user wish creates a context with different xclbin than the xclbin configured on a FPGA device,
 *               he/she need to first destroy the old context with @ref vvas_context_destroy() before creating
 *               new context. User shall provide \p dev_idx and \p xclbin_loc if there is a need to access FPGA device while calling this API.
 *
 * @return  Address of VvasContext on success\n
 *                NULL on failure
 */
VvasContext* 
vvas_context_create (int32_t dev_idx, char * xclbin_loc, VvasLogLevel log_level, VvasReturnType *vret)
{
  VvasContext *ctx;

  /* allocate context memory */
  ctx = (VvasContext *) calloc (1, sizeof (VvasContext));
  if (!ctx) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, log_level, "failed to allocate memory");
    if (vret)
      *vret = VVAS_RET_ALLOC_ERROR;
    return NULL;
  }

  /* Lets create the device context only when we have a valid device index.
   * User will pass -1 when VVAS context needs to be created for software
   * kernels.
   */
  if (dev_idx >= 0 && xclbin_loc) {
    /* open xrt device to create dev_handle to it */
    if (!vvas_xrt_open_device (dev_idx, &ctx->dev_handle)) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, log_level, "failed to open device with index %d", dev_idx);
      if (vret)
        *vret = VVAS_RET_ERROR;
      free (ctx);
      return NULL;
    }

    /* download xclbin */
    if (vvas_xrt_download_xclbin (xclbin_loc, ctx->dev_handle, &ctx->uuid)) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, log_level, "Failed to download xclbin");
      if (vret)
        *vret = VVAS_RET_ERROR;

      vvas_xrt_close_device (ctx->dev_handle);
      free (ctx);
      return NULL;
    }
    /* Make a copy of xclbin path into VVAS context */
    ctx->xclbin_loc = strdup (xclbin_loc);
  } else {
    ctx->dev_handle = NULL;
  }

  /* global local level */
  ctx->log_level = log_level;
  ctx->dev_idx = dev_idx;

  if (vret)
    *vret = VVAS_RET_SUCCESS;

  return ctx;
}

/**
 * @fn VvasReturnType vvas_context_destroy (VvasContext* vvas_ctx)
 * @param[in] vvas_ctx - Context to device
 * @brief Destroys device context
 * @details Before destroying the context, application should destroy modules which are using current context.
 * @return VvasReturnType
 */
VvasReturnType
vvas_context_destroy (VvasContext* vvas_ctx)
{
  if (!vvas_ctx) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "invalid argument");
    return VVAS_RET_INVALID_ARGS;
  }

  /* close handle to device */
  if (vvas_ctx->dev_handle)
    vvas_xrt_close_device (vvas_ctx->dev_handle);

  if (vvas_ctx->xclbin_loc)
    free (vvas_ctx->xclbin_loc);

  free (vvas_ctx);
  return VVAS_RET_SUCCESS;
}
