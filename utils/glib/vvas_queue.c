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

#include <stdio.h>
#include <glib.h>
#define VVAS_UTILS_INCLUSION
#include <vvas_utils/vvas_queue.h>
#undef VVAS_UTILS_INCLUSION

typedef struct
{
  /** Queue */
  GQueue *queue;
  /** Mutex lock to protect concurrent access to Queue */
  GMutex lock;
  /** Condition variable to signal waiting thread */
  GCond cond;
  /** Length of the queue */
  int32_t length;
  /** Flag to monitor exit */
  bool is_exit;
  /** Number of waiting threads */
  uint32_t waiting_thread;
} VvasQueuePrivate;

/**
 *  @fn VvasQueue * vvas_queue_new (int_t length)
 *  @param [in] length  Queue length, -1 for no limit on length
 *  @return VvasQueue
 *  @brief  This API allocates a new VvasQueue
 *  @note   This instance must be freed using @ref vvas_queue_free
 */
VvasQueue *
vvas_queue_new (int32_t length)
{
  VvasQueuePrivate *self;

  if (!length) {
    return NULL;
  }

  self = (VvasQueuePrivate *) calloc (1, sizeof (VvasQueuePrivate));
  if (!self) {
    return NULL;
  }

  self->length = length;
  self->is_exit = false;
  self->waiting_thread = 0;
  self->queue = g_queue_new ();
  g_mutex_init (&self->lock);
  g_cond_init (&self->cond);

  return (VvasQueue *) self;
}

/**
 *  @fn void vvas_queue_new (VvasQueue * vvas_queue)
 *  @param [in] vvas_queue  VvasQueue allocated using @ref vvas_queue_new
 *  @return None
 *  @brief  This API frees the memory allocated for the VvasQueue. If queue elements contain
 *          dynamically-allocated memory, they should be freed first.
 */
void
vvas_queue_free (VvasQueue * vvas_queue)
{
  VvasQueuePrivate *self = (VvasQueuePrivate *) vvas_queue;

  if (!self) {
    return;
  }

  /* Queue is getting freed, unblock any waiting thread */
  g_mutex_lock (&self->lock);
  /* Making this flag True so that others don't try to enqueue or dequeue */
  self->is_exit = true;
  while (self->waiting_thread) {
    /* There are thread(s) waiting for either enqueue or dequeue, let's
     * unblock them */
    g_cond_broadcast (&self->cond);
    g_mutex_unlock (&self->lock);
    /* Let's give other threads sometime to unblock and return from blocking
     * calls */
    g_usleep (20);
    g_mutex_lock (&self->lock);
  }
  g_queue_free (self->queue);
  g_mutex_unlock (&self->lock);

  /* Free Queue, clear mutex lock and condition variable */
  g_mutex_clear (&self->lock);
  g_cond_clear (&self->cond);

  /* Free myself :) */
  free (self);
}

/**
 *  @fn void vvas_queue_free_full (VvasQueue * vvas_queue, VvasQueueDestroyNotify free_func)
 *  @param [in] vvas_queue  VvasQueue allocated using @ref vvas_queue_new
 *  @param [in] free_func   Callback function which is called when a data element is destroyed.
 *                          It is passed the pointer to the data element and should free any
 *                          memory and resources allocated for it.
 *  @return None
 *  @brief  This API frees all the memory used by a VvasQueue, and calls the specified destroy
 *          function on every element's data.
 *  @note   \p free_func should not modify the queue (eg, by removing the freed element from it).
 */
