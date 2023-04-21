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

 
/** 
 * DOC: VVAS Hash APIs
 * This file contains APIs for handling HashMap related operations.
 */

#ifndef __VVAS_HASH_H__
#define __VVAS_HASH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#ifndef VVAS_UTILS_INCLUSION
#error "Don't include vvas_hash.h directly, instead use vvas_utils/vvas_utils.h"
#endif

/**
 * struct _VvasHashTable - Holds hash table handle.
 * */
typedef struct _VvasHashTable {
  /* No available field*/
} VvasHashTable;

/**
 * struct VvasHashTableIter - Holds hash table iteration information.
 * @dummy1: Pointer handle, this is internal member users need not pass value. 
 * @dummy2: Pointer handle, this is internal member users need not pass value.
 * @dummy3: Pointer handle, this is internal member users need not pass value.
 * @dummy4: int value, this is internal member users need not pass value.
 * @dummy5: bool value, this is internal member users need not pass value.
 * @dummy6: Pointer handle, this is internal member users need not pass value.
 */
typedef struct VvasHashTableIter 
{
  void *dummy1;
  void *dummy2;
  void *dummy3;
  int dummy4;
  bool dummy5;
  void *dummy6;
}VvasHashTableIter;

/**
 *  typedef VvasHashFunc - Function pointer to create hash value.
 *  @key: Key value.
 *
 *  Return: Hash value.
 * */
typedef uint32_t (* VvasHashFunc) (const void * key);

/**
 *  typedef VvasEqualFunc - Function pointer to compare key values.
 *  @a: Key value to be compared.
 *  @b: Key value to be compared.
 *
 *  Return: True if both are equal.
 * */
typedef bool (* VvasEqualFunc) (const void * a, const void * b);

/**
 *  typedef VvasDestroyNotify - Function pointer to be called when Destoryed.
 *  @data: Data to be destroyed.
 *
 *  Return: None.
 * */
typedef void (* VvasDestroyNotify) ( void * data);

/**
 *  typedef VvasHRFunc - Function pointer to called for each key value pair.
 *  @key: Key value.
 *  @value: Hash value.
 *  @user_data: User data to be passed.
 *
 *  Return: False to end loop.
 * */
typedef bool (* VvasHRFunc) (void * key, void * value, void * user_data);

/**
 *  vvas_direct_hash() - Converts gpointer to hash.
 *  @key:  Key Value.
 *
 *  Return: A Hash value.
 * */
uint32_t vvas_direct_hash(const void * key);

/**
 *  vvas_int_hash() - Converts gpointer to int value.
 *  @key:  Key Value.
 *
 *  Return: A Hash value.
 * */
uint32_t vvas_int_hash(const void * key);

/**
 *  vvas_str_hash() - Converts a string to hash value.
 *  @key:  Key Value.
 *
 *  Return: A Hash value.
 * */
uint32_t vvas_str_hash(const void * key);

/**
 *  vvas_direct_equal() - Compares pointers.
 *  @a:  Pointer to be compared.
 *  @b:  Pointer to be compared.
 *
 *  Return: True if equal.
 * */
bool vvas_direct_equal(const void * a, const void * b);

/**
 *  vvas_int_equal() - Compares two int values.
 *  @a:  Pointer to be compared.
 *  @b:  Pointer to be compared.
 *
 *  Return: True if equal.
 * */
bool vvas_int_equal(const void * a, const void * b);

/**
 *  vvas_int64_equal() - Compares two int64 values.
 *  @a:  Pointer to be compared.
 *  @b:  Pointer to be compared.
 *
 *  Return: True if equal.
 * */
bool vvas_int64_equal(const void * a, const void * b);

/**
 *  vvas_double_equal() - Compares two double values.
 *  @a:  Pointer to be compared.
 *  @b:  Pointer to be compared.
 *
 *  Return: True if equal.
 * */
bool vvas_double_equal(const void * a, const void * b);

/**
 *  vvas_str_equal() - Compares two strings. 
 *  @a:  Pointer to be compared.
 *  @b:  Pointer to be compared.
 *  
 *  Return: True if equal.
 * */
bool vvas_str_equal(const void * a, const void * b);

/**
 * vvas_hash_table_new() - Creates a new VvasHashTable with a 
 *                         reference count of 1.
 * @hash_func:  A function to create a hash value from a key.
 * @key_eq_func: A function to create a hash value from a key.
 *
 * Return: A new  VvasHashTable.
 * */
VvasHashTable* vvas_hash_table_new(VvasHashFunc hash_func,
                VvasEqualFunc key_eq_func);

/**
 * vvas_hash_table_new_full() - Creates a new VvasHashTable.
 * @hash_func: A function to create a hash value from a key.
 * @key_eq_func: A function to create a hash value from a key.
 * @key_destroy_fn: function to free the memory allocated for the key used
 *                when removing the entry from the VvasHashTable, or NULL 
 *                if you don’t want to supply such a function.
 * @value_destroy_fn: A function to free the memory allocated for the value
 *                used when removing the entry from the GHashTable, or NULL
 *                if you don’t want to supply such a function. 
 *                
 * Context: Creates a new VvasHashTable like vvas_hash_table_new() 
 *          with a reference count of 1 and allows to specify functions 
 *          to free the memory allocated for the key and value that get 
 *          called when removing the entry from the VvasHashTable.
 *          
 * @hash_func: A function to create a hash value from a key.                
 * Return: A new VvasHashTable.
 * */
