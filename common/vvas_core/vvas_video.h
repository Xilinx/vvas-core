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
 * DOC: VVAS Video Common APIs
 * This file contains structures and methods related to VVAS Video Frame.
 */

#ifndef __VVAS_VIDEO_H__
#define __VVAS_VIDEO_H__

#include <vvas_core/vvas_common.h>
#include <vvas_core/vvas_context.h>

#define VVAS_VIDEO_MAX_PLANES 4

/**
 * enum VvasCodecType - Codec types supported by VVAS Core APIs
 * @VVAS_CODEC_UNKNOWN: Unknown codec type
 * @VVAS_CODEC_H264: H264/AVC codec type
 * @VVAS_CODEC_H265: H265/HEVC codec type
 */
typedef enum {
  VVAS_CODEC_UNKNOWN = -1,
  VVAS_CODEC_H264,
  VVAS_CODEC_H265,
} VvasCodecType;

/** 
 * enum VvasVideoFormat - Represents video color formats supported by VVAS core APIs
 * @VVAS_VIDEO_FORMAT_UNKNOWN: Unknown color format
 * @VVAS_VIDEO_FORMAT_Y_UV8_420: planar 4:2:0 YUV with interleaved UV plane
 * @VVAS_VIDEO_FORMAT_RGBx: Packed RGB, 4 bytes per pixel
 * @VVAS_VIDEO_FORMAT_r210: Packed 4:4:4 RGB, 10 bits per channel
 * @VVAS_VIDEO_FORMAT_Y410: Packed 4:4:4 YUV, 10 bits per channel
 * @VVAS_VIDEO_FORMAT_BGRx: Packed BGR, 4 bytes per pixel
 * @VVAS_VIDEO_FORMAT_BGRA: Reverse rgb with alpha channel last
 * @VVAS_VIDEO_FORMAT_RGBA: RGB with alpha channel last
 * @VVAS_VIDEO_FORMAT_YUY2: Packed 4:2:2 YUV (Y0-U0-Y1-V0, Y2-U2-Y3-V2 Y4...)
 * @VVAS_VIDEO_FORMAT_NV16: Planar 4:2:2 YUV with interleaved UV plane
 * @VVAS_VIDEO_FORMAT_RGB: RGB packed into 24 bits without padding
 * @VVAS_VIDEO_FORMAT_v308: Packed 4:4:4 YUV
 * @VVAS_VIDEO_FORMAT_BGR: BGR packed into 24 bits without padding
 * @VVAS_VIDEO_FORMAT_I422_10LE: Planar 4:2:2 YUV, 10 bits per channel
 * @VVAS_VIDEO_FORMAT_NV12_10LE32: 10-bit variant of GST_VIDEO_FORMAT_NV12, packed into 32bit words (MSB 2 bits padding)
 * @VVAS_VIDEO_FORMAT_GRAY8: 8-bit grayscale
 * @VVAS_VIDEO_FORMAT_GRAY10_LE32: 10-bit grayscale, packed into 32bit words (2 bits padding)
 * @VVAS_VIDEO_FORMAT_I420: Planar 4:2:0 YUV
 */
typedef enum {
  VVAS_VIDEO_FORMAT_UNKNOWN = 0,
  VVAS_VIDEO_FORMAT_Y_UV8_420,
  VVAS_VIDEO_FORMAT_RGBx,
  VVAS_VIDEO_FORMAT_r210,
  VVAS_VIDEO_FORMAT_Y410,
  VVAS_VIDEO_FORMAT_BGRx,
  VVAS_VIDEO_FORMAT_BGRA,
  VVAS_VIDEO_FORMAT_RGBA,
  VVAS_VIDEO_FORMAT_YUY2,
  VVAS_VIDEO_FORMAT_NV16,
  VVAS_VIDEO_FORMAT_RGB,
  VVAS_VIDEO_FORMAT_v308,
  VVAS_VIDEO_FORMAT_BGR,
  VVAS_VIDEO_FORMAT_I422_10LE,
  VVAS_VIDEO_FORMAT_NV12_10LE32,
  VVAS_VIDEO_FORMAT_GRAY8,
  VVAS_VIDEO_FORMAT_GRAY10_LE32,
  VVAS_VIDEO_FORMAT_I420,
} VvasVideoFormat;

