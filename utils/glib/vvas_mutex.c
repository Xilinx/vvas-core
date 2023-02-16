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

#define VVAS_UTILS_INCLUSION
#include <vvas_utils/vvas_mutex.h>
#undef VVAS_UTILS_INCLUSION
#include <glib.h>

/**
 *  @fn void vvas_mutex_init(VvasMutex *mutex)
 *  @param [in] mutex - address of mutex object
 *  @brief This function initializes mutex.
 *  @return none
 */
void vvas_mutex_init(VvasMutex *mutex)
{
    return g_mutex_init((GMutex *)mutex);
}

/**
 *  @fn void vvas_mutex_lock(VvasMutex *mutex)
 *  @param [in] mutex - address of mutex object
 *  @brief This function Locks mutex. If mutex is already locked by another thread
 *         the current thread will block until mutex is unlocked by the
 *         other thread.
 *  @return none
 */
void vvas_mutex_lock(VvasMutex *mutex)
{
    return g_mutex_lock((GMutex *)mutex);
}

/**
 *  @fn void vvas_mutex_trylock(VvasMutex *mutex)
 *  @param [in] mutex - address of mutex object
 *  @brief This function will try to lock mutex. If mutex is already locked by
 *         another thread, it immediately returns FALSE 
 *  @return TRUE - if mutex lock is acquired. 
 *          FALSE - if failes to acquire lock. 
 */
bool vvas_mutex_trylock(VvasMutex *mutex)
{
    return g_mutex_trylock((GMutex *)mutex);
}

/**
 *  @fn void vvas_mutex_unlock(VvasMutex *mutex)
 *  @param [in] mutex - address of mutex object
 *  @brief This function unlocks mutex.
 *  @return none
 */
void vvas_mutex_unlock(VvasMutex *mutex)
{
    return g_mutex_unlock((GMutex *)mutex);
}

/**
 *  @fn void vvas_mutex_clear(VvasMutex *mutex)
 *  @param [in] mutex - address of mutex object
 *  @brief This function frees resources allocated to the mutex.
 *  @return none
 */
void vvas_mutex_clear(VvasMutex *mutex)
{
    return g_mutex_clear((GMutex *)mutex);
}

