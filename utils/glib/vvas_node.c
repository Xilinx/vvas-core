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
#include <vvas_utils/vvas_node.h>
#undef VVAS_UTILS_INCLUSION
#include <glib.h>
/**
 *  \brief This function creates new tree node.
 *  
 *  \param [in] Address of data.
 *  \return On Success returns node address.
 *          On Failure returns NULL.
 */
VvasTreeNode* vvas_treenode_new(void* data)
{
    return (VvasTreeNode *)g_node_new(data);
}
/**
 *  \brief This function deallcates node memory.
 *  
 *  \param [in] Address of node to be deallocated.
 *  \return none.
 */
void vvas_treenode_destroy(VvasTreeNode* node)
{
  g_node_destroy((GNode *)node);  
}

/**
 *  \brief This function recursively deep copies node data.
 *  
 *  \param [in] Address of source node to copy data from.
 *  \param [in] Address of function which is called to copy data in each node.
 *  \param [in] Additinal data to be passed to func.
 *  
 *  \return On Success returns address of the new node which contain copies of data.
 *          On Failure returns NULL. 
 */
VvasTreeNode* vvas_treenode_copy_deep(VvasTreeNode* node, vvas_treenode_copy_func func, void *data)
{
   return (VvasTreeNode*) g_node_copy_deep((GNode *)node, func, data);
}

/**
 *  \brief This function used to traverse a tree starting at the given node.
 *  
 *  \param [in] Node of the tree to start traversing.
 *  \param [in] Order in which nodes are to be traversed in a tree.
 *  \param [in] Order in which children of nodes to be visited.
 *  \param [in] Maximum depth of traversal, if max_depth is -1 then all nodes in tree are visited.
 *  \param [in] Function to be called for each node visit. 
 *  \param [in] User data to be passed to the function.
 *
 *  \return none.
 */
void vvas_treenode_traverse(VvasTreeNode *node, VvasTreeNodeTraverseType traverse_order, 
                            VvasTreeNodeTraverseFlags traverse_flags, int32_t max_depth,
                            vvas_treenode_traverse_func func, void *data)
{
 g_node_traverse((GNode*) node, (GTraverseType)traverse_order, (GTraverseFlags) traverse_flags,
                             max_depth, ( GNodeTraverseFunc )func, data);
}



/**
 *  \brief This function used to insert the node as the last child of the given parent node.
 *  
 *  \param [in] Parent node.
 *  \param [in] child node.
 *         
 *  \return none
 */
void vvas_treenode_append(VvasTreeNode *parent_node, VvasTreeNode *child_node)
{  
  g_node_append((GNode*)parent_node,(GNode*)child_node);  
}

/**
 *  \brief Calls given function for each child node of the Parent node. It 
 *         does not descend beneath the child nodes.
 *  
 *  \param [in] Parent node address.
 *  \param [in] Order in which children of nodes to be visited.
 *  \param [in] Function to be called for each child node visit. 
 *  \param [in] User data to be passed to the function.
 *
 *  \return none.
 */
void vvas_treenode_traverse_child(VvasTreeNode *node, VvasTreeNodeTraverseFlags traverse_flags,
                                  vvas_treenode_traverse_child_func func, void *data)
{
  g_node_children_foreach ((GNode *)node,(GTraverseFlags)traverse_flags, (GNodeForeachFunc )func, data);

}

/**
 *  \brief This function used to get the depth of the node.
 *  
 *  \param [in] Address of the node for which depth to be found out. 
 *         
 *  \return On Success returns depth of the given node
 *          On Failure returns 0, if node is null.
 *          
 *  \details The root node has depth of 1, for children of the root 
 *           node the depth is 2 and so on.
 */
uint32_t vvas_treenode_get_depth(VvasTreeNode *node)
{
  return g_node_depth((GNode *)node);
}

/**
 *  \brief This function used to get the maximum distance of the given node 
 *         from all leaf nodes.
 *  
 *  \param [in] Address of the node. 
 *         
 *  \return On Success returns max distance of the given node from all leaf node.
 *          On Failure returns 0, if node is null.
 *          
 *  \details If node has no children, 1 is returned. If node has 
 *           children, 2 is returned.
 */
uint32_t vvas_treenode_get_max_height(VvasTreeNode *node)
{
  return g_node_max_height((GNode *)node);
}

/**
 *  \brief This function used to get the number of child nodes of the given node.
 *  
 *  \param [in] Address of the parent node. 
 *         
 *  \return On Success returns number of child nodes of the parent node
 *          On Failure returns 0, if node is null.
 *          
 */
uint32_t vvas_treenode_get_n_childnodes(VvasTreeNode *root_node)
{
  return g_node_n_children ((GNode *)root_node);  
}

/**
 *  \brief This function will insert before the tree node.
 *  \param [in] Address of the parent node. 
 *  \param [in] Address of the sibiling node. 
 *  \param [in] Address of the  node to be inserted. 
 *         
 *  \return On Success returns inserted node
 *          On Failure returns NULL.
 *          
 */
VvasTreeNode* vvas_treenode_insert_before(VvasTreeNode *parent,
                                             VvasTreeNode* sibling,
                                             VvasTreeNode* node)
{
  return (VvasTreeNode*)  g_node_insert_before ((GNode*) parent,(GNode*) sibling,(GNode*) node);
}
