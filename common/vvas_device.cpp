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

/* Update of this file by the user is not encouraged */
#include <assert.h>
#include <vvas_core/vvas_device.h>
#include <unistd.h>
#include <iostream>
#include <cstdarg>

#ifdef XLNX_PCIe_PLATFORM
#include "xclbin.h"
#include <experimental/xrt_xclbin.h>
#endif

#define ERROR_PRINT(...) {\
  do {\
    printf("[%s:%d] ERROR : ",__func__, __LINE__);\
    printf(__VA_ARGS__);\
    printf("\n");\
  } while(0);\
}

#undef DEBUG_XRT_UTILS

#ifdef DEBUG_XRT_UTILS
#define DEBUG_PRINT(...) {\
  do {\
    printf("[%s:%d] ",__func__, __LINE__);\
    printf(__VA_ARGS__);\
    printf("\n");\
  } while(0);\
}
#else
#define DEBUG_PRINT(...) ((void)0)
#endif

struct sk_device_info
{
  int pid;
  void *device_handle;
  uint32_t dev_index;
};

#define MAX_DEVICES   (32)
#define P2ROUNDUP(x, align) (-(-(x) & -(align)))
#define WAIT_TIMEOUT 1000       // 1sec
#ifdef XLNX_U30_PLATFORM
#define ERT_START_CMD_PAYLOAD_SIZE ((1024 * sizeof(unsigned int)) - 2)  // ERT cmd can carry max 4088 bytes
#endif
#define MEM_BANK 0

