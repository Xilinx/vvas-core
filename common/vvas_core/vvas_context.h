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
 * DOC: Contains VVAS Context specific to an FPGA device
 *
 * This file contains context related structures and method declarations for VVAS core libraries
 */

#ifndef __VVAS_CONTEXT_H__
#define __VVAS_CONTEXT_H__

#include <vvas_core/vvas_log.h>
#include <vvas_core/vvas_common.h>
#include <vvas_core/vvas_device.h>

/**
 * struct VvasContext - Holds a context related to a device
 * @dev_idx: Device index to which current context belongs
 * @xclbin_loc: xclbin location which is used to configure a device
 * @dev_handle: Handle to a device with &VvasContext->dev_idx
 * @uuid: UUID of xclbin
 * @log_level: Loging level to be used by context
 */
typedef struct {
  int32_t dev_idx;
  char *xclbin_loc;
  vvasDeviceHandle dev_handle;
  uuid_t uuid;
  VvasLogLevel log_level;
} VvasContext;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * vvas_context_create() - Creates device handle by opening specified device index and download xclbin image on the same
 * @dev_idx: Index of the FPGA device. This can be -1 if no FPGA is present
 * @xclbin_loc: Location of xclbin to be downloaded on device index @dev_idx. This can be NULL as well in case user does not want to access FPGA device
 * @log_level: Logging level
 * @vret: Address to store return value. In case of error, @vret is useful in understanding the root cause
 *
 * User can create multiple contexts to a device with same xclbin. If user wish creates
 * a context with different xclbin than the xclbin configured on a FPGA device,
 * he/she need to first destroy the old context with vvas_context_destroy()
 * before creating new context. User shall provide @dev_idx and @xclbin_loc if
 * there is a need to access FPGA device while calling this API.
 *
 * Return:
 * Address of VvasContext on success
 * NULL on failure
 */
VvasContext* vvas_context_create (int32_t dev_idx, char * xclbin_loc, VvasLogLevel log_level, VvasReturnType *vret);

/**
 * vvas_context_destroy() - Destroys device context
 * @vvas_ctx: Context to device
 * Before destroying the context, application should destroy modules which are using current context.
 * Return: &enum VvasReturnType
 */
VvasReturnType vvas_context_destroy (VvasContext* vvas_ctx);

#ifdef __cplusplus
}
#endif

#endif /*__VVAS_CONTEXT_H__*/
