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
 * DOC: VVAS Queue APIs
 * This file contains APIs for handling Queue related operations.
 */

#ifndef _VVAS_QUEUE_H_
#define _VVAS_QUEUE_H_

#include <stdbool.h>
#include <vvas_core/vvas_common.h>

#ifndef VVAS_UTILS_INCLUSION
#error "Don't include vvas_queue.h directly, instead use vvas_utils/vvas_utils.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/**
 *  typedef VvasQueue - Handle for VvasQueue instance.
 */
  typedef void VvasQueue;

/**
 *  vvas_queue_new () - Allocates a new VvasQueue.
 *  @length: Queue length, -1 for no limit on length.   
 *  Context: This API allocates a new VvasQueue. This instance
 *           must be freed using @vvas_queue_free.
 *  Return:  Handle for VvasQueue.
 */
  VvasQueue *vvas_queue_new (int32_t length);

/**
 *  vvas_queue_free () - Frees memory allocated for the VvasQueue.
 *  @vvas_queue: VvasQueue allocated using vvas_queue_new.
 *  Context: This API frees the memory allocated for the VvasQueue. 
 *           If queue elements contain dynamically-allocated memory,
 *           then they should be freed first.
 *  Return: none.
 */
 void vvas_queue_free (VvasQueue * vvas_queue);

/**
 * typedef VvasQueueDestroyNotify - Destroy Notification callback.
 * @data: data to be freeed. 
 * Return: void. 
*/
  typedef void (*VvasQueueDestroyNotify) (void *data);

/**
 *  vvas_queue_free_full () - Free's all the memory used by a VvasQueue.
 *  @vvas_queue: VvasQueue allocated using @vvas_queue_new.
 *  @free_func: Callback function which is called when a data element
 *              is destroyed. It is passed the pointer to the data 
 *              element and should free any memory and resources 
 *              allocated for it.  
 *  Context: This API frees all the memory used by a VvasQueue, and calls the
 *           specified destroy function on every element's data. free_func 
 *           should not modify the queue (eg, by removing the freed element from it).
 *  Return: None.
 */
  void vvas_queue_free_full (VvasQueue * vvas_queue,
      VvasQueueDestroyNotify free_func);

/**
 *  vvas_queue_clear () -  Removes all the elements in queue.
 *  @vvas_queue: VvasQueue allocated using  @vvas_queue_new.
 *  
 *  Context: This API removes all the elements in queue. If queue elements contain
 *           dynamically-allocated memory, they should be freed first.
 *  Return: None.
 */
  void vvas_queue_clear (VvasQueue * vvas_queue);

/**
 *  vvas_queue_clear_full () - Free's all the memory used by a VvasQueue.
 *  @vvas_queue: VvasQueue allocated using @vvas_queue_new.
 *  @free_func: Callback function which is called when a data element is freed.
 *              It is passed the pointer to the data element and should free any
 *              memory and resources allocated for it.
 *  
 *  Context: This API frees all the memory used by a VvasQueue, and calls the 
 *           provided free_func on each item in the VvasQueue.free_func should
 *           not modify the queue (eg, by removing the freed element from it).
 *  Return: None.
 */
  void vvas_queue_clear_full (VvasQueue * vvas_queue,
      VvasQueueDestroyNotify free_func);

/**
 *  vvas_queue_is_empty () - Check's if vvas_queue is empty/not.
 *  @vvas_queue: VvasQueue allocated using @vvas_queue_new.
 *  Context: This API is to check if vvas_queue is empty or not.
 *  Return: Returns TRUE if vvas_queue is empty.
 */
  bool vvas_queue_is_empty (VvasQueue * vvas_queue);

/**
 *  vvas_queue_get_length () - Get's queue length.
 *  @vvas_queue:  VvasQueue allocated using @vvas_queue_new.
 *  Context: This API is to get the vvas_queue's length.
 *  Return: Returns the number of items in the queue.
 */
  uint32_t vvas_queue_get_length (VvasQueue * vvas_queue);

/**
 * typedef VvasQueueFunc - Queue iteration callback.
 * @data: Queue handle. 
 * @udata: user data. 
 * Context:  Call back function.
 * Return: void.
*/
  typedef void (*VvasQueueFunc) (void *data, void *udata);

/**
 *  vvas_queue_for_each () - Callback function called for each element.
 *  @vvas_queue: VvasQueue allocated using @vvas_queue_new.
 *  @func: A callback function to be called for each element of the queue.
 *  @user_data: user data to be passed.  
 *  Context: This API Calls func for each element in the queue passing 
 *           user_data to the function. func should not modify the queue.
 *  Return: None.
 */
  void vvas_queue_for_each (VvasQueue * vvas_queue, VvasQueueFunc func,
      void *user_data);

/**
 *  vvas_queue_enqueue () - Adds a new Queue element at the tail.
 *  @vvas_queue: VvasQueue allocated using @vvas_queue_new.
 *  @data: The data for the new element.
 *  
 *  Context:This API Adds a new element at the tail of the queue, this API will
 *          block if the queue is full. For non blocking enqueue use 
 *          @vvas_queue_enqueue_noblock.
 *  Return: None.
 */
  bool vvas_queue_enqueue (VvasQueue * vvas_queue, void *data);

/**
 *  vvas_queue_enqueue_noblock () - API Adds a new element at the tail.
 *  @vvas_queue: VvasQueue allocated using @vvas_queue_new.
 *  @data: The data for the new element.  
 *  Context:This API Adds a new element at the tail of the queue, this API will
 *          not block when queue is full.
 *  Return: Returns TRUE is data is enquired, FALSE otherwise.
 */
  bool vvas_queue_enqueue_noblock (VvasQueue * vvas_queue, void *data);

/**
 *  vvas_queue_dequeue () - Removes the first element of the queue.
 *  @vvas_queue: VvasQueue allocated using @vvas_queue_new.
 *  
 *  Context: This API removes the first element of the queue and returns its data.
 *          This API will block if the queue is empty. For non blocking dequeue use
 *          @vvas_queue_dequeue_noblock.
 *  Return: The data of the first element in the queue, or NULL if the queue is empty.
 */
  void *vvas_queue_dequeue (VvasQueue * vvas_queue);

/**
 *  vvas_queue_dequeue_noblock () - Removes the first element & returns its data.
 *  @vvas_queue: VvasQueue allocated using @vvas_queue_new.  
 *  Context: This API removes the first element of the queue and returns its data.
 *  Return: The data of the first element in the queue, or NULL if the queue is empty.
 */
  void *vvas_queue_dequeue_noblock (VvasQueue * vvas_queue);

/**
 *  vvas_queue_dequeue_timeout () - Removes the first element of the queue.
 *  @vvas_queue:  VvasQueue allocated using @vvas_queue_new.
 *  @timeout: Time in microseconds to wait for data in the queue.
 *  
 *  Context: This API removes the first element of the queue and returns its data.
 *          If the queue is empty, it will block for timeout microseconds, or until
 *          data becomes available.
 *
 *  Return: 
 *  * The data of the first element in the queue.
 *  * If no data is received before timeout, NULL is returned.
 */
  void *vvas_queue_dequeue_timeout (VvasQueue * vvas_queue, uint64_t timeout);

#ifdef __cplusplus
}
#endif
#endif                          /* _VVAS_QUEUE_H_ */

