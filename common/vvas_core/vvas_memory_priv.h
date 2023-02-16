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
 * DOC: Contains structures and methods private to VvasMemory object
 * Structure and methods declared here are mainly for VVAS core library implementation, not for application development
 */

#ifndef __VVAS_MEMORY_PRIV_H__
#define __VVAS_MEMORY_PRIV_H__

#include <vvas_core/vvas_memory.h>

/**
 * struct VvasMemoryPrivate - Holds private information related to memory
 * @mem_info: Allocation Information related to VvasMemory
 * @ctx: VVAS Context with which memory is allocated
 * @boh: BO handle
 * @mbank_idx: Memory bank index
 * @data: User space pointer to hold data
 * @size: Memory size
 * @free_cb: Callback function to be triggered when vvas_memory_free() is called
 * @user_data: User data set by the user in vvas_memory_alloc_from_data() API
 * @own_alloc: Memory (i.e. holded by @data ptr) is allocated by VvasMemory API or user application
 * @meta_data: Holds metadata like timestamps
 *
 */
typedef struct {
  VvasAllocationInfo mem_info;
  VvasContext *ctx;
  vvasBOHandle boh;
  int32_t mbank_idx;
  uint8_t *data;
  size_t size;
  VvasMemoryDataFreeCB free_cb;
  void *user_data;
  uint8_t own_alloc;
  VvasMetadata meta_data;
} VvasMemoryPrivate;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * vvas_memory_sync_data() - Initiates data synchronization between device and host based on &VvaseMemoryPrivate->sync_flags
 * @vvas_mem: Address of &struct VvasMemory
 * @flag: Flags to be used for synchronization of data between an FPGA and host
 *
 * Return: None
 */
void vvas_memory_sync_data (VvasMemory* vvas_mem, VvasDataSyncFlags flag);

/**
 * vvas_memory_set_sync_flag() - Sets flags on VvasMemory object
 * @vvas_mem: Address of &struct VvasMemory
 * @flag: Flags to be set on @vvas_mem
 *
 * Return: None
 */
void vvas_memory_set_sync_flag (VvasMemory* vvas_mem, VvasDataSyncFlags flag);

/**
 * vvas_memory_unset_sync_flag() - Clears specified flags from VvasMemory object
 * @vvas_mem: Address of &struct VvasMemory
 * @flag: Flags to be unset on @vvas_mem
 *
 * Return: None
 */
void vvas_memory_unset_sync_flag (VvasMemory* vvas_mem, VvasDataSyncFlags flag);

/**
 * vvas_memory_get_paddr() - Gets physical address corresponding to VvasMemory object
 * @vvas_mem: Address of &struct VvasMemory
 *
 * Return:
 * On success, Valid physical address
 * On error, returns (uint64_t)-1
 */
uint64_t vvas_memory_get_paddr (VvasMemory* vvas_mem);

/**
 * vvas_memory_get_bo() - Gets XRT BO corresponding to VvasMemory
 * @vvas_mem: Address of &struct VvasMemory
 *
 * Return:
 * Valid BO address upon success
 * NULL on failure
 */
void* vvas_memory_get_bo (VvasMemory *vvas_mem);

/**
 * vvas_memory_get_device_index() - Gets device index on which VvasMemory was allocated
 * @vvas_mem: Address of &struct VvasMemory
 *
 * Return:
 * Valid device index on success
 * -1 on failure
 */
int32_t vvas_memory_get_device_index (VvasMemory *vvas_mem);

/**
 * vvas_memory_get_bank_index() - Gets bank index on which VvasMemory was allocated
 * @vvas_mem: Address of &struct VvasMemory
 *
 * Return:
 * Valid bank index on success
 * -1 on failure
 */
int32_t vvas_memory_get_bank_index (VvasMemory *vvas_mem);
#ifdef __cplusplus
}
#endif

#endif
