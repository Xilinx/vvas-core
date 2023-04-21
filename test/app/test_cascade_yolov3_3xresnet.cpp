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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <jansson.h>
#include <limits.h>
#include <vvas_core/vvas_common.h>
#include <vvas_utils/vvas_utils.h>
#include <vvas_core/vvas_infer_prediction.h>
#include <vvas_core/vvas_infer_classification.h>
#include <vvas_core/vvas_context.h>
#include <vvas_core/vvas_parser.h>
#include <vvas_core/vvas_decoder.h>
#include <vvas_core/vvas_scaler.h>
#include <vvas_core/vvas_overlay.h>
#include <vvas_core/vvas_dpuinfer.hpp>
#include <vvas_core/vvas_metaconvert.h>

using namespace std;

#define DEFAULT_DEV_INDEX 0
#define SCALER_PPC 4
#define SCALER_STRIDE_ALIGNMENT (8 * SCALER_PPC)
#define YOLO_LEVEL 1
#define RESNET18_INFER_LEVEL 2
#define ALIGN(size,align) ((((size) + (align) - 1) / align) * align)
#define DEFAULT_FONT_SIZE 0.5
#define DEFAULT_FONT VVAS_FONT_HERSHEY_SIMPLEX
#define DEFAULT_THICKNESS 1
#define DEFAULT_RADIUS 3
#define DEFAULT_MASK_LEVEL 0

#define SCALER_IP_NAME "image_processing:{image_processing_1}"
#ifdef XLNX_V70_PLATFORM
#define DEFAULT_VDU_KERNEL_NAME "kernel_vdu_decoder:{kernel_vdu_decoder_0}"
#endif
#define DEFAULT_READ_SIZE 4096

typedef struct
{
  VvasContext *vvas_gctx;
  VvasScaler *crop_sc;
  VvasScalerRect src_rect;
  VvasScalerRect dst_rect;
  VvasList *free_oframes;
  VvasList *proc_oframes;
  VvasList *bbox_nodes;
  VvasVideoInfo out_vinfo;
} CropScalerHandle;

typedef struct _PredictionScaleData PredictionScaleData;
struct _PredictionScaleData
{
  VvasVideoInfo *from;
  VvasVideoInfo *to;
};

static VvasLogLevel gloglevel;

typedef struct
{
  VvasVideoFrame *src_vframe;
  /* free output frames */
  VvasList *free_oframes;
  /* processed output frames */
  VvasList *proc_oframes;
  VvasList *bbox_nodes;
} VvasAppCropFrameHandle;

typedef struct
{
  char *yolov3_json_path;
  char *car_make_json_path;
  char *car_type_json_path;
  char *car_color_json_path;
  VvasDpuInferConf yolov3_incfg;
  VvasDpuInferConf resnet18_car_make_incfg;
  VvasDpuInferConf resnet18_car_type_incfg;
  VvasDpuInferConf resnet18_car_color_incfg;

  VvasModelConf yolov3_outcfg;
  VvasModelConf resnet18_car_make_outcfg;
  VvasModelConf resnet18_car_type_outcfg;
  VvasModelConf resnet18_car_color_outcfg;

  char *metaconvert_json_path;
  VvasMetaConvertConfig mc_cfg;

  /** Vvas Global Device Context */
  VvasContext *vvas_gctx;
  /** Global log level */
  VvasLogLevel gloglevel;
  /** Input stream codec type */
  VvasCodecType codectype;
  /** Elementary stream parser */
  VvasParser *es_parser;
  /** Decoder context */
  VvasDecoder *dec_ctx;
  /** Scaler context for level-1 inference */
  VvasScaler *sc_ctx;
  /** Scaler context for cropping in level-2 inference */
  VvasScaler *sc_crop_ctx;
  /** Yolo VAI handle */
  VvasDpuInfer *yolov3_handle;
  /** Resnet Car make inference handle */
  VvasDpuInfer *resnet18_car_make_handle;
  /** Resnet Car type inference handle */
  VvasDpuInfer *resnet18_car_type_handle;
  /** Resnet Car color inference handle */
  VvasDpuInfer *resnet18_car_color_handle;
  /** Metaconvert handle */
  VvasMetaConvert *mc_handle;
  /** Scaler output frame produced by sc_ctx handle */
    vector < VvasVideoFrame * >scaler_outframes;
  VvasAppCropFrameHandle crop_handle;
  /** list to free when app is done */
  VvasList *dec_out_frames;
  int32_t parser_offset;
  uint8_t read_again;
  bool ppe_changed;
  uint8_t node_num;
} VvasAppHandle;



/**
 * Print the help text for this application
 */
static void
print_help_text (char *pn)
{
  printf ("Usage: %s [OPTIONS]\n", pn);
  printf ("  -i  Input elementary(H264/H265) steram file path\n");
  printf ("  -j JSON configuration of inference models\n");
  printf ("  -o  Output file path to write NV12 frames\n");
  printf ("  -x  xclbin location\n");
  printf ("  -d  Device index\n");
  printf ("  -l  log level : 0 to 3\n");
  printf ("  -n  Decoder name Ex : kernel_vdu_decoder:{kernel_vdu_decoder_0}\n");
  printf ("  -r loop back count\n");
  printf ("  -c  codec type : h264 / h265\n");
  printf ("  -h  Print this help and exit\n\n");
}

static VvasVideoFrame *
vvas_app_get_outframe (VvasAppHandle * app_handle)
{
  VvasVideoFrame *out_vframe = NULL;

  if (vvas_list_length (app_handle->crop_handle.free_oframes)) {
    VvasList *lfree_oframe = NULL;

    /* take output video frame from free list */
    lfree_oframe = vvas_list_first (app_handle->crop_handle.free_oframes);
    out_vframe = (VvasVideoFrame *) lfree_oframe->data;
    app_handle->crop_handle.free_oframes =
        vvas_list_remove (app_handle->crop_handle.free_oframes,
        lfree_oframe->data);
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
        "popped crop output frame %p from list", out_vframe);
  } else {
    VvasVideoInfo out_vinfo = { 0, };

    out_vinfo.width = app_handle->resnet18_car_make_outcfg.model_width;
    out_vinfo.height = app_handle->resnet18_car_make_outcfg.model_height;
    out_vinfo.fmt = app_handle->resnet18_car_make_incfg.model_format;

    /* allocate new video frame */
    out_vframe =
        vvas_video_frame_alloc (app_handle->vvas_gctx, VVAS_ALLOC_TYPE_CMA,
        VVAS_ALLOC_FLAG_NONE, 0, &out_vinfo, NULL);
    if (!out_vframe) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
          "failed to allocate scaler output video frame");
      return NULL;
    }

    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "allocated crop output frame %p",
        out_vframe);
  }

  return out_vframe;
}

static int
get_num_planes (VvasVideoFormat fmt)
{
  int num_planes = 0;
  switch (fmt) {
    case VVAS_VIDEO_FORMAT_Y_UV8_420:
      num_planes = 2;
      break;
    case VVAS_VIDEO_FORMAT_RGB:
    case VVAS_VIDEO_FORMAT_BGR:
    case VVAS_VIDEO_FORMAT_GRAY8:
    case VVAS_VIDEO_FORMAT_BGRA:
    case VVAS_VIDEO_FORMAT_RGBA:
      num_planes = 1;
      break;
    case VVAS_VIDEO_FORMAT_I420:
      num_planes = 3;
      break;
    default:
      LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, 
               "Unsupported video format : %d", fmt);
  }
  return num_planes;
}

static void
free_overlay_text_params (void *data)
{
  VvasOverlayTextParams *text_params = (VvasOverlayTextParams *) data;
  free (text_params->disp_text);
  free (text_params);
}

static void
compute_factors (VvasVideoInfo * from, VvasVideoInfo * to, double *hfactor,
    double *vfactor)
{

  if (!to || !from || !hfactor || !vfactor) {
    return;
  }

  if (!from->width || !from->height) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "Wrong width and height paramters");
    return;
  }

  *hfactor = to->width * 1.0 / from->width;
  *vfactor = to->height * 1.0 / from->height;
}

