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
 * DOC: Contains structures and methods private to VvasVideoFrame object
 *
 * Note : Structure and methods declared here are mainly for VVAS core library implementation, not for application development
 */

#ifndef __VVAS_VIDEO_PRIV_H__
#define __VVAS_VIDEO_PRIV_H__

#include <vvas_core/vvas_common.h>

#define VVAS_ROUND_UP_2(num)  (((num)+1)&~1)
#define VVAS_ROUND_UP_4(num)  (((num)+3)&~3)
#define ALIGN(size,align) ((((size) + (align) - 1) / align) * align)

/**
 * struct VvasVideoPlane - Information specific to a video plane
 * @boh:  BO handle
 * @data: User space pointer to hold plane data
 * @size: Holds size of the plane
 * @offset: Offset of the first valid data from the @data pointer
 * @stride: Stride of a video plane
 * @elevation: Elevation (in height direction) of a video plane
 */
typedef struct {
  vvasBOHandle boh;
  uint8_t *data;
  uint64_t size;
  uint64_t offset;
  size_t stride;
  size_t elevation;
} VvasVideoPlane;

/**
 * struct VvasVideoFramePriv - Holds information to a raw video frame (e.g. NV12/RGB video frame)
 * @mem_info: Allocation Information realted to VvasVideoFrame
 * @ctx: VVAS Context with which memory is allocated
 * @log_level: Logging level
 * @boh: BO handle of a video plane to synchronize data between an FPGA device and host
 * @mbank_idx: Memory bank index
 * @size: Video frame size
 * @num_planes: Number of planes in video frame
 * @width: Width of the video frame
 * @height: Height of the video frame
 * @fmt: Color format of the video frame
 * @alignment: Video frame alignment information
 * @planes: Array which holds plane's data pointer
 * @meta_data: Metadata like timestamps
 * @free_cb: Callback function to be triggered when vvas_video_frame_free() is called
 * @user_data: User data set by the user in vvas_memory_alloc_from_data() API
 * @own_alloc: Data is allocated by application or VVASVideoFrame API
 */
typedef struct {
  VvasAllocationInfo mem_info;
  VvasContext *ctx;
  VvasLogLevel log_level;
  vvasBOHandle boh;
  int32_t mbank_idx;
  size_t size;
  uint8_t num_planes;
  uint32_t width;
  uint32_t height;
  VvasVideoFormat fmt;
  VvasVideoAlignment alignment;
  VvasVideoPlane planes[VVAS_VIDEO_MAX_PLANES];
  VvasMetadata meta_data;
  VvasVideoFrameDataFreeCB free_cb;
  void *user_data;
  uint8_t own_alloc;
} VvasVideoFramePriv;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * vvas_video_frame_sync_data() - Data will be synchronized between device and host based on &enum VvasDataSyncFlags
 * @vvas_mem: Address of &struct VvasVideoFrame
 * @sync_flag: Flags used to synchronize data between an FPGA device and host
 *
 * Return: None
 */
void vvas_video_frame_sync_data (VvasVideoFrame* vvas_mem, VvasDataSyncFlags sync_flag);

/**
 * vvas_video_frame_set_sync_flag() - Enable flags @flag on video frame @vvas_mem
 * @vvas_mem: Address of &struct VvasVideoFrame
 * @flag: Flag to be set on @vvas_mem
 *
 * Return: None
 */
void vvas_video_frame_set_sync_flag (VvasVideoFrame* vvas_mem, VvasDataSyncFlags flag);

/**
 * vvas_video_frame_unset_sync_flag() - Disable flags @flag on video frame @vvas_mem
 * @vvas_mem: Address of &struct VvasVideoFrame
 * @flag: Flag to be set on @vvas_mem
 *
 * Return: None
 */
void vvas_video_frame_unset_sync_flag (VvasVideoFrame* vvas_mem, VvasDataSyncFlags flag);

/**
 * vvas_video_frame_get_frame_paddr() - Gets physical address corresponding to &struct VvasVideoFrame
 * @vvas_mem: Address of &struct VvasVideoFrame
 *
 * Return:
 * On success, a valid physical address
 * On error, returns (uint64_t)-1
 *
 */
uint64_t vvas_video_frame_get_frame_paddr (VvasVideoFrame* vvas_mem);

/**
 * vvas_video_frame_get_plane_paddr() - Gets physical address corresponding to a plane in video frame
 * @vvas_mem: Address of &struct VvasVideoFrame
 * @plane_idx: Plane index in a video frame
 *
 * Return:
 * On success, a valid physical address
 * On error, returns (uint64_t)-1
 */
uint64_t vvas_video_frame_get_plane_paddr (VvasVideoFrame* vvas_mem, uint8_t plane_idx);

/**
 * vvas_video_frame_get_bo() - Gets XRT BO corresponding to &struct VvasVideoFrame
 * @vvas_mem: Address of &struct VvasVideoFrame
 *
 * Return:
 * On success, a valid BO address
 * On failure, NULL will be returned
 */
void* vvas_video_frame_get_bo (VvasVideoFrame *vvas_mem);

/**
 * vvas_video_frame_get_plane_bo() - Gets XRT BO corresponding to a plane in video frame
 * @vvas_mem: Address of &struct VvasVideoFrame
 * @plane_idx: Plane index in a video frame
 *
 * Return:
 * On success, a valid BO address
 * On failure, NULL will be returned
 */
void* vvas_video_frame_get_plane_bo (VvasVideoFrame *vvas_mem, uint8_t plane_idx);

/**
 * vvas_video_frame_get_device_index() - Gets device index on which @struct VvasVideoFrame was allocated
 * @vvas_mem: Address of &struct VvasVideoFrame
 *
 * Return:
 * On success, a valid device index
 * -1 on failure
 */
int32_t vvas_video_frame_get_device_index (VvasVideoFrame *vvas_mem);

/**
 * vvas_video_frame_get_bank_index() - Gets memory bank index on which &struct VvasVideoFrame was allocated
 * @vvas_mem: Address of &struct VvasVideoFrame
 *
 * Return:
 * On success, a valid memory bank index
 * -1 on failure
 */
int32_t vvas_video_frame_get_bank_index (VvasVideoFrame *vvas_mem);

/**
 * vvas_video_frame_get_size() - Gets size of the video frame
 * @vvas_mem: Address of &struct VvasVideoFrame
 *
 * Return: Size of the video frame
 */
size_t vvas_video_frame_get_size (VvasVideoFrame* vvas_mem);

/**
 * vvas_video_frame_get_allocation_type() - Gets the allocation type used while allocating a video frame
 * @vvas_frame: Address of &struct VvasVideoFrame
 *
 * Return: Allocation type &enum VvasAllocationType
 */
VvasAllocationType vvas_video_frame_get_allocation_type (const VvasVideoFrame* vvas_frame);

/**
 * vvas_video_frame_get_allocation_flag() - Gets allocation flags used while allocating a video frame
 * @vvas_frame: Address of &struct VvasVideoFrame
 *
 * Return: Allocation flag &enum VvasAllocationFlags
 */
VvasAllocationFlags vvas_video_frame_get_allocation_flag (const VvasVideoFrame* vvas_frame);

/**
 * vvas_fill_planes() - Populates @vvas_frame based @info
 * @info: Video Info
 * @vvas_frame: Address of &struct VvasVideoFrame
 *
 * Return:
 * 0 on success
 * -1 on failure
 */
int8_t vvas_fill_planes (VvasVideoInfo * info, VvasVideoFrame *vvas_frame);

#ifdef __cplusplus
}
#endif

#endif
