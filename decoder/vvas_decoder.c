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

/** @file vvas_decoder.h
 *  @brief Contains VVAS core decoder APIs for decoding elementary streams
 */

#include "stdint.h"
#include "stdlib.h"
#include "string.h"
#include <vvas_core/vvas_video.h>
#include <vvas_core/vvas_video_priv.h>
#include <vvas_core/vvas_context.h>
#include <vvas_core/vvas_common.h>
#include <vvas_core/vvas_memory.h>
#include <vvas_core/vvas_log.h>
#include <vvas_utils/vvas_utils.h>
#include <vvas_core/vvas_decoder.h>
#include <vvas_decoder_priv.h>

#define MAX_OUT_BUF_NUM 149

#define WIDTH_ALIGN   256
#define HEIGHT_ALIGN   64

/** CMD intraction is on legacy xcl based such that driver can be verified
 * on the U30 card. We need to make changes to this APIs to make use of the
 * native XRT interface latter.
 */

/** @fn static void populate_obuf_db_fn(void *data, void* udata)
 *
 *  @param[in] data - list element data
 *  @param[in] udata - User data
 *
 *  @return On Success returns TRUE \n
 *          On Failure returns FALSE
 *
 *  @brief Internal function called for each list element to add new or update
 *  existing entry into the obuf_db
 */
static void populate_obuf_db_fn(void *data, void* udata){
  VvasDecoderPrivate *self = (VvasDecoderPrivate *)udata;
  uint64_t paddr;
  uint32_t *pidx;
  uintptr_t  idx;

  paddr = vvas_video_frame_get_frame_paddr((VvasVideoFrame *)data);

  /* Find if already exists in the db */
  pidx = vvas_hash_table_lookup(self->oidx_hash, (void *)paddr);
  if(!pidx) {
    idx = vvas_hash_table_size(self->oidx_hash);
    if (idx >= FRM_BUF_POOL_SIZE) {
      self->vf_max_error = true;
      return;
    }

    self->obuf_db[idx].vframe = data;
    self->obuf_db[idx].is_free = TRUE;
    self->obuf_db[idx].paddr = paddr;
    LOGD(self, "[%02lu] vframe = %p, paddr = 0x%lx", idx,
      self->obuf_db[idx].vframe, self->obuf_db[idx].paddr);
    self->obuf_db[idx].size
        = vvas_video_frame_get_size((VvasVideoFrame *)data);
    vvas_hash_table_insert(self->oidx_hash, (void *)paddr, (void *)(idx+1));
  } else {
    /* a new vframe might be having same old phy address */
    uint32_t i = ((uintptr_t)pidx) - 1;
    self->obuf_db[i].vframe = data;
  }
}

/** @fn static bool allocate_in_buffers (VvasDecoderPrivate *pinst)
 *
 *  @param[in] pinst - Pointer to Decoder instance
 *
 *  @return On Success returns TRUE \n
 *          On Failure returns FALSE
 *
 *  @brief Internal function to allocate input buffers
 */
static bool allocate_in_buffers (VvasDecoderPrivate *pinst){
  int32_t iret = 0, i = 0;

  LOGI(pinst, "Inside allocate_in_buffers");
  for (i = 0; i < MAX_IBUFFS; i++) {
    /* allocate input buffer */
    iret = vvas_xrt_alloc_xrt_buffer (pinst->hskd,
        pinst->max_ibuf_size, VVAS_BO_FLAGS_NONE,
        pinst->in_mem_bank, pinst->xrt_bufs_in[i]);
    if (iret < 0) {
      LOGE(pinst, "failed to allocate input buffer..");
      goto error_in;
    }
  }

  return TRUE;

error_in:
  for(i=0; i<MAX_IBUFFS; i++) {
    if(pinst->xrt_bufs_in[i]) {
      vvas_xrt_free_xrt_buffer(pinst->xrt_bufs_in[i]);
    }
  }

  return FALSE;
}

/** @fn static bool allocate_out_buffers (VvasDecoderPrivate *pinst)
 *
 *  @param[in] pinst - Pointer to Decoder instance
 *
 *  @return On Success returns TRUE \n
 *          On Failure returns FALSE
 *
 *  @brief Internal function to allocate out buffers
 */
static bool allocate_out_buffers (VvasDecoderPrivate *pinst){
  uint64_t *out_bufs_addr;
  int32_t iret;
  uint32_t sz = 0, i;
  #ifdef HDR_DATA_SUPPORT
  uint64_t *hdr_bufs_addr;
  #endif

  if (pinst->state < VVAS_DEC_STATE_CONFIGURED) {
    LOGE(pinst, "Decoder is not yet configure");
    return FALSE;
  }

  sz = vvas_hash_table_size(pinst->oidx_hash);

  if (sz < pinst->ocfg->min_out_buf) {
    LOGE(pinst, "Entries(%d) in oidx_hash is smaller than min required(%d)",
      sz, pinst->ocfg->min_out_buf);
  }

  pinst->dec_out_bufs_handle = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (pinst->dec_out_bufs_handle == NULL) {
    LOGE(pinst, "Failed to allocate decoder output buffers structure");
    return FALSE;
  }

  LOGD(pinst, "Before dec_out_bufs_handle alloc");
  iret = vvas_xrt_alloc_xrt_buffer (pinst->hskd,
       sz * sizeof (uint64_t), VVAS_BO_FLAGS_NONE,
      pinst->out_mem_bank, pinst->dec_out_bufs_handle);
  if (iret < 0) {
    LOGE(pinst, "Failed to allocate decoder out buffers handle..");
    goto error;
  }

  out_bufs_addr = (uint64_t *) (pinst->dec_out_bufs_handle->user_ptr);

  #ifdef HDR_DATA_SUPPORT
  pinst->hdr_out_bufs_handle = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (pinst->hdr_out_bufs_handle == NULL) {
    LOGE(pinst, "Failed to allocate HDR output buffers structure");
    goto error;
  }

  iret = vvas_xrt_alloc_xrt_buffer (pinst->hskd,
      sz * sizeof (uint64_t), VVAS_BO_FLAGS_NONE,
      pinst->out_mem_bank, pinst->hdr_out_bufs_handle);
  if (iret < 0) {
    LOGE(pinst, "Failed to allocate HDR out buffers handle..");
    goto error;
  }

  hdr_bufs_addr = (uint64_t *) (pinst->hdr_out_bufs_handle->user_ptr);

  if (pinst->hdr_out_bufs_arr)
    free (pinst->hdr_out_bufs_arr);

  pinst->hdr_out_bufs_arr = (xrt_buffer *) calloc (FRM_BUF_POOL_SIZE,
                                                   sizeof (xrt_buffer));
  if (!pinst->hdr_out_bufs_arr) {
    LOGE(pinst, "Failed to allocate memory for HDR buffers array");
    goto error;
  }
  #endif

  for (i=0; i<sz; i++){
    out_bufs_addr[i] = pinst->obuf_db[i].paddr;
    LOGD(pinst, "%2d : out_bufs_addr[%02d] = 0x%lx", i, i, out_bufs_addr[i]);
    #ifdef HDR_DATA_SUPPORT
    /* allocate HDR buffers */
    iret = vvas_xrt_alloc_xrt_buffer (pinst->hskd,
        sizeof (video_hdr_data), VVAS_BO_FLAGS_NONE,
        pinst->out_mem_bank, &pinst->hdr_out_bufs_arr[i]);
    if (iret < 0) {
      LOGE(pinst, "Failed to allocate HDR buffer at index %d", i);
      goto error;
    }
    hdr_bufs_addr[i] = pinst->hdr_out_bufs_arr[i].phy_addr;
    #endif
  }

  LOGD(pinst, "Before syncbo dec_out_bufs_handle");
  iret = vvas_xrt_sync_bo (pinst->dec_out_bufs_handle->bo,
      VVAS_BO_SYNC_BO_TO_DEVICE, pinst->dec_out_bufs_handle->size, 0);
  if (iret != 0) {
    LOGE(pinst, "dec out buf synbo failed - %d, reason : %s", iret,
      strerror (errno));
    goto error;
  }

  #ifdef HDR_DATA_SUPPORT
  iret = vvas_xrt_sync_bo (pinst->hdr_out_bufs_handle->bo,
      VVAS_BO_SYNC_BO_TO_DEVICE, pinst->hdr_out_bufs_handle->size, 0);
  if (iret != 0) {
    LOGE(pinst, "HDR out buf synbo failed - %d, reason : %s", iret,
      strerror (errno));
    goto error;
  }
  #endif

  return TRUE;

error:
  #ifdef HDR_DATA_SUPPORT
  if(pinst->hdr_out_bufs_handle) {
    vvas_xrt_free_xrt_buffer(pinst->hdr_out_bufs_handle);
    free(pinst->hdr_out_bufs_handle);
    pinst->hdr_out_bufs_handle = NULL;
  }
  #endif

  if(pinst->dec_out_bufs_handle) {
    vvas_xrt_free_xrt_buffer(pinst->dec_out_bufs_handle);
    free(pinst->dec_out_bufs_handle);
    pinst->dec_out_bufs_handle = NULL;
  }

  #ifdef HDR_DATA_SUPPORT
  if(pinst->hdr_out_bufs_arr){
    for (i=0; i<sz; i++) {
      vvas_xrt_free_xrt_buffer(&pinst->hdr_out_bufs_arr[i]);
    }

    free(pinst->hdr_out_bufs_arr);
    pinst->hdr_out_bufs_arr = NULL;
  }
  #endif

  return FALSE;
}

