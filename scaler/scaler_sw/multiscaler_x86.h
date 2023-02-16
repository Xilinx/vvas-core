/*
* Copyright (C) 2020-2022 Xilinx, Inc. 
* Copyright (C) 2022-2023 Advanced Micro Devices, Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation the
* rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
* sell copies of the Software, and to permit persons to whom the Software
* is furnished to do so, subject to the following conditions:
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
* KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO
* EVENT SHALL XILINX BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
* OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE. Except as contained in this notice, the name of the Xilinx shall
* not be used in advertising or otherwise to promote the sale, use or other
* dealings in this Software without prior written authorization from Xilinx.
*/

#ifndef __MULTISCALERX86_H__
#define __MULTISCALERX86_H__ 


#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>

#include "multi_scaler_sw.h"
#include <vvas_core/vvas_log.h>

typedef unsigned char U8;
typedef unsigned short int U16;
typedef unsigned int U32;
typedef unsigned long long int U64;
typedef signed char I8;
typedef signed short int I16;
typedef signed int I32;
typedef signed long long int I64;

/**
 *  @fn int v_multi_scaler_sw (U32 num_outs, MULTI_SCALER_DESC_STRUCT * desc_start, bool need_preprocess)
 *
 *  @param [in] num_outs           Number of scaling outputs expected.
 *  @param [in] desc               Descriptor filled with input info.
 *  @param [in] need_preprocess    Flag to enable/disable pre-processing
 *  @param [in] log_level          Threshold log level.
 *  @return 0 on Success
 *         -1 on Failure
 *  @brief  This API is used to do the scaling of input image.
 */
int
v_multi_scaler_sw (U32 num_outs, MULTI_SCALER_DESC_STRUCT * desc_start, bool need_preprocess, VvasLogLevel log_level);

#endif /* __MULTISCALERX86_H__  */
