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

#include <glib.h>

#include <vvas_utils/vvas_buffer_pool.h>
#include <vvas_utils/vvas_utils.h>

typedef struct
{
  /** List to keep track of allocated buffers */
  VvasList *buffers_list;
  /** Queue to keep track of free buffers */
  VvasQueue *free_queue;
  /** Mutex lock to protect against concurrent access of this structure */
  GMutex lock;
  /** VvasContext in case of CMA buffers */
  VvasContext *vvas_ctx;
  /** Buffer Pool Properties */
  VvasBufferPoolConfig config;
  /* Number of buffers allocated */
  int32_t buffer_count;
  /** Log level */
  VvasLogLevel log_level;
  /** Buffer release notify callback */
  vvas_buffer_pool_buffer_relase_callback buffer_release_cb;
  /** User data to be passed to the release notify callback */
  void *user_data;
} VvasBufferPoolPrivate;

static VvasVideoBuffer *
vvas_buffer_pool_alloc_video_frame (VvasBufferPoolPrivate * self)
{
  VvasVideoBuffer *v_buffer;
  VvasReturnType ret;

  v_buffer = (VvasVideoBuffer *) calloc (1, sizeof (VvasVideoBuffer));
  if (!v_buffer) {
    LOG_ERROR (self->log_level, "Couldn't allocate video buffer");
    return NULL;
  }

  v_buffer->buffer_pool = self;

  v_buffer->video_frame =
      vvas_video_frame_alloc (self->vvas_ctx, self->config.alloc_type,
      self->config.alloc_flag, self->config.mem_bank_idx,
      &(self->config.video_info), &ret);

  if (v_buffer->video_frame) {
    /* keep track of number of buffers allocated */
    self->buffer_count++;
    LOG_DEBUG (self->log_level, "%p: Allocated: %p, video_buffer: %p, total buffers: %d",
        self, v_buffer, v_buffer->video_frame, self->buffer_count);
    self->buffers_list = vvas_list_append (self->buffers_list, v_buffer);
  } else {
    LOG_ERROR (self->log_level, "Couldn't allocate video frame, ret: %d", ret);
    free (v_buffer);
    v_buffer = NULL;
  }

  return v_buffer;
}

VvasBufferPool *
vvas_buffer_pool_create (VvasContext * vvas_ctx,
    const VvasBufferPoolConfig * config, VvasLogLevel log_level)
{
  VvasBufferPoolPrivate *self;

  if (!vvas_ctx || !config) {
    return NULL;
  }

  if (!config->minimum_buffers) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, vvas_ctx->log_level,
        "Invalid minimum buffers");
    return NULL;
  }

  self = (VvasBufferPoolPrivate *) calloc (1, sizeof (VvasBufferPoolPrivate));
  if (!self) {
    LOG_ERROR (DEFAULT_VVAS_LOG_LEVEL, "Failed to allocate memory for buffer pool instance");
    return NULL;
  }

  /* copy user given pool properties into our context(Shallow structure copy) */
  self->config = *config;
  self->vvas_ctx = vvas_ctx;
  self->log_level = log_level;

  /* Allocate queues to hold buffers */
  if (config->maximum_buffers) {
    self->free_queue = vvas_queue_new (config->maximum_buffers);
  } else {
    /* Maximum buffers is 0 which means no limit on the maximum buffers,
     * create queue to store unlimited number of buffers.
     */
    self->free_queue = vvas_queue_new (-1);
  }
  if (!self->free_queue) {
    LOG_ERROR (self->log_level, "%p: Couldn't allocate queue", self);
    goto error;
  }

  /* allocate minimum numbers of buffers and put them in free queue */
  for (int8_t idx = 0; idx < config->minimum_buffers; idx++) {
    VvasVideoBuffer *v_buffer;
    v_buffer = vvas_buffer_pool_alloc_video_frame (self);
    if (!v_buffer->video_frame) {
      LOG_ERROR (self->log_level, "%p: Couldn't allocate video_frame", self);
      goto error;
    }
    /* Video frame allocated, push this in free queue */
    vvas_queue_enqueue (self->free_queue, v_buffer);
  }

  g_mutex_init (&self->lock);

  return (VvasBufferPool *) self;

error:
  if (self->free_queue) {
    vvas_queue_free (self->free_queue);
  }

  if (self->buffers_list) {
    vvas_list_free (self->buffers_list);
  }

  if (self) {
    free (self);
  }
  return NULL;
}

static void
vvas_buffer_pool_free_video_frames (VvasVideoBuffer *v_buffer)
{
  if (!v_buffer) {
    return;
  }

  vvas_video_frame_free (v_buffer->video_frame);
  free (v_buffer);
}

static void
vvas_buffer_pool_print_frames (void *data, void *user_data)
{
  VvasBufferPoolPrivate * self = (VvasBufferPoolPrivate *) user_data;
  VvasVideoBuffer *v_buffer = (VvasVideoBuffer *) data;

  if (!v_buffer) {
    return;
  }

  LOG_DEBUG (self->log_level, "%p: buffer: %p", self, v_buffer);
}