/** @fn static bool destroy_in_buffers (VvasDecoderPrivate  *pinst)
 *
 *  @param[in] pinst - Pointer to Decoder instance
 *
 *  @return On Success returns TRUE \n
 *          On Failure returns FALSE
 *
 *  @brief Internal function to de-allocate input buffers
 */
static bool destroy_in_buffers (VvasDecoderPrivate  *pinst){
  uint32_t i = 0;

  for(i=0; i<MAX_IBUFFS; i++) {
    if(pinst->xrt_bufs_in[i]) {
      vvas_xrt_free_xrt_buffer(pinst->xrt_bufs_in[i]);
    }
  }

  return TRUE;
}

/** @fn static bool destroy_out_buffers (VvasDecoderPrivate  *pinst)
 *
 *  @param[in] pinst - Pointer to Decoder instance
 *
 *  @return On Success returns TRUE \n
 *          On Failure returns FALSE
 *
 *  @brief Internal function to de-allocate out buffers
 */
static bool destroy_out_buffers (VvasDecoderPrivate  *pinst){
  #ifdef HDR_DATA_SUPPORT
  uint32_t i = 0;
  uint32_t sz = vvas_hash_table_size(pinst->oidx_hash);
  if(pinst->hdr_out_bufs_handle) {
    vvas_xrt_free_xrt_buffer(pinst->hdr_out_bufs_handle);
    free(pinst->hdr_out_bufs_handle);
    pinst->hdr_out_bufs_handle = NULL;
  }
  #endif

  if(pinst->dec_out_bufs_handle) {
    vvas_xrt_free_xrt_buffer(pinst->dec_out_bufs_handle);
    free(pinst->dec_out_bufs_handle);
    pinst->dec_out_bufs_handle = NULL;
  }

  #ifdef HDR_DATA_SUPPORT
  if(pinst->hdr_out_bufs_arr){

    for(i=0; i<sz; i++) {
      vvas_xrt_free_xrt_buffer(&pinst->hdr_out_bufs_arr[i]);
    }

    free(pinst->hdr_out_bufs_arr);
    pinst->hdr_out_bufs_arr = NULL;
  }
  #endif

  return TRUE;
}

/** @fn static bool allocate_internal_buffers (VvasDecoderPrivate *pinst)
 *
 *  @param[in] pinst - Pointer to Decoder instance
 *
 *  @return On Success returns TRUE \n
 *          On Failure returns FALSE
 *
 *  @brief Internal function to allocate the buffers needed for VCU_PREINIT
 */
static bool allocate_internal_buffers (VvasDecoderPrivate *pinst) {
  int32_t i, iret;

  /* Allocate the memory for BO container */
  pinst->cmd_buf = (xrt_buffer *) calloc(1, sizeof(xrt_buffer));
  if (pinst->cmd_buf == NULL) {
    LOGE(pinst, "Failed to allocate cmd_buf memory");
    return FALSE;
  }

  pinst->sk_payload_buf = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (pinst->sk_payload_buf == NULL) {
    LOGE(pinst, "Failed to allocate sk_payload_buf memory");
    goto error;
  }

  pinst->dec_cfg_buf = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (pinst-> dec_cfg_buf == NULL) {
    LOGE(pinst, "Failed to allocate dec_cfg_buf memory");
    goto error;
  }

  for (i = 0; i < MAX_IBUFFS; i++) {
    pinst->xrt_bufs_in[i] = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
    if(!pinst->xrt_bufs_in[i]) {
      LOGE(pinst, "Failed to allocate xrt_bufs_in[%d] memory", i);
      goto error;
    }
  }

  iret =
    vvas_xrt_alloc_xrt_buffer (pinst->hskd,
                              ERT_CMD_SIZE,
                              VVAS_BO_FLAGS_NONE,
                              pinst->in_mem_bank, pinst->cmd_buf);
  if (iret < 0) {
    LOGE(pinst, "failed to allocate ert command buffer ");
    goto error;
  }

  /* Allocate softkernel payload buffer */
  iret = vvas_xrt_alloc_xrt_buffer (pinst->hskd,
      sizeof (sk_payload_data), VVAS_BO_FLAGS_NONE, pinst->in_mem_bank,
      pinst->sk_payload_buf);
  if (iret < 0) {
    LOGE(pinst, "Failed to allocate softkernel payload buffer ");
    goto error;
  }

  /* Allocate decoder config buffer */
  iret = vvas_xrt_alloc_xrt_buffer (pinst->hskd,
      sizeof (dec_params_t), VVAS_BO_FLAGS_NONE, pinst->in_mem_bank,
      pinst->dec_cfg_buf);
  if (iret < 0) {
    LOGE(pinst, "Failed to allocate decoder config buffer");
    goto error;
  }

  return TRUE;

error:
  if (pinst->dec_cfg_buf) {
    vvas_xrt_free_xrt_buffer(pinst->dec_cfg_buf);
    free(pinst->dec_cfg_buf);
    pinst->dec_cfg_buf = NULL;
  }

  if (pinst->cmd_buf) {
    vvas_xrt_free_xrt_buffer(pinst->cmd_buf);
    free(pinst->cmd_buf);
    pinst->cmd_buf = NULL;
  }

  for (i = 0; i < MAX_IBUFFS; i++) {
    if (pinst->xrt_bufs_in[i]) {
      free(pinst->xrt_bufs_in[i]);
      pinst->xrt_bufs_in[i] = NULL;
    }
  }

  return FALSE;
}