extern "C"
{

/* Kernel APIs */
int32_t
vvas_xrt_exec_buf (vvasDeviceHandle dev_handle, vvasKernelHandle kern_handle,
    vvasRunHandle * run_handle, const char *format, va_list args)
{
  int i = 0, arg_index = 0;
  char c;
  xrt::kernel * kernel = (xrt::kernel *) kern_handle;
  auto kernel_hand = *kernel;
  xrt::run * run = new xrt::run (kernel_hand);

  while ((c = format[i++]) != '\0') {
    try {
      switch (c) {
        case 'i': {
          int i_value;
          i_value = va_arg (args, int);
          run->set_arg (arg_index, i_value);
          arg_index++;
        }
        break;
        case 'u': {
          unsigned int u_value;
          u_value = va_arg (args, unsigned int);
          run->set_arg (arg_index, u_value);
          arg_index++;
        }
        break;
        case 'f': {
          float f_value;
          /* va_arg doesn't support float.
           * If float is passed it will be automatically promoted to double */
          f_value = (float) va_arg (args, double);
          run->set_arg (arg_index, f_value);
          arg_index++;
        }
        break;
        case 'F': {
          double d_value;
          d_value = va_arg (args, double);
          run->set_arg (arg_index, d_value);
          arg_index++;
        }
        break;
        case 'c': {
          char c_value;
          c_value = (char) va_arg (args, int);
          run->set_arg (arg_index, c_value);
          arg_index++;
        }
        break;
        case 'C': {
          unsigned char c_value;
          c_value = (unsigned char) va_arg (args, unsigned int);
          run->set_arg (arg_index, c_value);
          arg_index++;
        }
        break;
        case 'S': {
          short s_value;
          s_value = (short) va_arg (args, int);
          run->set_arg (arg_index, s_value);
          arg_index++;
        }
        break;
        case 'U': {
          unsigned short s_value;
          s_value = (unsigned short) va_arg (args, unsigned int);
          run->set_arg (arg_index, s_value);
          arg_index++;
        }
        break;
        case 'l': {
          unsigned long long l_value;
          l_value = va_arg (args, unsigned long long);
          run->set_arg (arg_index, l_value);
          arg_index++;
        }
        break;
        case 'd': {
          long long d_value;
          d_value = va_arg (args, long long);
          run->set_arg (arg_index, d_value);
          arg_index++;
        }
        break;
        case 'p': {
          void *p_value;
          p_value = va_arg (args, void *);
          if (p_value) {
            run->set_arg (arg_index, p_value);
          }
          arg_index++;
        }
        break;
        case 's': {
          arg_index++;
        }
        break;
        case 'b': {
          void *b_value = NULL;
          b_value = va_arg (args, void *);

          if (b_value) {
            run->set_arg (arg_index, *((xrt::bo *) b_value));
          }
          arg_index++;
        }
        break;
        default:
          ERROR_PRINT ("Wrong format specifier");
          delete run;
          return -1;
      }
    } catch (std::exception &ex) {
      ERROR_PRINT ("failed to set argument idx %d. reason : %s",
          arg_index, ex.what());
      return -1;
    };
  }

  try {
    run->start ();
  } catch (std::exception &ex) {
    ERROR_PRINT ("failed to start kernel. reason : %s", ex.what());
    return -1;
  };

  *run_handle = run;
  return 0;
}

int32_t
vvas_xrt_exec_wait (vvasDeviceHandle dev_handle, vvasRunHandle run_handle,
    int32_t timeout)
{
  xrt::run * handle = (xrt::run *) run_handle;
  int iret = 0;

  try {
    iret = handle->wait (timeout);
  } catch (std::exception &ex) {
    ERROR_PRINT ("failed to wait. reason : %s", ex.what());
    return ERT_CMD_STATE_ERROR;
  };

  return iret;
}

void
vvas_xrt_free_run_handle (vvasRunHandle run_handle)
{
  xrt::run * handle = (xrt::run *) run_handle;

  delete handle;
  run_handle = NULL;
  return;
}

/* BO Related APIs */
uint64_t
vvas_xrt_get_bo_phy_addres (vvasBOHandle bo)
{
  xrt::bo * bo_handle = (xrt::bo *) bo;

  return bo_handle->address ();
}

vvasBOHandle
vvas_xrt_import_bo (vvasDeviceHandle dev_handle, int32_t fd)
{
  xrt::device * device = (xrt::device *) dev_handle;
  xrt::bo *boh = NULL;

  try {
    boh = new xrt::bo (*device, fd);
  } catch (std::exception &ex) {
    ERROR_PRINT ("failed to import BO with fd %d. reason : %s", fd, ex.what());
    return NULL;
  };

  return (vvasBOHandle)boh;
}

int32_t
vvas_xrt_export_bo (vvasBOHandle bo)
{
  xrt::bo * bo_handle = (xrt::bo *) bo;
  int32_t fd;

  try {
    fd = bo_handle->export_buffer ();
  } catch (std::exception &ex) {
    ERROR_PRINT ("failed to export BO. reason : %s", ex.what());
    return -1;
  };

  return fd;
}

vvasBOHandle
vvas_xrt_alloc_bo (vvasDeviceHandle dev_handle, size_t size,
    vvas_bo_flags flags, uint32_t mem_bank)
{
  xrt::device * device = (xrt::device *) dev_handle;
  xrt::bo * bo_handle;

  try {
    bo_handle = new xrt::bo (*device, size, flags, mem_bank);
  } catch (std::exception &ex) {
    ERROR_PRINT ("failed to allocate BO of size %zu and flags %u. reason : %s",
        size, flags, ex.what());
    return NULL;
  };

  return (vvasBOHandle) bo_handle;
}

vvasBOHandle
vvas_xrt_create_sub_bo (vvasBOHandle parent, size_t size, size_t offset)
{
  xrt::bo * bo_handle = (xrt::bo *) parent;
  xrt::bo * sub_bo_handle;

  try {
    sub_bo_handle = new xrt::bo (*bo_handle, size, offset);
  } catch (std::exception &ex) {
    ERROR_PRINT ("failed to create sub BO of size %zu. reason : %s", size, ex.what());
    return NULL;
  };

  return sub_bo_handle;
}

void vvas_xrt_free_bo (vvasBOHandle bo)
{
  xrt::bo * bo_handle = (xrt::bo *) bo;

  if (bo_handle) {
    delete bo_handle;
    bo = NULL;
  }

  return;
}

int
vvas_xrt_unmap_bo (vvasBOHandle bo, void *addr)
{
  /* As of now there is no API in XRT Native for unmout. Just keeping
   * as a placeholder for any future API addition */
  return 0;
}

void *
vvas_xrt_map_bo (vvasBOHandle bo, bool write /* Not used as of now, as its always read and write */ )
{
  xrt::bo * bo_handle = (xrt::bo *) bo;
  void *data = NULL;

  try {
    data = bo_handle->map ();
  } catch (std::exception &ex) {
    ERROR_PRINT ("failed to map BO. reason : %s", ex.what());
    return NULL;
  };

  return data;
}

int32_t
vvas_xrt_write_bo (vvasBOHandle bo, const void *src, size_t size, size_t seek)
{
  xrt::bo * bo_handle = (xrt::bo *) bo;

  try {
    bo_handle->write (src, size, seek);
  } catch (std::exception &ex) {
    ERROR_PRINT ("failed to write BO of size %zu. reason : %s", size, ex.what());
    return -1;
  };

  return 0;
}

int32_t
vvas_xrt_sync_bo (vvasBOHandle bo, vvas_bo_sync_direction dir, size_t size,
    size_t offset)
{
  xrt::bo * bo_handle = (xrt::bo *) bo;

  try {
    bo_handle->sync ((xclBOSyncDirection) dir, size, offset);
  } catch (std::exception &ex) {
    ERROR_PRINT ("failed to sync BO of size %zu in direction %s. reason : %s",
        size, dir == VVAS_BO_SYNC_BO_TO_DEVICE ? "to-device" :
            dir == VVAS_BO_SYNC_BO_FROM_DEVICE ? "from-device" : "unknown",
                ex.what());
    return -1;
  };

  return 0;
}

int32_t
vvas_xrt_read_bo (vvasBOHandle bo, void *dst, size_t size, size_t skip)
{
  xrt::bo * bo_handle = (xrt::bo *) bo;

  try {
    bo_handle->read (dst, size, skip);
  } catch (std::exception &ex) {
    ERROR_PRINT ("failed to read BO of size %zu. reason : %s", size, ex.what());
    return -1;
  };
  return 0;
}

/* Device APIs */
int32_t
vvas_xrt_close_context (vvasKernelHandle kern_handle)
{
  xrt::kernel * kernel = (xrt::kernel *) kern_handle;

  delete kernel;
  kern_handle = NULL;
  return 0;
}

int32_t
vvas_xrt_open_context (vvasDeviceHandle handle, uuid_t xclbinId,
    vvasKernelHandle * kernelHandle, const char *kernel_name, bool shared)
{
  xrt::device * device = (xrt::device *) handle;
  xrt::kernel::cu_access_mode mode;
  xrt::kernel *kern_handle = NULL;

  if (!handle) {
    ERROR_PRINT ("Device handle is NULL");
    return -1;
  }

  if (shared) {
    mode = xrt::kernel::cu_access_mode::shared;
  } else {
    mode = xrt::kernel::cu_access_mode::exclusive;
  }

  try {
    kern_handle = new xrt::kernel (*device, xclbinId, kernel_name, mode);
  } catch (std::exception &ex) {
    ERROR_PRINT ("failed to open context for kernel name %s, shared %u. reason : %s",
        kernel_name, shared, ex.what());
    return -1;
  };

  *kernelHandle = kern_handle;
  return 0;
}

void
vvas_xrt_close_device (vvasDeviceHandle dev_handle)
{
  xrt::device * device = (xrt::device *) dev_handle;

  delete device;
  dev_handle = NULL;
  return;
}

int32_t
vvas_xrt_open_device (int32_t dev_idx, vvasDeviceHandle * dev_handle)
{
  xrt::device *device = NULL;

  try {
    device = new xrt::device (dev_idx);
  } catch (std::exception &ex) {
    ERROR_PRINT ("failed to open device with idx %d. reason : %s",
        dev_idx, ex.what());
    return 0;
  };

  *dev_handle = device;
  return 1;
}

void
vvas_xrt_write_reg (vvasKernelHandle kern_handle, uint32_t offset, uint32_t data)
{
  xrt::kernel * kernel = (xrt::kernel *) kern_handle;

  try {
    kernel->write_register (offset, data);
  } catch (std::exception &ex) {
    ERROR_PRINT ("failed to write to register at offset %u. reason : %s",
        offset, ex.what());
  };

  return;
}

void
vvas_xrt_read_reg (vvasKernelHandle kern_handle, uint32_t offset, uint32_t * data)
{
  xrt::kernel * kernel = (xrt::kernel *) kern_handle;

  try {
    *data = kernel->read_register (offset);
  } catch (std::exception &ex) {
    ERROR_PRINT ("failed to read to register at offset %u. reason : %s",
        offset, ex.what());
  };

  return;
}

int
vvas_xrt_alloc_xrt_buffer (vvasDeviceHandle dev_handle, unsigned int size,
    vvas_bo_flags bo_flags, unsigned int mem_bank, xrt_buffer * buffer)
{
  xrt::device * device = (xrt::device *) dev_handle;

  auto bo_handle = new xrt::bo (*device, size, bo_flags, mem_bank);

  buffer->user_ptr = bo_handle->map ();
  buffer->bo = bo_handle;
  buffer->size = size;
  buffer->phy_addr = bo_handle->address ();

  return 0;
}

void
vvas_xrt_free_xrt_buffer (xrt_buffer * buffer)
{
  xrt::bo * bo_handle = (xrt::bo *) buffer->bo;
  delete bo_handle;
}

int
vvas_xrt_download_xclbin (const char *bit, vvasDeviceHandle handle,
    uuid_t * xclbinId)
{
  xrt::device * device = (xrt::device *) handle;
  xrt::uuid uuid;
  int ret = 0;

  if (access (bit, F_OK) == 0) {
    std::string xclbin_fnm (bit);
    xrt::xclbin * xclbin = new xrt::xclbin (xclbin_fnm);

    try {
      uuid = device->load_xclbin (*xclbin);
    } catch (std::exception &ex) {
      ERROR_PRINT ("failed to load xclbin at location %s. reason : %s", bit, ex.what());
      delete xclbin;
      return -1;
    };

    uuid_copy (*xclbinId, uuid.get ());
    delete xclbin;
  } else {
    ERROR_PRINT ("Xclbin file is not available in the location : %s", bit);
    ret = -1;
  }

  return ret;
}

int
vvas_xrt_get_xclbin_uuid (vvasDeviceHandle handle, uuid_t * xclbinId)
{
  xrt::device * device = (xrt::device *) handle;
  xrt::uuid uuid;

  try {
    uuid = device->get_xclbin_uuid ();
  } catch (std::exception &ex) {
    ERROR_PRINT ("failed to get xclbin uuid. reason : %s", ex.what());
    return -1;
  };

  uuid_copy (*xclbinId, uuid.get ());
  return 0;
}

#ifdef XLNX_U30_PLATFORM
void
vvas_free_ert_xcl_xrt_buffer (xclDeviceHandle handle, xrt_buffer * buffer)
{
  xclBufferHandle bh = 0;
  if (buffer->bo)
    bh = *((xclBufferHandle *) (buffer->bo));

  if (buffer->user_ptr && buffer->size)
    xclUnmapBO (handle, bh, buffer->user_ptr);
  if (handle && bh > 0)
    xclFreeBO (handle, bh);

  if (buffer->bo)
    free (buffer->bo);
  memset (buffer, 0x00, sizeof (xrt_buffer));
}

int32_t
vvas_softkernel_xrt_open_device (int32_t dev_idx,
    xclDeviceHandle xcl_dev_hdl, vvasDeviceHandle * dev_handle)
{
  xrt::device * device = NULL;

  try {
    device = new xrt::device  {xcl_dev_hdl};
  } catch (std::exception &ex) {
    ERROR_PRINT ("failed to open device. reason : %s", ex.what());
    return 0;
  };

  *dev_handle = device;
  return 1;
}

/* TODO: vvas_xrt_send_softkernel_command() will be updated in 2022.1 XRT */
int
vvas_xrt_send_softkernel_command (xclDeviceHandle handle,
    xrt_buffer * sk_ert_buf, unsigned int *payload, unsigned int num_idx,
    unsigned int cu_mask, int timeout)
{
  struct ert_start_kernel_cmd *ert_cmd =
      (struct ert_start_kernel_cmd *) (sk_ert_buf->user_ptr);
  int ret = 0;
  int retry_cnt = 0;

  if (NULL == sk_ert_buf || NULL == payload ||
      (num_idx * sizeof (unsigned int)) > ERT_START_CMD_PAYLOAD_SIZE
      || !num_idx) {
      //ut<<"Invalid argument";
      //     //cout<<     ("invalid arguments. sk_buf = %p, payload = %p and num idx = %d",
      //          //     sk_ert_buf, payload, num_idx);
    return -1;
  }

  ert_cmd->state = ERT_CMD_STATE_NEW;
  ert_cmd->opcode = ERT_SK_START;

  ert_cmd->extra_cu_masks = 3;

  if (cu_mask > 31) {
    ert_cmd->cu_mask = 0;
    if (cu_mask > 63) {
      ert_cmd->data[0] = 0;
      if (cu_mask > 96) {
        ert_cmd->data[1] = 0;
        ert_cmd->data[2] = (1 << (cu_mask - 96));
      } else {
        ert_cmd->data[1] = (1 << (cu_mask - 64));
        ert_cmd->data[2] = 0;
      }
    } else {
      ert_cmd->data[0] = (1 << (cu_mask - 32));
    }
  } else {
    ert_cmd->cu_mask = (1 << cu_mask);
    ert_cmd->data[0] = 0;
    ert_cmd->data[1] = 0;
    ert_cmd->data[2] = 0;
  }

  ert_cmd->count = num_idx + 4;
  memcpy (&ert_cmd->data[3], payload, num_idx * sizeof (unsigned int));

  xclBufferHandle *hdl = (xclBufferHandle *) (sk_ert_buf->bo);
  ret = xclExecBuf (handle, *hdl);
  if (ret < 0) {
    ERROR_PRINT ("[handle %p & bo %p] ExecBuf failed with ret = %d. reason : %s",
          handle, sk_ert_buf->bo, ret, strerror (errno));
    return ret;
  }
  do {
    ret = xclExecWait (handle, timeout);
    if (ret < 0) {
      ERROR_PRINT ("ExecWait ret = %d. reason : %s", ret, strerror (errno));
      return ret;
    } else if (!ret) {
      if (retry_cnt++ >= 10) {
        ERROR_PRINT ("[handle %p] ExecWait ret = %d. reason : %s", handle,
            ret, strerror (errno));
        return -1;
      }
      ERROR_PRINT ("[handle %p & bo %p] timeout...retry execwait\n", handle,
          sk_ert_buf->bo);
    }
  } while (ert_cmd->state != ERT_CMD_STATE_COMPLETED);

  return 0;
}
#else /* V70 */
int
vvas_xrt_send_softkernel_command (vvasDeviceHandle dev_handle,
    vvasKernelHandle kern_handle, xrt_buffer * sk_ert_buf,
    unsigned int *payload, unsigned int num_idx, int timeout)
{
  xrt::kernel * kernel = (xrt::kernel *) kern_handle;
  auto kernel_handle = *kernel;
  int ret = 0;
  int retry_count = MAX_EXEC_WAIT_RETRY_CNT;

  if (NULL == sk_ert_buf || NULL == payload || !num_idx) {
    return -1;
  }

  memset (sk_ert_buf->user_ptr, 0, 4096);
  memcpy (sk_ert_buf->user_ptr, payload, num_idx * sizeof (unsigned int));
  ret = vvas_xrt_sync_bo (sk_ert_buf->bo, VVAS_BO_SYNC_BO_TO_DEVICE,
      num_idx * sizeof (unsigned int), 0);
  if (ret != 0) {
    ERROR_PRINT ("failed to sync command buffer. reason : %d, %s",
        ret, strerror (errno));
    return -1;
  }

  xrt::bo * cmd_bo = (xrt::bo *) (sk_ert_buf->bo);
  auto run_hdl = kernel_handle (*(cmd_bo));

  ret = run_hdl.wait (timeout);
  if (ret < 0) {
    ERROR_PRINT ("[bo %p] XRT run handle failed with ret = %d. reason : %s",
        sk_ert_buf->bo, ret, strerror (errno));
    return ret;
  }
  do {
    ret = vvas_xrt_exec_wait (dev_handle, &run_hdl, timeout);
    /* Lets try for MAX count unless there is a error or completion */
    if (ret == ERT_CMD_STATE_TIMEOUT) {
      if (retry_count-- <= 0) {
        ERROR_PRINT ("Max retry count %d reached..returning error",
            MAX_EXEC_WAIT_RETRY_CNT);
        return -1;
      }
    } else if (ret == ERT_CMD_STATE_ERROR) {
      ERROR_PRINT ("ExecWait ret = %d", ret);
      return -1;
    }
  } while (ret != ERT_CMD_STATE_COMPLETED);
  return 0;
}
#endif

#ifdef XLNX_PCIe_PLATFORM
size_t
vvas_xrt_get_num_compute_units (const char *xclbin_filename)
{
  std::string xclbin_fnm (xclbin_filename);
  xrt::xclbin * xclbin = new xrt::xclbin (xclbin_fnm);
  size_t num_cus = 0;

  try {
    auto kernels = xclbin->get_kernels ();

    for (auto & it:kernels) {
      num_cus += it.get_cus ().size ();
    }
  } catch (std::exception &ex) {
    ERROR_PRINT ("failed to get number compute units from xclbin path %s. "
        "reason : %s", xclbin_filename, ex.what());
    return 0;
  };

  delete xclbin;
  return num_cus;
}

size_t
vvas_xrt_get_num_kernels (const char *xclbin_filename)
{
  size_t num_kernels;
  std::string xclbin_fnm (xclbin_filename);
  xrt::xclbin * xclbin = new xrt::xclbin (xclbin_fnm);

  num_kernels = xclbin->get_kernels ().size ();
  delete xclbin;
  return num_kernels;
}
#endif
}                               /* End of extern C */
