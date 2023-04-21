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
 * DOC: VVAS Memory APIs
 * This file contains structures and methods related VVAS memory.
 *
 */

#ifndef __VVAS_MEMORY_H__
#define __VVAS_MEMORY_H__

#include <vvas_core/vvas_common.h>
#include <vvas_core/vvas_context.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  typedef VvasMemoryDataFreeCB - Callback function to be called to free memory pointed by @data,
 *                                                           when VvasMemory handle is getting freed using @vvas_memory_free() API.
 *  @data: Address of the data pointer
 *  @user_data: User data pointer sent via @vvas_memory_alloc_from_data() API
 *
 *  Return: None
 */
typedef void (*VvasMemoryDataFreeCB)(void *data, void *user_data);

/**
 * struct VvasMemoryMapInfo - Stores information related to VvasMemory after mapping
 * @data: Pointer to memory which is mapped to user space using VvasDataMapFlags
 * @size: Size of the mapped memory
 */
typedef struct {
  uint8_t *data;
  size_t size;
} VvasMemoryMapInfo;

typedef void VvasMemory;

/**
 * vvas_memory_alloc () - Allocates memory specified by @size and other arguments passed to this API
 * @vvas_ctx: Address of VvasContext handle created using @vvas_context_create()
 * @mem_type: Type of the memory need to be allocated
 * @mem_flags: Flags of type &enum VvasAllocationFlags
 * @mbank_idx: Index of the memory bank on which memory need to be allocated
 * @size: Size of the memory to be allocated.
 * @ret: Address to store return value. In case of error, @ret is useful in understanding the root cause
 *
 * Return:
 * * On Success, returns VvasMemory handle,
 * * On Failure, returns NULL
 */
VvasMemory* vvas_memory_alloc (VvasContext *vvas_ctx, VvasAllocationType mem_type, VvasAllocationFlags mem_flags, uint8_t mbank_idx, size_t size, VvasReturnType *ret);

/**
 * vvas_memory_alloc_from_data () - Allocates VvasMemory handle from @data pointer and @size
 * @vvas_ctx: Address of VvasContext handle created using @vvas_context_create()
 * @data: Pointer to data which needs to encapsulated in &struct VvasMemory
 * @size: Size of the memory to which @data pointer is pointing
 * @free_cb: Callback function to be called during @vvas_memory_free() API.
 * @user_data: User defined data
 * @ret: Address to store return value. Upon case of error, @ret is useful in understanding the root cause
 *
 * When application allocates memory and needs to send it to VVAS core APIs, this API is useful to wrap this memory pointer into VvasMemory handle.
 *
 * Return:
 * * On Success, returns VvasMemory handle,
 * * On Failure, returns NULL
 */
VvasMemory* vvas_memory_alloc_from_data (VvasContext *vvas_ctx, uint8_t *data, size_t size, VvasMemoryDataFreeCB free_cb, void *user_data, VvasReturnType *ret);

/**
 * vvas_memory_free() - Frees the memory allocated by @vvas_memory_alloc() API
 * @vvas_mem: Address of &struct VvasMemory object
 *
 * Return:  None
 */
void vvas_memory_free (VvasMemory* vvas_mem);

/**
 * vvas_memory_map() - Maps @vvas_mem to user space using @flags. Based on &VvasMemory->sync_flags, data will be synchronized between host and device.
 * @vvas_mem: Address of &struct VvasMemory object
 * @flags: Flags used to map @vvas_mem
 * @info: Structure which gets populated after mapping is successful
 *
 * Return: &enum VvasReturnType
 */
VvasReturnType vvas_memory_map (VvasMemory* vvas_mem, VvasDataMapFlags flags, VvasMemoryMapInfo *info);

/**
 * vvas_memory_unmap() - Unmaps @vvas_mem from user space
 * @vvas_mem: Address of &struct VvasMemory object
 * @info: Memory map information populated during vvas_memory_map() API
 *
 * Return: &enum VvasReturnType
 */
VvasReturnType vvas_memory_unmap (VvasMemory* vvas_mem, VvasMemoryMapInfo *info);

/**
 * vvas_memory_set_metadata() - Sets &VvasMetadata metadata on &struct VvasMemory object
 * @vvas_mem: Address of &struct VvasMemory object
 * @meta_data: Address of &struct VvasMetadata to be set on @vvas_mem
 *
 * Return: None
 */
void vvas_memory_set_metadata (VvasMemory* vvas_mem, VvasMetadata *meta_data);

/**
 * vvas_memory_get_metadata() - Gets &VvasMetadata metadata from &struct VvasMemory object
 * @vvas_mem: Address of &struct VvasMemory object
 * @meta_data: Address of &struct VvasMetadata to be populated by this API
 *.
 * Return: None
 */
void vvas_memory_get_metadata (VvasMemory* vvas_mem, VvasMetadata *meta_data);

#ifdef __cplusplus
}
#endif
#endif
