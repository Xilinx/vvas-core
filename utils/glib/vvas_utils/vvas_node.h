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
 * DOC: VVAS Node APIs
 * This file contains APIs for handling Node related operations.
 */

#ifndef __VVAS_NODE_H__
#define __VVAS_NODE_H__

#include <stdint.h>
#include <stdbool.h>

#ifndef VVAS_UTILS_INCLUSION
#error "Don't include vvas_node.h directly, instead use vvas_utils/vvas_utils.h"
#endif

/* While converting to RST files, kernel-doc is not able to 
 * parse funtion pointer typedef with return type as void *.
 * Adding Macro to avoid parse warnings. 
 */
#ifndef VOID_POINTER
#define VOID_POINTER     void* 
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * typedef VvasTreeNode - Holds the reference to VvasTreeNode handle. 
 * */
typedef struct _VvasTreeNode VvasTreeNode;

/** 
 * struct _VvasTreeNode - This structure is for creating Node instance.
 * @data:  Handle for storing data.
 * @next: Handle for storing next node.
 * @prev: Handle for storing Previous node.
 * @parent: Handle for storing Parent node.
 * @children: Handle for storing children.
 * */
struct _VvasTreeNode {
  void*  data;
  VvasTreeNode *next;
  VvasTreeNode *prev;
  VvasTreeNode *parent;
  VvasTreeNode *children;
};

/**
 * enum VvasTreeNodeTraverseType - This enum is defines node traverse type.
 * @IN_ORDER: Visits a node's left child first, then the node itself, then its right child.
 * @PRE_ORDER: Visits a node, then its children.
 * @POST_ORDER: Visits a node's children, then the node itself.
 * */
typedef enum {
  IN_ORDER,
  PRE_ORDER,
  POST_ORDER
} VvasTreeNodeTraverseType;

/**
 * enum VvasTreeNodeTraverseFlags - This enum defines which nodes to be visited.
 * @TRAVERSE_LEAFS: Only leaf nodes should be visited. 
 * @TRAVERSE_NON_LEAFS: Only non-leaf nodes should be visited.
 * @TRAVERSE_ALL: All nodes should be visited.
 * */
typedef enum {
  TRAVERSE_LEAFS      = 1 << 0,
  TRAVERSE_NON_LEAFS  = 1 << 1,
  TRAVERSE_ALL        = TRAVERSE_LEAFS | TRAVERSE_NON_LEAFS
} VvasTreeNodeTraverseFlags;

/**
 *  vvas_treenode_new() - Creates new tree node.
 *  @data: Address of data.
 *  Context: This function creates new tree node.
 *  
 *  Return: 
 *  * On Success returns node address.
 *  * On Failure returns NULL.
 */ 
VvasTreeNode* vvas_treenode_new(void* data); 
    
/** 
 *  vvas_treenode_destroy() - Deallocates tree node.
 *  @node: Address of node.
 *  Context: This function deallocates node.
 *  Return: None.
 */ 
void vvas_treenode_destroy(VvasTreeNode* node);
    
/** 
 *  typedef vvas_treenode_copy_func - Copy node data. 
 *  @src: Data to be copied.
 *  @data: Additional data.
 *  
 *  Context: This function is used to copy node data while performing deep-copy 
 *          of a tree node.
 *  Return: 
 *  * On Success returns address of the new node.
 *  * On Failure returns NULL. 
 */ 
typedef VOID_POINTER (*vvas_treenode_copy_func)(const void* src, void* data);
    
/** 
 *  VvasTreeNode* vvas_treenode_copy_deep() - Deep copies node. 
 *  @node: Address of source node to copy data from.
 *  @func: Address of function which is called to copy data in each node.
 *  @data: Additional data to be passed to func.
 *  Context: This function recursively deep copies node data.
 *   
 *  Return: 
 *  * On Success returns address of the new node which contain copies of data.
 *  * On Failure returns NULL. 
 */ 
VvasTreeNode* vvas_treenode_copy_deep(VvasTreeNode* node,
                                       vvas_treenode_copy_func func,
                                       void *data);
                                   
/** 
 *  typedef vvas_treenode_traverse_func - To traverse tree node.
 *  @Node: Address of node.
 *  @data: User data to be passed to the node.
 *  Context: This function is passed for vvas_treenode_traverse and is called for  
 *          each node visited, traverse can be halted by returning TRUE. 
 *   
 *  Return: 
 *  * TRUE to stop the traverse.
 *  * FALSE to continue traverse.
 */ 