static void
prediction_scale_ip (VvasInferPrediction * self, VvasVideoInfo * to,
    VvasVideoInfo * from)
{
  double hfactor = 0.0, vfactor = 0.0;

  if (!self || !to || !from)
    return;

  compute_factors (from, to, &hfactor, &vfactor);

  LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
      "source bbox : x = %d, y = %d, width = %d, height = %d",
      self->bbox.x, self->bbox.y, self->bbox.width, self->bbox.height);

  self->bbox.x = nearbyintf (self->bbox.x * hfactor);
  self->bbox.y = nearbyintf (self->bbox.y * vfactor);
  self->bbox.width = nearbyintf (self->bbox.width * hfactor);
  self->bbox.height = nearbyintf (self->bbox.height * vfactor);

  LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
      "destination bbox : x = %d, y = %d, width = %d, height = %d",
      self->bbox.x, self->bbox.y, self->bbox.width, self->bbox.height);
}

static int
node_scale_ip (VvasTreeNode * node, void *data)
{
  VvasInferPrediction *self = (VvasInferPrediction *) node->data;
  PredictionScaleData *sdata = (PredictionScaleData *) data;

  prediction_scale_ip (self, sdata->to, sdata->from);

  return 0;
}

static int
vvas_app_parse_master_json (char *mjson_path, VvasAppHandle * app_handle)
{
  json_t *root = NULL, *value = NULL;
  json_error_t error;

  root = json_load_file (mjson_path, JSON_DECODE_ANY, &error);
  if (!root) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "failed to load json file(%s) reason %s", mjson_path, error.text);
    return -1;
  }

  value = json_object_get (root, "yolov3-config-path");
  if (json_is_string (value)) {
    app_handle->yolov3_json_path = strdup ((char *) json_string_value (value));
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "yolov3-config-path = %s",
        (char *) json_string_value (value));
  } else {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "level1-config-path is not of string type or not present");
    goto error;
  }

  value = json_object_get (root, "resnet18-carmake-config-path");
  if (json_is_string (value)) {
    app_handle->car_make_json_path =
        strdup ((char *) json_string_value (value));
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
        "resnet18-carmake-config-path = %s",
        (char *) json_string_value (value));
  } else {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "resnet18-carmake-config-path is not of string type or not present");
    goto error;
  }

  value = json_object_get (root, "resnet18-cartype-config-path");
  if (json_is_string (value)) {
    app_handle->car_type_json_path =
        strdup ((char *) json_string_value (value));
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
        "resnet18-cartype-config-path = %s",
        (char *) json_string_value (value));
  } else {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "resnet18-cartype-config-path is not of string type or not present");
    goto error;
  }

  value = json_object_get (root, "resnet18-carcolor-config-path");
  if (json_is_string (value)) {
    app_handle->car_color_json_path =
        strdup ((char *) json_string_value (value));
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
        "resnet18-carcolor-config-path = %s",
        (char *) json_string_value (value));
  } else {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "resnet18-carcolor-config-path is not of string type or not present");
    goto error;
  }

  value = json_object_get (root, "metaconvert-config-path");
  if (json_is_string (value)) {
    app_handle->metaconvert_json_path =
        strdup ((char *) json_string_value (value));
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
        "metaconvert-config-path = %s",
        (char *) json_string_value (value));
  } else {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "metaconvert-config-path is not of string type or not present");
    goto error;
  }


  json_decref (root);
  return 0;

error:
  json_decref (root);
  return -1;
}

static void
reset_dpuinfer_conf (VvasDpuInferConf * dpu_conf)
{
  dpu_conf->model_path = NULL;
  dpu_conf->model_name = NULL;
  dpu_conf->model_format = VVAS_VIDEO_FORMAT_UNKNOWN;
  dpu_conf->modelclass = NULL;
  dpu_conf->batch_size = 0;
  dpu_conf->need_preprocess = true;
  dpu_conf->performance_test = true;
  dpu_conf->objs_detection_max = UINT_MAX;
  dpu_conf->filter_labels = NULL;
  dpu_conf->num_filter_labels = 0;
  dpu_conf->float_feature = true;
  dpu_conf->segoutfmt = VVAS_VIDEO_FORMAT_UNKNOWN;
  dpu_conf->segoutfactor = 1;
}

static void
free_dpuinfer_conf (VvasDpuInferConf * dpu_conf)
{
  if (dpu_conf->model_path)
    free (dpu_conf->model_path);
  if (dpu_conf->model_name)
    free (dpu_conf->model_name);
  if (dpu_conf->modelclass)
    free (dpu_conf->modelclass);
  if (dpu_conf->filter_labels) {
    for (auto i = 0; i < dpu_conf->num_filter_labels; i++) {
      if (dpu_conf->filter_labels[i])
        free (dpu_conf->filter_labels[i]);
    }
    delete[]dpu_conf->filter_labels;
  }
}

static VvasVideoFormat
get_vvas_video_fmt (char *name)
{
  if (!strncmp (name, "RGB", 3))
    return VVAS_VIDEO_FORMAT_RGB;
  else if (!strncmp (name, "BGR", 3))
    return VVAS_VIDEO_FORMAT_BGR;
  else if (!strncmp (name, "GRAY8", 5))
    return VVAS_VIDEO_FORMAT_GRAY8;
  else
    return VVAS_VIDEO_FORMAT_UNKNOWN;
}

static bool
parse_dpu_json (char *json_file, VvasDpuInferConf * dpu_conf, int gloglevel)
{
  json_t *root, *kernel, *kconfig, *value, *label = NULL;
  json_error_t error;

  root = json_load_file (json_file, JSON_DECODE_ANY, &error);
  if (!root) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "failed to load json file(%s) reason %s", json_file, error.text);
    goto error;
  }

  kernel = json_object_get (root, "kernel");
  if (!json_is_object (kernel)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "failed to find kernel object");
    goto error;
  }

  kconfig = json_object_get (kernel, "config");
  if (!json_is_object (kconfig)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "config is not of object type");
    goto error;
  }

  value = json_object_get (kconfig, "model-path");
  if (json_is_string (value)) {
    dpu_conf->model_path = strdup ((char *) json_string_value (value));
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "model-path ---> %s",
        (char *) json_string_value (value));
  } else {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "model-path is not of string type");
    goto error;
  }

  value = json_object_get (kconfig, "model-name");
  if (json_is_string (value)) {
    dpu_conf->model_name = strdup ((char *) json_string_value (value));
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "model-name ---> %s",
        (char *) json_string_value (value));
  } else {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "model-name is not of string type");
    goto error;
  }

  value = json_object_get (kconfig, "model-format");
  if (!json_is_string (value)) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
        "model-format is not proper, taking BGR as default");
    dpu_conf->model_format = VVAS_VIDEO_FORMAT_BGR;
  } else {
    dpu_conf->model_format =
        get_vvas_video_fmt ((char *) json_string_value (value));
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "model-format ---> %d",
        dpu_conf->model_format);
  }
  if (dpu_conf->model_format == VVAS_VIDEO_FORMAT_UNKNOWN) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "SORRY NOT SUPPORTED MODEL FORMAT %s",
        (char *) json_string_value (value));
    goto error;
  }

  value = json_object_get (kconfig, "model-class");
  if (json_is_string (value)) {
    dpu_conf->modelclass = strdup ((char *) json_string_value (value));
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "model-class ---> %s",
        (char *) json_string_value (value));
  } else {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "model-class is not of string type");
    goto error;
  }

  value = json_object_get (kconfig, "seg-out-format");
  if (json_is_integer (value)) {
    dpu_conf->segoutfmt = VvasVideoFormat (json_integer_value (value));
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "seg-out-fmt ---> %d",
        dpu_conf->segoutfmt);
  }

  value = json_object_get (kconfig, "batch-size");
  if (!value || !json_is_integer (value)) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "Taking batch-size as 1");
    dpu_conf->batch_size = 1;
  } else {
    dpu_conf->batch_size = json_integer_value (value);
  }

  value = json_object_get (kconfig, "vitis-ai-preprocess");
  if (!value || !json_is_boolean (value)) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "Setting need_preprocess as TRUE");
    dpu_conf->need_preprocess = true;
  } else {
    dpu_conf->need_preprocess = json_boolean_value (value);
  }

  value = json_object_get (kconfig, "performance-test");
  if (!value || !json_is_boolean (value)) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
        "Setting performance_test as TRUE");
    dpu_conf->performance_test = true;
  } else {
    dpu_conf->performance_test = json_boolean_value (value);
  }

  value = json_object_get (kconfig, "max-objects");
  if (!value || !json_is_integer (value)) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "Setting max-objects as %d",
        UINT_MAX);
    dpu_conf->objs_detection_max = UINT_MAX;
  } else {
    dpu_conf->objs_detection_max = json_integer_value (value);
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "Setting max-objects as %d",
        dpu_conf->objs_detection_max);
  }

  value = json_object_get (kconfig, "segoutfactor");
  if (json_is_integer (value)) {
    dpu_conf->segoutfactor = json_integer_value (value);;
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "Setting segoutfactor as %d",
        dpu_conf->segoutfactor);
  }

  value = json_object_get (kconfig, "float-feature");
  if (json_is_boolean (value)) {
    dpu_conf->float_feature = json_boolean_value (value);
    dpu_conf->float_feature = json_boolean_value (value);
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "Setting float-feature as %d",
        dpu_conf->float_feature);
  }

  value = json_object_get (kconfig, "filter-labels");
  if (json_is_array (value)) {
    dpu_conf->num_filter_labels = json_array_size (value);
    dpu_conf->filter_labels = new char *[dpu_conf->num_filter_labels];
    for (auto i = 0; i < dpu_conf->num_filter_labels; i++) {
      label = json_array_get (value, i);
      if (json_is_string (label)) {
        dpu_conf->filter_labels[i] =
            strdup ((char *) json_string_value (label));
        LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "Adding filter label %s",
            dpu_conf->filter_labels[i]);
      } else {
        dpu_conf->filter_labels[i] = NULL;
        LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
            "Filter label is not of string type");
      }
    }
  } else {
    dpu_conf->num_filter_labels = 0;
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "No filter labels given");
  }

  json_decref (root);
  return true;

