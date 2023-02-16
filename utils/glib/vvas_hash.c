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

/** @file vvas_hash.c
 *  @brief Contains VvasHashTable APIs, for now we have wrapped the GHashTable
 *  from glib
 **/
#define VVAS_UTILS_INCLUSION
#include <vvas_utils/vvas_hash.h>
#undef VVAS_UTILS_INCLUSION
#include <glib.h>

struct VvasHashTable {
  /* No available field*/
};

uint32_t vvas_direct_hash(const void * key) {
  return (uint32_t)g_direct_hash((gconstpointer) key);
}

uint32_t vvas_int_hash(const void * key) {
  return (uint32_t)g_int_hash((gconstpointer) key);
}

uint32_t vvas_str_hash(const void * key) {
  return (uint32_t)g_str_hash((gconstpointer) key);
}

bool vvas_direct_equal(const void * a, const void * b) {
	return (bool) g_direct_equal((gconstpointer) a, (gconstpointer) b);
}

bool vvas_int_equal(const void * a, const void * b) {
	return (bool) g_int_equal((gconstpointer) a, (gconstpointer) b);
}

bool vvas_int64_equal(const void * a, const void * b) {
	return (bool) g_int64_equal((gconstpointer) a, (gconstpointer) b);
}

bool vvas_double_equal(const void * a, const void * b) {
	return (bool) g_double_equal((gconstpointer) a, (gconstpointer) b);
}

bool vvas_str_equal(const void * a, const void * b) {
	return (bool) g_str_equal((gconstpointer) a, (gconstpointer) b);
}

/**
 *  @brief Creates a new VvasHashTable with a reference count of 1.
 *  @param A function to create a hash value from a key.
 *  @param A function to create a hash value from a key.
 *
 *  @return A new @ref VvasHashTable
 * */
VvasHashTable* vvas_hash_table_new(VvasHashFunc hash_func,
                VvasEqualFunc key_eq_func){
  return (VvasHashTable *) g_hash_table_new((GHashFunc) hash_func,
            (GEqualFunc) key_eq_func);
}

/**
 *  @brief Creates a new VvasHashTable like vvas_hash_table_new() with a
 *  reference count of 1 and allows to specify functions to free the memory
 *  allocated for the key and value that get called when removing the entry
 *  from the VvasHashTable.
 *  @param A function to create a hash value from a key
 *  @param A function to create a hash value from a key
 *  @param A function to free the memory allocated for the key used when
 *  removing the entry from the VvasHashTable, or NULL if you don’t want to
 *  supply such a function.
 *  @param A function to free the memory allocated for the value used when
 *  removing the entry from the GHashTable, or NULL if you don’t want to supply
 *  such a function.
 *
 *  @return A new @ref VvasHashTable
 * */
VvasHashTable* vvas_hash_table_new_full(VvasHashFunc hash_func,
                VvasEqualFunc key_eq_func, VvasDestroyNotify key_destroy_fn,
                VvasDestroyNotify value_destroy_fn){
  return (VvasHashTable *) g_hash_table_new_full((GHashFunc) hash_func,
              (GEqualFunc) key_eq_func, (GDestroyNotify) key_destroy_fn,
              (GDestroyNotify) value_destroy_fn);
}

/**
 *  @brief Inserts a new key and value into a @ref VvasHashTable
 *  @param @ref VvasHashTable
 *  @param A key to insert.
 *  @param The value to associate with the key.
 *
 *  @return TRUE if the key did not exist yet.
 * */
bool vvas_hash_table_insert(VvasHashTable* hash_table, void * key,
				void * value){
  return (bool) g_hash_table_insert((GHashTable *) hash_table, (gpointer) key,
            (gpointer) value);
}

/**
 *  @brief Looks up key in Hash Table
 *  @param @ref VvasHashTable
 *  @param The key to look up
 *
 *  @return assosiates value or NULL if key is not found
 *
 * */
void * vvas_hash_table_lookup(VvasHashTable* hash_table, const void * key){
  return (gpointer) g_hash_table_lookup((GHashTable *) hash_table,
              (gconstpointer) key);
}

/**
 *  @brief Checks if key exit in the hash table
 *  @param @ref VvasHashTable
 *  @param A key to check
 *
 *  @return TRUE if key found, FALSE if key not found
 * */
bool vvas_hash_table_contains(VvasHashTable* hash_table, void * key){
  return (bool) g_hash_table_contains((GHashTable *) hash_table, (gpointer)key);
}

/**
 *  @brief Returns number of elements in the hash table
 *  @param @ref VvasHashTable
 *
 *  @return Number of element in the hash table
 * */
uint32_t vvas_hash_table_size(VvasHashTable* hash_table){
  return (uint32_t) g_hash_table_size((GHashTable *) hash_table);
}

/**
 *  @brief Removes a key and its associated value from a hash table
 *  @param @ref VvasHashTable
 *  @param The Key to remove
 *
 *  @return TRUE if key was found and removed from hash table
 * */
bool vvas_hash_table_remove(VvasHashTable* hash_table, void * key){
  return (bool) g_hash_table_remove((GHashTable *) hash_table, (gpointer)key);
}

/**
 *  @brief Calls the given function for each key/value pair in the hash table.
 *  if function returns TRUE, then key/value pair is removed from the hasj table
 *  @param @ref VvasHashTable
 *  @param The function to call for each key/value pair.
 *  @param user data to be passed to the function
 *
 *  @return The number of key/value pair removed
 * */
uint32_t vvas_hash_table_foreach_remove(VvasHashTable* hash_table,
            VvasHRFunc func, void * user_data){
  return (uint32_t) g_hash_table_foreach_remove((GHashTable *) hash_table,
            (GHRFunc) func, (gpointer) user_data);
}

/**
 *  @brief Removes all key/value from the hash table
 *  @param @ref VvasHashTable
 * */
void vvas_hash_table_remove_all(VvasHashTable* hash_table){
  return g_hash_table_remove_all((GHashTable *) hash_table);
}

/**
 *  @brief Decrement the refrence count by one. Once the refrence count drops to
 *  zero all the key/value will be destroyed.
 *  @param @ref VvasHashTable
 * */
void vvas_hash_table_unref(VvasHashTable* hash_table){
  return g_hash_table_unref((GHashTable *)hash_table);
}

/**
 *  @brief Destroys all keys and values in the VvasHashTable and decrements 
 *  its reference count by 1. If keys and/or values are dynamically allocated,
 *  you should either free them first or create the VvasHashTable with destroy
 *  notifiers using vvas_hash_table_new_full(). In the latter case the destroy
 *  functions you supplied will be called on all keys and values during the
 *  destruction phase.
 *  @param @ref VvasHashTable
 * */
void vvas_hash_table_destroy(VvasHashTable* hash_table){
  return g_hash_table_destroy((GHashTable *) hash_table);
}

/**
 *  @brief returns the iterator for hastable.   
 *  @param @ref vvashashtable
 * */
void vvas_hash_table_iter_init(VvasHashTable *hash_map, VvasHashTableIter *iter) {

  return g_hash_table_iter_init((GHashTableIter * )iter, (GHashTable *) hash_map);
}


/**
 *  @brief Allows to iterate through the table and updates information in Key 
 *  and values params passed. 
 *  @param @ref VvasHashTable
 * */
bool vvas_hash_table_iter_next(VvasHashTableIter *iter, void **key,  void **value) {
  return g_hash_table_iter_next((GHashTableIter * )iter, key, value ); 
}


