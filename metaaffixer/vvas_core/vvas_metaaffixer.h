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
 *DOC:  VVAS Metaaffixer APIs
 * This file describes APIs for metaaffixer that can be used to scale the
 * inference metadata as per the different resolution than the original resolution.
 * The infer meta data to be scaled and attached is decided based on matching the
 * PTS of the source and the destination frames.
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
 *  @sync_pts: if FALSE then last received infer meta data
 *                               is used for scaling. Else reference infer metadata is
 *                               chosen based on PTS of input frame.
 *  @vinfo: Input Frame Information
 *  @metadata: Metadata of input frame
 *  @respcode: Metaaffixer response code.
 *  @ScaledMetaData: Scaled meta data is updated here.
 *
 *  Context: This function returns scaled metadata based on input frame info
 *  Return: 
 *  * On Success returns VVAS_SUCCESS
 *  * On Failure returns VVAS_RET_ERROR 
 */
 VvasReturnType vvas_metaaffixer_get_frame_meta(VvasMetaAffixer *handle,
                                                bool sync_pts,
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
 *  Context: This function will submit meta data information.
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