error:
  json_decref (root);
  return false;
}

static bool
parse_metaconvert_json (char *json_file, VvasMetaConvertConfig * mc_conf, int gloglevel)
{
  json_t *root = NULL, *config = NULL, *val = NULL, *karray = NULL;
  json_error_t error;
  uint32_t index;

  /* get root json object */
  root = json_load_file (json_file, JSON_DECODE_ANY, &error);
  if (!root) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "failed to load json file. reason %s",
        error.text);
    goto error;
  }

  config = json_object_get (root, "config");
  if (!json_is_object (config)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "config is not of object type");
    goto error;
  }

  val = json_object_get (config, "display-level");
  if (!val || !json_is_integer (val)) {
    mc_conf->level = 0;
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
      "display_level is not set, so process all nodes at all levels");
  } else {
    if (json_integer_value (val) < 0) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "display level should be greater than or equal to 0");
      goto error;
    }
    mc_conf->level = json_integer_value (val);
  }

  val = json_object_get (config, "font-size");
  if (!val || !json_is_integer (val))
    mc_conf->font_size = 0.5;
  else
    mc_conf->font_size = json_integer_value (val);

  val = json_object_get (config, "font");
  if (!val || !json_is_integer (val))
    mc_conf->font_type = DEFAULT_FONT;
  else {
    index = json_integer_value (val);
    if (index >= 0 && index <= 5)
      mc_conf->font_type = (VvasFontType) index;
    else {
      LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
          "font value out of range. Setting default");
      mc_conf->font_type = DEFAULT_FONT;
    }
  }

  val = json_object_get (config, "thickness");
  if (!val || !json_is_integer (val))
    mc_conf->line_thickness = DEFAULT_THICKNESS;
  else
    mc_conf->line_thickness = json_integer_value (val);

  val = json_object_get (config, "radius");
  if (!val || !json_is_integer (val))
    mc_conf->radius = DEFAULT_RADIUS;
  else
    mc_conf->radius = json_integer_value (val);

  val = json_object_get (config, "mask-level");
  if (!val || !json_is_integer (val))
    mc_conf->mask_level = DEFAULT_MASK_LEVEL;
  else
    mc_conf->mask_level = json_integer_value (val);

  val = json_object_get (config, "draw-above-bbox-flag");
  if (!val || !json_is_boolean (val))
    mc_conf->draw_above_bbox_flag = true;
  else
    mc_conf->draw_above_bbox_flag = json_boolean_value (val);

  val = json_object_get (config, "y-offset");
  if (!val || !json_is_integer (val))
    mc_conf->y_offset = 0;
  else
    mc_conf->y_offset = json_integer_value (val);

  karray = json_object_get (config, "label-filter");
  if (!json_is_array (karray)) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
        "label_filter not set, adding only class name");
    mc_conf->allowed_labels_count = 1;
    mc_conf->allowed_labels = (char **) calloc (mc_conf->allowed_labels_count, sizeof (char*));
    mc_conf->allowed_labels[0] = strdup("class");
  } else {
    mc_conf->allowed_labels_count = json_array_size (karray);
    mc_conf->allowed_labels = (char **) calloc (mc_conf->allowed_labels_count, sizeof (char*));
    for (index = 0; index < json_array_size (karray); index++) {
      mc_conf->allowed_labels[index] = strdup(json_string_value (json_array_get (karray, index)));
    }
  }

  /* get classes array */
  karray = json_object_get (config, "classes");
  if (!karray) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
      "classification filtering not found, allowing all classes");
    mc_conf->allowed_classes_count = 0;
  } else {
    if (!json_is_array (karray)) {
      LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "classes key is not of array type");
      goto error;
    }
    mc_conf->allowed_classes_count = json_array_size (karray);
    mc_conf->allowed_classes = (VvasFilterObjectInfo **) calloc (mc_conf->allowed_classes_count, sizeof (VvasFilterObjectInfo*));

    for (index = 0; index < mc_conf->allowed_classes_count; index++) {
      VvasFilterObjectInfo *allowed_class = (VvasFilterObjectInfo *) calloc (1, sizeof (VvasFilterObjectInfo));
      mc_conf->allowed_classes[index] = allowed_class;
      json_t *classes;

      classes = json_array_get (karray, index);
      if (!classes) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "failed to get class object");
        goto error;
      }

      val = json_object_get (classes, "name");
      if (!json_is_string (val)) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
            "name is not found for array %d", index);
        goto error;
      } else {
        strncpy (allowed_class->name,
            (char *) json_string_value (val), META_CONVERT_MAX_STR_LENGTH - 1);
        allowed_class->name[META_CONVERT_MAX_STR_LENGTH-1] = '\0';
        LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "name %s", allowed_class->name);
      }

      val = json_object_get (classes, "green");
      if (!val || !json_is_integer (val))
        allowed_class->color.green = 0;
      else
        allowed_class->color.green = json_integer_value (val);

      val = json_object_get (classes, "blue");
      if (!val || !json_is_integer (val))
        allowed_class->color.blue = 0;
      else
        allowed_class->color.blue = json_integer_value (val);

      val = json_object_get (classes, "red");
      if (!val || !json_is_integer (val))
        allowed_class->color.red = 0;
      else
        allowed_class->color.red = json_integer_value (val);

      val = json_object_get (classes, "masking");
      if (!val || !json_is_integer (val))
        allowed_class->do_mask = 0;
      else
        allowed_class->do_mask = json_integer_value (val);

    }
  }

  if (root)
    json_decref (root);
  return true;

error:
  json_decref (root);
  for (uint8_t i = 0; i < mc_conf->allowed_classes_count; i++) {
    if (mc_conf->allowed_classes[i]) {
      free (mc_conf->allowed_classes[i]);
      mc_conf->allowed_classes[i] = NULL;
    }
  }

  if (mc_conf->allowed_classes) {
    free (mc_conf->allowed_classes);
    mc_conf->allowed_classes = NULL;
  }

  return false;
}

