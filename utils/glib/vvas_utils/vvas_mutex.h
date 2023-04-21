
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
 *
 */


/** 
 * DOC: VVAS Mutex APIs
 * This file contains APIs for handling synchronization.
 */

#ifndef __VVAS_MUTEX_H__
#define __VVAS_MUTEX_H__

#include <stdint.h>
#include <stdbool.h>

#ifndef VVAS_UTILS_INCLUSION
#error "Don't include vvas_mutex.h directly, instead use vvas_utils/vvas_utils.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * typedef VvasMutex - Holds the reference to Mutex handle. 
 * */
typedef union _VvasMutex VvasMutex;

/** 
 *  union _VvasMutex - This union is for creating Mutex instance.
 *  @p: Pointer for Mutex Handle.
 *  @i: Mutex count.
 */
union _VvasMutex {
  void*  p;
  int32_t i[2];
};

/**
 *  vvas_mutex_init() - Initializes mutex.
 *  @mutex: Handle of mutex.
 *  Return: None.
 */
void vvas_mutex_init(VvasMutex *mutex);

/**
 *  vvas_mutex_lock() - Locks mutex.
 *  @mutex: Handle of mutex.
 *  
 *  Context:This function Locks mutex. If mutex is already locked by another thread
 *          the current thread will block until mutex is unlocked by the
 *          other thread.
 *  Return: None.
 */
void vvas_mutex_lock(VvasMutex *mutex);

/**
 *  vvas_mutex_trylock() - Try's lock on Mutex.
 *  @mutex: Address of mutex object.
 *  
 *  Context: This function will try to lock mutex. If mutex is already locked by
 *          another thread, it immediately returns FALSE.
 *  Return:
 *  * TRUE - If mutex lock is acquired. 
 *  * FALSE - If failes to acquire lock. 
 */
bool vvas_mutex_trylock(VvasMutex *mutex);

/**
 *  vvas_mutex_unlock() - Unlocks Mutex.
 *  @mutex: Address of mutex object.
 *  
 *  Context: This function unlocks mutex.
 *  Return: None.
 */
void vvas_mutex_unlock(VvasMutex *mutex);

/**
 *  vvas_mutex_clear() - Clears Mutex.
 *  @mutex: Address of mutex object.
 *  
 *  Context: This function frees resources allocated to the mutex.
 *  Return: None.
 */
void vvas_mutex_clear(VvasMutex *mutex);


#ifdef __cplusplus
}
#endif

#endif