void
vvas_queue_free_full (VvasQueue * vvas_queue, VvasQueueDestroyNotify free_func)
{
  VvasQueuePrivate *self = (VvasQueuePrivate *) vvas_queue;

  if (!self || !free_func) {
    return;
  }

  /* Queue is getting freed, unblock any waiting thread */
  g_mutex_lock (&self->lock);
  self->is_exit = true;
  while (self->waiting_thread) {
    /* There are thread(s) waiting for either enqueue or dequeue, let's
     * unblock them */
    g_cond_broadcast (&self->cond);
    g_mutex_unlock (&self->lock);
    g_usleep (20);
    g_mutex_lock (&self->lock);
  }
  /* No waiting thread, free queue now */
  g_queue_free_full (self->queue, free_func);
  g_mutex_unlock (&self->lock);

  /* Clear mutex lock and condition variable */
  g_mutex_clear (&self->lock);
  g_cond_clear (&self->cond);

  /* Free myself :) */
  free (self);
}

/**
 *  @fn void vvas_queue_clear (VvasQueue * vvas_queue)
 *  @param [in] vvas_queue  VvasQueue allocated using @ref vvas_queue_new
 *  @return None
 *  @brief  This API removes all the elements in queue. If queue elements contain dynamically-allocated
 *          memory, they should be freed first.
 */
void
vvas_queue_clear (VvasQueue * vvas_queue)
{
  VvasQueuePrivate *self = (VvasQueuePrivate *) vvas_queue;

  if (!self) {
    return;
  }

  g_mutex_lock (&self->lock);
  g_queue_clear (self->queue);
  /* Entries from the queue are removed, lets notify about it to all the
   * waiting threads */
  g_cond_broadcast (&self->cond);
  g_mutex_unlock (&self->lock);
}

/**
 *  @fn void vvas_queue_clear_full (VvasQueue * vvas_queue, VvasQueueDestroyNotify free_func)
 *  @param [in] vvas_queue  VvasQueue allocated using @ref vvas_queue_new
 *  @param [in] free_func   Callback function which is called when a data element is freed.
 *                          It is passed the pointer to the data element and should free any
 *                          memory and resources allocated for it.
 *  @return None
 *  @brief  This API frees all the memory used by a VvasQueue, and calls the provided free_func on
 *          each item in the VvasQueue.
 *  @note   \p free_func should not modify the queue (eg, by removing the freed element from it).
 */
void
vvas_queue_clear_full (VvasQueue * vvas_queue, VvasQueueDestroyNotify free_func)
{
  VvasQueuePrivate *self = (VvasQueuePrivate *) vvas_queue;

  if (!self || !free_func) {
    return;
  }

  g_mutex_lock (&self->lock);
  g_queue_clear_full (self->queue, free_func);
  /* Entries from the queue are removed, lets notify about it to all the
   * waiting threads */
  g_cond_broadcast (&self->cond);
  g_mutex_unlock (&self->lock);
}

/**
 *  @fn bool vvas_queue_is_empty (VvasQueue * vvas_queue)
 *  @param [in] vvas_queue  VvasQueue allocated using @ref vvas_queue_new
 *  @return Returns TRUE if \p vvas_queue is empty.
 *  @brief  This API is to check if \p vvas_queue is empty or not.
 */
bool
vvas_queue_is_empty (VvasQueue * vvas_queue)
{
  VvasQueuePrivate *self = (VvasQueuePrivate *) vvas_queue;
  bool is_empty = true;

  if (!self) {
    return is_empty;
  }

  g_mutex_lock (&self->lock);
  is_empty = g_queue_is_empty (self->queue);
  g_mutex_unlock (&self->lock);

  return is_empty;
}

/**
 *  @fn uint32_t vvas_queue_get_length (VvasQueue * vvas_queue)
 *  @param [in] vvas_queue  VvasQueue allocated using @ref vvas_queue_new
 *  @return Returns the number of items in the queue
 *  @brief  This API is to get the \p vvas_queue's length
 */
uint32_t
vvas_queue_get_length (VvasQueue * vvas_queue)
{
  VvasQueuePrivate *self = (VvasQueuePrivate *) vvas_queue;
  uint32_t queue_length;

  if (!self) {
    return 0;
  }

  g_mutex_lock (&self->lock);
  queue_length = g_queue_get_length (self->queue);
  queue_length = queue_length - self->waiting_thread;
  g_mutex_unlock (&self->lock);

  return queue_length;
}