static bool
vvas_app_scaler_crop_each_bbox (const VvasTreeNode * node, void *user_data)
{
  VvasInferPrediction *prediction = (VvasInferPrediction *) node->data;
  VvasAppHandle *app_handle = (VvasAppHandle *) user_data;
  VvasAppCropFrameHandle *crop_handle = &app_handle->crop_handle;
  VvasScalerRect src_rect;
  VvasScalerRect dst_rect;
  VvasScalerPpe ppe;
  VvasReturnType vret = VVAS_RET_SUCCESS;

  LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "received prediction node %p",
      prediction);

  if (vvas_treenode_get_depth ((VvasTreeNode *) node) != RESNET18_INFER_LEVEL)
    return FALSE;

  src_rect.x = prediction->bbox.x;
  src_rect.y = prediction->bbox.y;
  src_rect.width = prediction->bbox.width;
  src_rect.height = prediction->bbox.height;
  src_rect.frame = crop_handle->src_vframe;

  dst_rect.x = 0;
  dst_rect.y = 0;
  dst_rect.width = app_handle->resnet18_car_make_outcfg.model_width;
  dst_rect.height = app_handle->resnet18_car_make_outcfg.model_height;
  if (!app_handle->ppe_changed) {
    dst_rect.frame = vvas_app_get_outframe (app_handle);
  } else {
    dst_rect.frame =
        (VvasVideoFrame *) vvas_list_nth_data (crop_handle->proc_oframes,
        app_handle->node_num);
    app_handle->node_num++;
  }

  if (!app_handle->ppe_changed) {
    ppe.mean_r = app_handle->resnet18_car_make_outcfg.mean_r;
    ppe.mean_g = app_handle->resnet18_car_make_outcfg.mean_g;
    ppe.mean_b = app_handle->resnet18_car_make_outcfg.mean_b;
    ppe.scale_r = app_handle->resnet18_car_make_outcfg.scale_r;
    ppe.scale_g = app_handle->resnet18_car_make_outcfg.scale_g;
    ppe.scale_b = app_handle->resnet18_car_make_outcfg.scale_b;
  } else {
    ppe.mean_r = app_handle->resnet18_car_color_outcfg.mean_r;
    ppe.mean_g = app_handle->resnet18_car_color_outcfg.mean_g;
    ppe.mean_b = app_handle->resnet18_car_color_outcfg.mean_b;
    ppe.scale_r = app_handle->resnet18_car_color_outcfg.scale_r;
    ppe.scale_g = app_handle->resnet18_car_color_outcfg.scale_g;
    ppe.scale_b = app_handle->resnet18_car_color_outcfg.scale_b;
  }

  LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
      "length of free output frames list = %d",
      vvas_list_length (crop_handle->free_oframes));

  for (uint32_t i = 0; i < vvas_list_length (crop_handle->free_oframes); i++) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, " crop frame addr [%d] = %p", i,
        vvas_list_nth_data (crop_handle->free_oframes, i));
  }

  LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
      "cropping frame with bbox : frame = %p, x = %d, y = %d, width = %d, height = %d",
      src_rect.frame, prediction->bbox.x, prediction->bbox.y,
      prediction->bbox.width, prediction->bbox.height);

  /* hold output frame sent to scaler */
  if (!app_handle->ppe_changed) {
    crop_handle->proc_oframes =
        vvas_list_append (crop_handle->proc_oframes, dst_rect.frame);
    crop_handle->bbox_nodes =
        vvas_list_append (crop_handle->bbox_nodes, prediction);
  }

  vret =
      vvas_scaler_channel_add (app_handle->sc_crop_ctx, &src_rect, &dst_rect,
      &ppe, NULL);
  if (VVAS_IS_ERROR (vret)) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
        "failed add processing channel in scaler");
    return TRUE;
  }

  return FALSE;
}