/** @fn destroy_internal_buffers (VvasDecoderPrivate *pinst)
 *
 *  @param[in] pinst - Pointer to Decoder instance
 *
 *  @return On Success returns TRUE \n
 *          On Failure returns FALSE
 *
 *  @brief Internal function to de-allocate internal buffers created
 */
static bool destroy_internal_buffers (VvasDecoderPrivate *pinst) {
  uint32_t i;

  if (pinst->sk_payload_buf) {
    vvas_xrt_free_xrt_buffer(pinst->sk_payload_buf);
    free(pinst->sk_payload_buf);
    pinst->sk_payload_buf = NULL;
  }

  if (pinst->dec_cfg_buf) {
    vvas_xrt_free_xrt_buffer(pinst->dec_cfg_buf);
    free(pinst->dec_cfg_buf);
    pinst->dec_cfg_buf = NULL;
  }

  if (pinst->cmd_buf) {
    vvas_xrt_free_xrt_buffer(pinst->cmd_buf);
    free(pinst->cmd_buf);
    pinst->cmd_buf = NULL;
  }

  for (i = 0; i < MAX_IBUFFS; i++) {
    if (pinst->xrt_bufs_in[i]) {
      free(pinst->xrt_bufs_in[i]);
      pinst->xrt_bufs_in[i] = NULL;
    }
  }

  return TRUE;
}

/** @fn static bool send_command (VvasDecoderPrivate *pinst,
 *                                VcuCmdType cmd_id,
 *                                void *data)
 *  @param[in] pinst - Decoder Instance pointer
 *  @param[in] cmd_id - VCU Command enum to be sent
 *  @param[in] data   - context data pointer if need for a command
 *
 *  @return On Success returns TRUE \n
 *          On Failure returns FALSE
 *
 *  @brief Internal API to compose and send command to VCU kernel
 */