/**
 *  @fn void vvas_queue_for_each (VvasQueue * vvas_queue, VvasQueueFunc func, void * user_data)
 *  @param [in] vvas_queue  VvasQueue allocated using @ref vvas_queue_new
 *  @param [in] func        A callback function to be called for each element of the queue
 *  @return None
 *  @brief  This API Calls \p func for each element in the queue passing \p user_data to the function.
 *  @note   \p func should not modify the queue.
 */
void
vvas_queue_for_each (VvasQueue * vvas_queue, VvasQueueFunc func, void *user_data)
{
  VvasQueuePrivate *self = (VvasQueuePrivate *) vvas_queue;

  if (!self || !func) {
    return;
  }

  g_mutex_lock (&self->lock);
  g_queue_foreach (self->queue, func, user_data);
  g_mutex_unlock (&self->lock);
}

/**
 *  @fn void vvas_queue_enqueue (VvasQueue * vvas_queue, void * data)
 *  @param [in] vvas_queue  VvasQueue allocated using @ref vvas_queue_new
 *  @param [in] data        The data for the new element
 *  @return TRUE if data can be enqueued, FALSE otherwise
 *  @brief  This API Adds a new element at the tail of the queue, this API will
 *          block if the queue is full. For non blocking enqueue use @ref vvas_queue_enqueue_noblock
 */
bool
vvas_queue_enqueue (VvasQueue * vvas_queue, void *data)
{
  VvasQueuePrivate *self = (VvasQueuePrivate *) vvas_queue;
  bool ret = false;

  if (!self || !data) {
    return ret;
  }

  if (!self->is_exit) {
    g_mutex_lock (&self->lock);
    if (self->length > 0) {
      /* Limited queue, check if there is space in the queue */
      uint32_t queue_length;
      queue_length = g_queue_get_length (self->queue);
      while (queue_length >= self->length) {
        /* No space in the queue, wait for space */
        self->waiting_thread++;
        g_cond_wait (&self->cond, &self->lock);
        self->waiting_thread--;
        /* self->is_exit can be changed to true while we were waiting */
        if (self->is_exit) {
          break;
        }
        queue_length = g_queue_get_length (self->queue);
      }
    }

    if (!self->is_exit) {
      /* Push data at the tail of the queue, and signal any waiting thread */
      g_queue_push_tail (self->queue, data);
      g_cond_signal (&self->cond);
      ret = true;
    }
    g_mutex_unlock (&self->lock);
  }

  return ret;
}

/**
 *  @fn bool vvas_queue_enqueue (VvasQueue * vvas_queue, void * data)
 *  @param [in] vvas_queue  VvasQueue allocated using @ref vvas_queue_new
 *  @param [in] data        The data for the new element
 *  @return Returns TRUE is data is enquired, FALSE otherwise
 *  @brief  This API Adds a new element at the tail of the queue, this API will
 *          not block when queue is full.
 */
bool
vvas_queue_enqueue_noblock (VvasQueue * vvas_queue, void *data)
{
  VvasQueuePrivate *self = (VvasQueuePrivate *) vvas_queue;
  bool ret = true;

  if (!self || !data) {
    return false;
  }

  g_mutex_lock (&self->lock);

  if (self->length > 0) {
    /* Limited queue, check if there is space in the queue */
    uint32_t queue_length;
    queue_length = g_queue_get_length (self->queue);
    if (queue_length < self->length) {
      /* Queue has space, add data to the tail of the queue, and signal
       * waiting thread */
      g_queue_push_tail (self->queue, data);
      g_cond_signal (&self->cond);
    } else {
      /* Queue is already full, can't add this elements */
      ret = false;
    }
  } else {
    /* No limit on the queue length, add data to the tail of the queue and
     * signal waiting thread */
    g_queue_push_tail (self->queue, data);
    g_cond_signal (&self->cond);
  }
  g_mutex_unlock (&self->lock);
  return ret;
}