VvasHashTable* vvas_hash_table_new_full(VvasHashFunc hash_func,
                VvasEqualFunc key_eq_func, VvasDestroyNotify key_destroy_fn,
                VvasDestroyNotify value_destroy_fn);

/**
 * vvas_hash_table_insert() - Inserts a new key and value into a  
 *                            VvasHashTable. 
 * @hash_table: Handle for VvasHashTable.
 * @key: A key to insert.
 * @value: The value to associate with the key.
 * 
 * Return: TRUE if the key did not exist yet.
 * */
bool vvas_hash_table_insert(VvasHashTable* hash_table, void * key,
				void * value);

/**
 *  vvas_hash_table_lookup() - Looks up key in Hash Table
 *  @hash_table: Handle for VvasHashTable.
 *  @key: The key to look up.
 *  
 *  Return: Associates value or NULL if key is not found.
 * */
void * vvas_hash_table_lookup(VvasHashTable* hash_table, const void * key);

/**
 *  vvas_hash_table_contains() - Checks if key exit in the hash table
 *  @hash_table: Handle for VvasHashTable.
 *  @key: A key to check.
 *  
 *  Return: TRUE if key found, FALSE if key not found.
 * */
bool vvas_hash_table_contains(VvasHashTable* hash_table, void * key);

/**
 *  vvas_hash_table_size() - Returns number of elements in the hash table
 *  @hash_table: Handle for VvasHashTable.
 *  Return: Number of element in the hash table.
 * */
uint32_t vvas_hash_table_size(VvasHashTable* hash_table);

/**
 *  vvas_hash_table_remove() - Removes a key and its associated value 
 *                             from a hash table.
 *  @hash_table: Handle for VvasHashTable.
 *  @key: The Key to remove.
 *  
 *  Return: TRUE if key was found and removed from hash table.
 * */
bool vvas_hash_table_remove(VvasHashTable* hash_table, void * key);

/**
 *  vvas_hash_table_foreach_remove() - Calls the function for each remove
 *  @hash_table: Handle for VvasHashTable.
 *  @func: The function to call for each key/value pair.
 *  @user_data: user data to be passed to the function.
 *  
 *  Context: Calls the given function for each key/value pair in the hash table.
 *           if function returns TRUE, then key/value pair is removed from 
 *           the hash table.
 *  Return: The number of key/value pair removed.
 * */
uint32_t vvas_hash_table_foreach_remove(VvasHashTable* hash_table,
            VvasHRFunc func, void * user_data);

/**
 *  vvas_hash_table_remove_all() - Removes all key/value from the hash table
 *  @hash_table: Handle for VvasHashTable.
 *  
 *  Return: None.
 * */ 
void vvas_hash_table_remove_all(VvasHashTable* hash_table);

/**
 *  vvas_hash_table_unref() - Decrement the reference count by one. 
 *  @hash_table: Handle for VvasHashTable.
 *  Context: Decrement the reference count by one. 
 *           Once the reference count drops to zero all the key/value 
 *           will be destroyed.
 *  Return: None.
 * */
void vvas_hash_table_unref(VvasHashTable* hash_table);

/**
 *  vvas_hash_table_destroy() - Destroys all keys and values in hash table
 *  @hash_table: Handle for VvasHashTable.
 * 
 *  Context: Destroys all keys and values in the VvasHashTable and decrements 
 *           its reference count by 1. If keys and/or values are dynamically 
 *           allocated, you should either free them first or create the 
 *           VvasHashTable with destroy notifiers using 
 *           vvas_hash_table_new_full(). In the latter case the destroy
 *           functions you supplied will be called on all keys and values 
 *           during the destruction phase.
 *  
 *  Return: None.
 * */
void vvas_hash_table_destroy(VvasHashTable* hash_table);

/**
 *  vvas_hash_table_iter_init() - Returns the iterator for hash table.   
 *  @hash_table: Handle for VvasHashTable.
 *  @iter:  Handle for table iterator.
 *  
 *  Return: None.
 * */
void vvas_hash_table_iter_init(VvasHashTable* hash_table, VvasHashTableIter *iter);

/**
 * vvas_hash_table_iter_next() - Allows to iterate through the table. 
 * @iter: Handle for VvasHashTableIter.
 * @key: Pointer to update.
 * @value: Pointer to update.
 * Context: Allows to iterate through the table and updates 
 *          information in Key and values params passed.
 * Return: None.
 * */
bool vvas_hash_table_iter_next(VvasHashTableIter *iter, void **key,  void **value);

#ifdef __cplusplus
}
#endif
#endif /*#ifndef __VVAS_HASH_H__*/
