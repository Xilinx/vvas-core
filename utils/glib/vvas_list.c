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

/** @file vvas_list.c
 *  @brief Contains VvasList APIs, for now we have wrapped the GList from glib
 * */
#define VVAS_UTILS_INCLUSION
#include <vvas_utils/vvas_list.h>
#undef VVAS_UTILS_INCLUSION
#include <glib.h>

/**
 *  @brief Frees all the memory allocated and used by VvasList
 *  @param[in] pointer to @ref VvasList
 * */
void vvas_list_free(VvasList* list) {
  return g_list_free((GList *)list);
}

/**
 *  @brief Add a new element at the end of the list
 *  @param[in] Pointer to @ref VvasList in which element has to be added
 *  @param[in] Element to be added into the list
 *
 *  @return On success Returns the updated list.
 *          On Failure returns NULL
 * */
VvasList* vvas_list_append(VvasList* list, void *data){
  return (VvasList*) g_list_append((GList *)list, (gpointer) data);
}

/**
 *  @brief This find the number of elements in a @ref VvasList
 *  @param[in] Pointer to @ref VvasList
 *
 *  @return Number of element in the @ref VvasList
 * */
uint32_t vvas_list_length(VvasList* list){
  return (uint32_t) g_list_length((GList *)list);
}

/**
 *  @brief Gets the data of the element at the given position
 *  @param[in] Pointer to @ref VvasList
 *  @param[in] Postion of the element
 *
 *  @return on Success returns the pointer to Element data
 *          on Failure return NULL
 * */
void* vvas_list_nth_data(VvasList* list, uint32_t n){
  return (void *)g_list_nth_data((GList *)list, (guint)n);
}

/**
 *  @brief Removes an element from the list. If two element contains the same
 *  data only the first is removed. If none of the elements contains the data
 *  @ref VvasList is unchanged
 *  @param[in] Pointer to @ref VvasList
 *  @param[in] Pointer to the data to be removed. Data is owned by the caller
 *
 *  @return Updated @ref VvasList
 * */
VvasList* vvas_list_remove(VvasList* list, const void* data){
  return (VvasList *)g_list_remove((GList *)list, (gconstpointer)data);
}

/**
 *  @brief Gets the first element of the @ref VvasList
 *  @param[in] Pointer to @ref VvasList
 *
 *  @return pointer to the first elemenet of the @ref VvasList, i.e head
 * */
VvasList* vvas_list_first(VvasList* list){
  return (VvasList *)g_list_first((GList *)list);
}


/**
 *	@brief Call a function for each element of the VvasList
 * 	@param[in] A pointer to @ref VvasList
 *  @param[in] A callback function to be called for each element of the list
 *  @param[in] User data to pass to the function.
 */
void vvas_list_foreach(VvasList* list, VvasFunc func, void *data){
	return g_list_foreach((GList *)list, (GFunc) func, (gpointer) data);
}

/**
 *  @brief Frees all the memory allocated and used by VvasList
 *  @param[in] pointer to @ref VvasList
 *  @param[in] pointer to @ref vvas_list_free_full, pointer to destroy function 
 * */
void vvas_list_free_full(VvasList* list,vvas_list_free_notify func) {
  return g_list_free_full((GList *)list, (GDestroyNotify)func);
}

/**
 *  @brief create a deep copy of the list node passed
 *  @param[in] Pointer to @ref VvasList
 *  @param[in] Pointer to @ref vvas_list_copy_func 
 *  @param[in] Pointer to @ref  user data 
 *
 *  @return pointer to the first elemenet of the @ref VvasList, i.e head
 * */
VvasList* vvas_list_copy_deep(VvasList* list, vvas_list_copy_func func, void* data){
  return (VvasList *)g_list_copy_deep((GList *)list, func, data);
}

/**
 *  @fn void * vvas_list_find(VvasList * list, void * data)
 *  @brief Gets finds the element having data in the list
 *  @param[in] list VvasList
 *  @param[in] data data to look for
 *
 *  @return on Success returns the pointer to Element data found
 *          on Failure return NULL
 * */
void* vvas_list_find (VvasList * list, void * data) {
  return g_list_find ((GList *)list, data);
}