static bool send_command(VvasDecoderPrivate *pinst, VcuCmdType cmd_id,
              void *data) {
  sk_payload_data *payload_buf;
  unsigned int payload_data[ERT_CMD_DATA_LEN] = {0};
  struct timespec timenow;
  uint64_t timestamp;
  uint32_t i, num_idx = 0;
  int32_t ret = 0;

  #define CMD_EXEC_TIMEOUT 1000

  if ((cmd_id < VCU_PREINIT) || (cmd_id > VCU_DEINIT)){
    LOGE(pinst, "Invalid CMD ID(%d), Valid Range is [%d, %d]", cmd_id,
      VCU_PREINIT, VCU_DEINIT);
    return FALSE;
  }

  /* Initialize payload buf */
  payload_buf = (sk_payload_data *) (pinst->sk_payload_buf->user_ptr);
  memset (payload_buf, 0, pinst->sk_payload_buf->size);

  payload_buf->cmd_id = cmd_id;

  if (cmd_id == VCU_INIT) {
    payload_buf->obuff_num = vvas_hash_table_size(pinst->oidx_hash);
  } else if (cmd_id == VCU_PUSH) {
    payload_buf->ibuff_valid_size = pinst->ibuff_param.insize;
    payload_buf->ibuff_meta.pts = pinst->ibuff_param.meta.pts;
    payload_buf->host_to_dev_ibuf_idx = pinst->host_to_dev_ibuf_idx;
  }

  clock_gettime (CLOCK_MONOTONIC, &timenow);
  timestamp = ((timenow.tv_sec * 1e6) + (timenow.tv_nsec/ 1e3));

  /* Set the XRT/ERT Command buffer */
  payload_data[num_idx++] = 0;
  payload_data[num_idx++] = cmd_id;
  payload_data[num_idx++] = getpid ();
  payload_data[num_idx++] = timestamp & 0xFFFFFFFF;
  payload_data[num_idx++] = (timestamp >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = pinst->sk_payload_buf->phy_addr & 0xFFFFFFFF;
  payload_data[num_idx++] =
      ((uint64_t) (pinst->sk_payload_buf->phy_addr) >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = sizeof (sk_payload_data);

  if (cmd_id == VCU_PREINIT) {
    payload_data[num_idx++] = pinst->dec_cfg_buf->phy_addr & 0xFFFFFFFF;
    payload_data[num_idx++] =
        ((uint64_t) (pinst->dec_cfg_buf->phy_addr) >> 32) & 0xFFFFFFFF;
    payload_data[num_idx++] = pinst->dec_cfg_buf->size;
      ret = vvas_xrt_sync_bo(pinst->dec_cfg_buf->bo, VVAS_BO_SYNC_BO_TO_DEVICE,
              pinst->dec_cfg_buf->size, 0);
    if ( ret != 0 ) {
      LOGE(pinst, "syncbo to device failed for dec_cfg_buf - ret(%d),"
        "reason : %s", ret,  strerror(errno));
        return FALSE;
    }
  } else if (cmd_id == VCU_INIT) {
    for (i = 0; i < MAX_IBUFFS; i++) {
      payload_data[num_idx++] = pinst->xrt_bufs_in[i]->phy_addr & 0xFFFFFFFF;
      payload_data[num_idx++] =
          ((uint64_t) (pinst->xrt_bufs_in[i]->phy_addr) >> 32) & 0xFFFFFFFF;
      payload_data[num_idx++] = pinst->xrt_bufs_in[i]->size;
    }
    payload_data[num_idx++] =
      pinst->dec_out_bufs_handle->phy_addr & 0xFFFFFFFF;
    payload_data[num_idx++] =
      ((uint64_t) (pinst->dec_out_bufs_handle->phy_addr) >> 32) & 0xFFFFFFFF;
    payload_data[num_idx++] = pinst->dec_out_bufs_handle->size;

    #ifdef HDR_DATA_SUPPORT
    payload_data[num_idx++] =
      pinst->hdr_out_bufs_handle->phy_addr & 0xFFFFFFFF;
    payload_data[num_idx++] =
      ((uint64_t) (pinst->hdr_out_bufs_handle->phy_addr) >> 32) & 0xFFFFFFFF;
    payload_data[num_idx++] = pinst->hdr_out_bufs_handle->size;
    #endif
  } else if (cmd_id == VCU_PUSH) {
    payload_data[num_idx++] =
      pinst->xrt_bufs_in[pinst->host_to_dev_ibuf_idx]->phy_addr & 0xFFFFFFFF;
    payload_data[num_idx++] =
      ((uint64_t) (pinst->xrt_bufs_in[pinst->host_to_dev_ibuf_idx]->phy_addr)
        >> 32) & 0xFFFFFFFF;
    payload_data[num_idx++] = pinst->max_ibuf_size;

    for (i = 0; i < FRM_BUF_POOL_SIZE; i++) {
      payload_buf->obuf_info[i].freed_obuf_index = 0xBAD;
      #ifdef HDR_DATA_SUPPORT
      payload_buf->hdrbuf_info[i].freed_obuf_index = 0xBAD;
      #endif
    }

    payload_buf->valid_oidxs = vvas_list_length(pinst->free_buf_list);
    for (i=0; i<payload_buf->valid_oidxs; i++) {
      VvasVideoFrame* vframe;
      uint64_t paddr;
      uint32_t idx;
      vframe = (VvasVideoFrame*)vvas_list_nth_data(pinst->free_buf_list, i);
      paddr = vvas_video_frame_get_frame_paddr(vframe);

      idx = ((uintptr_t)vvas_hash_table_lookup(pinst->oidx_hash, (void *)paddr) - 1);
      payload_buf->obuf_info[i].freed_obuf_index = idx;
      payload_buf->obuf_info[i].freed_obuf_paddr = pinst->obuf_db[idx].paddr;
      payload_buf->obuf_info[i].freed_obuf_size = pinst->obuf_db[idx].size;
      LOGD(pinst, "payload_buf->obuf_info[%d] index=%d paddr=0x%lx size=%lu",
        i, payload_buf->obuf_info[i].freed_obuf_index,
        payload_buf->obuf_info[i].freed_obuf_paddr,
        payload_buf->obuf_info[i].freed_obuf_size);
    }
  }

  ret = vvas_xrt_sync_bo (pinst->sk_payload_buf->bo, VVAS_BO_SYNC_BO_TO_DEVICE,
          pinst->sk_payload_buf->size, 0);
  if(ret != 0) {
    LOGE(pinst, "Failed to syncbo the sk_payload_buf ret = %d", ret);
  }

  LOGI(pinst, "Sending %s command to softkernel", VcuCmdId2Str[cmd_id]);
  ret = vvas_xrt_send_softkernel_command(pinst->hskd, pinst->kernel_handle, pinst->cmd_buf,
          payload_data, num_idx, CMD_EXEC_TIMEOUT);
  if(ret < 0) {
    LOGE(pinst, "Failed to send %s command to softkernel - ret=%d, reason : %s",
      VcuCmdId2Str[cmd_id], ret, strerror (errno));
    return FALSE;
  } else {
    if (cmd_id == VCU_DEINIT) {
      return TRUE;
    }

    memset(payload_buf, 0, pinst->sk_payload_buf->size);
    ret = vvas_xrt_sync_bo(pinst->sk_payload_buf->bo,
            VVAS_BO_SYNC_BO_FROM_DEVICE, pinst->sk_payload_buf->size, 0);
    if( ret != 0) {
      LOGE(pinst, "synbo failed - %d, reason : %s", ret, strerror(errno));
      return FALSE;
    }

    /* temporary fix: sleep of 1 msec to fix CR-1156069 */
    usleep(1000);

    if(!payload_buf->cmd_rsp)
      return FALSE;
  }

  return TRUE;
}

/** @fn VvasDecoder* vvas_decoder_create (VvasContext *vvas_ctx,
 *                                        uint8_t *dec_name,
 *                                        VvasCodecType dec_type,
 *                                        uint8_t hw_instance_id,
 *                                        VvasLogLevel log_level);
 *
 *  @param[in] vvas_ctx - Address of VvasContext handle created using @ref
 *  vvas_context_create
 *  @param[in] dec_name - Name of the decoder to be used for decoding
 *  @param[in] dec_type - Type of the decoder to be used (i.e. H264/H265)
 *  @param[in] hw_instance_id - Decoder instance index in a multi-instance decoder
 *             Incase of V70, this represents HW instance index and can have
 *             any value from the range 0..1.
 *  @param[in] log_level - Logging level
 *
 *  @return  On Success returns VvasDecoder handle pointer \n
 *           On Failure returns NULL
 *
 *  @brief Creates decoder's instance handle
 *
 */
VvasDecoder* vvas_decoder_create (VvasContext *vvas_ctx, uint8_t *dec_name,
                VvasCodecType dec_type, uint8_t hw_instance_id,
                VvasLogLevel log_level)
{
  VvasDecoderPrivate *self = NULL;

  /* Validate Log level */
  if (log_level > LOG_LEVEL_DEBUG || log_level < LOG_LEVEL_ERROR) {
    LOG_MESSAGE(LOG_LEVEL_ERROR, LOG_LEVEL_DEBUG, "Invalid log_level");
    return NULL;
  }

  /* Validate vvas_ctx */
  if (vvas_ctx == NULL) {
    LOG_MESSAGE(LOG_LEVEL_ERROR, LOG_LEVEL_DEBUG,
      "Invaid vvas_ctx = %p", vvas_ctx);
    return NULL;
  }

  /* Validate the hw_instance_id */
  if (hw_instance_id > (MAX_VDU_HW_INSTANCES - 1)) {
    LOG_MESSAGE(LOG_LEVEL_ERROR, LOG_LEVEL_DEBUG,
      "PS kernel VDU HW instance id=%d is not in the valid range [0, %d]", hw_instance_id, (MAX_VDU_HW_INSTANCES-1));
    return NULL;
  }

  /* Validate dec_type */
  if (dec_type != VVAS_CODEC_H264 && dec_type != VVAS_CODEC_H265) {
    LOG_MESSAGE(LOG_LEVEL_ERROR, LOG_LEVEL_DEBUG,
      "Invalid dec_type %d, valid range is [%d - %d]", dec_type,
      VVAS_CODEC_H264, VVAS_CODEC_H265);
    return NULL;
  }

  self = (VvasDecoderPrivate *)malloc(sizeof(VvasDecoderPrivate));
  if(self == NULL) {
    LOG_MESSAGE(LOG_LEVEL_ERROR, LOG_LEVEL_DEBUG, "Failed to allocate the memory");
    return NULL;
  }

  /* Clear the allocated memory */
  memset(self, 0, sizeof(VvasDecoderPrivate));

  self->dec_type = dec_type;
  self->log_level = log_level;
  memcpy (self->module_name, (uint8_t *)MODULE_NAME, MODULE_NAME_SZ-1);

  self->vvas_ctx = vvas_ctx;

  if (!dec_name) {
    LOGE(self, "kernel name is mandatory as arguement. \
               Ex: kernel_vdu_decoder:{kernel_vdu_decoder_0}\n");
    goto failed;
  } else {
      uint32_t sz = strlen((char *) dec_name);
      if (sz <= KERNEL_NAME_SZ) {
        memcpy(self->kernel_name, dec_name, sz);
      } else {
        LOGE(self, "decoder kernel name is too long\n");
        goto failed;
      }
  }

  self->in_mem_bank = VIDEODEC_DEFAULT_MEM_BANK;
  self->out_mem_bank = VIDEODEC_DEFAULT_MEM_BANK;

  self->host_to_dev_ibuf_idx = 0;

  /* Allocate memory for icfg and ocfg */
  self->icfg = malloc(sizeof(VvasDecoderInCfg));
  if (self->icfg == NULL) {
    LOGE(self, "Failed to allocate icfg memory");
    goto failed;
  }

  self->ocfg = malloc(sizeof(VvasDecoderOutCfg));
  if (self->ocfg == NULL) {
    LOGE(self, "Failed to allocate icfg memory");
    goto failed;
  }

  /* memset the icfg and ocfg */
  memset(self->icfg, 0, sizeof(VvasDecoderInCfg));
  memset(self->ocfg, 0, sizeof(VvasDecoderOutCfg));

  /* Initialize the hash table */
  self->oidx_hash = vvas_hash_table_new(vvas_direct_hash, vvas_direct_equal);

  self->hskd = self->vvas_ctx->dev_handle;
  if (vvas_xrt_open_context
      (self->vvas_ctx->dev_handle, self->vvas_ctx->uuid, &self->kernel_handle, (char *) self->kernel_name,
       false)) {
     LOGE(self, "failed to open XRT context ");
     goto failed;
   }

  if(allocate_internal_buffers(self) == FALSE) {
    LOGE(self, "Failed to allocate internal buffers");
    goto failed;
  }

  self->hw_instance_id = hw_instance_id;

  /* Move the state to created */
  self->state = VVAS_DEC_STATE_CREATED;

  /* Set the handle for VvasDecoder handle verification */
  self->handle = self;

  LOGD(self, "decoder created dev_idx=%d\n",
    self->vvas_ctx->dev_idx);

  return (VvasDecoder *) self;

failed:
  if (self->icfg) {
    free (self->icfg);
  }
  if (self->ocfg) {
    free (self->ocfg);
  }
  if (self->oidx_hash) {
    vvas_hash_table_unref (self->oidx_hash);
  }
  if (self->kernel_handle) {
    vvas_xrt_close_context (self->kernel_handle);
  }
  free(self);

  return NULL;
}

/** @fn VvasReturnType vvas_decoder_config(VvasDecoder* dec_handle,
 *                                         VvasDecoderInCfg *icfg,
 *                                         VvasDecoderOutCfg *ocfg)
 *  @param[in] dec_handle - Decoder handle pointer
 *  @param[in] icfg - Decoder input configuration
 *  @param[out] ocfg - Decoder output configuration
 *
 *  @return VvasReturnType
 *
 *  @brief Configures decoder with VvasDecoderInCfg and produces
 *  VvasDecoderOutCfg. Applications can populate VvasDecoderInCfg
 */
VvasReturnType vvas_decoder_config(VvasDecoder* dec_handle,
                    VvasDecoderInCfg *icfg, VvasDecoderOutCfg *ocfg){
  VvasDecoderPrivate *self = (VvasDecoderPrivate *) dec_handle;
  dec_params_t *dcfg;
  sk_payload_data *payload_buf;

  /* Check the handle validity */
  if(!self || self->handle != dec_handle) {
    LOG_MESSAGE(LOG_LEVEL_ERROR, LOG_LEVEL_DEBUG, "Invalid Handle");
    return VVAS_RET_INVALID_ARGS;
  }
  dcfg = (dec_params_t *) (self->dec_cfg_buf->user_ptr);
  memset (dcfg, 0, self->dec_cfg_buf->size);

  /* Validate bitdepth - only 8-bit and 10-bit is supported */
  if (icfg->bitdepth != 8 && icfg->bitdepth != 10) {
    LOGE(self, "bitdepth=%d is not supported, supported values are 8 and 10\n",
      icfg->bitdepth);
    return VVAS_RET_INVALID_ARGS;
  }

  /* Validate chroma_mode */
  if (icfg->chroma_mode != 420) {
    LOGE(self, "chroma_mode=%d not supported, only supported values is 420\n",
      icfg->chroma_mode);
    return VVAS_RET_INVALID_ARGS;
  }

  /* Validate entropy_buffers_count - valid range 2-10*/
  if (icfg->entropy_buffers_count < DEC_ENT_BUF_CNT_MIN ||
      icfg->entropy_buffers_count > DEC_ENT_BUF_CNT_MAX) {
    LOGE(self, "entropy_buffers_count=%d is out size valid range [%d,%d]\n",
      icfg->entropy_buffers_count, DEC_ENT_BUF_CNT_MIN, DEC_ENT_BUF_CNT_MAX);
    return VVAS_RET_INVALID_ARGS;
  }

  /* validate scan-type, only progressive supported */
  if (icfg->scan_type != DECCFG_SCANTYPE_P) {
    LOGE(self, "scan_type=%d not supported only progressive scan (%d) is "
      "supported\n", icfg->scan_type, DECCFG_SCANTYPE_P);
    return VVAS_RET_INVALID_ARGS;
  }

  /* validate codec_type and profile */
  if (icfg->codec_type == DECCFG_CODEC_AVC){
    if (icfg->profile != AVC_PROFILE_IDC_BASELINE &&
        icfg->profile != AVC_PROFILE_IDC_CONSTRAINED_BASELINE &&
        icfg->profile != AVC_PROFILE_IDC_MAIN &&
        icfg->profile != AVC_PROFILE_IDC_HIGH &&
        icfg->profile != AVC_PROFILE_IDC_HIGH10 &&
        icfg->profile != AVC_PROFILE_IDC_HIGH10_INTRA) {
      LOGE(self, "AVC Codec profile=%d not supported, only supported profiles "
        "are \n\tbaseline(%d), constrained-baseline(%d), main(%d), high(%d),"
        "\n\thigh-10(%d), high-10-intra(%d)\n", icfg->profile,
        AVC_PROFILE_IDC_BASELINE, AVC_PROFILE_IDC_CONSTRAINED_BASELINE,
        AVC_PROFILE_IDC_MAIN, AVC_PROFILE_IDC_HIGH, AVC_PROFILE_IDC_HIGH10,
        AVC_PROFILE_IDC_HIGH10_INTRA);
      return VVAS_RET_INVALID_ARGS;
    }

    /* validate frame size */
    if (icfg->height < AVC_FRAME_HEIGHT_MIN || icfg->height > FRAME_HEIGHT_MAX
        || icfg->width < AVC_FRAME_WIDTH_MIN || icfg->width > FRAME_WIDTH_MAX )
    {
      LOGE(self, "width x height(%dx%d) is not sopported valid size should be "
        "in the range %dx%d to %dx%d\n", icfg->width, icfg->height,
        AVC_FRAME_WIDTH_MIN, AVC_FRAME_HEIGHT_MIN, FRAME_WIDTH_MAX,
        FRAME_HEIGHT_MAX);
      return VVAS_RET_INVALID_ARGS;
    }
  } else if (icfg->codec_type == DECCFG_CODEC_HEVC) {
    if ( icfg->profile != HEVC_PROFILE_IDC_MAIN && icfg->profile !=
        HEVC_PROFILE_IDC_MAIN10 && icfg->profile != HEVC_PROFILE_IDC_RExt) {
      LOGE(self, "HEVC Codec profile=%d not supported, only supported profiles "
        "are\n\tmain(%d), main-10(%d) and main-intra/main-10-intra(%d)\n",
        icfg->profile, HEVC_PROFILE_IDC_MAIN, HEVC_PROFILE_IDC_MAIN10,
        HEVC_PROFILE_IDC_RExt);
      return VVAS_RET_INVALID_ARGS;
    }

    /* validate frame size */
    if (icfg->height < HEVC_FRAME_HEIGHT_MIN || icfg->height > FRAME_HEIGHT_MAX
        || icfg->width < HEVC_FRAME_WIDTH_MIN || icfg->width > FRAME_WIDTH_MAX )
    {
      LOGE(self, "width x height(%dx%d) is not sopported valid size should be "
        "in the range %dx%d to %dx%d\n", icfg->width, icfg->height,
        HEVC_FRAME_WIDTH_MIN, HEVC_FRAME_HEIGHT_MIN, FRAME_WIDTH_MAX,
        FRAME_HEIGHT_MAX);
      return VVAS_RET_INVALID_ARGS;
    }
  } else {
    LOGE(self, "codec_type=%d not supported, only supported codecs are AVC(%d) "
      "and HEVC(%d)\n", icfg->codec_type, DECCFG_CODEC_AVC, DECCFG_CODEC_HEVC);
    return VVAS_RET_INVALID_ARGS;
  }

  self->max_ibuf_size = icfg->height * icfg->width;
  dcfg->bitdepth = icfg->bitdepth;
  dcfg->codec_type = icfg->codec_type;
  dcfg->low_latency = icfg->low_latency;
  dcfg->entropy_buffers_count = icfg->entropy_buffers_count;
  dcfg->frame_rate = icfg->frame_rate;
  dcfg->clk_ratio = icfg->clk_ratio;
  dcfg->profile = icfg->profile;
  dcfg->level = icfg->level;
  dcfg->height = icfg->height;
  dcfg->width = icfg->width;
  dcfg->chroma_mode = icfg->chroma_mode;
  dcfg->scan_type = icfg->scan_type;
  dcfg->splitbuff_mode = icfg->splitbuff_mode;

  dcfg->instance_id = self->hw_instance_id;
  dcfg->i_frame_only = icfg->i_frame_only;

  if(send_command(self, VCU_PREINIT, NULL)){
    LOGD(self, "send_command(%p, VCU_PREINIT, NULL) Successfull", self);
    payload_buf = (sk_payload_data *) (self->sk_payload_buf->user_ptr);
    ocfg->min_out_buf = payload_buf->obuff_num;
    ocfg->mem_bank_id = self->out_mem_bank;
    ocfg->vinfo.fmt = (dcfg->bitdepth == 8 ? VVAS_VIDEO_FORMAT_Y_UV8_420 :
                          VVAS_VIDEO_FORMAT_NV12_10LE32);
    ocfg->vinfo.width   = icfg->width;
    ocfg->vinfo.height   = icfg->height;
    ocfg->vinfo.n_planes = 2;

    if (ocfg->vinfo.fmt == VVAS_VIDEO_FORMAT_NV12_10LE32) {
      uint32_t width_byte = ((ocfg->vinfo.width+2)/3)*4;
      ocfg->vinfo.alignment.padding_right =
                   (ALIGN(width_byte, WIDTH_ALIGN) - width_byte) * 0.75;
    } else {
      ocfg->vinfo.alignment.padding_right =
                   ALIGN(ocfg->vinfo.width, WIDTH_ALIGN) - ocfg->vinfo.width;
    }

    ocfg->vinfo.alignment.padding_left = 0;
    ocfg->vinfo.alignment.padding_top = 0;
    ocfg->vinfo.alignment.padding_bottom =
                   ALIGN(ocfg->vinfo.height, HEIGHT_ALIGN) - ocfg->vinfo.height;
    ocfg->vinfo.alignment.stride_align[0] = WIDTH_ALIGN-1;
    ocfg->vinfo.alignment.stride_align[1] = WIDTH_ALIGN-1;

    memcpy(self->icfg, icfg, sizeof(VvasDecoderInCfg));
    memcpy(self->ocfg, ocfg, sizeof(VvasDecoderOutCfg));

    self->state = VVAS_DEC_STATE_CONFIGURED;
  } else {
    LOGE(self, "send_command failed for VCU_PREINIT\n");
    return VVAS_RET_ERROR;
  }

  return VVAS_RET_SUCCESS;
}

/** @fn VvasReturnType vvas_decoder_submit_frames(VvasDecoder dec_handle,
 *                                                VvasMemory *nalu,
 *                                                VvasList *loutframes)
 *
 *  @param[in] dec_handle - Decoder handle pointer
 *  @param[in] nalu - Complete NALU frame. send NULL pointer on End of stream
 *  @param[in] loutframes - List of free output frames for decoding process
 *
 *  @return VVAS_RET_SEND_AGAIN if \p nalu is not consumed completely. In this
 *  case, send same NALU again\n
 *
 *  @brief Submits one NALU frame and free output frames to decoder for decoding
 */
VvasReturnType vvas_decoder_submit_frames(VvasDecoder* dec_handle,
                  VvasMemory *nalu, VvasList *loutframes)
{
  VvasDecoderPrivate *self = (VvasDecoderPrivate *) dec_handle;
  sk_payload_data *payload_buf;
  VvasMemoryMapInfo mem_info;
  VvasReturnType ret = VVAS_RET_SUCCESS;
  VvasMetadata in_meta = {0};
  uint32_t iret = 0;

  /* Check the handle validity */
  if(!self || self->handle != dec_handle) {
    LOG_MESSAGE(LOG_LEVEL_ERROR, LOG_LEVEL_DEBUG, "Invalid Handle\n");
    return VVAS_RET_INVALID_ARGS;
  }

  /* Check state and return state if decoder not configured yet */
  if (self->state < VVAS_DEC_STATE_CONFIGURED) {
    LOGE(self, "decode instance not yet configured, please invoke"
      "vvas_decoder_config first");
    return VVAS_RET_ERROR;
  }

  /* Check the state and do VCU_INIT if not already done */
  if (self->state < VVAS_DEC_STATE_INITED) {
    /* Issue VCU_INIT if decoder is not in VCU_INIT_DONE state */
    uint32_t list_sz = vvas_list_length(loutframes);
    LOGD(self, "list_sz=%d", list_sz);
    if (list_sz < self->ocfg->min_out_buf) {
      LOGE(self, "out_frame list has %d atlease %d frames needed for the decode",
        list_sz, self->ocfg->min_out_buf);
      return VVAS_RET_INVALID_ARGS;
    }

    /* List all the unique video frame into the local db */
    vvas_list_foreach (loutframes, populate_obuf_db_fn, self);
    if (self->vf_max_error == true) {
      LOGE(self, "There is more than supported(%d) unique vframe passed by user",
        FRM_BUF_POOL_SIZE);
      self->vf_max_error = false;
      return VVAS_RET_ERROR;
    }

    /* Allocate the XRT buffer for passing output-video/HDR frame paddr, Also
    allocate HDR frames as its not passed by user */
    if(!allocate_out_buffers(self)) {
      LOGE(self, "failed to allocate xrt out buffers");
      return VVAS_RET_ERROR;
    }

    /* Allocate XRT buffer for encoded input frame */
    if(!allocate_in_buffers(self)) {
      LOGE(self, "failed to allocate xrt in buffers");
      destroy_out_buffers(self);
      return VVAS_RET_ERROR;
    }

    /* Send VCU_INIT Command to kernel */
    if(send_command(self, VCU_INIT, NULL)){
      LOGD(self, "VCU_INIT Successfull");
      self->state = VVAS_DEC_STATE_INITED;
    } else {
      LOGE(self, "Failed to send VCU_INIT command");
      destroy_out_buffers(self);
      destroy_in_buffers(self);
      return VVAS_RET_ERROR;
    }
  } else {
    /* VCU_INIT already invoked, enqueue any new video frame into local db */
    vvas_list_foreach (loutframes, populate_obuf_db_fn, self);
    if (self->vf_max_error == true) {
      LOGE(self, "There is more than supported(%d) unique vframe passed by user",
        FRM_BUF_POOL_SIZE);
      self->vf_max_error = false;
      return VVAS_RET_ERROR;
    }
    self->free_buf_list = loutframes;
  }

  if (nalu == NULL) {
    /* nalu == NULL signifies the user intent to call VCU_FLUSH */

    /* Check the current state and return as appropriate */
    if (self->state < VVAS_DEC_STATE_STARTED) {
      LOGE(self, "Current State is %d, cann't do FLUSH", self->state);
      return VVAS_RET_ERROR;
    }

    if (vvas_list_length(self->free_buf_list)) {
      self->ibuff_param.insize = 0;
      /* Send VCU_PUSH to kernel to send the free frame information */
      if (send_command(self, VCU_PUSH, NULL)){
        LOGD(self, "Post flush VCU_PUSH CMD Successfull");
        payload_buf = (sk_payload_data *) (self->sk_payload_buf->user_ptr);

        /* Check if input frame has been consumed or required to be sent again*/
        if(payload_buf->dev_to_host_ibuf_idx != 0xBAD) {
          LOGI(self, "input buffer index %d consumed",
            self->host_to_dev_ibuf_idx);
          self->host_to_dev_ibuf_idx = payload_buf->dev_to_host_ibuf_idx;
        } else {
          LOGI(self, "input buffer index %d NOT consumed",
            self->host_to_dev_ibuf_idx);
          return VVAS_RET_SEND_AGAIN;
        }
      } else {
        LOGE(self, "Post flush VCU_PUSH CMD Failed");
        return VVAS_RET_ERROR;
      }
    }

    if (self->state == VVAS_DEC_STATE_FINISHED) {
      LOGI(self, "VCU_FLUSH already done\n");
      return VVAS_RET_SUCCESS;
    }

    LOGI(self, "Input stream ended, sending flush to collect all remaining "
      "frames with decoder");
    /* Send VCU_FLUSH command to kernel */
    if(send_command(self, VCU_FLUSH, NULL)) {
      self->state = VVAS_DEC_STATE_FINISHED;
      /* xrt in buffer can be deallocated now - keeping this to able to do send
      command after flush */
      LOGD(self, "VCU_FLUSH - Successfull");
      return VVAS_RET_SUCCESS;
    } else {
      LOGE(self, "VCU_FLUSH - Failed");
      return VVAS_RET_ERROR;
    }
  } else {
    /* Ready to send the encoded frames to kernel, check if its initialized */
    if (self->state < VVAS_DEC_STATE_INITED) {
      LOGE(self, "Decoder is not initialized yet");
      return VVAS_RET_ERROR;
    }

    /* Map the input VvasMemory frame for coping data to XRT buffer */
    ret = vvas_memory_map(nalu, VVAS_DATA_MAP_READ, &mem_info);
    if (ret != VVAS_RET_SUCCESS) {
      LOGE(self, "failed to map the encoded frame memory for read ret=%d", ret);
      return VVAS_RET_ERROR;
    }

    /* Write encoded input frame into the input XRT buffer for sending it to
    kernel */
    iret = vvas_xrt_write_bo(self->xrt_bufs_in[self->host_to_dev_ibuf_idx]->bo,
              mem_info.data, mem_info.size, 0);
    if (iret !=0) {
      LOGE(self, "Failed to copy the in frame in xrt_bufs_in[%d] iret = %d",
        self->host_to_dev_ibuf_idx, iret);
      return VVAS_RET_ERROR;
    }

    /* Sync data to device */
    iret = vvas_xrt_sync_bo (self->xrt_bufs_in[self->host_to_dev_ibuf_idx]->bo,
            VVAS_BO_SYNC_BO_TO_DEVICE, mem_info.size, 0);
    if (iret !=0) {
      LOGE(self, "Failed syncbo xrt_bufs_in[%d] bo iret = %d",
        self->host_to_dev_ibuf_idx, iret);
      return VVAS_RET_ERROR;
    }

    /* Set valid input data size */
    self->ibuff_param.insize = mem_info.size;

    /* Extract the PTS data from input VvasMemory frame */
    vvas_memory_get_metadata(nalu, &in_meta);
    self->ibuff_param.meta.pts = in_meta.pts;

    /* Unmap the VvasMemory input frame as copy and sync has been done */
    vvas_memory_unmap(nalu, &mem_info);
  }

  /* Send VCU_PUSH to kernel to enqueue the encoded data */
  if (send_command(self, VCU_PUSH, NULL)){
    LOGD(self, "VCU_PUSH CMD Successfull");
    self->state = VVAS_DEC_STATE_STARTED;
    payload_buf = (sk_payload_data *) (self->sk_payload_buf->user_ptr);

    /* Check if input frame has been consumed or required to be sent again */
    if(payload_buf->dev_to_host_ibuf_idx != 0xBAD) {
      LOGI(self, "input buffer index %d consumed", self->host_to_dev_ibuf_idx);
      self->host_to_dev_ibuf_idx = payload_buf->dev_to_host_ibuf_idx;
    } else {
      LOGI(self, "input buffer index %d NOT consumed",
        self->host_to_dev_ibuf_idx);
      return VVAS_RET_SEND_AGAIN;
    }
  } else {
    LOGE(self, "VCU_PUSH CMD Failed");
    return VVAS_RET_ERROR;
  }

  return  VVAS_RET_SUCCESS;
}

/** @fn VvasReturnType vvas_decoder_get_decoded_frame(VvasDecoder* dec_handle,
 *                                                    VvasVideoFrame **output)
 *  @param[in] dec_handle - Decoder handle pointer
 *  @param[out] output - Video frame which contains decoded data
 *
 *  @return  VVAS_RET_EOS on End of stream \n
 *           VVAS_RET_NEED_MOREDATA if decoder does not produce any decoded
 *           buffers
 *
 *  @brief This API gets decoded data from decoder
 */
VvasReturnType vvas_decoder_get_decoded_frame(VvasDecoder* dec_handle,
                  VvasVideoFrame **output)
{
  uint32_t idx = 0;
  sk_payload_data *payload_buf;
  VvasMetadata out_meta_data = {0};
  VvasDecoderPrivate *self = (VvasDecoderPrivate *) dec_handle;

  /* Check handle for validity */
  if(!self || self->handle != dec_handle) {
    LOG_MESSAGE(LOG_LEVEL_ERROR, LOG_LEVEL_DEBUG, "Invalid Handle");
    return VVAS_RET_INVALID_ARGS;
  }

  /* Check if there are cashed output frames which can be sent */
  if (self->last_rcvd_payload.free_index_cnt) {
    LOGD(self, "There are %d available out frame from previous command",
      self->last_rcvd_payload.free_index_cnt);
    idx = self->last_rcvd_payload.obuff_index[self->last_rcvd_oidx];
    out_meta_data.pts
      = self->last_rcvd_payload.obuff_meta[self->last_rcvd_oidx].pts;
    *output = self->obuf_db[idx].vframe;

    /* Set the PTS meta data into the output video frame */
    vvas_video_frame_set_metadata(*output, &out_meta_data);

    /* Set sync flag */
    vvas_video_frame_set_sync_flag(*output, VVAS_DATA_SYNC_FROM_DEVICE);

    LOGD(self, "VCU_RECEIVE output=%p", *output);
    self->last_rcvd_payload.free_index_cnt--;
    self->last_rcvd_oidx++;

    return VVAS_RET_SUCCESS;
  } else if (self->last_rcvd_payload.end_decoding) {
  /* Check if EOS has been recived */
    LOGI(self, "EOS recevied from softkernel");
    return VVAS_RET_EOS;
  }

  /* If there is no cashed output frame query the kernel by sending
  VCU_RECEIVE */
  if(send_command(self, VCU_RECEIVE, NULL)){
    LOGD(self, "VCU_RECEIVE CMD Successfull");
    payload_buf = (sk_payload_data *) (self->sk_payload_buf->user_ptr);
    memcpy (&self->last_rcvd_payload, payload_buf, sizeof (sk_payload_data));
  } else {
    LOGE(self, "VCU_RECEIVE CMD Failed");
    return VVAS_RET_ERROR;
  }

  /* Kernel might return multiple output frame, cache the same and return one
  frame to user */
  if (self->last_rcvd_payload.free_index_cnt) {
    self->last_rcvd_oidx = 0;
    LOGD(self, "VCU_RECEIVE got %d free_index_cnt",
      self->last_rcvd_payload.free_index_cnt);
    idx = self->last_rcvd_payload.obuff_index[self->last_rcvd_oidx];
    
    if (idx >= FRM_BUF_POOL_SIZE) {
      LOGE(self, "Invalid index:%d last_rcvd_oidx:%d", idx, self->last_rcvd_oidx);
      return VVAS_RET_ERROR;
    }
    
    out_meta_data.pts
      = self->last_rcvd_payload.obuff_meta[self->last_rcvd_oidx].pts;
    *output = self->obuf_db[idx].vframe;

    /* Set the PTS meta data into the output video frame */
    vvas_video_frame_set_metadata(*output, &out_meta_data);

    /* Set sync flag */
    vvas_video_frame_set_sync_flag(*output, VVAS_DATA_SYNC_FROM_DEVICE);

    LOGD(self, "VCU_RECEIVE output=%p", *output);
    self->last_rcvd_payload.free_index_cnt--;
    self->last_rcvd_oidx++;

    return VVAS_RET_SUCCESS;
  } else if (self->last_rcvd_payload.end_decoding) {
  /* Check if EOS has been recived */
    LOGI(self, "EOS recevied from softkernel");
    return VVAS_RET_EOS;
  }

  return VVAS_RET_NEED_MOREDATA;
}

/** @fn VvasReturnType vvas_decoder_destroy (VvasDecoder* dec_handle)
 *
 *  @param[in] dec_handle - Decoder handle pointer
 *
 *  @return  returns VvasReturnType
 *
 *  @brief Destroys decoded handle
 */
VvasReturnType vvas_decoder_destroy (VvasDecoder* dec_handle)
{
  int32_t iret;
  VvasDecoderPrivate *self = (VvasDecoderPrivate *) dec_handle;

  if(!self || self->handle != dec_handle) {
    LOG_MESSAGE(LOG_LEVEL_ERROR, LOG_LEVEL_DEBUG, "Invalid Handle");
    return VVAS_RET_INVALID_ARGS;
  }

  destroy_in_buffers(self);
  destroy_out_buffers(self);

  /* Issue DEINIT Command */
  if(send_command(self, VCU_DEINIT, NULL)) {
    LOGD(self, "VCU_DEINIT - Successfull");
    self->state = VVAS_DEC_STATE_CREATED;
  }

  if (self->icfg) {
    free(self->icfg);
    self->icfg = NULL;
  }

  if (self->ocfg) {
    free(self->ocfg);
    self->ocfg = NULL;
  }

  vvas_hash_table_destroy(self->oidx_hash);

  /* Clear all the internel XRT buffer allocations */
  destroy_internal_buffers(self);

  iret = vvas_xrt_close_context (self->kernel_handle);
  if (iret != 0)
    LOGE(self, "failed to close xrt context");

  /* Poisen the valid marker to avoid handling the freeed context accidently
  getting used */
  self->handle = 0;

  free(self);

  return iret ? VVAS_RET_ERROR : VVAS_RET_SUCCESS;
}