static VvasReturnType
vvas_app_create_module_contexts (VvasAppHandle * app_handle, int32_t dev_idx,
    char *xclbin_loc, char *dec_name)
{
  VvasVideoInfo vinfo;
  VvasReturnType vret = VVAS_RET_SUCCESS;
  int num_planes = 0, idx = 0;

  /* Create VVAS Global Context for a specific device
   * - Downloads xclbin on to a device
   */
  app_handle->vvas_gctx =
      vvas_context_create (dev_idx, xclbin_loc, gloglevel, &vret);
  if (!app_handle->vvas_gctx) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "Failed to create vvas global context");
    return vret;
  }

  LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "input codec type = %d",
      app_handle->codectype);

  /* Create parser context */
  app_handle->es_parser =
      vvas_parser_create (app_handle->vvas_gctx, app_handle->codectype,
      gloglevel);
  if (!app_handle->es_parser) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "Failed to create parser context");
    return VVAS_RET_ERROR;
  }

  /* Create decoder context */
  app_handle->dec_ctx =
      vvas_decoder_create (app_handle->vvas_gctx, (uint8_t *)dec_name, VVAS_CODEC_H264, 0,
      gloglevel);
  if (!app_handle->dec_ctx) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "Failed to create decoder context");
    return VVAS_RET_ERROR;
  }

  /* Create scaler context */
  app_handle->sc_ctx =
      vvas_scaler_create (app_handle->vvas_gctx, SCALER_IP_NAME, gloglevel);
  if (!app_handle->sc_ctx) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "failed to create scaler context");
    return VVAS_RET_ERROR;
  }

  /* Create scaler context for cropping */
  app_handle->sc_crop_ctx =
      vvas_scaler_create (app_handle->vvas_gctx, SCALER_IP_NAME, gloglevel);
  if (!app_handle->sc_crop_ctx) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "failed to create scaler crop context");
    return VVAS_RET_ERROR;
  }

  reset_dpuinfer_conf (&app_handle->yolov3_incfg);

  /* parse user json file and prepare configuration */
  if (!parse_dpu_json (app_handle->yolov3_json_path, &app_handle->yolov3_incfg,
          gloglevel)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "Error parsing json file");
    return VVAS_RET_ERROR;
  }

  /* create Yolov3 handle */
  app_handle->yolov3_handle =
      vvas_dpuinfer_create (&app_handle->yolov3_incfg, gloglevel);
  if (!app_handle->yolov3_handle) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "failed to create yolov3 handle");
    return VVAS_RET_ERROR;
  }

  /* get yolov3 model requirements */
  vret =
      vvas_dpuinfer_get_config (app_handle->yolov3_handle,
      &app_handle->yolov3_outcfg);
  if (VVAS_IS_ERROR (vret)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "Failed to get ppe config from DPU");
    return vret;
  }

  if (app_handle->yolov3_incfg.batch_size >
      app_handle->yolov3_outcfg.batch_size) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "Batch size configured (%d) greater than supported by model (%d)",
        app_handle->yolov3_incfg.batch_size,
        app_handle->yolov3_outcfg.batch_size);
    return VVAS_RET_ERROR;
  }

  /* Configure level-1 scaler destination with resolution/format required by model */
  memset (&vinfo, 0, sizeof (vinfo));
  vinfo.width = app_handle->yolov3_outcfg.model_width;
  vinfo.height = app_handle->yolov3_outcfg.model_height;
  vinfo.fmt = app_handle->yolov3_incfg.model_format;
  vinfo.alignment.padding_left = 0;
  vinfo.alignment.padding_right =
      ALIGN (vinfo.width, SCALER_STRIDE_ALIGNMENT) - vinfo.width;
  vinfo.alignment.padding_top = 0;
  vinfo.alignment.padding_bottom = ALIGN (vinfo.height, 2) - vinfo.height;
  num_planes = get_num_planes (vinfo.fmt);
  if (!num_planes) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "Unsupported video format");
      return VVAS_RET_ERROR;
  }
  for (idx = 0; idx < num_planes; idx++) {
    vinfo.alignment.stride_align[idx] = SCALER_STRIDE_ALIGNMENT-1;
  }

  for (uint8_t i = 0; i < app_handle->yolov3_outcfg.batch_size; i++) {
    VvasVideoFrame *sc_outframe =
        vvas_video_frame_alloc (app_handle->vvas_gctx, VVAS_ALLOC_TYPE_CMA,
        VVAS_ALLOC_FLAG_NONE, 0, &vinfo, NULL);
    if (!sc_outframe) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
          "failed to allocate scaler output video frame");
      return VVAS_RET_ALLOC_ERROR;
    }
    app_handle->scaler_outframes.push_back (sc_outframe);
  }

  reset_dpuinfer_conf (&app_handle->resnet18_car_make_incfg);

  /* parse user json file and prepare configuration */
  if (!parse_dpu_json (app_handle->car_make_json_path,
          &app_handle->resnet18_car_make_incfg, gloglevel)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "Error parsing json file");
    return VVAS_RET_ERROR;
  }

  /* create Resnet18 car make handle */
  app_handle->resnet18_car_make_handle =
      vvas_dpuinfer_create (&app_handle->resnet18_car_make_incfg, gloglevel);
  if (!app_handle->resnet18_car_make_handle) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "failed to create car make resnet18 handle");
    return VVAS_RET_ERROR;
  }

  /* get car make model requirements */
  vret =
      vvas_dpuinfer_get_config (app_handle->resnet18_car_make_handle,
      &app_handle->resnet18_car_make_outcfg);
  if (VVAS_IS_ERROR (vret)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "Failed to get config from DPU");
    return vret;
  }

  if (app_handle->resnet18_car_make_incfg.batch_size >
      app_handle->resnet18_car_make_outcfg.batch_size) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "Batch size configured (%d) greater than supported by model (%d)",
        app_handle->resnet18_car_make_incfg.batch_size,
        app_handle->resnet18_car_make_outcfg.batch_size);
    return VVAS_RET_ERROR;
  }

  reset_dpuinfer_conf (&app_handle->resnet18_car_type_incfg);

  /* parse user json file and prepare configuration */
  if (!parse_dpu_json (app_handle->car_type_json_path,
          &app_handle->resnet18_car_type_incfg, gloglevel)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "Error parsing json file");
    return VVAS_RET_ERROR;
  }

  /* create Resnet18 car type handle */
  app_handle->resnet18_car_type_handle =
      vvas_dpuinfer_create (&app_handle->resnet18_car_type_incfg, gloglevel);
  if (!app_handle->resnet18_car_type_handle) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "failed to create car type resnet18 handle");
    return VVAS_RET_ERROR;
  }

  /* get car type model requirements */
  vret =
      vvas_dpuinfer_get_config (app_handle->resnet18_car_type_handle,
      &app_handle->resnet18_car_type_outcfg);
  if (VVAS_IS_ERROR (vret)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "Failed to get config from DPU");
    return vret;
  }

  if (app_handle->resnet18_car_type_incfg.batch_size >
      app_handle->resnet18_car_type_outcfg.batch_size) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "Batch size configured (%d) greater than supported by model (%d)",
        app_handle->resnet18_car_type_incfg.batch_size,
        app_handle->resnet18_car_type_outcfg.batch_size);
    return VVAS_RET_ERROR;
  }

  reset_dpuinfer_conf (&app_handle->resnet18_car_color_incfg);

  /* parse user json file and prepare configuration */
  if (!parse_dpu_json (app_handle->car_color_json_path,
          &app_handle->resnet18_car_color_incfg, gloglevel)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "Error parsing json file");
    return VVAS_RET_ERROR;
  }

  /* create Resnet18 car color handle */
  app_handle->resnet18_car_color_handle =
      vvas_dpuinfer_create (&app_handle->resnet18_car_color_incfg, gloglevel);
  if (!app_handle->resnet18_car_color_handle) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "failed to create car color resnet18 handle");
    return VVAS_RET_ERROR;
  }

  /* get car color model requirements */
  vret =
      vvas_dpuinfer_get_config (app_handle->resnet18_car_color_handle,
      &app_handle->resnet18_car_color_outcfg);
  if (VVAS_IS_ERROR (vret)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "Failed to get config from DPU");
    return vret;
  }

  if (app_handle->resnet18_car_color_incfg.batch_size >
      app_handle->resnet18_car_color_outcfg.batch_size) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "Batch size configured (%d) greater than supported by model (%d)",
        app_handle->resnet18_car_color_incfg.batch_size,
        app_handle->resnet18_car_color_outcfg.batch_size);
    return VVAS_RET_ERROR;
  }

  /* parse user json file and prepare configuration */
  if (!parse_metaconvert_json (app_handle->metaconvert_json_path, &app_handle->mc_cfg,
          gloglevel)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "Error parsing json file");
    return VVAS_RET_ERROR;
  }

  app_handle->mc_handle = vvas_metaconvert_create (app_handle->vvas_gctx, &app_handle->mc_cfg , gloglevel, NULL);
  if (!app_handle->mc_handle) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "Failed to create metaconvert handle");
    return VVAS_RET_ERROR;
  }

  return VVAS_RET_SUCCESS;
}

static void
vvas_app_destroy_module_contexts (VvasAppHandle * app_handle)
{
  if (app_handle->dec_out_frames)
    vvas_list_free_full (app_handle->dec_out_frames, vvas_video_frame_free);

  for (uint8_t i = 0; i < app_handle->scaler_outframes.size (); i++) {
    vvas_video_frame_free (app_handle->scaler_outframes[i]);
    app_handle->scaler_outframes.clear ();
  }

  if (app_handle->es_parser)
    vvas_parser_destroy (app_handle->es_parser);
  if (app_handle->dec_ctx)
    vvas_decoder_destroy (app_handle->dec_ctx);
  if (app_handle->sc_ctx)
    vvas_scaler_destroy (app_handle->sc_ctx);
  if (app_handle->sc_crop_ctx)
    vvas_scaler_destroy (app_handle->sc_crop_ctx);
  if (app_handle->yolov3_handle)
    vvas_dpuinfer_destroy (app_handle->yolov3_handle);
  if (app_handle->resnet18_car_make_handle)
    vvas_dpuinfer_destroy (app_handle->resnet18_car_make_handle);
  if (app_handle->resnet18_car_type_handle)
    vvas_dpuinfer_destroy (app_handle->resnet18_car_type_handle);
  if (app_handle->resnet18_car_color_handle)
    vvas_dpuinfer_destroy (app_handle->resnet18_car_color_handle);

  if (app_handle->mc_cfg.allowed_labels_count) {
    for (uint32_t idx = 0; idx < app_handle->mc_cfg.allowed_labels_count; idx++)
      free (app_handle->mc_cfg.allowed_labels[idx]);

    free (app_handle->mc_cfg.allowed_labels);
  }

  if (app_handle->mc_cfg.allowed_classes_count && app_handle->mc_cfg.allowed_classes) {
    free (app_handle->mc_cfg.allowed_classes);
    app_handle->mc_cfg.allowed_classes = NULL;
  } 

  if (app_handle->mc_handle)
    vvas_metaconvert_destroy (app_handle->mc_handle);
  if (app_handle->vvas_gctx)
    vvas_context_destroy (app_handle->vvas_gctx);
}

static VvasReturnType
vvas_app_get_es_frame (VvasAppHandle * app_handle, FILE * infp,
    VvasMemory * es_buf, VvasMemoryMapInfo * es_buf_info,
    VvasMemory ** au_frame, VvasDecoderInCfg ** incfg)
{
  VvasReturnType vret = VVAS_RET_SUCCESS;
  size_t valid_es_size = DEFAULT_READ_SIZE;
  bool is_eos = false;

  while (1) {
    if (app_handle->read_again) {
      valid_es_size = fread (es_buf_info->data, 1, DEFAULT_READ_SIZE, infp);
      if (valid_es_size == 0) {
        LOG_MESSAGE (LOG_LEVEL_INFO, gloglevel,
            "End of stream happened..inform parser");
        is_eos = true;
        es_buf = NULL;
      }
      LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "read %zu bytes from file\n",
          valid_es_size);
      app_handle->parser_offset = 0;
    }

    /* get a complete frame */
    vret =
        vvas_parser_get_au (app_handle->es_parser, es_buf, valid_es_size,
        au_frame, &app_handle->parser_offset, incfg, is_eos);

    app_handle->read_again =
        app_handle->parser_offset < (int32_t) valid_es_size ? 0 : 1;

    if (VVAS_IS_ERROR (vret)) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
          "failed to parse elementary stream");
      return vret;
    } else if (vret == VVAS_RET_NEED_MOREDATA) {
      LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
          "parser needs more input data. offset = %d",
          app_handle->parser_offset);
      continue;
    } else if (vret == VVAS_RET_SUCCESS || vret == VVAS_RET_EOS) {
      return vret;
    }
  }

  return vret;
}