/**
 *  @fn void * vvas_queue_dequeue (VvasQueue * vvas_queue)
 *  @param [in] vvas_queue  VvasQueue allocated using @ref vvas_queue_new
 *  @return The data of the first element in the queue, or NULL if the queue is destroyed.
 *  @brief  This API removes the first element of the queue and returns its data.
 *          This API will block if the queue is empty. For non blocking dequeue use
 *          @ref vvas_queue_dequeue_noblock
 */
void *
vvas_queue_dequeue (VvasQueue * vvas_queue)
{
  VvasQueuePrivate *self = (VvasQueuePrivate *) vvas_queue;
  uint32_t queue_length;
  void *data = NULL;

  if (!self) {
    return NULL;
  }

  if (!self->is_exit) {
    g_mutex_lock (&self->lock);
    queue_length = g_queue_get_length (self->queue);
    while (!queue_length) {
      self->waiting_thread++;
      g_cond_wait (&self->cond, &self->lock);
      self->waiting_thread--;
      /* self->is_exit can be changed to true while we were waiting */
      if (self->is_exit) {
        break;
      }
      queue_length = g_queue_get_length (self->queue);
    }
    if (!self->is_exit) {
      data = g_queue_pop_head (self->queue);
      /* Wakeup blocked thread which may be waiting for free space in the queue */
      g_cond_signal (&self->cond);
    }
    g_mutex_unlock (&self->lock);
  }

  return data;
}

/**
 *  @fn void * vvas_queue_dequeue (VvasQueue * vvas_queue)
 *  @param [in] vvas_queue  VvasQueue allocated using @ref vvas_queue_new
 *  @return The data of the first element in the queue, or NULL if the queue is empty
 *  @brief  This API removes the first element of the queue and returns its data.
 */
void *
vvas_queue_dequeue_noblock (VvasQueue * vvas_queue)
{
  VvasQueuePrivate *self = (VvasQueuePrivate *) vvas_queue;
  void *data = NULL;

  if (!self) {
    return NULL;
  }

  g_mutex_lock (&self->lock);
  data = g_queue_pop_head (self->queue);
  if (data) {
    g_cond_signal (&self->cond);
  }
  g_mutex_unlock (&self->lock);

  return data;
}

/**
 *  @fn void * vvas_queue_dequeue_timeout (VvasQueue * vvas_queue, uint64_t timeout)
 *  @param [in] vvas_queue  VvasQueue allocated using @ref vvas_queue_new
 *  @return The data of the first element in the queue, If no data is received before
 *          the timeout, NULL is returned
 *  @brief  This API removes the first element of the queue and returns its data.
 *          If the queue is empty, it will block for \p timeout microseconds, or until
 *          data becomes available.
 */
void *
vvas_queue_dequeue_timeout (VvasQueue * vvas_queue, uint64_t timeout)
{
  VvasQueuePrivate *self = (VvasQueuePrivate *) vvas_queue;
  int64_t end_time;
  void *data = NULL;
  bool is_signalled;

  if (!self) {
    return NULL;
  }

  g_mutex_lock (&self->lock);
  data = g_queue_pop_head (self->queue);
  if (!data && !self->is_exit) {
    /* No data in the queue, wait for user given time */
    end_time = g_get_monotonic_time () + timeout;
    is_signalled = g_cond_wait_until (&self->cond, &self->lock, end_time);
    if (is_signalled) {
      if (g_queue_get_length (self->queue) > 0) {
        data = g_queue_pop_head (self->queue);
      }
    }
  }
  if (data) {
    /* We removed data from the queue, signal thread which may be waiting for
     * free space in the queue */
    g_cond_signal (&self->cond);
  }
  g_mutex_unlock (&self->lock);
  return data;
}