VvasReturnType
vvas_buffer_pool_free (VvasBufferPool * pool)
{
  VvasBufferPoolPrivate *self = (VvasBufferPoolPrivate *) pool;
  int32_t buffer_free_count = 0, free_buf_count = 0;
  if (!self) {
    return VVAS_RET_INVALID_ARGS;
  }

  g_mutex_lock (&self->lock);

  LOG_DEBUG (self->log_level, "%p: Buffers list....", self);
  vvas_list_foreach (self->buffers_list, vvas_buffer_pool_print_frames, self);

  free_buf_count = vvas_queue_get_length (self->free_queue);
  LOG_DEBUG (self->log_level, "%p: buffer_count: %u, free_buf_count: %u", self,
        self->buffer_count, free_buf_count);

  LOG_DEBUG (self->log_level, "%p: pool has %d outstanding buffers", self,
      self->buffer_count - free_buf_count);

  while (buffer_free_count < self->buffer_count) {
    VvasVideoBuffer * v_buffer = (VvasVideoBuffer *)
        vvas_queue_dequeue (self->free_queue);
    LOG_DEBUG (self->log_level, "%p: Freeing buffer: %p, video_frame: %p",
        self, v_buffer, v_buffer->video_frame);
    vvas_buffer_pool_free_video_frames (v_buffer);
    buffer_free_count++;
  }

  vvas_queue_free (self->free_queue);

  vvas_list_free (self->buffers_list);
  g_mutex_unlock (&self->lock);

  g_mutex_clear (&self->lock);

  LOG_DEBUG (self->log_level, "%p pool freed", self);

  free (self);

  return VVAS_RET_SUCCESS;
}

VvasVideoBuffer *
vvas_buffer_pool_acquire_buffer (VvasBufferPool * pool)
{
  VvasBufferPoolPrivate *self = (VvasBufferPoolPrivate *) pool;
  VvasVideoBuffer *v_buffer;

  if (!self) {
    return NULL;
  }

  /* Check if we have any buffer in our free queue */
  v_buffer = vvas_queue_dequeue_noblock (self->free_queue);
  if (!v_buffer) {
    /* No free buffer */
    g_mutex_lock (&self->lock);
    if ((!self->config.maximum_buffers) ||
        (self->buffer_count < self->config.maximum_buffers)) {
      /* In case when maximum buffers is zero i.e. no upper limit on the number
       * of buffers, create new buffer.
       */
      /* We can allocate new buffer as we have not allocated maximum buffers */
      v_buffer = vvas_buffer_pool_alloc_video_frame (self);
      g_mutex_unlock (&self->lock);
    } else if (self->config.block_on_empty){
      /* Already allocated maximum buffers, we can wait on free_queue to get
       * free buffer in blocking call */
      LOG_DEBUG (self->log_level, "%p: Max buffer allocation reached, wait for free buffer", self);
      g_mutex_unlock (&self->lock);
      v_buffer = vvas_queue_dequeue (self->free_queue);
    } else {
      LOG_DEBUG (self->log_level, "%p: No free buffers available in the pool", self);
      g_mutex_unlock (&self->lock);
    }
  }

  LOG_DEBUG (self->log_level, "%p: Acquired buffer: %p", self, v_buffer);
  return v_buffer;
}

VvasReturnType
vvas_buffer_pool_release_buffer (VvasVideoBuffer * buffer)
{
  VvasBufferPoolPrivate *self;

  if (!buffer) {
    return VVAS_RET_INVALID_ARGS;
  }

  self = (VvasBufferPoolPrivate *) buffer->buffer_pool;

  if (self->buffer_release_cb) {
    /* Notify about buffer release */
    self->buffer_release_cb (self->user_data);
  }

  /* This buffer is release by the user, put it back in free_queue */
  LOG_DEBUG (self->log_level, "%p: buffer released: %p", self, buffer);
  vvas_queue_enqueue (self->free_queue, buffer);

  return VVAS_RET_SUCCESS;
}

VvasReturnType
vvas_buffer_pool_set_release_buffer_notify_cb (VvasBufferPool * pool,
    vvas_buffer_pool_buffer_relase_callback cb, void *user_data)
{
  VvasBufferPoolPrivate *self;

  if (!pool || !cb) {
    return VVAS_RET_INVALID_ARGS;
  }

  self = (VvasBufferPoolPrivate *) pool;
  self->buffer_release_cb = cb;
  self->user_data = user_data;

  return VVAS_RET_SUCCESS;
}

VvasReturnType
vvas_buffer_pool_get_configuration (VvasBufferPool * pool,
    VvasBufferPoolConfig * config)
{
  VvasBufferPoolPrivate *self = (VvasBufferPoolPrivate *) pool;

  if (!self || !config) {
    return VVAS_RET_INVALID_ARGS;
  }

  /* Shallow config copy */
  *config = self->config;

  return VVAS_RET_SUCCESS;
}
