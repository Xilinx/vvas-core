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
 *  DOC: This header file includes all core utility modules. 
 *         
 */
#ifndef _VVAS_UTILS_
#define _VVAS_UTILS_

/* Ensures user does not directly include utility header files */
#define VVAS_UTILS_INCLUSION

/*
 * Below section for GLIB supported utils
 */
#ifdef VVAS_GLIB_UTILS

#include <vvas_utils/vvas_node.h>
#include <vvas_utils/vvas_list.h>
#include <vvas_utils/vvas_hash.h>
#include <vvas_utils/vvas_mutex.h>
#include <vvas_utils/vvas_queue.h>

#else /* End of VVAS_GLIB_UTILS */

/*
 * Throw error 
 */
#error "Glib utils is only supported"

#endif

#undef VVAS_UTILS_INCLUSION 


#endif
