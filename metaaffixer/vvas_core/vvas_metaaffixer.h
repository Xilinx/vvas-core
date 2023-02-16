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
 *DOC:  VVAS Metaaffixer API's
 *This file contains prototypes which will scale  infer metadata
 *to given input resolution. The infer meta data to use for a given 
 *input frame is decided based on PTS. If the PTS of the given input 
 *frame is less than PTS of current infer metadata then previous 
 *infer meta data is choosen & scaled to requested resolution. If 
 *not, current infer metadata is used. 
 */

#ifndef __VVAS_METAAFFIXER_H__
#define __VVAS_METAAFFIXER_H__

#include <vvas_core/vvas_infer_prediction.h>
#include <vvas_core/vvas_video.h>

typedef void  VvasMetaAffixer;

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * enum VvasMetaAffixerRespCode - This enum represents Metaaffixer
 *                                 response code.
 * @VVAS_METAAFFIXER_PASS: Indicates operation is success. 
 * @VVAS_METAAFFIXER_NO_FRAME_OVERLAP: Indicates no frame overlap occured. 
 * @VVAS_METAAFFIXER_NULL_VALUE: Indicates NULL value received. 
 */
typedef enum  {
  VVAS_METAAFFIXER_PASS = 0x01,
  VVAS_METAAFFIXER_NO_FRAME_OVERLAP = 0x02,
  VVAS_METAAFFIXER_NULL_VALUE = 0x03
}VvasMetaAffixerRespCode;


/**
 *  vvas_metaaffixer_create () - Creates metaaffixer handle 
 *  @inferframe_dur: Duration of the infer frame.                                                  
 *  @infer_queue_size:  Represents Max queue size of the infer frame
 *  @loglevel:Indicates log level
 *  Context: This function will allocate internal resources and  
 *          return the handle.
 *  Return:
 *  * On Sucess returns handle of Metaaffixer handle
 *  * On Failure returns NULL
 */
 VvasMetaAffixer* vvas_metaaffixer_create(uint64_t inferframe_dur,
                                          uint32_t infer_queue_size, 
                                          VvasLogLevel loglevel);

/**
 *  vvas_metaaffixer_destroy() - Destroys metaaffixer handle
 *  @handle: MetaAffixer handle to be destroyed 
 *  Context: This function will destroy all memory allocated for handle instanced passed 
 *  Return: None
 */
void vvas_metaaffixer_destroy(VvasMetaAffixer *handle);

/**
 *  vvas_metaaffixer_get_frame_meta() - Provides scaled metadata. 
 *  @handle: Address of context handle @ref VvasMetaAffixer
 *  @sync_infer_lastpts: if TRUE then last received infer meta data  
 *                               is used for scaling else reference infer metadata is 
 *                               chosen cored on PTS of input frame.
 *  @vinfo: Input Frame Information
 *  @metadata: Metadata of input frame
 *  @respcode: Metaaffixer response code.
 *  @ScaledMetaData: Scaled meta data is updated here.
 *
 *  Context: This function returns scaled metadata cored on input frame info
 *  Return: 
 *  * On Success returns VVAS_SUCCESS
 *  * On Failure returns VVAS_RET_ERROR 
 */
 VvasReturnType vvas_metaaffixer_get_frame_meta(VvasMetaAffixer *handle,
                                                bool sync_infer_lastpts,
                                                VvasVideoInfo *vinfo,
                                                VvasMetadata *metadata,
                                                VvasMetaAffixerRespCode *respcode,
                                                VvasInferPrediction **ScaledMetaData);
 
/**
 *  vvas_metaaffixer_submit_infer_meta() - Submit infer metadata.  
 *  @handle: Context handle @ref VvasMetaAffixer
 *  @vinfo: Address of frame info
 *  @metadata: Metadata of frame
 *  @infer: Infer metadata associated with infer frame
 *  
 *  Context: This function will submit meta data information into table 
 *  Return: 
 *  * On Success returns VVAS_RET_SUCCESS
 *  * On Failure returns VVAS_RET_ERROR
 */
VvasReturnType vvas_metaaffixer_submit_infer_meta(VvasMetaAffixer *handle,
                                                  VvasVideoInfo *vinfo,
                                                  VvasMetadata *metadata,      
                                                  VvasInferPrediction *infer);

#ifdef __cplusplus
}
#endif
#endif