/**
 * struct VvasVideoAlignment - Contains video alignment information
 * @padding_right: Padding to the right
 * @padding_left: Padding to the left
 * @padding_top: Padding to the top
 * @padding_bottom: Padding to the bottom
 * @stride_align: Extra alignment requirement for strides (which is in bytes)
 */

typedef struct {
  uint32_t padding_right;
  uint32_t padding_left;
  uint32_t padding_top;
  uint32_t padding_bottom;
  uint32_t stride_align[VVAS_VIDEO_MAX_PLANES];
}VvasVideoAlignment;

/**
 * struct VvasVideoInfo - Contains infomation related to a video frame
 * @width: Width of a video frame
 * @height: Height of a video frame
 * @fmt: Video frame color format
 * @n_planes: Number of planes in video frame color format
 * @stride: Array of stride values
 * @elevation: Array of elevation values
 * @alignment: Video frame's alignment information
 */
typedef struct {
  int32_t width;
  int32_t height;
  VvasVideoFormat fmt;
  uint32_t n_planes;
  size_t stride[VVAS_VIDEO_MAX_PLANES];
  size_t elevation[VVAS_VIDEO_MAX_PLANES];
  VvasVideoAlignment alignment;
} VvasVideoInfo;

/**
 * struct VvasVideoPlaneInfo - Structure contains information specific to a video frame plane
 * @data: Pointer to a video frame plane data
 * @size: Size of a video plane
 * @offset: Offset of the first valid data from the @data pointer
 * @stride: Stride of a video plane
 * @elevation: Elevation (in height direction) of a video plane
 */
typedef struct {
  uint8_t *data;
  size_t size;
  size_t offset;
  int32_t stride;
  int32_t elevation;
} VvasVideoPlaneInfo;

/**
 * struct VvasVideoFrameMapInfo - Structure contains information specific to a video frame after mapping operation
 * @nplanes: Number of planes in a video frame
 * @size: Video frame size
 * @width: Width of the mapped video frame
 * @height: Height of the mapped video frame
 * @fmt: Video frame color format
 * @alignment: Video frame's Alignment information
 * @planes: Array containing video plane specific information
 */
typedef struct {
  uint8_t nplanes;
  size_t size;
  int32_t width;
  int32_t height;
  VvasVideoFormat fmt;
  VvasVideoAlignment alignment;
  VvasVideoPlaneInfo planes[VVAS_VIDEO_MAX_PLANES];
} VvasVideoFrameMapInfo;

typedef void VvasVideoFrame;

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  typedef VvasVideoFrameDataFreeCB - Callback function to be called to free memory pointed by @data,
 *                                                                 when VvasMemory handle is getting freed using @vvas_video_frame_free() API.
 *  @data: Array of data pointers to video planes
 *  @user_data: User data pointer sent via @vvas_video_frame_alloc_from_data() API
 *
 *  Return: None
 */
typedef void (*VvasVideoFrameDataFreeCB)(void *data[VVAS_VIDEO_MAX_PLANES], void *user_data);

/**
 * vvas_video_frame_alloc () - Allocates memory based on VvasVideoInfo structure
 *
 * @vvas_ctx: Address of VvasContext handle created using vvas_context_create()
 * @alloc_type: Type of the memory need to be allocated
 * @alloc_flags: Allocation flags used to allocate video frame
 * @mbank_idx: Index of the memory bank on which memory need to be allocated
 * @vinfo: Address of VvasVideoInfo which contains video frame specific information
 * @ret: Address to store return value. In case of error, @ret is useful in understanding the root cause
 *
 * Return:
 * * On success, returns VvasVideoFrame handle and
 * * On failure, returns NULL
 */
