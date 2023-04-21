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
 * DOC: VVAS List APIs
 * This file contains APIs for handling List related operations.
 */

#ifndef __VVAS_LIST_H__
#define __VVAS_LIST_H__

#include <stdint.h>
#ifndef VVAS_UTILS_INCLUSION
#error "Don't include vvas_list.h directly, instead use vvas_utils/vvas_utils.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* While converting to RST files, kernel-doc is not able to 
 * parse funtion pointer typedef with return type as void *.
 * Adding Macro to avoid parse warnings. 
 */
#ifndef VOID_POINTER
#define VOID_POINTER     void* 
#endif

/**
 *  struct VvasList - Data structure for the list element.
 *  @data: Holds the address of data.
 *  @next: Holds the address of next element in list.
 *  @prev: Holds the address of previous element in list.
 * */
typedef struct VvasList {
  void* data;
  struct VvasList* next;
  struct VvasList* prev;
} VvasList;

/**
 *  vvas_list_free() - Frees all the memory allocated and used by VvasList.
 *  @list: Pointer to  VvasList.
 *  
 *  Return: None
 * */
void vvas_list_free(VvasList* list);

/**
 *  vvas_list_append() - Add a new element at the end of the list.
 *  @list: Pointer to  VvasList in which element has to be added.
 *  @data: Element to be added into the list.
 *
 *  Return: 
 *  * On success Returns the updated list.
 *  * On Failure returns NULL.
 * */
VvasList* vvas_list_append(VvasList* list, void *data);

/**
 *  vvas_list_length() - Returns number of elements in a  VvasList
 *  @list: Pointer to  VvasList.
 *
 *  Return: Number of element in the  VvasList.
 * */
uint32_t vvas_list_length(VvasList* list);

/**
 *  vvas_list_find() - Finds the element having data in the list.
 *  @list: Pointer to  VvasList.
 *  @data: Data to look for.
 *
 *  Return:
 *  * On Success returns the pointer to Element data found.
 *  * On Failure return NULL.
 */
void* vvas_list_find (VvasList * list, void * data);

/**
 *  vvas_list_nth_data() - Gets the data of the element at the given position.
 *  @list: Pointer to  VvasList.
 *  @n: Position of the element.
 *
 *  Return: 
 *  * On Success returns the pointer to Element data.
 *  * On Failure return NULL.
 * */
void* vvas_list_nth_data(VvasList* list, uint32_t n);

/**
 *  vvas_list_remove() - Removes element from given list. 
 *  @list: Pointer to  VvasList.
 *  @data: Pointer to the data to be removed. Data is owned by the caller
 *
 *  Return: Updated  VvasList
 * */
VvasList* vvas_list_remove(VvasList* list, const void* data);

/**
 *  vvas_list_first() - Gets the first element of the  VvasList.
 *  @list: Pointer to  VvasList.
 *
 *  Return: Pointer to the first element of the  VvasList, i.e head.
 * */
VvasList* vvas_list_first(VvasList* list);

/**
 *  typedef VvasFunc - Call back function for vvas_list_foreach.
 *  @data: List element data handle.
 *  @udata: User data.
 *
 *  Return: None
 */
typedef void (* VvasFunc)(void *data, void *udata);


/**
 *  vvas_list_foreach() - Call's the function for each element of the VvasList.
 *  @list: A pointer to  VvasList.
 *  @func: A callback function to be called for each element of the list.
 *  @data: User data to pass to the function.
 * */
void vvas_list_foreach(VvasList* list, VvasFunc func, void *data);

/**
 *  typedef vvas_list_free_notify - This is the function prototype to be 
 *  passed for vvas_list_free_full.
 *  @data: Data handle to be freed
 *
 *  Return: None
 * */
typedef void (*vvas_list_free_notify)(void *data);

/**
 *  vvas_list_free_full() - Frees all the memory allocated and used by VvasList.
 *  @list: Pointer to  VvasList.
 *  @func: Pointer to  vvas_list_free_full, pointer to destroy function.
 *  
 *  Return: None
 * */
void vvas_list_free_full(VvasList* list, vvas_list_free_notify func);

/**
 *  typedef vvas_list_copy_func - This function will be called for list copy.
 *  @src_list: Source data handle. 
 *  @data: User data.
 *
 *  Return: New copied list.
 * */
typedef VOID_POINTER (*vvas_list_copy_func) (const void* src_list, void* data);

/**
 *  vvas_list_copy_deep - Performs deep copy of the list node passed.
 *  @list: Pointer to  VvasList.
 *  @func: Pointer to  vvas_list_copy_func.
 *  @data: Pointer to user data. 
 *
 *  Return: Pointer to the first element of the  VvasList, i.e head.
 * */
VvasList* vvas_list_copy_deep(VvasList* list, vvas_list_copy_func func, void* data);

#ifdef __cplusplus
}
#endif
#endif /*#ifndef __VVAS_LIST_H__*/