static VvasReturnType
vvas_app_configure_decoder (VvasAppHandle * app_handle,
    VvasDecoderInCfg * incfg, VvasList ** outbuf_list)
{
  uint32_t i;
  VvasDecoderOutCfg outcfg;
  VvasList *dec_outbuf_list = NULL;
  VvasReturnType vret = VVAS_RET_SUCCESS;
  uint8_t num_outbufs = 0;

  /* configure the decoder with parameters received from parser */
  vret = vvas_decoder_config (app_handle->dec_ctx, incfg, &outcfg);
  if (VVAS_IS_ERROR (vret)) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
        "Failed to configure decoder vret=%d", vret);
    return VVAS_RET_ERROR;
  }

  LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
      "minimum number of output buffers required for decoder = %d",
      outcfg.min_out_buf);

  LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "vinfo = %dx%d - %d",
      outcfg.vinfo.width, outcfg.vinfo.height, outcfg.vinfo.fmt);

  LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
      "vinfo alignment : left = %d, right = %d, top = %d, bottom = %d",
      outcfg.vinfo.alignment.padding_left, outcfg.vinfo.alignment.padding_right,
      outcfg.vinfo.alignment.padding_top,
      outcfg.vinfo.alignment.padding_bottom);

  num_outbufs = outcfg.min_out_buf + app_handle->yolov3_outcfg.batch_size;

  LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
      "number of decoder output buffers to be allocated = %d", num_outbufs);

  /* allocate minimum number of output frames suggested by decoder */
  for (i = 0; i < num_outbufs; i++) {
    VvasVideoFrame *out_vframe = NULL;

    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "Allocating %d output frame", i);

    out_vframe =
        vvas_video_frame_alloc (app_handle->vvas_gctx, VVAS_ALLOC_TYPE_CMA,
        VVAS_ALLOC_FLAG_NONE, outcfg.mem_bank_id, &(outcfg.vinfo), NULL);
    if (!out_vframe) {
      LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
          "failed to allocate %dth video frame", i);
      return VVAS_RET_ERROR;
    }

    dec_outbuf_list = vvas_list_append (dec_outbuf_list, out_vframe);
    app_handle->dec_out_frames =
        vvas_list_append (app_handle->dec_out_frames, out_vframe);
  }

  *outbuf_list = dec_outbuf_list;

  return VVAS_RET_SUCCESS;
}

static VvasReturnType
vvas_app_scale_decoded_frame (VvasAppHandle * app_handle,
    VvasVideoFrame * in_vframe, VvasVideoFrame * out_vframe)
{
  VvasScalerRect src_rect;
  VvasScalerRect dst_rect;
  VvasVideoInfo out_vinfo;
  VvasVideoInfo vinfo;
  VvasReturnType vret = VVAS_RET_SUCCESS;
  VvasScalerPpe ppe;

  vvas_video_frame_get_videoinfo (in_vframe, &vinfo);
  src_rect.x = 0;
  src_rect.y = 0;
  src_rect.width = vinfo.width;
  src_rect.height = vinfo.height;
  src_rect.frame = in_vframe;   /* send decoder output frame to scaler as input */

  /* get output frame information */
  vvas_video_frame_get_videoinfo (out_vframe, &out_vinfo);
  dst_rect.x = 0;
  dst_rect.y = 0;
  dst_rect.width = out_vinfo.width;
  dst_rect.height = out_vinfo.height;
  dst_rect.frame = out_vframe;  /* scaler output frame */

  ppe.mean_r = app_handle->yolov3_outcfg.mean_r;
  ppe.mean_g = app_handle->yolov3_outcfg.mean_g;
  ppe.mean_b = app_handle->yolov3_outcfg.mean_b;
  ppe.scale_r = app_handle->yolov3_outcfg.scale_r;
  ppe.scale_g = app_handle->yolov3_outcfg.scale_g;
  ppe.scale_b = app_handle->yolov3_outcfg.scale_b;

  vret =
      vvas_scaler_channel_add (app_handle->sc_ctx, &src_rect, &dst_rect, &ppe, NULL);
  if (VVAS_IS_ERROR (vret)) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
        "failed add processing channel in scaler");
    return vret;
  }

  /* scale input frame */
  vret = vvas_scaler_process_frame (app_handle->sc_ctx);
  if (VVAS_IS_ERROR (vret)) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "failed to process scaler");
    return vret;
  }

  return vret;
}

static VvasReturnType
vvas_app_write_output (VvasVideoFrame * output_frame, FILE * outfp)
{
  int idx;
  VvasReturnType vret = VVAS_RET_SUCCESS;
  VvasVideoFrameMapInfo out_vmap_info;
  uint32_t offset = 0;

  vret =
      vvas_video_frame_map (output_frame, VVAS_DATA_MAP_READ, &out_vmap_info);
  if (VVAS_IS_ERROR (vret)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "failed map output frame");
    return VVAS_RET_ERROR;
  }

  for (idx = 0; idx < out_vmap_info.height; idx++) {
    fwrite (out_vmap_info.planes[0].data + offset, 1, out_vmap_info.width,
        outfp);
    offset += out_vmap_info.planes[0].stride;
  }

  offset = 0;
  for (idx = 0; idx < (out_vmap_info.height >> 1); idx++) {
    fwrite (out_vmap_info.planes[1].data + offset, 1, out_vmap_info.width,
        outfp);
    offset += out_vmap_info.planes[0].stride;
  }
  vvas_video_frame_unmap (output_frame, &out_vmap_info);
  fflush (outfp);

  return VVAS_RET_SUCCESS;
}