typedef bool (*vvas_treenode_traverse_func)(const VvasTreeNode *Node, void *data);
    
/** 
 *  vvas_treenode_traverse() - Traverse a tree node.
 *  @node: Node of the tree to start traversing.
 *  @traverse_order: Order in which nodes are to be traversed in a tree.
 *  @traverse_flags: Order in which children of nodes to be visited.
 *  @max_depth: Maximum depth of traversal, if max_depth is -1 then all nodes in tree are visited.
 *  @func: Function to be called for each node visit. 
 *  @data: User data to be passed to the function.
 *  
 *  Context: This function used to traverse a tree starting at the given node.
 *  Return: None.
 */ 
void vvas_treenode_traverse(VvasTreeNode *node,
                             VvasTreeNodeTraverseType traverse_order, 
                             VvasTreeNodeTraverseFlags traverse_flags,
                             int32_t max_depth,
                             vvas_treenode_traverse_func func,
                             void *data);
    
/** 
 *  vvas_treenode_append() - Inserts node from bottom. 
 *  @parent_node: Parent node.
 *  @child_node: child node.
 *  Context: This function used to insert the node as the last child of the given parent node.
 *  
 *  Return: None.
 */ 
void vvas_treenode_append(VvasTreeNode *parent_node, VvasTreeNode *child_node);
    
/** 
 *  typedef vvas_treenode_traverse_child_func - To Traverse child node func callback.
 *  @Node: Address of node.
 *  @data: User data to be passed to the node.
 *  
 *  Context: This function is passed for vvas_treenode_traverse_child, It is called with each child node 
 *          together with the user data passed.
 *  Return: None.
 */ 
typedef void (*vvas_treenode_traverse_child_func)(VvasTreeNode *Node, void *data);
    
/** 
 *  vvas_treenode_traverse_child() - Traverses child nodes. 
 *  @node: Parent node address.
 *  @traverse_flags: Order in which children of nodes to be visited.
 *  @func: Function to be called for each child node visit. 
 *  @data: User data to be passed to the function.
 *  
 *  Context:Calls given function for each child node of the Parent node. It 
 *          does not descend beneath the child nodes.
 *  Return: None.
 */ 
void vvas_treenode_traverse_child(VvasTreeNode *node,
                                   VvasTreeNodeTraverseFlags traverse_flags,
                                   vvas_treenode_traverse_child_func func,
                                   void *data);
                             
/** 
 *  vvas_treenode_get_depth() - Gets the depth of the node. 
 *  @node: Address of the node for which depth to be found out. 
 *  
 *  Context: This function used to get the depth of the node.        
 *  Return: 
 *  * On Success returns depth of the given node.
 *  * On Failure returns 0, if node is null. The root node has 
 *    depth of 1, for children of the root node the depth is 
 *    2 and so on.
 */ 
uint32_t vvas_treenode_get_depth(VvasTreeNode *node);
    
/** 
 *  vvas_treenode_get_max_height() - Gets the max height of the node.
 *  @node: Address of the node.
 *  
 *  Context:This function used to get the maximum distance of the given node 
 *          from all leaf nodes. 
 *  Return: 
 *  * On Success returns max distance of the given node from all leaf node.
 *  * On Failure returns 0, if node is null.If node has no children, 1 
 *    is returned. If node has  children, 2 is returned.
 */ 
uint32_t vvas_treenode_get_max_height(VvasTreeNode *node);
    
/** 
 *  vvas_treenode_get_n_childnodes() - Gets number of child nodes.  
 *  @root_node: Address of the parent node.
 *  
 *  Context: This function used to get the number of child nodes of the given node.
 *  Return: 
 *  * On Success returns number of child nodes of the parent node.
 *  * On Failure returns 0, if node is null.
 */ 
uint32_t vvas_treenode_get_n_childnodes(VvasTreeNode *root_node);
    
/** 
 *  vvas_treenode_insert_before() - inserts before given node.
 *  @parent: Address of the parent node.
 *  @sibling: Address of the sibling node.
 *  @node: Address of the  node to be inserted.
 *  Context: This function will insert before the tree node.
 *  
 *  Return: 
 *  * On Success returns number of child nodes of the parent node.
 *  * On Failure returns 0, if node is null.
 */
VvasTreeNode* vvas_treenode_insert_before(VvasTreeNode *parent,
                                          VvasTreeNode* sibling,
                                          VvasTreeNode* node);
#ifdef __cplusplus
}
#endif

#endif