VvasVideoFrame* vvas_video_frame_alloc (VvasContext *vvas_ctx,
                                        VvasAllocationType alloc_type,
                                        VvasAllocationFlags alloc_flags,
                                        uint8_t mbank_idx,
                                        VvasVideoInfo *vinfo,
                                        VvasReturnType *ret);

/**
 * vvas_video_frame_alloc_from_data() - Allocates memory based on data pointers provided by user
 *
 * @vvas_ctx: Address of VvasContext handle created using vvas_context_create()
 * @vinfo: Video information related a frame
 * @data: Array of data pointers to each plane
 * @free_cb: Pointer to callback function to be called when &struct VvasVideoFrame is freed
 * @user_data: User data to be passed to callback function @free_cb
 * @ret: Address to store return value. Upon case of error, @ret is useful in understanding the root cause
 *
 * Return:
 * * On success, returns &struct VvasVideoFrame handle and
 * * On failure, returns NULL
 */
VvasVideoFrame* vvas_video_frame_alloc_from_data (VvasContext *vvas_ctx,
                                        VvasVideoInfo *vinfo,
                                        void *data[VVAS_VIDEO_MAX_PLANES],
                                        VvasVideoFrameDataFreeCB free_cb,
                                        void *user_data,
                                        VvasReturnType *ret);

/**
 * vvas_video_frame_map () - Maps @vvas_vframe to user space using @map_flags. Based on &struct VvasMemory->sync_flags, data will be synchronized between host and the device.
 * @vvas_vframe: Address of &struct VvasVideoFrame
 * @map_flags: Flags used to map @vvas_vframe
 * @info: Structure which gets populated after mapping is successful
 *
 * Return: &struct VvasReturnType
 */
VvasReturnType vvas_video_frame_map (VvasVideoFrame* vvas_vframe,
                                     VvasDataMapFlags map_flags,
                                     VvasVideoFrameMapInfo *info);

/**
 * vvas_video_frame_unmap() - Unmaps @vvas_vframe which was mapped earlier
 * @vvas_vframe: Address of &struct VvasVideoFrame
 * @info: Pointer to information which was populated during vvas_video_frame_map() API
 *
 * Return: &struct VvasReturnType
 */
VvasReturnType vvas_video_frame_unmap (VvasVideoFrame* vvas_vframe, VvasVideoFrameMapInfo *info);

/**
 * vvas_video_frame_free () - Frees the video frame allocated during vvas_video_frame_alloc() API
 * @vvas_vframe: Address of &struct VvasVideoFrame
 *
 * Return: None
 */
void vvas_video_frame_free (VvasVideoFrame* vvas_vframe);

/**
 * vvas_video_frame_set_metadata() - Sets metadata on VvasVideoFrame
 * @vvas_mem: Address of &struct VvasVideoFrame
 * @meta_data: Address of &struct VvasMetadata to be set on @vvas_mem
 *
 * Return:  None
 */
void vvas_video_frame_set_metadata (VvasVideoFrame* vvas_mem, VvasMetadata *meta_data);

/**
 * vvas_video_frame_get_metadata () - Gets metadata on VvasVideoFrame
 * @vvas_mem: Address of &struct VvasVideoFrame
 * @meta_data: Address of &struct VvasMetadata to store metadata from @vvas_mem
 *
 * Return: None
 */
void vvas_video_frame_get_metadata (VvasVideoFrame* vvas_mem, VvasMetadata *meta_data);

/**
 * vvas_video_frame_get_videoinfo() - Gets video frame information from VvasVideoFrame
 * @vvas_mem: Address of &struct VvasVideoFrame
 * @vinfo: Video frame information of &struct VvasVideoInfo
 *
 * Return: None
 */
void vvas_video_frame_get_videoinfo(VvasVideoFrame* vvas_mem, VvasVideoInfo *vinfo);

#ifdef __cplusplus
}
#endif

#endif /* __VVAS_MEMORY_H__ */