int
main (int argc, char *argv[])
{
  VvasAppHandle app_handle = { 0, };
  int opt;
  char *xclbin_loc = NULL;
  char *inpath = NULL;
  char *outpath = NULL;
  char *dec_name = NULL;
  int32_t dev_idx = DEFAULT_DEV_INDEX;
  VvasReturnType vret = VVAS_RET_SUCCESS;
  VvasDecoderInCfg *incfg;
  VvasList *dec_outbuf_list = NULL;
  VvasMemory *es_buf = NULL;
  FILE *infp = NULL;
  FILE *outfp = NULL;
  VvasMemoryMapInfo es_buf_info;
  VvasMemory *au_frame = NULL;
  uint8_t parser_eos = 0;
  char *mjson_path = NULL;
  int gret = -1;
  VvasVideoInfo *from_vinfo = NULL, *to_vinfo = NULL;
  uint32_t cur_repeat_cnt = 1;
  uint32_t max_repeat_cnt = 1;
  uint8_t configure_decoder = 1;
  int proc_cnt = 0;
  vector < VvasVideoFrame * >cached_dec_outframes;
  uint8_t yolov3_cur_bsize = 0;

  gloglevel = LOG_LEVEL_WARNING;

  while ((opt = getopt (argc, argv, "i:o:x:d:l:n:c:j:r:h")) != -1) {
    switch (opt) {
      case 'i':
        inpath = strdup (optarg);
        break;
      case 'o':
        outpath = strdup (optarg);
        break;
      case 'x':
        xclbin_loc = strdup (optarg);
        break;
      case 'j':
        mjson_path = strdup (optarg);
        break;
      case 'd':
        dev_idx = atoi (optarg);
        break;
      case 'r':
        max_repeat_cnt = atoi (optarg);
        break;
      case 'l':
        gloglevel = (VvasLogLevel) atoi (optarg);
        break;
      case 'n':
        dec_name = strdup (optarg);
        break;
      case 'c':
        if (!strcmp ("h264", optarg)) {
          app_handle.codectype = VVAS_CODEC_H264;
        } else if (!strcmp ("h265", optarg)) {
          app_handle.codectype = VVAS_CODEC_H265;
        } else {
          LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "Invaid parser type");
          return -1;
        }
        break;
      case 'h':
        print_help_text (argv[0]);
        return 0;
    }
  }

  if (!inpath || !xclbin_loc || !dec_name) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "invalid arguments");
    print_help_text (argv[0]);
    return -1;
  }

  app_handle.parser_offset = 0;
  app_handle.read_again = 1;

  if (vvas_app_parse_master_json (mjson_path, &app_handle)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "failed to master json file %s",
        mjson_path);
    return -1;
  }

  /* create all require module contexts */
  vret = vvas_app_create_module_contexts (&app_handle, dev_idx, xclbin_loc,
		                                  dec_name);
  if (vret != VVAS_RET_SUCCESS) {
    goto exit;
  }

  if (outpath) {
    outfp = fopen (outpath, "w+");
    if (!outfp) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
          "failed to open file %s for writing", outpath);
      goto exit;
    }
  }

  /* open input elementary stream for reading */
  infp = fopen (inpath, "r");
  if (!infp) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "failed to open file %s", inpath);
    goto exit;
  }

  /* Allocate memory for elementary stream buffer */
  es_buf = vvas_memory_alloc (app_handle.vvas_gctx, VVAS_ALLOC_TYPE_NON_CMA,
      VVAS_ALLOC_FLAG_NONE, 0, DEFAULT_READ_SIZE, &vret);
  if (vret != VVAS_RET_SUCCESS) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "Failed to alloc vvas_memory for elementary stream buffer");
    goto exit;
  }

  /* map es_buf to get user space address */
  vret = vvas_memory_map (es_buf, VVAS_DATA_MAP_WRITE, &es_buf_info);
  if (vret != VVAS_RET_SUCCESS) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
        "ERROR: Failed to map the es_buf for write vret = %d", vret);
    goto exit;
  }

  from_vinfo = (VvasVideoInfo *) calloc (1, sizeof (VvasVideoInfo));
  if (!from_vinfo) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "failed to allocate memory");
    goto exit;
  }

  to_vinfo = (VvasVideoInfo *) calloc (1, sizeof (VvasVideoInfo));
  if (!to_vinfo) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel, "failed to allocate memory");
    goto exit;
  }

  while (1) {
    VvasVideoFrame *dec_out_vframe = NULL;

    if (!au_frame && !parser_eos) {
      vret =
          vvas_app_get_es_frame (&app_handle, infp, es_buf, &es_buf_info,
          &au_frame, &incfg);
      if (VVAS_IS_ERROR (vret)) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
            "failed to get elementary stream buffer");
        goto exit;
      }

      if (configure_decoder) {
        vret =
            vvas_app_configure_decoder (&app_handle, incfg, &dec_outbuf_list);
        if (VVAS_IS_ERROR (vret)) {
          LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
              "failed to configure decoder");
          goto exit;
        }

        free (incfg);
        configure_decoder = 0;
      }

      if (vret == VVAS_RET_EOS) {
        printf ("\nParser End of stream\n");
        parser_eos = 1;
      }
    }

    /* submit input frame and free output frames for decoding */
    vret =
        vvas_decoder_submit_frames (app_handle.dec_ctx, au_frame,
        dec_outbuf_list);
    if (VVAS_IS_ERROR (vret)) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
          "submit_frames() failed ret = %d", vret);
      goto exit;
    } else if (vret == VVAS_RET_SEND_AGAIN) {
      LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
          "input buffer is not consumed. send again");
    } else if (vret == VVAS_RET_SUCCESS && au_frame) {
      LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "input buffer consumed");
      if (au_frame) {
        vvas_memory_free (au_frame);
        au_frame = NULL;
      }
    }
    vvas_list_free (dec_outbuf_list);   /* list will be freed but not the output video frames */
    dec_outbuf_list = NULL;

    /* get output frame from decoder */
    vret = vvas_decoder_get_decoded_frame (app_handle.dec_ctx, &dec_out_vframe);
    if (vret == VVAS_RET_NEED_MOREDATA) {
      /* need more data to decode */
      LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "decoder need more input data");
    } else if (vret == VVAS_RET_SUCCESS) {
      VvasOverlayFrameInfo overlay;
      VvasOverlayShapeInfo shape_info;
      VvasVideoFrame *yolov3_dpu_inputs[MAX_NUM_OBJECT] = { 0, };
      VvasInferPrediction *yolov3_pred[MAX_NUM_OBJECT] = { 0, };

      LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel, "Got decoded buffer");

      VvasVideoFrame *sc_outframe =
          app_handle.scaler_outframes[yolov3_cur_bsize];

      /* scale and convert decoder output frame */
      vret =
          vvas_app_scale_decoded_frame (&app_handle, dec_out_vframe,
          sc_outframe);
      if (VVAS_IS_ERROR (vret)) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
            "failed to get scaled frame = %d", vret);
        goto exit;
      }
      LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
          "Prepared yolov3 batch buffer idx %d", yolov3_cur_bsize);

      cached_dec_outframes.push_back (dec_out_vframe);
      yolov3_cur_bsize++;

      if (yolov3_cur_bsize < app_handle.yolov3_outcfg.batch_size) {
        continue;
      } else {
        for (uint8_t i = 0; i < yolov3_cur_bsize; i++) {
          yolov3_dpu_inputs[i] = app_handle.scaler_outframes[i];
        }
      }

      vret =
          vvas_dpuinfer_process_frames (app_handle.yolov3_handle,
          yolov3_dpu_inputs, yolov3_pred, yolov3_cur_bsize);
      if (VVAS_IS_ERROR (vret)) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
            "failed to do yolov3 inference = %d", vret);
        goto exit;
      }

      for (uint8_t idx = 0; idx < yolov3_cur_bsize; idx++) {
        VvasVideoFrame *cur_dec_outframe = cached_dec_outframes[idx];
        VvasInferPrediction *cur_yolov3_pred = yolov3_pred[idx];

        if (cur_yolov3_pred) {
          PredictionScaleData data = { 0, };

          data.from = from_vinfo;
          data.to = to_vinfo;

          vvas_video_frame_get_videoinfo (yolov3_dpu_inputs[idx], from_vinfo);
          vvas_video_frame_get_videoinfo (cur_dec_outframe, to_vinfo);

          /* scale metadata to original buffer (i.e. decoder output buffer) */
          vvas_treenode_traverse (cur_yolov3_pred->node, IN_ORDER, TRAVERSE_ALL,
              -1, (vvas_treenode_traverse_func) node_scale_ip, &data);

          app_handle.crop_handle.src_vframe = cur_dec_outframe;

          /* prepare list of output frames to crop from main buffer */
          app_handle.ppe_changed = FALSE;
          vvas_treenode_traverse (cur_yolov3_pred->node, PRE_ORDER,
              TRAVERSE_ALL, RESNET18_INFER_LEVEL,
              vvas_app_scaler_crop_each_bbox, &app_handle);

          if (vvas_list_length (app_handle.crop_handle.proc_oframes)) {
            uint32_t i;
            uint32_t len;
            VvasInferPrediction *resnet18_pred[MAX_NUM_OBJECT] = { 0, };
            uint32_t cur_batch_size = 0;
            VvasVideoFrame *resnet18_dpu_inputs[MAX_NUM_OBJECT] = { 0, };

            /* crop frames using scaler */
            vret = vvas_scaler_process_frame (app_handle.sc_crop_ctx);
            if (VVAS_IS_ERROR (vret)) {
              LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
                  "failed to crop frames by scaler");
              goto exit;
            }

            cur_batch_size =
                vvas_list_length (app_handle.crop_handle.proc_oframes);

            if (cur_batch_size > app_handle.resnet18_car_make_incfg.batch_size) {
              LOG_MESSAGE (LOG_LEVEL_WARNING, gloglevel,
                  "Current frame has more objects (%d) than batch size %d",
                  cur_batch_size,
                  app_handle.resnet18_car_make_incfg.batch_size);
              cur_batch_size = app_handle.resnet18_car_make_incfg.batch_size;
            }

            LOG_MESSAGE (LOG_LEVEL_DEBUG, gloglevel,
                "resnet18 current batch size = %d", cur_batch_size);

            for (i = 0; i < cur_batch_size; i++) {
              VvasVideoFrame *crop_oframe = NULL;

              crop_oframe =
                  (VvasVideoFrame *) vvas_list_nth_data (app_handle.
                  crop_handle.proc_oframes, i);
              resnet18_pred[i] =
                  (VvasInferPrediction *)
                  vvas_list_nth_data (app_handle.crop_handle.bbox_nodes, i);

              resnet18_dpu_inputs[i] = crop_oframe;
            }

            vret =
                vvas_dpuinfer_process_frames
                (app_handle.resnet18_car_color_handle, resnet18_dpu_inputs,
                resnet18_pred, cur_batch_size);
            if (VVAS_IS_ERROR (vret)) {
              LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
                  "failed to do resnet18 make inference = %d", vret);
              goto exit;
            }

            vret =
                vvas_dpuinfer_process_frames
                (app_handle.resnet18_car_make_handle, resnet18_dpu_inputs,
                resnet18_pred, cur_batch_size);
            if (VVAS_IS_ERROR (vret)) {
              LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
                  "failed to do resnet18 make inference = %d", vret);
              goto exit;
            }

