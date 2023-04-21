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

/**
 * DOC: VVAS Common APIs
 * This file contains common structures, enumerations and method declarations for VVAS core libraries.
 */

#ifndef __VVAS_COMMON_H__
#define __VVAS_COMMON_H__

#include <stdint.h>
#include <stddef.h>

/* Checks whether VvasReturnType is an error or not */
#define VVAS_IS_ERROR(ret) ((ret) < 0)

/* Default log level to be used if user is not setting */
#define DEFAULT_VVAS_LOG_LEVEL LOG_LEVEL_WARNING

/* Checks whether allocation type is valid or not */
#define ALLOC_TYPE_IS_VALID(alloc_type) (((alloc_type) == VVAS_ALLOC_TYPE_CMA) || ((alloc_type) == VVAS_ALLOC_TYPE_NON_CMA))

/**
 * enum VvasReturnType - Enum representing VVAS core APIs' return type
 * @VVAS_RET_ALLOC_ERROR: Memory allocation error
 * @VVAS_RET_INVALID_ARGS: Invalid arguments to APIs
 * @VVAS_RET_ERROR: Generic error
 * @VVAS_RET_SUCCESS: Success
 * @VVAS_RET_EOS: End of stream
 * @VVAS_RET_SEND_AGAIN: Call the API without changing arguments
 * @VVAS_RET_NEED_MOREDATA: Core APIs need more data to complete a specific operation
 * @VVAS_RET_CAPS_CHANGED: Capabilities changed
 *
 * Note: Negative number represents error and positive number is not an error
 */
typedef enum {
  VVAS_RET_ALLOC_ERROR = -3,
  VVAS_RET_INVALID_ARGS = -2,
  VVAS_RET_ERROR = -1,
  VVAS_RET_SUCCESS = 0,
  VVAS_RET_EOS = 1,
  VVAS_RET_SEND_AGAIN = 2,
  VVAS_RET_NEED_MOREDATA = 3,
  VVAS_RET_CAPS_CHANGED = 4,
} VvasReturnType;

/**
 * enum VvasDataSyncFlags - Flags to synchronize data between host and FPGA device
 * @VVAS_DATA_SYNC_NONE:  No DMA Synchronization required
 * @VVAS_DATA_SYNC_FROM_DEVICE: Synchronize data from device to host
 * @VVAS_DATA_SYNC_TO_DEVICE: Synchronize data from host to device
 *
 * Data will be synchronized from device to host or host to device via DMA operation.
 * Here host is an x86 machine and device is an FPGA device
 */
typedef enum {
  VVAS_DATA_SYNC_NONE = 0,
  VVAS_DATA_SYNC_FROM_DEVICE = 1 << 0,
  VVAS_DATA_SYNC_TO_DEVICE = 1 << 1,
} VvasDataSyncFlags;

/**
 * enum VvasDataMapFlags - Flags used while mapping memory
 * @VVAS_DATA_MAP_NONE: Default flag
 * @VVAS_DATA_MAP_READ: Map memory in read mode
 * @VVAS_DATA_MAP_WRITE: Map memory in write mode
 *
 * Memory flags used while mapping underlying memory to user space
 */
typedef enum {
  VVAS_DATA_MAP_NONE = 0,
  VVAS_DATA_MAP_READ = 1 << 0,
  VVAS_DATA_MAP_WRITE = 1 << 1,
} VvasDataMapFlags;

/**
 * enum VvasAllocationType - Enum representing VVAS allocation type
 * @VVAS_ALLOC_TYPE_UNKNOWN: Unknown allocation type
 * @VVAS_ALLOC_TYPE_CMA: Physically contiguous Memory will be allocated by backend drivers (i.e XRT)
 * @VVAS_ALLOC_TYPE_NON_CMA: Memory will be allocated using malloc API
 */
typedef enum {
  VVAS_ALLOC_TYPE_UNKNOWN,
  VVAS_ALLOC_TYPE_CMA,
  VVAS_ALLOC_TYPE_NON_CMA,
} VvasAllocationType;

/**
 * enum VvasAllocationFlags - Enum representing VVAS allocation flags. Can be extended in future.
 * @VVAS_ALLOC_FLAG_UNKNOWN:  Unknown allocation type
 * @VVAS_ALLOC_FLAG_NONE: To create memory both on FPGA device and host. This is the default option
 */
typedef enum {
  VVAS_ALLOC_FLAG_UNKNOWN = -1,
  VVAS_ALLOC_FLAG_NONE = 0,
} VvasAllocationFlags;

/**
 * struct VvasAllocationInfo - Structure to store information related memory allocation
 * @mbank_idx: Index of the Memory bank from which memory is allocated
 * @alloc_type: Memory allocation type &enum VvasAllocationType
 * @alloc_flags: Flags used to allocate memory &enum VvasAllocationFlags
 * @map_flags: Flags to indicate current mapping type &enum VvasDataMapFlags
 * @sync_flags: Flags which represents data synchronization requirement @enum VvasDataSyncFlags
 */
typedef struct {
  uint8_t mbank_idx;
  VvasAllocationType alloc_type;
  VvasAllocationFlags alloc_flags;
  VvasDataMapFlags map_flags;
  VvasDataSyncFlags sync_flags;
} VvasAllocationInfo;

/**
 * struct VvasMetadata - Structure to store frame metadata
 * @pts: Presentation timestamp
 * @dts: Decoding timestamp
 * @duration: Duration of the frame
 */
typedef struct {
  uint64_t pts;
  uint64_t dts;
  uint64_t duration;
} VvasMetadata;

/**
 * enum VvasFontType - Fonts supported by VVAS core
 * @VVAS_FONT_HERSHEY_SIMPLEX: Normal size sans-serif font
 * @VVAS_FONT_HERSHEY_PLAIN: Small size sans-serif font
 * @VVAS_FONT_HERSHEY_DUPLEX: Normal size sans-serif font (more complex than VVAS_FONT_HERSHEY_SIMPLEX)
 * @VVAS_FONT_HERSHEY_COMPLEX: Normal size serif font
 * @VVAS_FONT_HERSHEY_TRIPLEX: Normal size serif font (more complex than VVAS_FONT_HERSHEY_COMPLEX)
 * @VVAS_FONT_HERSHEY_COMPLEX_SMALL: Smaller version of VVAS_FONT_HERSHEY_COMPLEX
 */
typedef enum {
  VVAS_FONT_HERSHEY_SIMPLEX,
  VVAS_FONT_HERSHEY_PLAIN,
  VVAS_FONT_HERSHEY_DUPLEX,
  VVAS_FONT_HERSHEY_COMPLEX,
  VVAS_FONT_HERSHEY_TRIPLEX,
  VVAS_FONT_HERSHEY_COMPLEX_SMALL,
} VvasFontType;

#endif /*__VVAS_COMMON_H__*/
