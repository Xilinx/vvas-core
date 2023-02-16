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

#ifndef __VVAS_BUFFER_POOL_H__
#define __VVAS_BUFFER_POOL_H__

#include <stdint.h>
#include <stdbool.h>
#include <vvas_core/vvas_common.h>
#include <vvas_core/vvas_video.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef void VvasBufferPool;

  typedef struct
  {
  /** Video info */
    VvasVideoInfo video_info;
  /** Minimum buffers in the buffer pool */
    uint32_t minimum_buffers;
  /** Maximum buffers in the buffer pool */
    uint32_t maximum_buffers;
  /** Buffer allocation type CMA/Non CMA */
    VvasAllocationType alloc_type;
  /** Buffer allocation flags */
    VvasAllocationFlags alloc_flag;
  /** Memory bank on which to allocate buffer (In case of HW/CMA buffer) */
    uint8_t mem_bank_idx;
  /** If set, acquire_buffer() will block when pool is empty */
    bool block_on_empty;
  } VvasBufferPoolConfig;

  typedef struct
  {
  /** VvasBufferPool to which this video_frame/video_memory belongs to */
    VvasBufferPool *buffer_pool;
  /** Video Frame */
    VvasVideoFrame *video_frame;
  /** User data, can be used to keep metadata for video_frame */
    void *user_data;
  } VvasVideoBuffer;


  VvasBufferPool *vvas_buffer_pool_create (VvasContext * vvas_ctx,
      const VvasBufferPoolConfig * props, VvasLogLevel log_level);

  VvasReturnType vvas_buffer_pool_free (VvasBufferPool * pool);

  VvasVideoBuffer *vvas_buffer_pool_acquire_buffer (VvasBufferPool * pool);
  VvasReturnType vvas_buffer_pool_release_buffer (VvasVideoBuffer * buffer);

  VvasReturnType vvas_buffer_pool_get_configuration (VvasBufferPool * pool,
      VvasBufferPoolConfig * props);

  typedef void (*vvas_buffer_pool_buffer_relase_callback) (void *user_data);

  VvasReturnType vvas_buffer_pool_set_release_buffer_notify_cb (VvasBufferPool *
      pool, vvas_buffer_pool_buffer_relase_callback cb, void *user_data);

#ifdef __cplusplus
}
#endif
#endif                          /* __VVAS_BUFFER_POOL_H__ */