#ifdef XLNX_U30_PLATFORM
            /*
             * IN V70 platform, pre-processing requirement is same for all the
             * Resnet models, hence avoiding this extra process for V70.
             */
            app_handle.ppe_changed = TRUE;
            app_handle.node_num = 0;
            vvas_treenode_traverse (cur_yolov3_pred->node, PRE_ORDER,
                TRAVERSE_ALL, RESNET18_INFER_LEVEL,
                vvas_app_scaler_crop_each_bbox, &app_handle);

            vret = vvas_scaler_process_frame (app_handle.sc_crop_ctx);
            if (VVAS_IS_ERROR (vret)) {
              LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
                  "failed to crop frames by scaler");
              goto exit;
            }
#endif
            vret =
                vvas_dpuinfer_process_frames
                (app_handle.resnet18_car_type_handle, resnet18_dpu_inputs,
                resnet18_pred, cur_batch_size);
            if (VVAS_IS_ERROR (vret)) {
              LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
                  "failed to do resnet18 make inference = %d", vret);
              goto exit;
            }

            len = vvas_list_length (app_handle.crop_handle.proc_oframes);

            for (i = 0; i < len; i++) {
              VvasList *lfree_oframe = NULL;
              VvasVideoFrame *crop_frame = NULL;

              /* take output video frame from free list */
              lfree_oframe =
                  vvas_list_first (app_handle.crop_handle.proc_oframes);
              crop_frame = (VvasVideoFrame *) lfree_oframe->data;
              app_handle.crop_handle.proc_oframes =
                  vvas_list_remove (app_handle.crop_handle.proc_oframes,
                  crop_frame);
              app_handle.crop_handle.free_oframes =
                  vvas_list_append (app_handle.crop_handle.free_oframes,
                  crop_frame);
            }
            vvas_list_free (app_handle.crop_handle.proc_oframes);
            vvas_list_free (app_handle.crop_handle.bbox_nodes);
            app_handle.crop_handle.bbox_nodes = NULL;
            app_handle.crop_handle.proc_oframes = NULL;
          }

          memset (&shape_info, 0x0, sizeof (VvasOverlayShapeInfo));

          /* convert VvasInferPrediction tree to overlay metadata */
          vret = vvas_metaconvert_prepare_overlay_metadata (app_handle.mc_handle,
              cur_yolov3_pred->node, &shape_info);
          if (VVAS_IS_ERROR(vret)) {
            LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
                "failed to convert inference metadata to overlay metadata");
            goto exit;
          }

          vvas_inferprediction_free (cur_yolov3_pred);

          overlay.shape_info = shape_info;
          overlay.frame_info = cur_dec_outframe;

          vvas_overlay_process_frame (&overlay);

          vvas_list_free_full (shape_info.rect_params, free);
          vvas_list_free_full (shape_info.text_params,
              free_overlay_text_params);
        }

        if (outfp) {
          /* write output frame to a file */
          vret = vvas_app_write_output (cur_dec_outframe, outfp);
          if (VVAS_IS_ERROR (vret)) {
            LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
                "failed write output frame");
            goto exit;
          }
        }

        printf ("Processed frames count #%d\r", ++proc_cnt);

        dec_outbuf_list = vvas_list_append (dec_outbuf_list, cur_dec_outframe);
      }

      yolov3_cur_bsize = 0;     /* reset current batch size */
      cached_dec_outframes.clear ();
    } else if (vret == VVAS_RET_EOS) {
      printf ("\nDecoder end of stream\n");

      if (cur_repeat_cnt < max_repeat_cnt) {
        cur_repeat_cnt++;
        printf ("\nrestarting the playback : #%d...\n", cur_repeat_cnt);
        fseek (infp, 0, SEEK_SET);
        /* reset app state */
        parser_eos = 0;
        app_handle.parser_offset = 0;
        app_handle.read_again = 1;
        configure_decoder = 1;
        proc_cnt = 0;
        yolov3_cur_bsize = 0;   /* reset current batch size */
        cached_dec_outframes.clear ();

        vvas_parser_destroy (app_handle.es_parser);
        app_handle.es_parser =
            vvas_parser_create (app_handle.vvas_gctx, app_handle.codectype,
            gloglevel);
        if (!app_handle.es_parser) {
          LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
              "Failed to create parser context");
          return VVAS_RET_ERROR;
        }

        vvas_decoder_destroy (app_handle.dec_ctx);
        vvas_list_free (dec_outbuf_list);
        dec_outbuf_list = NULL;

        vvas_list_free_full (app_handle.dec_out_frames, vvas_video_frame_free);
        app_handle.dec_out_frames = NULL;

        /* Create decoder context */
        app_handle.dec_ctx =
#ifdef XLNX_V70_PLATFORM
            vvas_decoder_create (app_handle.vvas_gctx, (uint8_t *)DEFAULT_VDU_KERNEL_NAME, VVAS_CODEC_H264, 0,
#else
            vvas_decoder_create (app_handle.vvas_gctx, NULL, VVAS_CODEC_H264, 0,
#endif
            gloglevel);
        if (!app_handle.dec_ctx) {
          LOG_MESSAGE (LOG_LEVEL_ERROR, gloglevel,
              "Failed to create decoder context");
          return VVAS_RET_ERROR;
        }
        continue;
      } else {
        printf ("\nreached intended iterations : #%d...\n", cur_repeat_cnt);
        break;
      }
    }
  }

  gret = 0;

exit:                          /* Cleanup */
  if (from_vinfo)
    free (from_vinfo);

  if (to_vinfo)
    free (to_vinfo);

  if (dec_outbuf_list)
    vvas_list_free (dec_outbuf_list);

  if (es_buf) {
    vvas_memory_unmap (es_buf, &es_buf_info);
    vvas_memory_free (es_buf);
  }
  if (au_frame)
    vvas_memory_free (au_frame);

  if (app_handle.crop_handle.free_oframes)
    vvas_list_free_full (app_handle.crop_handle.free_oframes,
        vvas_video_frame_free);

  free_dpuinfer_conf (&app_handle.yolov3_incfg);
  free_dpuinfer_conf (&app_handle.resnet18_car_make_incfg);
  free_dpuinfer_conf (&app_handle.resnet18_car_type_incfg);
  free_dpuinfer_conf (&app_handle.resnet18_car_color_incfg);

  vvas_app_destroy_module_contexts (&app_handle);

  if (app_handle.yolov3_json_path)
    free (app_handle.yolov3_json_path);

  if (app_handle.car_make_json_path)
    free (app_handle.car_make_json_path);

  if (app_handle.car_type_json_path)
    free (app_handle.car_type_json_path);

  if (app_handle.car_color_json_path)
    free (app_handle.car_color_json_path);

  if (app_handle.metaconvert_json_path)
    free (app_handle.metaconvert_json_path);

  if (infp)
    fclose (infp);

  if (outfp)
    fclose (outfp);

  if (inpath)
    free (inpath);

  if (outpath)
    free (outpath);

  if (mjson_path)
    free (mjson_path);

  if (xclbin_loc)
    free (xclbin_loc);

  if (dec_name)
    free (dec_name);

  return gret;
}
