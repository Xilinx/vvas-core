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

#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <jansson.h>
#include <limits.h>
#include <signal.h>
#include <math.h>

#include <vvas_utils/vvas_buffer_pool.h>
#include <vvas_utils/vvas_utils.h>

#include <vvas_core/vvas_common.h>
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

typedef pthread_t Thread;
typedef void *(*thread_function) (void *);

/****************************************************/
typedef uint64_t VvasClockTime;
typedef int64_t VvasClockTimeDiff;

#define SECOND_IN_NS (VvasClockTimeDiff)1000000000
#define SECOND_IN_US (VvasClockTimeDiff)1000000
#define SECOND_IN_MS (VvasClockTimeDiff)1000

#define VVAS_TIME_FORMAT "%u:%02u:%02u.%09u"

#define TIME_IN_NS(t) ((t.tv_sec * SECOND_IN_NS) + t.tv_nsec)

#define VVAS_TIME_ARGS(t) \
        (uint32_t) (((VvasClockTime)(t)) / (SECOND_IN_NS * 60 * 60)), \
        (uint32_t) ((((VvasClockTime)(t)) / (SECOND_IN_NS * 60)) % 60), \
        (uint32_t) ((((VvasClockTime)(t)) / SECOND_IN_NS) % 60), \
        (uint32_t) (((VvasClockTime)(t)) % SECOND_IN_NS)


/**
 *  vvas_get_clocktime() - Get Clock Time
 *
 *  Return: VvasClockTime
 *
 */
static VvasClockTime
vvas_get_clocktime ()
{
  struct timespec now;
  clock_gettime (CLOCK_MONOTONIC, &now);
  return TIME_IN_NS (now);
}

/******************************************************/
#define OVERLAY_THREAD_NAME         "OVERLAY"
#define PARSER_THREAD_NAME          "PARSER"
#define DECODER_THREAD_NAME         "DECODER"
#define SCALER_THREAD_NAME          "SCALER"
#define CROP_SCALER_THREAD_NAME     "CROP_SCALER"
#define YOLOV3_THREAD_NAME          "YOLOV3"
#define RESNET18_THREAD_NAME        "RESNET18"
#define SINK_THREAD_NAME            "SINK"
#define FUNNEL_THREAD_NAME          "FUNNEL"
#define DEFUNNEL_THREAD_NAME        "DEFUNNEL"
#define LAUNCHER_THREAD_NAME        "Launcher"

#define FUNNEL_WAIT_TIME    (36)        /* 36 ms in us, assuming 30 FPS */

#define MAX_STREAMS_PER_PIPELINE    4
#define YOLO_LEVEL                  1
#define RESNET18_INFER_LEVEL        2

#define V70_DEVICE_INDEX    0

#define FPS_DISPLAY_INTERVAL    5       // in second

/*
 *  @brief Time to wait in milliseconds before pushing current batch
 */
#define DEFAULT_BATCH_SUBMIT_TIMEOUT  0

static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;

#define WIDTH_ALIGN   256
#define HEIGHT_ALIGN   64
#define DEFAULT_READ_SIZE 4096
#define DEFAULT_DEV_INDEX 0
#define SCALER_PPC 4
#define SCALER_STRIDE_ALIGNMENT (8 * SCALER_PPC)
#define ALIGN(size,align) ((((size) + (align) - 1) / align) * align)

#ifdef XLNX_U30_PLATFORM
#define SCALER_IP_NAME      "v_multi_scaler:{v_multi_scaler_1}"
#define DECODER_IP_NAME     NULL
#else
#define SCALER_IP_NAME      "image_processing:{image_processing_%hhu}"
#define DECODER_IP_NAME     "kernel_vdu_decoder:{kernel_vdu_decoder_%hhu}"
#endif

#define DEFAULT_FONT_SIZE 0.5
#define DEFAULT_FONT VVAS_FONT_HERSHEY_SIMPLEX
#define DEFAULT_THICKNESS 1
#define DEFAULT_RADIUS 3
#define DEFAULT_MASK_LEVEL 0

#define DEFAULT_FPS_DISPLAY_INTERVAL    5       //In seconds
#define DEFAULT_ADDITIONAL_DEC_BUFFERS  14
#define DEFAULT_STREAM_REPREAT_COUNT    1

#define MAXIMUM_SUPPORTED_STREAMS       16

typedef enum
{
  VVAS_APP_SINK_TYPE_FAKESINK,
  VVAS_APP_SINK_TYPE_FILESINK,
  VVAS_APP_SINK_TYPE_DISPLAY,
} VvasSinkType;

typedef enum
{
  CAR_CLASSIFICATION_TYPE_CAR_COLOR = 0,
  CAR_CLASSIFICATION_TYPE_CAR_MAKE,
  CAR_CLASSIFICATION_TYPE_CAR_TYPE,
  CAR_CLASSIFICATION_TYPE_MAX,
  CAR_CLASSIFICATION_TYPE_LAST = CAR_CLASSIFICATION_TYPE_CAR_TYPE,
} CarClassificationType;

typedef struct _PredictionScaleData PredictionScaleData;
struct _PredictionScaleData
{
  VvasVideoInfo from;
  VvasVideoInfo to;
};

typedef struct
{
  VvasDecoder *vvas_decoder;
  VvasBufferPool *buffer_pool;
  pthread_mutex_t decoder_mutex;
  pthread_cond_t free_buffer_cond;

  /** This hash table contains the mapping of <key>:<value> = VvasVideoFrame:VvasVideoBuffer */
  VvasHashTable *out_buf_hash_table;
  bool are_free_buffers_available;
} DecoderContext;

typedef struct
{
  VvasParser *vvas_parser;
  int32_t parser_offset;
  uint8_t read_again;
} ParserContext;

typedef enum
{
  VVAS_EOS_NONE,
  VVAS_STREAM_EOS,
  VVAS_PIPELINE_EOS,
} VvasEOSType;

typedef struct
{
  /** Parsed frame */
  VvasMemory *parsed_frame;
  /** Decoder input configuration */
  VvasDecoderInCfg *dec_cfg;
  /** Parser EOS? */
  VvasEOSType eos_type;
} ParserBuffer;

typedef struct
{
  /** Decoded buffer, produced by decoder and consumed by scaler_1, scaler_2,
   * overlay and sink, sink will free the buffer */
  VvasVideoBuffer *main_buffer;
  /** Level 1 scaled, pre-processed buffer, produced by scaler_1 and consumed
   * by yolov3, yolov3 will free the buffer */
  VvasVideoBuffer *level_1_scaled_buffer;
  /** List of Level 2 cropped, pre-processed VvasVideoBuffers, produced by
   * scaler_2 and consumed by resnet18, last resnet18 will free the buffer */
  VvasList *level_2_cropped_buffers;
  /** End of stream/pipeline information */
  VvasEOSType eos_type;
  /* Stream/Source ID */
  uint8_t stream_id;
} VvasPipelineBuffer;

typedef struct
{
  VvasScaler *vvas_scaler;
  VvasVideoFrame *src_frame;
  VvasBufferPool *buffer_pool;
  VvasScalerPpe *ppe;
  VvasPipelineBuffer *pipeline_buf;
  const VvasModelConf *model_config;
} CropScalerData;

typedef struct
{
  char *input_file[MAX_STREAMS_PER_PIPELINE];

  VvasContext *parser_vvas_ctx[MAX_STREAMS_PER_PIPELINE];

  VvasQueue *parser_out_queue[MAX_STREAMS_PER_PIPELINE];
  VvasQueue *decoder_out_queue[MAX_STREAMS_PER_PIPELINE];
  VvasQueue *scaler_out_queue[MAX_STREAMS_PER_PIPELINE];
  VvasQueue *funnel_out_queue;
  VvasQueue *yolov3_out_queue;
  VvasQueue *crop_scaler_out_queue;
  VvasQueue *resnet18_out_queue[CAR_CLASSIFICATION_TYPE_MAX];
  VvasQueue *defunnel_out_queue[MAX_STREAMS_PER_PIPELINE];
  VvasQueue *overlay_out_queue[MAX_STREAMS_PER_PIPELINE];

  VvasCodecType codec_type[MAX_STREAMS_PER_PIPELINE];

  Thread parser_thread[MAX_STREAMS_PER_PIPELINE];
  Thread decoder_thread[MAX_STREAMS_PER_PIPELINE];
  Thread scaler_thread[MAX_STREAMS_PER_PIPELINE];
  Thread funnel_thread;
  Thread yolov3_thread;
  Thread crop_scaler_thread;
  Thread resnet18_threads[CAR_CLASSIFICATION_TYPE_MAX];
  Thread defunnel_thread;
  Thread overlay_thread[MAX_STREAMS_PER_PIPELINE];
  Thread sink_thread[MAX_STREAMS_PER_PIPELINE];

  uint64_t sink_frame_render_count[MAX_STREAMS_PER_PIPELINE];
  pthread_mutex_t frame_rate_lock[MAX_STREAMS_PER_PIPELINE];
  pthread_mutex_t pipeline_lock;

  VvasClockTime start_ts[MAX_STREAMS_PER_PIPELINE];

  bool is_sink_thread_alive[MAX_STREAMS_PER_PIPELINE];

  bool is_stream_error[MAX_STREAMS_PER_PIPELINE];

  uint8_t instance_id;
  int32_t num_streams;

} PipelineContext;

typedef struct
{
  PipelineContext *pipeline_ctx;
  uint8_t instance_id;
} ThreadData;

/* Global context available to all */
static VvasSinkType gsink_type = VVAS_APP_SINK_TYPE_FAKESINK;
static VvasLogLevel glog_level = LOG_LEVEL_WARNING;
static uint32_t stream_repeat_count = DEFAULT_STREAM_REPREAT_COUNT;
static bool is_interrupt = false;
static VvasClockTime app_start_time = 0;
static int32_t dev_idx = V70_DEVICE_INDEX;

static uint16_t additional_decoder_buffers = DEFAULT_ADDITIONAL_DEC_BUFFERS;
static uint32_t fps_display_interval = DEFAULT_FPS_DISPLAY_INTERVAL;
static uint32_t batch_timeout = DEFAULT_BATCH_SUBMIT_TIMEOUT;

static char *xclbin_location = NULL;
static char *yolov3_json_path = NULL;
static char *car_make_json_path = NULL;
static char *car_type_json_path = NULL;
static char *car_color_json_path = NULL;

static VvasDpuInferConf yolov3_config;
static VvasDpuInferConf resnet18_car_make_config;
static VvasDpuInferConf resnet18_car_type_config;
static VvasDpuInferConf resnet18_car_color_config;

static VvasModelConf yolov3_model_requirement;
static VvasModelConf resnet18_make_type_color_req;

static char *metaconvert_json_path = NULL;
static VvasMetaConvertConfig metaconvert_cfg;
static VvasList *input_files = NULL;

/**
 *  vvas_app_logger() - Application logger
 *
 *  @level: Signal number
 *  @func_name: Function name
 *  @line_num: Line number
 *  @format: Args to be printed
 *
 *  Return: None
 *
 */
static void
vvas_app_logger (VvasLogLevel level, const char *func_name,
    int line_num, const char *format, ...)
{
  if (level <= glog_level) {
    char *str = NULL;
    char thread_name[16] = { 0 };
    va_list v_args;

    switch (level) {
      case LOG_LEVEL_ERROR:
        str = (char *) "ERROR";
        break;
      case LOG_LEVEL_WARNING:
        str = (char *) "WARNING";
        break;
      case LOG_LEVEL_INFO:
        str = (char *) "INFO";
        break;
      default:
      case LOG_LEVEL_DEBUG:
        str = (char *) "DEBUG";
        break;
    }

    va_start (v_args, format);
    Thread thread_id;
    thread_id = pthread_self ();
    pthread_getname_np (thread_id, thread_name, 16);

    pthread_mutex_lock (&print_mutex);
    VvasClockTime now = vvas_get_clocktime ();
    VvasClockTimeDiff elapsed = now - app_start_time;
    printf (VVAS_TIME_FORMAT, VVAS_TIME_ARGS (elapsed));
    printf (" [%s:%d] 0x%0lx %s %s: ", func_name, line_num,
        thread_id, thread_name, str);
    vprintf (format, v_args);
    printf ("\n");
    pthread_mutex_unlock (&print_mutex);

    va_end (v_args);
  }
}

#define VVAS_APP_LOGGER(log_level, ...) \
    vvas_app_logger (log_level, __func__, __LINE__, __VA_ARGS__)

#define VVAS_APP_ERROR_LOG(...) \
    VVAS_APP_LOGGER (LOG_LEVEL_ERROR, __VA_ARGS__)

#define VVAS_APP_WARNING_LOG(...) \
    VVAS_APP_LOGGER (LOG_LEVEL_WARNING, __VA_ARGS__)

#define VVAS_APP_INFO_LOG(...) \
    VVAS_APP_LOGGER (LOG_LEVEL_INFO, __VA_ARGS__)

#define VVAS_APP_DEBUG_LOG(...) \
    VVAS_APP_LOGGER (LOG_LEVEL_DEBUG, __VA_ARGS__)

#define VVAS_APP_LOG(...) \
  do { \
    VvasClockTime _now; \
    pthread_mutex_lock (&print_mutex); \
    _now = vvas_get_clocktime(); \
    VvasClockTimeDiff _elapsed = _now - app_start_time; \
    printf (VVAS_TIME_FORMAT, VVAS_TIME_ARGS(_elapsed)); \
    printf (" "); \
    printf (__VA_ARGS__); \
    printf ("\n"); \
    pthread_mutex_unlock (&print_mutex); \
  } while (0); \


/**
 *  vvas_app_siginterrupt_handler() - This function is a callback which gets
 *  called whenever this application receives SIGINT signal.
 *
 *  @signum: Signal number
 *
 *  Return: None
 *
 */
static void
vvas_app_siginterrupt_handler (int signum)
{
  pthread_mutex_lock (&global_mutex);
  VVAS_APP_LOG ("Interrupted (SIGINT)");
  is_interrupt = true;
  pthread_mutex_unlock (&global_mutex);
}

/**
 * Print the help text for this application
 */
static void
print_help_text (char *pn)
{
  printf ("Usage: %s [OPTIONS]\n", pn);
  printf ("  -j JSON configuration file\n");
  printf ("  -h Print this help and exit\n\n");
}

/**
 *  @fn VvasCodecType get_video_codec_type (const char *file_path)
 *  @param [in] file_path    Elementary Video file path
 *  @return VvasCodecType
 *  @brief  This function find the type of elementary stream based on its extension.
 */
static VvasCodecType
get_video_codec_type (const char *file_path)
{
  VvasCodecType codec_type;;

  if (strstr (file_path, ".h264") || strstr (file_path, ".264") ||
      strstr (file_path, ".avc")) {
    codec_type = VVAS_CODEC_H264;
  } else if (strstr (file_path, ".h265") || strstr (file_path, ".hevc") ||
      strstr (file_path, ".265")) {
    codec_type = VVAS_CODEC_H265;
  } else {
    codec_type = VVAS_CODEC_UNKNOWN;
  }

  return codec_type;
}

/**
 *  @fn static int vvas_app_parse_master_json (const char *mjson_path)
 *  @param [in] mjson_path    Elementary Video file path
 *  @return Returns 0 on success, -1 on failure
 *  @brief  This function parsers master json file.
 */
static int
vvas_app_parse_master_json (const char *mjson_path)
{
  json_t *root = NULL, *value = NULL;
  json_error_t error;
  size_t input_files_size;
  uint32_t actual_number_of_files = 0;

  root = json_load_file (mjson_path, JSON_DECODE_ANY, &error);
  if (!root) {
    VVAS_APP_ERROR_LOG ("failed to load json file(%s) reason %s",
        mjson_path, error.text);
    return -1;
  }

  value = json_object_get (root, "log-level");
  if (json_is_integer (value)) {
    glog_level = (VvasLogLevel) json_integer_value (value);
  } else {
    VVAS_APP_ERROR_LOG ("log-level is not set, considering it is as WARNING");
  }

  value = json_object_get (root, "yolov3-config-path");
  if (json_is_string (value)) {
    yolov3_json_path = strdup ((char *) json_string_value (value));
    VVAS_APP_DEBUG_LOG ("yolov3-config-path = %s",
        (char *) json_string_value (value));
  } else {
    VVAS_APP_ERROR_LOG ("level1-config-path is not of string type or "
        "not present");
    goto error;
  }

  value = json_object_get (root, "resnet18-carmake-config-path");
  if (json_is_string (value)) {
    car_make_json_path = strdup ((char *) json_string_value (value));
    VVAS_APP_DEBUG_LOG ("resnet18-carmake-config-path = %s",
        (char *) json_string_value (value));
  } else {
    VVAS_APP_ERROR_LOG ("resnet18-carmake-config-path is not of string type "
        "or not present");
    goto error;
  }

  value = json_object_get (root, "resnet18-cartype-config-path");
  if (json_is_string (value)) {
    car_type_json_path = strdup ((char *) json_string_value (value));
    VVAS_APP_DEBUG_LOG ("resnet18-cartype-config-path = %s",
        (char *) json_string_value (value));
  } else {
    VVAS_APP_ERROR_LOG ("resnet18-cartype-config-path is not of string type "
        "or not present");
    goto error;
  }

  value = json_object_get (root, "resnet18-carcolor-config-path");
  if (json_is_string (value)) {
    car_color_json_path = strdup ((char *) json_string_value (value));
    VVAS_APP_DEBUG_LOG ("resnet18-carcolor-config-path = %s",
        (char *) json_string_value (value));
  } else {
    VVAS_APP_ERROR_LOG ("resnet18-carcolor-config-path is not of string type"
        " or not present");
    goto error;
  }

  value = json_object_get (root, "metaconvert-config-path");
  if (json_is_string (value)) {
    metaconvert_json_path = strdup ((char *) json_string_value (value));
    VVAS_APP_DEBUG_LOG ("metaconvert-config-path = %s",
        (char *) json_string_value (value));
  } else {
    VVAS_APP_ERROR_LOG ("metaconvert-config-path is not of string type or "
        "not present");
    goto error;
  }

  value = json_object_get (root, "xclbin-location");
  if (json_is_string (value)) {
    xclbin_location = strdup ((char *) json_string_value (value));
    VVAS_APP_DEBUG_LOG ("xclbin-location = %s",
        (char *) json_string_value (value));
  } else {
    VVAS_APP_ERROR_LOG ("xclbin-location is not set");
    goto error;
  }

  value = json_object_get (root, "dev-idx");
  if (json_is_integer (value)) {
    dev_idx = json_integer_value (value);
    if (dev_idx < 0) {
      VVAS_APP_WARNING_LOG ("Device Index can't be negative: %d, Taking default",
          dev_idx);
      dev_idx = V70_DEVICE_INDEX;
    }
  } else {
    VVAS_APP_WARNING_LOG ("Device index is not set, taking default: %d",
        dev_idx);
  }

  VVAS_APP_DEBUG_LOG ("Device Index = %d", dev_idx);

  value = json_object_get (root, "sink-type");
  if (json_is_integer (value)) {
    gsink_type = (VvasSinkType) json_integer_value (value);
    VVAS_APP_DEBUG_LOG ("sink-type = %s", gsink_type ? "filesink" : "fakesink");
  } else {
    VVAS_APP_ERROR_LOG ("sink-type is not set, considering fakesink");
  }

  value = json_object_get (root, "additional-decoder-buffers");
  if (json_is_integer (value)) {
    additional_decoder_buffers = json_integer_value (value);
  } else {
    VVAS_APP_WARNING_LOG ("additional-decoder-buffers is not set, "
        "taking default value: %u", DEFAULT_ADDITIONAL_DEC_BUFFERS);
  }

  if (!additional_decoder_buffers) {
    VVAS_APP_WARNING_LOG ("additional-decoder-buffers can't be zero,"
        "taking default: %u", DEFAULT_ADDITIONAL_DEC_BUFFERS);
    additional_decoder_buffers = DEFAULT_ADDITIONAL_DEC_BUFFERS;
  }

  VVAS_APP_DEBUG_LOG ("additional_decoder_buffers: %hu",
      additional_decoder_buffers);

  value = json_object_get (root, "batch-timeout");
  if (json_is_integer (value)) {
    batch_timeout = json_integer_value (value);
    VVAS_APP_DEBUG_LOG ("batch-timeout: %u milliseconds", batch_timeout);
  } else {
    VVAS_APP_WARNING_LOG ("batch-timeout is not set, "
        "taking default value: %u", DEFAULT_BATCH_SUBMIT_TIMEOUT);
  }

  value = json_object_get (root, "fps-display-interval");
  if (json_is_integer (value)) {
    fps_display_interval = json_integer_value (value);
    if (!fps_display_interval) {
      VVAS_APP_WARNING_LOG
          ("fps-display-interval can't be zero, taking default");
      fps_display_interval = DEFAULT_FPS_DISPLAY_INTERVAL;
    }
  }

  VVAS_APP_DEBUG_LOG ("fps_display_interval: %u second(s)",
      fps_display_interval);

  value = json_object_get (root, "repeat-count");
  if (json_is_integer (value)) {
    stream_repeat_count = (VvasLogLevel) json_integer_value (value);
  } else {
    VVAS_APP_ERROR_LOG ("repeat-count is not set, considering it is as %u",
        DEFAULT_STREAM_REPREAT_COUNT);
  }

  if (!stream_repeat_count) {
    stream_repeat_count = DEFAULT_STREAM_REPREAT_COUNT;
  }

  value = json_object_get (root, "input-streams");
  if (!json_is_array (value)) {
    VVAS_APP_ERROR_LOG ("input-files is not set or not an array");
    goto error;
  }

  input_files_size = json_array_size (value);

  for (size_t i = 0; i < input_files_size; i++) {
    json_t *array_value = json_array_get (value, i);
    char *input_file = (char *) json_string_value (array_value);
    /* Check read access to this file */
    if (access (input_file, R_OK)) {
      /* Can't read this file */
      VVAS_APP_ERROR_LOG ("Can't access file %s", input_file);
      continue;
    }

    if (VVAS_CODEC_UNKNOWN == get_video_codec_type (input_file)) {
      /* Coded type for this file in not known, skipping this file */
      VVAS_APP_ERROR_LOG ("Codec unknown for %s", input_file);
      continue;
    }

    if (actual_number_of_files >= MAXIMUM_SUPPORTED_STREAMS) {
      VVAS_APP_WARNING_LOG ("Can process only %u number of files",
          MAXIMUM_SUPPORTED_STREAMS);
      break;
    }

    VVAS_APP_DEBUG_LOG ("Input_file: %s", input_file);
    input_files = vvas_list_append (input_files, strdup (input_file));
    actual_number_of_files++;
  }

  json_decref (root);
  return 0;

error:
  json_decref (root);
  return -1;
}

/**
 *  @fn parse_metaconvert_json (char *json_file, VvasMetaConvertConfig * mc_conf)
 *  @param [in] json_file    MetaConvert json file
 *  @param [out] mc_conf     VvasMetaConvertConfig
 *  @return VvasCodecType
 *  @brief  This function parses meta convert json.
 */
static bool
parse_metaconvert_json (char *json_file, VvasMetaConvertConfig * mc_conf)
{
  json_t *root = NULL, *config = NULL, *val = NULL, *karray = NULL;
  json_error_t error;
  uint32_t index;

  /* get root json object */
  root = json_load_file (json_file, JSON_DECODE_ANY, &error);
  if (!root) {
    VVAS_APP_DEBUG_LOG ("failed to load json file. reason %s", error.text);
    goto error;
  }

  config = json_object_get (root, "config");
  if (!json_is_object (config)) {
    VVAS_APP_ERROR_LOG ("config is not of object type");
    goto error;
  }

  val = json_object_get (config, "display-level");
  if (!val || !json_is_integer (val)) {
    mc_conf->level = 0;
    VVAS_APP_DEBUG_LOG ("display_level is not set, so process all nodes at "
        "all levels");
  } else {
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
    if (index <= 5)
      mc_conf->font_type = (VvasFontType) index;
    else {
      VVAS_APP_ERROR_LOG ("font value out of range. Setting default");
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
    VVAS_APP_DEBUG_LOG ("label_filter not set, adding only class name");
    mc_conf->allowed_labels_count = 1;
    mc_conf->allowed_labels =
        (char **) calloc (mc_conf->allowed_labels_count, sizeof (char *));
    mc_conf->allowed_labels[0] = strdup ("class");
  } else {
    mc_conf->allowed_labels_count = json_array_size (karray);
    mc_conf->allowed_labels =
        (char **) calloc (mc_conf->allowed_labels_count, sizeof (char *));
    for (index = 0; index < json_array_size (karray); index++) {
      mc_conf->allowed_labels[index] =
          strdup (json_string_value (json_array_get (karray, index)));
    }
  }

  /* get classes array */
  karray = json_object_get (config, "classes");
  if (!karray) {
    VVAS_APP_DEBUG_LOG
        ("classification filtering not found, allowing all classes");
    mc_conf->allowed_classes_count = 0;
  } else {
    if (!json_is_array (karray)) {
      VVAS_APP_DEBUG_LOG ("classes key is not of array type");
      goto error;
    }
    mc_conf->allowed_classes_count = json_array_size (karray);
    mc_conf->allowed_classes =
        (VvasFilterObjectInfo **) calloc (mc_conf->allowed_classes_count,
        sizeof (VvasFilterObjectInfo *));

    for (index = 0; index < mc_conf->allowed_classes_count; index++) {
      VvasFilterObjectInfo *allowed_class =
          (VvasFilterObjectInfo *) calloc (1, sizeof (VvasFilterObjectInfo));
      mc_conf->allowed_classes[index] = allowed_class;
      json_t *classes;

      classes = json_array_get (karray, index);
      if (!classes) {
        VVAS_APP_ERROR_LOG ("failed to get class object");
        goto error;
      }

      val = json_object_get (classes, "name");
      if (!json_is_string (val)) {
        VVAS_APP_ERROR_LOG ("name is not found for array %d", index);
        goto error;
      } else {
        strncpy (allowed_class->name,
            (char *) json_string_value (val), META_CONVERT_MAX_STR_LENGTH - 1);
        allowed_class->name[META_CONVERT_MAX_STR_LENGTH - 1] = '\0';
        VVAS_APP_DEBUG_LOG ("name %s", allowed_class->name);
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
  if (mc_conf->allowed_classes) {
    if (mc_conf->allowed_classes_count) {
      for (index = 0; index < mc_conf->allowed_classes_count; index++) {
        free (mc_conf->allowed_classes[index]);
        mc_conf->allowed_classes[index] = NULL;
      }
    }
    free (mc_conf->allowed_classes);
    mc_conf->allowed_classes = NULL;
  }
  json_decref (root);
  return false;
}

/**
 *  get_num_planes() - Get number of planes
 *
 *  @fmt: VvasVideoFormat
 *
 *  Return: Returns number of planes for @fmt
 *
 */
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
      VVAS_APP_ERROR_LOG ("Unsupported video format : %d", fmt);
  }
  return num_planes;
}

/**
 *  reset_dpuinfer_conf() - Reset DPU configuration
 *
 *  @dpu_conf: VvasDpuInferConf
 *
 *  Return: None
 *
 */
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

/**
 *  free_dpuinfer_conf() - Free DPU configuration
 *
 *  @dpu_conf: VvasDpuInferConf
 *
 *  Return: None
 *
 */
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

/**
 *  parse_dpu_json() - Parse DPU JSON file
 *
 *  @json_file: DPU json file
 *  @dpu_conf: VvasDpuInferConf
 *  @gloglevel: Log level
 *
 *  Return: Returns TRUE on success, FALSE on failure.
 *
 */
static bool
parse_dpu_json (char *json_file, VvasDpuInferConf * dpu_conf, int gloglevel)
{
  json_t *root, *kernel, *kconfig, *value, *label = NULL;
  json_error_t error;

  root = json_load_file (json_file, JSON_DECODE_ANY, &error);
  if (!root) {
    VVAS_APP_ERROR_LOG ("failed to load json file(%s) reason %s",
        json_file, error.text);
    goto error;
  }

  kernel = json_object_get (root, "kernel");
  if (!json_is_object (kernel)) {
    VVAS_APP_ERROR_LOG ("failed to find kernel object");
    goto error;
  }

  kconfig = json_object_get (kernel, "config");
  if (!json_is_object (kconfig)) {
    VVAS_APP_ERROR_LOG ("config is not of object type");
    goto error;
  }

  value = json_object_get (kconfig, "model-path");
  if (json_is_string (value)) {
    dpu_conf->model_path = strdup ((char *) json_string_value (value));
    VVAS_APP_DEBUG_LOG ("model-path: %s", (char *) json_string_value (value));
  } else {
    VVAS_APP_ERROR_LOG ("model-path is not of string type");
    goto error;
  }

  value = json_object_get (kconfig, "model-name");
  if (json_is_string (value)) {
    dpu_conf->model_name = strdup ((char *) json_string_value (value));
    VVAS_APP_DEBUG_LOG ("model-name: %s", (char *) json_string_value (value));
  } else {
    VVAS_APP_ERROR_LOG ("model-name is not of string type");
    goto error;
  }

  value = json_object_get (kconfig, "model-format");
  if (!json_is_string (value)) {
    VVAS_APP_DEBUG_LOG ("model-format is not proper, taking BGR as default");
    dpu_conf->model_format = VVAS_VIDEO_FORMAT_BGR;
  } else {
    dpu_conf->model_format =
        get_vvas_video_fmt ((char *) json_string_value (value));
    VVAS_APP_DEBUG_LOG ("model-format: %d", dpu_conf->model_format);
  }
  if (dpu_conf->model_format == VVAS_VIDEO_FORMAT_UNKNOWN) {
    VVAS_APP_ERROR_LOG ("SORRY NOT SUPPORTED MODEL FORMAT %s",
        (char *) json_string_value (value));
    goto error;
  }

  value = json_object_get (kconfig, "model-class");
  if (json_is_string (value)) {
    dpu_conf->modelclass = strdup ((char *) json_string_value (value));
    VVAS_APP_DEBUG_LOG ("model-class: %s", (char *) json_string_value (value));
  } else {
    VVAS_APP_ERROR_LOG ("model-class is not of string type");
    goto error;
  }

  value = json_object_get (kconfig, "seg-out-format");
  if (json_is_integer (value)) {
    dpu_conf->segoutfmt = VvasVideoFormat (json_integer_value (value));
    VVAS_APP_DEBUG_LOG ("seg-out-fmt: %d", dpu_conf->segoutfmt);
  }

  value = json_object_get (kconfig, "batch-size");
  if (!value || !json_is_integer (value)) {
    VVAS_APP_DEBUG_LOG ("Taking batch-size as 1");
    dpu_conf->batch_size = 1;
  } else {
    dpu_conf->batch_size = json_integer_value (value);
  }

  value = json_object_get (kconfig, "vitis-ai-preprocess");
  if (!value || !json_is_boolean (value)) {
    VVAS_APP_DEBUG_LOG ("Setting need_preprocess as FALSE");
    dpu_conf->need_preprocess = false;
  } else {
    dpu_conf->need_preprocess = json_boolean_value (value);
  }

  value = json_object_get (kconfig, "performance-test");
  if (!value || !json_is_boolean (value)) {
    VVAS_APP_DEBUG_LOG ("Setting performance_test as TRUE");
    dpu_conf->performance_test = true;
  } else {
    dpu_conf->performance_test = json_boolean_value (value);
  }

  value = json_object_get (kconfig, "max-objects");
  if (!value || !json_is_integer (value)) {
    VVAS_APP_DEBUG_LOG ("Setting max-objects as %d", UINT_MAX);
    dpu_conf->objs_detection_max = UINT_MAX;
  } else {
    dpu_conf->objs_detection_max = json_integer_value (value);
    VVAS_APP_DEBUG_LOG ("Setting max-objects as %d",
        dpu_conf->objs_detection_max);
  }

  value = json_object_get (kconfig, "segoutfactor");
  if (json_is_integer (value)) {
    dpu_conf->segoutfactor = json_integer_value (value);;
    VVAS_APP_DEBUG_LOG ("Setting segoutfactor as %d", dpu_conf->segoutfactor);
  }

  value = json_object_get (kconfig, "float-feature");
  if (json_is_boolean (value)) {
    dpu_conf->float_feature = json_boolean_value (value);
    dpu_conf->float_feature = json_boolean_value (value);
    VVAS_APP_DEBUG_LOG ("Setting float-feature as %d", dpu_conf->float_feature);
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
        VVAS_APP_DEBUG_LOG ("Adding filter label %s",
            dpu_conf->filter_labels[i]);
      } else {
        dpu_conf->filter_labels[i] = NULL;
        VVAS_APP_ERROR_LOG ("Filter label is not of string type");
      }
    }
  } else {
    dpu_conf->num_filter_labels = 0;
    VVAS_APP_DEBUG_LOG ("No filter labels given");
  }

  json_decref (root);
  return true;

error:
  json_decref (root);
  return false;
}

/**
 *  vvas_app_prepare_dpu_configuration() - Prepare DPU configuration
 *
 *  Return: Returns TRUE on success, FALSE on failure.
 *
 */
static bool
vvas_app_prepare_dpu_configuration ()
{
  VvasDpuInfer *yolov3_handle = NULL;
  VvasDpuInfer *resnet18_handle = NULL;
  VvasReturnType vret = VVAS_RET_SUCCESS;
  bool ret = false;

  reset_dpuinfer_conf (&yolov3_config);
  reset_dpuinfer_conf (&resnet18_car_make_config);
  reset_dpuinfer_conf (&resnet18_car_type_config);
  reset_dpuinfer_conf (&resnet18_car_color_config);

  /* parse user json files */
  if (!parse_dpu_json (yolov3_json_path, &yolov3_config, glog_level)) {
    VVAS_APP_ERROR_LOG ("Error parsing json file");
    goto error;
  }

  if (!parse_dpu_json (car_make_json_path, &resnet18_car_make_config,
          glog_level)) {
    VVAS_APP_ERROR_LOG ("Error parsing json file");
    goto error;
  }

  if (!parse_dpu_json (car_type_json_path, &resnet18_car_type_config,
          glog_level)) {
    VVAS_APP_ERROR_LOG ("Error parsing json file");
    goto error;
  }

  if (!parse_dpu_json (car_color_json_path, &resnet18_car_color_config,
          glog_level)) {
    VVAS_APP_ERROR_LOG ("Error parsing json file");
    goto error;
  }

  /* Get the model requirements (Resolution, format and PPE) */

  /* create Yolov3 handle */
  yolov3_handle = vvas_dpuinfer_create (&yolov3_config, LOG_LEVEL_WARNING);
  if (!yolov3_handle) {
    VVAS_APP_ERROR_LOG ("failed to create yolov3 handle");
    goto error;
  }

  /* get yolov3 model requirements */
  vret = vvas_dpuinfer_get_config (yolov3_handle, &yolov3_model_requirement);
  if (VVAS_IS_ERROR (vret)) {
    VVAS_APP_ERROR_LOG ("Failed to get config from DPU");
    goto error;
  }

  if (yolov3_config.batch_size > yolov3_model_requirement.batch_size) {
    VVAS_APP_ERROR_LOG ("YoloV3 batch size configured [%u] is "
        "greater than the supported by the model [%u]",
        yolov3_config.batch_size, yolov3_model_requirement.batch_size);
    goto error;
  }

  resnet18_handle = vvas_dpuinfer_create (&resnet18_car_make_config,
      LOG_LEVEL_WARNING);
  if (!resnet18_handle) {
    VVAS_APP_ERROR_LOG ("failed to create yolov3 handle");
    goto error;
  }

  /* get Resnet18 model requirements for Car Make and Type */
  vret =
      vvas_dpuinfer_get_config (resnet18_handle, &resnet18_make_type_color_req);
  if (VVAS_IS_ERROR (vret)) {
    VVAS_APP_ERROR_LOG ("Failed to get config from DPU");
    goto error;
  }

  if (resnet18_car_make_config.batch_size >
      resnet18_make_type_color_req.batch_size) {
    VVAS_APP_ERROR_LOG ("Resnet18 Car Make batch size configured [%u] is "
        "greater than the supported by the model [%u]",
        resnet18_car_make_config.batch_size,
        resnet18_make_type_color_req.batch_size);
    goto error;
  }

  VVAS_APP_DEBUG_LOG ("Yolov3 Requirements: "
      "Resolution: %dx%d, batch_size: %d, mean: %f,%f,%f, "
      "scale: %f,%f,%f, format: %d, bacth-size: %hhu, need-preprocess: %d",
      yolov3_model_requirement.model_width,
      yolov3_model_requirement.model_height,
      yolov3_model_requirement.batch_size, yolov3_model_requirement.mean_r,
      yolov3_model_requirement.mean_g, yolov3_model_requirement.mean_b,
      yolov3_model_requirement.scale_r, yolov3_model_requirement.scale_g,
      yolov3_model_requirement.scale_b, yolov3_config.model_format,
      yolov3_config.batch_size, yolov3_config.need_preprocess);

  VVAS_APP_DEBUG_LOG ("Resnet18 Car Make, type and color Requirements: "
      "Resolution: %dx%d, batch_size: %d, mean: %f,%f,%f, "
      "scale: %f,%f,%f, format: %d, batch-size: %hhu, need-preprocess: %d",
      resnet18_make_type_color_req.model_width,
      resnet18_make_type_color_req.model_height,
      resnet18_make_type_color_req.batch_size,
      resnet18_make_type_color_req.mean_r, resnet18_make_type_color_req.mean_g,
      resnet18_make_type_color_req.mean_b, resnet18_make_type_color_req.scale_r,
      resnet18_make_type_color_req.scale_g,
      resnet18_make_type_color_req.scale_b,
      resnet18_car_make_config.model_format,
      resnet18_car_make_config.batch_size,
      resnet18_car_make_config.need_preprocess);

  ret = true;

error:

  if (yolov3_handle) {
    vvas_dpuinfer_destroy (yolov3_handle);
  }

  if (resnet18_handle) {
    vvas_dpuinfer_destroy (resnet18_handle);
  }

  return ret;
}

/**
 *  vvas_app_create_thread() - Create Poxis thread
 *
 *  @thread_name: Name of thread
 *  @thread_routine: Thread routine to be executed
 *  @thrd: Thread instance to store
 *  @thread_args: Argument to the thread
 *
 *  Return: Returns TRUE on success, FALSE on failure.
 *
 */
static bool
vvas_app_create_thread (char *thread_name, thread_function thread_routine,
    Thread * thrd, void *thread_args)
{
  VVAS_APP_DEBUG_LOG ("Starting %s Thread", thread_name);
  if (pthread_create (thrd, NULL, thread_routine, thread_args)) {
    VVAS_APP_ERROR_LOG ("Couldn't create %s thread", thread_name);
    return false;
  }

  /* Thread created successfully, set its name */
  pthread_setname_np (*thrd, thread_name);

  return true;
}

/**
 *  vvas_app_write_output() - This function writes video frame data into file.
 *
 *  @output_frame: VvasVideoFrame
 *  @outfp: File pointer in which output_frame has to be written
 *
 *  Return: Returns VvasReturnType.
 *
 */
static VvasReturnType
vvas_app_write_output (VvasVideoFrame * output_frame, FILE * outfp)
{
  int idx;
  VvasReturnType vret = VVAS_RET_SUCCESS;
  VvasVideoFrameMapInfo out_vmap_info;
  uint32_t offset = 0;

  /* TODO: Currently we are dumping NV12 and RGB/BGR frames,
   * but decoder can give NV12 10 bit data also, or we can make this function
   * generic to dump buffer of any type */

  vret =
      vvas_video_frame_map (output_frame, VVAS_DATA_MAP_READ, &out_vmap_info);
  if (VVAS_IS_ERROR (vret)) {
    VVAS_APP_ERROR_LOG ("failed map output frame");
    return VVAS_RET_ERROR;
  }

  if (VVAS_VIDEO_FORMAT_Y_UV8_420 == out_vmap_info.fmt) {
    /* NV12 video frame */
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
  } else if ((VVAS_VIDEO_FORMAT_RGB == out_vmap_info.fmt) ||
      (VVAS_VIDEO_FORMAT_BGR == out_vmap_info.fmt)) {
    /* RGB/BGR video frame */
    offset = 0;
    for (idx = 0; idx < out_vmap_info.height; idx++) {
      fwrite (out_vmap_info.planes[0].data + offset, 1, out_vmap_info.width * 3,
          outfp);
      offset += out_vmap_info.planes[0].stride;
    }
  }
  vvas_video_frame_unmap (output_frame, &out_vmap_info);
  fflush (outfp);

  return VVAS_RET_SUCCESS;
}

/**
 *  vvas_app_get_es_frame() - This function gets one Access Unit frame from the input file.
 *
 *  @parser_ctx: ParserContext pointer
 *  @infp: Input file
 *  @es_buf: VvasMemory
 *  @es_buf_info: VvasMemoryMapInfo
 *  @au_frame: Pointer to VvasMemory
 *  @incfg: Pointer to VvasDecoderInCfg
 *  @is_eos: Is last frame.
 *
 *  Return: Returns VvasReturnType.
 *
 */
static VvasReturnType
vvas_app_get_es_frame (ParserContext * parser_ctx, FILE * infp,
    VvasMemory * es_buf, VvasMemoryMapInfo * es_buf_info,
    VvasMemory ** au_frame, VvasDecoderInCfg ** incfg, bool *is_eos)
{
  VvasReturnType vret = VVAS_RET_SUCCESS;
  size_t valid_es_size = DEFAULT_READ_SIZE;
  bool got_eos = *is_eos;

  while (1) {
    if (parser_ctx->read_again) {
      valid_es_size = fread (es_buf_info->data, 1, DEFAULT_READ_SIZE, infp);
      if (valid_es_size == 0) {
        VVAS_APP_DEBUG_LOG ("End of stream happened..inform parser");
        got_eos = true;
        es_buf = NULL;
      }
      parser_ctx->parser_offset = 0;
    }

    /* get a complete frame */
    vret = vvas_parser_get_au (parser_ctx->vvas_parser, es_buf,
        valid_es_size, au_frame, &parser_ctx->parser_offset, incfg, got_eos);

    parser_ctx->read_again =
        parser_ctx->parser_offset < (int32_t) valid_es_size ? 0 : 1;

    if (VVAS_IS_ERROR (vret)) {
      VVAS_APP_ERROR_LOG ("failed to parse elementary stream");
      return vret;
    } else if (vret == VVAS_RET_NEED_MOREDATA) {
      continue;
    } else if (vret == VVAS_RET_SUCCESS || vret == VVAS_RET_EOS) {
      return vret;
    }
  }

  return vret;
}

/**
 *  buffer_release_callback() - This function is callback which is called from the buffer
 *  pool whenever any buffer is released.
 *
 *  @user_data: Callback data which was set by user to the buffer pool.
 *
 *  Return: None
 *
 */
static void
buffer_release_callback (void *user_data)
{
  DecoderContext *decoder_ctx = (DecoderContext *) user_data;
  pthread_mutex_lock (&decoder_ctx->decoder_mutex);
  decoder_ctx->are_free_buffers_available = true;
  /* Decoder maybe waiting for free buffers, signal decoder thread */
  pthread_cond_signal (&decoder_ctx->free_buffer_cond);
  pthread_mutex_unlock (&decoder_ctx->decoder_mutex);
}

/**
 *  get_decoder_output_buffers() - Get Decoder buffer from the Decoder's buffer pool.
 *
 *  @decoder_ctx: DecoderContext.
 *  @outbuf_list: List of output buffers
 *
 *  Return: Returns True on success, False on failure
 *
 */
static bool
get_decoder_output_buffers (DecoderContext * decoder_ctx,
    VvasList ** outbuf_list)
{
  /* acquire buffer from the pool */
  VvasVideoBuffer *v_buffer;
  VvasList *dec_outbuf_list = *outbuf_list;
  bool bret = true;
  v_buffer = vvas_buffer_pool_acquire_buffer (decoder_ctx->buffer_pool);
  if (!v_buffer) {
    return false;
  }

  VVAS_APP_DEBUG_LOG ("Acquired buffer: %p", v_buffer);

  /* we acquired VvasVideoBuffer from the pool, but to feed decoder, we need
   * VvasVideoFrame, but par passing buffers to another thread, we will be using
   * VvasVideoBuffe, hence creating as hash map of VvasVideoFrame and
   * VvasVideoBuffer. We will get decoded buffer in terms of VvasVideoFrame, and
   * using this frame, we will get back VvasVideoBuffer from the hash map.
   */
  dec_outbuf_list = vvas_list_append (dec_outbuf_list, v_buffer->video_frame);

  if (!decoder_ctx->out_buf_hash_table) {
    decoder_ctx->out_buf_hash_table =
        vvas_hash_table_new (vvas_direct_hash, vvas_direct_equal);
  }

  if (!vvas_hash_table_lookup (decoder_ctx->out_buf_hash_table,
          v_buffer->video_frame)) {
    VVAS_APP_DEBUG_LOG ("Adding buffer %p in Hash Table", v_buffer);
    bret = vvas_hash_table_insert (decoder_ctx->out_buf_hash_table,
        v_buffer->video_frame, v_buffer);
    if (bret == false) {
      VVAS_APP_ERROR_LOG ("Failed to insert the key/value in the hash table");
      return false;
    }
  }

  *outbuf_list = dec_outbuf_list;
  return true;
}

static void
print_decoder_input_config (const VvasDecoderInCfg * incfg)
{
  VVAS_APP_DEBUG_LOG ("Decoder input config: Resolution %ux%u",
      incfg->width, incfg->height);
  VVAS_APP_DEBUG_LOG ("Decoder input config: Profile %u, level: %u",
      incfg->profile, incfg->level);
  VVAS_APP_DEBUG_LOG ("Decoder input config: Codec Type %u, bit depth: %u, "
      "chroma mode: %u, scan_type: %u", incfg->codec_type,
      incfg->bitdepth, incfg->chroma_mode, incfg->scan_type);
  VVAS_APP_DEBUG_LOG ("Decoder input config: Frame Rate " "%u, "
      "Entropy buf count: %u", incfg->frame_rate, incfg->entropy_buffers_count);
  VVAS_APP_DEBUG_LOG ("Decoder input config: Splitbuf mode %u, clock ratio: %u,"
      " low_latecy: %u", incfg->splitbuff_mode, incfg->clk_ratio,
      incfg->low_latency);
}

/**
 *  vvas_app_configure_decoder() - Configure VVAS Decoder.
 *
 *  @vvas_ctx: VVAS Context
 *  @decoder_ctx: Decoder contexts
 *  @incfg: Decoder input configuration
 *  @outbuf_list: Pointer to List of output buffers
 *
 *  Return: Returns VvasReturnType
 *
 */
static VvasReturnType
vvas_app_configure_decoder (VvasContext * vvas_ctx,
    DecoderContext * decoder_ctx, VvasDecoderInCfg * incfg,
    VvasList ** outbuf_list)
{
  uint32_t i;
  VvasDecoderOutCfg outcfg;
  VvasBufferPoolConfig pool_config = { 0 };
  VvasReturnType vret = VVAS_RET_SUCCESS;

  print_decoder_input_config (incfg);

  /* configure the decoder with parameters received from parser */
  vret = vvas_decoder_config (decoder_ctx->vvas_decoder, incfg, &outcfg);
  if (VVAS_IS_ERROR (vret)) {
    VVAS_APP_ERROR_LOG ("Failed to configure decoder vret=%d", vret);
    return VVAS_RET_ERROR;
  }

  VVAS_APP_DEBUG_LOG ("minimum number of output buffers required for "
      "decoder = %d", outcfg.min_out_buf);

  VVAS_APP_DEBUG_LOG ("vinfo = %dx%d - %d",
      outcfg.vinfo.width, outcfg.vinfo.height, outcfg.vinfo.fmt);

  VVAS_APP_DEBUG_LOG
      ("vinfo alignment : left = %d, right = %d, top = %d, bottom = %d",
      outcfg.vinfo.alignment.padding_left, outcfg.vinfo.alignment.padding_right,
      outcfg.vinfo.alignment.padding_top,
      outcfg.vinfo.alignment.padding_bottom);

  /* We have got minimum buffer requirement of decoder, let's create new buffer pool */
  pool_config.alloc_flag = VVAS_ALLOC_FLAG_NONE;
  pool_config.alloc_type = VVAS_ALLOC_TYPE_CMA;
  pool_config.block_on_empty = false;
  pool_config.mem_bank_idx = outcfg.mem_bank_id;

  pool_config.minimum_buffers = outcfg.min_out_buf + additional_decoder_buffers;
  pool_config.maximum_buffers = pool_config.minimum_buffers + 1;
  pool_config.video_info = outcfg.vinfo;

  if ((pool_config.maximum_buffers > FRM_BUF_POOL_SIZE)||
     (pool_config.minimum_buffers > FRM_BUF_POOL_SIZE)) {
    VVAS_APP_WARNING_LOG ("Decoder can't have more than %u buffers",
        FRM_BUF_POOL_SIZE);
    pool_config.maximum_buffers = FRM_BUF_POOL_SIZE;
    pool_config.minimum_buffers =
        (pool_config.minimum_buffers > pool_config.maximum_buffers) ?
        (pool_config.maximum_buffers - 1) : pool_config.minimum_buffers;
  }

  VVAS_APP_DEBUG_LOG ("Decoder Pool minimum buffers[%u], maximum_buffers[%u]",
      pool_config.minimum_buffers, pool_config.maximum_buffers);

  decoder_ctx->buffer_pool =
      vvas_buffer_pool_create (vvas_ctx, &pool_config, LOG_LEVEL_WARNING);
  if (!decoder_ctx->buffer_pool) {
    VVAS_APP_ERROR_LOG ("Couldn't allocate buffer pool for decoder");
    return VVAS_RET_ALLOC_ERROR;
  }

  vvas_buffer_pool_set_release_buffer_notify_cb (decoder_ctx->buffer_pool,
      buffer_release_callback, decoder_ctx);

  /* Buffer pool with minimum numbers of buffers allocated, let's acquire buffers */
  for (i = 0; i < pool_config.minimum_buffers; i++) {
    get_decoder_output_buffers (decoder_ctx, outbuf_list);
  }

  return VVAS_RET_SUCCESS;
}

static void
compute_factors (VvasVideoInfo * from, VvasVideoInfo * to, double *hfactor,
    double *vfactor)
{
  if (!to || !from || !hfactor || !vfactor) {
    return;
  }

  if (!from->width || !from->height) {
    VVAS_APP_ERROR_LOG ("Wrong width and height paramters");
    return;
  }

  *hfactor = to->width * 1.0 / from->width;
  *vfactor = to->height * 1.0 / from->height;
}

/**
 *  prediction_scale_ip() - Scale VvasInferPrediction
 *
 *  @self: Inference prediction data
 *  @to: VideoInfo from which inference data need to be scaled
 *  @from: VideoInfo to which inference data need to be scaled
 *
 *  Return: None
 *
 */
static void
prediction_scale_ip (VvasInferPrediction * self, VvasVideoInfo * to,
    VvasVideoInfo * from)
{
  double hfactor = 0.0, vfactor = 0.0;

  if (!self || !to || !from)
    return;

  compute_factors (from, to, &hfactor, &vfactor);

  self->bbox.x = nearbyintf (self->bbox.x * hfactor);
  self->bbox.y = nearbyintf (self->bbox.y * vfactor);
  self->bbox.width = nearbyintf (self->bbox.width * hfactor);
  self->bbox.height = nearbyintf (self->bbox.height * vfactor);
}

static int
node_scale_ip (VvasTreeNode * node, void *data)
{
  VvasInferPrediction *self = (VvasInferPrediction *) node->data;
  PredictionScaleData *sdata = (PredictionScaleData *) data;

  prediction_scale_ip (self, &sdata->to, &sdata->from);

  return 0;
}

static void
free_overlay_text_params (void *data)
{
  VvasOverlayTextParams *text_params = (VvasOverlayTextParams *) data;

  if (!text_params) {
    return;
  }

  if (text_params->disp_text) {
    free (text_params->disp_text);
  }

  if (text_params) {
    free (text_params);
  }
}

/**
 *  vvas_app_scaler_crop_each_bbox() - Crop buffers based on Inference metadata
 *
 *  @node: Inference prediction data
 *  @user_data: CropScalerData
 *
 *  Return: Returns True on success, False on failure.
 *
 */
static bool
vvas_app_scaler_crop_each_bbox (const VvasTreeNode * node, void *user_data)
{
  VvasInferPrediction *prediction = (VvasInferPrediction *) node->data;
  CropScalerData *crop_scale_data = (CropScalerData *) user_data;
  VvasScalerRect src_rect;
  VvasScalerRect dst_rect;
  VvasReturnType vret = VVAS_RET_SUCCESS;

  if (vvas_treenode_get_depth ((VvasTreeNode *) node) != RESNET18_INFER_LEVEL)
    return false;

  VVAS_APP_DEBUG_LOG ("received prediction node %p", prediction);

  /* Get output buffer from the crop scaler's output buffer pool */
  VvasVideoBuffer *output_buffer;
  output_buffer =
      vvas_buffer_pool_acquire_buffer (crop_scale_data->buffer_pool);
  if (!output_buffer) {
    return true;
  }

  src_rect.x = prediction->bbox.x;
  src_rect.y = prediction->bbox.y;
  src_rect.width = prediction->bbox.width;
  src_rect.height = prediction->bbox.height;
  src_rect.frame = crop_scale_data->src_frame;

  dst_rect.x = 0;
  dst_rect.y = 0;
  dst_rect.width = crop_scale_data->model_config->model_width;
  dst_rect.height = crop_scale_data->model_config->model_height;
  dst_rect.frame = output_buffer->video_frame;

  VVAS_APP_DEBUG_LOG
      ("cropping frame bbox : frame: %p, x: %d, y: %d, res: %dx%d",
      src_rect.frame, prediction->bbox.x, prediction->bbox.y,
      prediction->bbox.width, prediction->bbox.height);

  /* Store the predication node as metadata for this buffer */
  output_buffer->user_data = prediction;

  vret = vvas_scaler_channel_add (crop_scale_data->vvas_scaler, &src_rect,
      &dst_rect, crop_scale_data->ppe, NULL);
  if (VVAS_IS_ERROR (vret)) {
    VVAS_APP_DEBUG_LOG ("failed to add processing channel in scaler");
    vvas_buffer_pool_release_buffer (output_buffer);
    return true;
  }

  crop_scale_data->pipeline_buf->level_2_cropped_buffers = vvas_list_append
      (crop_scale_data->pipeline_buf->level_2_cropped_buffers, output_buffer);

  return false;
}

/**
 *  vvas_app_make_pipeline_exit() - Force pipeline to exit
 *
 *  @pipeline_ctx: Pipeline context
 *
 *  Return: None
 *
 */
static void
vvas_app_make_pipeline_exit (PipelineContext * pipeline_ctx)
{
  pthread_mutex_lock (&pipeline_ctx->pipeline_lock);
  printf ("Forcing pipeline[%hhu] to exit ...\n", pipeline_ctx->instance_id);
  for (uint8_t idx = 0; idx < pipeline_ctx->num_streams; idx++) {
    pipeline_ctx->is_stream_error[idx] = true;
  }
  pthread_mutex_unlock (&pipeline_ctx->pipeline_lock);
}

/**
 *  vvas_app_make_stream_exit() - Force pipeline's stream to exit
 *
 *  @pipeline_ctx: Pipeline context
 *  @stream_id: Stream ID of the pipeline
 *
 *  Return: None
 *
 */
static void
vvas_app_make_stream_exit (PipelineContext * pipeline_ctx, uint8_t stream_id)
{
  pthread_mutex_lock (&pipeline_ctx->pipeline_lock);
  printf ("Forcing Stream[%hhu.%hhu] to exit ...\n",
      pipeline_ctx->instance_id, stream_id);
  pipeline_ctx->is_stream_error[stream_id] = true;
  pthread_mutex_unlock (&pipeline_ctx->pipeline_lock);
}

/**
 *  vvas_app_free_pipeline_buffer() - Free pipeline buffer
 *
 *  @buffer: Pipeline buffer
 *
 *  Return: None
 *
 */
static void
vvas_app_free_pipeline_buffer (VvasPipelineBuffer * buffer)
{
  if (!buffer)
    return;

  if (buffer->level_1_scaled_buffer) {
    vvas_buffer_pool_release_buffer (buffer->level_1_scaled_buffer);
  }

  if (buffer->level_2_cropped_buffers) {
    uint32_t crop_buf_len;
    crop_buf_len = vvas_list_length (buffer->level_2_cropped_buffers);
    for (uint32_t i = 0; i < crop_buf_len; i++) {
      VvasVideoBuffer *v_buffer;
      v_buffer = (VvasVideoBuffer *)
          vvas_list_nth_data (buffer->level_2_cropped_buffers, i);
      vvas_buffer_pool_release_buffer (v_buffer);
    }
    vvas_list_free (buffer->level_2_cropped_buffers);
  }

  if (buffer->main_buffer) {
    VvasInferPrediction *infer_metadta;
    infer_metadta = (VvasInferPrediction *) buffer->main_buffer->user_data;
    if (infer_metadta) {
      vvas_inferprediction_free (infer_metadta);
    }
    vvas_buffer_pool_release_buffer (buffer->main_buffer);
  }
  free (buffer);
  buffer = NULL;
}

/**
 *  vvas_app_create_vvas_context() - Create VVAS Context
 *
 *  @pipeline_ctx: Pipeline context
 *  @xclbin_loc: XCLBIN location
 *
 *  Return: VvasContext
 *
 */
static VvasContext *
vvas_app_create_vvas_context (const PipelineContext * pipeline_ctx,
    char *xclbin_loc)
{
  VvasContext *vvas_ctx;
  VvasReturnType vret = VVAS_RET_SUCCESS;
  int32_t device_idx;

  if (xclbin_loc) {
#ifdef XLNX_U30_PLATFORM
    /* U30 platform */

    /* Each pipeline has 4 stream i.e. 1080p30 * 4, hence 2 pipeline means
     * 1080p30 * 8, means we have occupied decoder completely.
     * So to run more than 2 pipelines, we need another device.
     * on V70, in same device we have multiple instances of decoder, hence
     * this is not needed for V70.
     */

    if (pipeline_ctx->instance_id <= 1) {
      device_idx = 0;
    } else {
      device_idx = 1;
    }
#else
    device_idx = dev_idx;
#endif
  } else {
    /* XCLBIN in not given, allocate SW VVAS context */
    device_idx = -1;
  }

  vvas_ctx =
      vvas_context_create (device_idx, xclbin_loc, LOG_LEVEL_WARNING, &vret);
  if (!vvas_ctx) {
    VVAS_APP_ERROR_LOG ("Failed to create VVAS Context, ret: %d", vret);
  }
  return vvas_ctx;
}

/**
 *  vvas_app_create_dpuinfer() - Create DPU instance
 *
 *  @dpu_config: DPU configuration
 *  @log_level: logging level
 *
 *  Return: VvasDpuInfer instance
 *
 */
static VvasDpuInfer *
vvas_app_create_dpuinfer (VvasDpuInferConf * dpu_config, VvasLogLevel log_level)
{
  /* calling vvas_dpuinfer_create() concurrently from different threads is
   * causing below issue.
   *     terminate called after throwing an instance of 'std::runtime_error'
   *     what():  Error: Could not acquire CU.
   *     Aborted (core dumped)
   * Hence protecting concurrent call to vvas_dpuinfer_create().
   */
  VvasDpuInfer *dpu_instance;
  pthread_mutex_lock (&global_mutex);
  dpu_instance = vvas_dpuinfer_create (dpu_config, log_level);
  pthread_mutex_unlock (&global_mutex);
  return dpu_instance;
}

static void
release_frames (void *data, void *user_data)
{
  VvasVideoFrame *buffer = (VvasVideoBuffer *) data;
  VvasHashTable *hash_table = (VvasHashTable *) user_data;

  VvasVideoBuffer *v_buffer;

  if (!buffer) {
    return;
  }

  v_buffer = (VvasVideoBuffer *) vvas_hash_table_lookup (hash_table, buffer);
  if (v_buffer) {
    vvas_buffer_pool_release_buffer (v_buffer);
  }
}

static uint64_t
calculate_decoder_resubmit_time (const VvasDecoderInCfg * incfg)
{
  uint32_t clock_ratio = incfg->clk_ratio ? incfg->clk_ratio : 1;
  uint32_t pixel_rate = (incfg->width * incfg->height * (uint64_t)incfg->frame_rate ) / clock_ratio;
  /* VCU decoder maximum capacity of 4K@60 is considered in below calculation */
  uint32_t max_pixel_rate = 3840 * 2160 * 60;
  uint64_t max_timeout_ms = 15 * SECOND_IN_MS;  /* 15 m seconds */

  pixel_rate = (!pixel_rate) ? 1 : pixel_rate;
  pixel_rate = (pixel_rate > max_pixel_rate) ? max_pixel_rate : pixel_rate;

  VVAS_APP_DEBUG_LOG ("Pixel_rate: %u, max_pixel_rate: %u, max_timeout_ms: %lu",
      pixel_rate, max_pixel_rate, max_timeout_ms);

  return max_timeout_ms / (max_pixel_rate / pixel_rate);
}

static void
vvas_app_free_parser_buffer (ParserBuffer * parser_buffer)
{
  if (parser_buffer) {
    if (parser_buffer->parsed_frame) {
      vvas_memory_free (parser_buffer->parsed_frame);
    }
    if (parser_buffer->dec_cfg) {
      free (parser_buffer->dec_cfg);
    }
    VVAS_APP_DEBUG_LOG ("Freeing buffer: %p", parser_buffer);
    free (parser_buffer);
  }
}

static bool
is_stream_interrupted (PipelineContext * pipeline_ctx, uint8_t stream_id)
{
  bool is_interrupted = false, is_stream_error = false;
  bool is_global_interrupt = false;

  pthread_mutex_lock (&global_mutex);
  is_global_interrupt = is_interrupt;
  pthread_mutex_unlock (&global_mutex);

  pthread_mutex_lock (&pipeline_ctx->pipeline_lock);
  is_stream_error = pipeline_ctx->is_stream_error[stream_id];
  pthread_mutex_unlock (&pipeline_ctx->pipeline_lock);

  is_interrupted = is_stream_error || is_global_interrupt;
  return is_interrupted;
}

/**
 *  vvas_sink_thread() - Sink Thread Routine
 *
 *  @args: Sink thread arguments
 *
 *  Return: NULL
 *
 */
static void *
vvas_sink_thread (void *args)
{
  ThreadData *sink_thread_data = (ThreadData *) args;
  PipelineContext *pipeline_ctx = sink_thread_data->pipeline_ctx;
  FILE *fp = NULL;
  uint8_t instance_num = sink_thread_data->instance_id;
  VvasReturnType vret = VVAS_RET_SUCCESS;
  bool is_error = false, is_pipeline_playing = false;

  VVAS_APP_DEBUG_LOG ("Sink Thread_%hhu started", instance_num);

  while (true) {
    VvasPipelineBuffer *pipeline_buf = (VvasPipelineBuffer *)
        vvas_queue_dequeue (pipeline_ctx->overlay_out_queue[instance_num]);

    if (pipeline_buf) {
      VVAS_APP_DEBUG_LOG ("Got Buffer: %p", pipeline_buf);

      if (VVAS_PIPELINE_EOS == pipeline_buf->eos_type) {
        VVAS_APP_DEBUG_LOG ("Got EOS");
        free (pipeline_buf);
        break;
      }

      if (is_error) {
        vvas_app_free_pipeline_buffer (pipeline_buf);
        continue;
      }

      if (pipeline_buf->main_buffer) {
        if (!is_pipeline_playing) {
          is_pipeline_playing = true;
          VVAS_APP_LOG ("[Sink_%hhu.%hhu] is playing now ...",
              pipeline_ctx->instance_id, instance_num);

          /* Sink got its first buffer, rendering has been started now */
          pipeline_ctx->start_ts[instance_num] = vvas_get_clocktime ();
        }

        if (VVAS_APP_SINK_TYPE_FILESINK == gsink_type) {
          if (!fp) {
            char file_name[256] = { 0 };
            sprintf (file_name, "output_%hhu_%hhu.nv12",
                pipeline_ctx->instance_id, instance_num);
            fp = fopen (file_name, "wb");
          }
          vret =
              vvas_app_write_output (pipeline_buf->main_buffer->video_frame,
              fp);
          if (VVAS_IS_ERROR (vret)) {
            VVAS_APP_ERROR_LOG ("failed to write output frame");
            is_error = true;
            vvas_app_make_stream_exit (pipeline_ctx, instance_num);
            vvas_app_free_pipeline_buffer (pipeline_buf);
            pipeline_buf = NULL;
            continue;
          }
        }
        vvas_buffer_pool_release_buffer (pipeline_buf->main_buffer);
        pthread_mutex_lock (&pipeline_ctx->frame_rate_lock[instance_num]);
        pipeline_ctx->sink_frame_render_count[instance_num]++;
        pthread_mutex_unlock (&pipeline_ctx->frame_rate_lock[instance_num]);
      }
      if (pipeline_buf) {
        VVAS_APP_DEBUG_LOG ("Freeing buf: %p", pipeline_buf);
        free (pipeline_buf);
      }
      pipeline_buf = NULL;
    }
  }                             /* End of while loop */

  pthread_mutex_lock (&pipeline_ctx->pipeline_lock);
  pipeline_ctx->is_sink_thread_alive[instance_num] = false;
  pthread_mutex_unlock (&pipeline_ctx->pipeline_lock);

  if (fp) {
    fclose (fp);
  }

  VVAS_APP_DEBUG_LOG ("Exiting Sink thread");
  is_pipeline_playing = true;
  VVAS_APP_LOG ("[Sink_%hhu.%hhu] got EOS", pipeline_ctx->instance_id,
      instance_num);
  free (sink_thread_data);
  pthread_exit (NULL);
}

/**
 *  vvas_overlay_thread() - Overlay Thread Routine
 *
 *  @args: Overlay thread arguments
 *
 *  Return: NULL
 *
 */
static void *
vvas_overlay_thread (void *args)
{
  ThreadData *overlay_thread_data = (ThreadData *) args;
  PipelineContext *pipeline_ctx = overlay_thread_data->pipeline_ctx;
  VvasContext *vvas_ctx = NULL;
  VvasPipelineBuffer *pipeline_buf = NULL;
  VvasMetaConvert *metaconvert_ctx = NULL;
  uint8_t instance_num = overlay_thread_data->instance_id;
  bool is_error = false;

  vvas_ctx = vvas_app_create_vvas_context (pipeline_ctx, NULL);
  if (!vvas_ctx) {
    VVAS_APP_ERROR_LOG ("Failed to create VVAS Context");
    is_error = true;
    goto error;
  }

  metaconvert_ctx = vvas_metaconvert_create (vvas_ctx,
      &metaconvert_cfg, LOG_LEVEL_WARNING, NULL);
  if (!metaconvert_ctx) {
    VVAS_APP_ERROR_LOG ("Failed to create metaconvert handle");
    is_error = true;
    goto error;
  }

  VVAS_APP_DEBUG_LOG ("Overlay Thread_%hhu started", instance_num);

  while (true) {
    VvasInferPrediction *cur_yolov3_pred;

    pipeline_buf = (VvasPipelineBuffer *)
        vvas_queue_dequeue (pipeline_ctx->defunnel_out_queue[instance_num]);

    VVAS_APP_DEBUG_LOG ("Got Buffer: %p", pipeline_buf);

    if (VVAS_PIPELINE_EOS == pipeline_buf->eos_type) {
      VVAS_APP_DEBUG_LOG ("Got EOS");
      break;
    }

    cur_yolov3_pred = (VvasInferPrediction *)
        pipeline_buf->main_buffer->user_data;

    if (cur_yolov3_pred) {
      VvasOverlayFrameInfo overlay;
      VvasOverlayShapeInfo shape_info;
      VvasReturnType vret = VVAS_RET_SUCCESS;

      memset (&shape_info, 0x0, sizeof (VvasOverlayShapeInfo));

      /* convert VvasInferPrediction tree to overlay metadata */
      vret = vvas_metaconvert_prepare_overlay_metadata (metaconvert_ctx,
          cur_yolov3_pred->node, &shape_info);
      if (VVAS_IS_ERROR (vret)) {
        VVAS_APP_ERROR_LOG
            ("failed to convert inference metadata to overlay metadata");
        is_error = true;
        break;
      }

      vvas_inferprediction_free (cur_yolov3_pred);
      pipeline_buf->main_buffer->user_data = NULL;

      overlay.shape_info = shape_info;
      overlay.frame_info = pipeline_buf->main_buffer->video_frame;

      vret = vvas_overlay_process_frame (&overlay);
      if (VVAS_IS_ERROR (vret)) {
        VVAS_APP_ERROR_LOG ("Failed to draw, %d", vret);
        is_error = true;
      }

      vvas_list_free_full (shape_info.rect_params, free);
      vvas_list_free_full (shape_info.text_params, free_overlay_text_params);

      if (is_error) {
        break;
      }
    }

    /* Send this buffer downstream */
    VVAS_APP_DEBUG_LOG ("Pushing buffer downstream: %p", pipeline_buf);

    vvas_queue_enqueue (pipeline_ctx->overlay_out_queue[instance_num],
        pipeline_buf);
    pipeline_buf = NULL;
  }                             /* End of while loop */

error:
  if (is_error) {
    /* We got Error in this thread, notify main streaming thread */
    vvas_app_make_stream_exit (pipeline_ctx, instance_num);

    if (pipeline_buf) {
      vvas_app_free_pipeline_buffer (pipeline_buf);
    }

    while (true) {
      /* Wait for EOS from upstream */
      pipeline_buf = (VvasPipelineBuffer *)
          vvas_queue_dequeue (pipeline_ctx->defunnel_out_queue[instance_num]);

      if (VVAS_PIPELINE_EOS == pipeline_buf->eos_type) {
        /* Got EOS break the loop and send EOS downstream */
        break;
      } else {
        /* As we ran into error, we can't process this buffer and send it
         * downstream, hence freeing it */
        vvas_app_free_pipeline_buffer (pipeline_buf);
      }
    }
  }

  VVAS_APP_DEBUG_LOG ("Sending EOS downstream");
  vvas_queue_enqueue (pipeline_ctx->overlay_out_queue[instance_num],
      pipeline_buf);

  if (metaconvert_ctx) {
    vvas_metaconvert_destroy (metaconvert_ctx);
  }

  if (vvas_ctx) {
    vvas_context_destroy (vvas_ctx);
  }

  VVAS_APP_DEBUG_LOG ("Exiting Overlay thread");

  free (overlay_thread_data);
  pthread_exit (NULL);
}

/**
 *  vvas_defunnel_thread() - Defunnel Thread Routine
 *
 *  @args: Defunnel thread arguments
 *
 *  Return: NULL
 *
 */
static void *
vvas_defunnel_thread (void *args)
{
  ThreadData *defunnel_thread_data = (ThreadData *) args;
  PipelineContext *pipeline_ctx = defunnel_thread_data->pipeline_ctx;
  bool exit = false;

  VvasHashTable *hash_table =
      vvas_hash_table_new_full (vvas_int_hash, vvas_int_equal, free, NULL);
  if (!hash_table) {
    VVAS_APP_ERROR_LOG ("Couldn't create hash table");
  }

  /* Prepare hashtable key:stream_id, value:output_queue */
  for (uint8_t idx = 0; idx < pipeline_ctx->num_streams; idx++) {
    uint8_t *key = (uint8_t *) calloc (1, sizeof (uint8_t));
    *key = idx;

    VVAS_APP_DEBUG_LOG ("Adding key: %hhu, value: %p",
        idx, pipeline_ctx->defunnel_out_queue[idx]);

    if (!vvas_hash_table_insert (hash_table, key,
            pipeline_ctx->defunnel_out_queue[idx])) {
      VVAS_APP_ERROR_LOG ("Couldn't add to hash table");
      vvas_app_make_pipeline_exit (pipeline_ctx);
    }
  }

  VVAS_APP_DEBUG_LOG ("De-Funnel thread is up now");

  while (!exit) {
    VvasPipelineBuffer *pipeline_buf = NULL;
    VvasQueue *output_queue = NULL;

    pipeline_buf = (VvasPipelineBuffer *) vvas_queue_dequeue
        (pipeline_ctx->resnet18_out_queue[CAR_CLASSIFICATION_TYPE_LAST]);

    VVAS_APP_DEBUG_LOG ("Got Buffer for stream: %hhu: %p",
        pipeline_buf->stream_id, pipeline_buf);

    output_queue = (VvasQueue *)
        vvas_hash_table_lookup (hash_table, &pipeline_buf->stream_id);
    if (!output_queue) {
      //TODO: Error handling
      VVAS_APP_ERROR_LOG ("Couldn't get from hash table");
    }

    if (VVAS_STREAM_EOS == pipeline_buf->eos_type) {
      VVAS_APP_DEBUG_LOG ("Got EOS for stream: %hhu", pipeline_buf->stream_id);

      /* Convert this STREAM EOS to PIPELINE_EOS */
      pipeline_buf->eos_type = VVAS_PIPELINE_EOS;

      /* Remove the entry of output queue from the hash table */
      vvas_hash_table_remove (hash_table, &pipeline_buf->stream_id);

      /* Send it to output queue */
      vvas_queue_enqueue (output_queue, pipeline_buf);

    } else if (VVAS_PIPELINE_EOS == pipeline_buf->eos_type) {
      /* Got Pipeline EOS, need to send this all existing output queues, and
       * exit this de-funnel thread */
      uint32_t hash_table_size = vvas_hash_table_size (hash_table);
      uint8_t *stream_id = NULL;
      VvasQueue *out_queue = NULL;
      VvasHashTableIter itr;

      VVAS_APP_DEBUG_LOG ("Got Pipeline EOS");

      /* Free this pipeline buffer */
      vvas_app_free_pipeline_buffer (pipeline_buf);

      vvas_hash_table_iter_init (hash_table, &itr);

      /* Iterate all the entries in get the output queues */
      for (uint32_t idx = 0; idx < hash_table_size; idx++) {
        vvas_hash_table_iter_next (&itr, (void **) &stream_id,
            (void **) &out_queue);

        if (stream_id && out_queue) {
          /* Create a copy of EOS buffer and send it to this out_queue */
          pipeline_buf =
              (VvasPipelineBuffer *) calloc (1, sizeof (VvasPipelineBuffer));
          pipeline_buf->stream_id = *stream_id;
          VVAS_APP_DEBUG_LOG ("Sending EOS to %hhu", *stream_id);
          pipeline_buf->eos_type = VVAS_PIPELINE_EOS;
          vvas_queue_enqueue (out_queue, pipeline_buf);
          pipeline_buf = NULL;
        }
      }

      exit = true;

    } else {
      VVAS_APP_DEBUG_LOG ("Pushing buffer to output queue: %p", output_queue);
      vvas_queue_enqueue (output_queue, pipeline_buf);
    }
  }

  if (hash_table) {
    vvas_hash_table_destroy (hash_table);
  }

  free (defunnel_thread_data);
  VVAS_APP_DEBUG_LOG ("Exiting Defnnel thread");
  pthread_exit (NULL);
}

/**
 *  vvas_funnel_thread() - Funnel Thread Routine
 *
 *  @args: Funnel thread arguments
 *
 *  Return: NULL
 *
 */
static void *
vvas_funnel_thread (void *args)
{
  ThreadData *funnel_thread_data = (ThreadData *) args;
  PipelineContext *pipeline_ctx = funnel_thread_data->pipeline_ctx;
  VvasClockTime dequeue_time[MAX_STREAMS_PER_PIPELINE] = { 0 };
  bool exit = false;

  VvasList *input_queues = NULL;

  /* Prepare list of input queues */
  for (uint8_t idx = 0; idx < pipeline_ctx->num_streams; idx++) {
    input_queues =
        vvas_list_append (input_queues, pipeline_ctx->scaler_out_queue[idx]);
  }

  VVAS_APP_DEBUG_LOG ("Funnel thread is up now");

  while (!exit) {
    for (uint32_t idx = 0; idx < vvas_list_length (input_queues); idx++) {
      VvasQueue *in_queue =
          (VvasQueue *) vvas_list_nth_data (input_queues, idx);
      VvasPipelineBuffer *pipeline_buf = NULL;
      uint32_t queue_length = 0;

      queue_length = vvas_queue_get_length (in_queue);
      //VVAS_APP_DEBUG_LOG ("[%u] queue_length: %u", idx, queue_length);
      if (!queue_length) {
        VvasClockTime now, wait_time;
        VvasClockTimeDiff elapsed_time, elapsed_time_ms;

        now = vvas_get_clocktime ();
        now = now / 1000;

        elapsed_time = (dequeue_time[idx]) ? (now - dequeue_time[idx]) : 0;
        elapsed_time_ms = elapsed_time / 1000;
        //VVAS_APP_DEBUG_LOG ("[%u] elapsed_time: %lu, elapsed_time_ms: %lu",
        //    idx, elapsed_time, elapsed_time_ms );

        if (elapsed_time_ms < FUNNEL_WAIT_TIME) {
          wait_time = (FUNNEL_WAIT_TIME * 1000) - (elapsed_time);
          //VVAS_APP_DEBUG_LOG ("Wait_time: %lu", wait_time);
          pipeline_buf = (VvasPipelineBuffer *)
              vvas_queue_dequeue_timeout (in_queue, wait_time);
          if (pipeline_buf) {
            //VVAS_APP_DEBUG_LOG ("Signalled in %lu", ((vvas_get_clocktime() / 1000) - now));
          }
        }
      } else {
        pipeline_buf = (VvasPipelineBuffer *) vvas_queue_dequeue (in_queue);
      }

      if (!pipeline_buf) {
        /* No buffer from this input queue, skip to next */
        //VVAS_APP_DEBUG_LOG ("Didn't get data from %p queue, skipping", in_queue);
        continue;
      }

      dequeue_time[idx] = vvas_get_clocktime ();
      dequeue_time[idx] = dequeue_time[idx] / 1000;

      VVAS_APP_DEBUG_LOG ("Got buffer from %hhu: %p",
          pipeline_buf->stream_id, pipeline_buf);

      if (VVAS_PIPELINE_EOS == pipeline_buf->eos_type) {
        VVAS_APP_DEBUG_LOG ("Stream %hhu is now at EOS",
            pipeline_buf->stream_id);

        /* Remove this in_queue from the list of input queues */
        input_queues = vvas_list_remove (input_queues, in_queue);
        if (!vvas_list_length (input_queues)) {
          /* This was the last in_queue, there is no more input queue, send
           * pipeline EOS and exit */
          exit = true;
        } else {
          /* There are other in_queues in the list, convert this pipeline EOS
           * to stream based EOS */
          pipeline_buf->eos_type = VVAS_STREAM_EOS;
        }
      }

      /* Send this pipeline buffer to output queue */
      VVAS_APP_DEBUG_LOG ("Pushing buffer downstream: %p", pipeline_buf);
      vvas_queue_enqueue (pipeline_ctx->funnel_out_queue, pipeline_buf);
    }
  }

  if (input_queues) {
    vvas_list_free (input_queues);
  }

  free (funnel_thread_data);

  VVAS_APP_DEBUG_LOG ("Exiting funnel thread");
  pthread_exit (NULL);
}

/**
 *  vvas_resnet18_thread() - Resnet18 Thread Routine
 *
 *  @args: Resnet18 thread arguments
 *
 *  Return: NULL
 *
 */
static void *
vvas_resnet18_thread (void *args)
{
  ThreadData *resnet18_thread_data = (ThreadData *) args;
  PipelineContext *pipeline_ctx = resnet18_thread_data->pipeline_ctx;
  VvasDpuInfer *resnet18_handle = NULL;
  VvasQueue *input_queue = NULL, *output_queue = NULL;
  VvasPipelineBuffer *pipeline_buf = NULL;
  VvasPipelineBuffer *partial_buffer = NULL;
  CarClassificationType classification_type;
  uint32_t num_partial_buffers = 0;
  uint32_t batch_size, resnet18_batch_timeout = batch_timeout;
  bool is_error = false, is_eos = false;

  classification_type =
      (CarClassificationType) resnet18_thread_data->instance_id;

  /* Assumption: Pre-processing, resolution and batch size of all three
   * Resnet18 models car make, type and color are same.
   */
  batch_size = resnet18_car_make_config.batch_size;

  switch (classification_type) {

    case CAR_CLASSIFICATION_TYPE_CAR_COLOR:{
      resnet18_handle =
          vvas_app_create_dpuinfer (&resnet18_car_color_config,
          LOG_LEVEL_ERROR);

      input_queue = pipeline_ctx->crop_scaler_out_queue;
      output_queue =
          pipeline_ctx->resnet18_out_queue[CAR_CLASSIFICATION_TYPE_CAR_COLOR];
    }
      break;

    case CAR_CLASSIFICATION_TYPE_CAR_MAKE:{
      resnet18_handle =
          vvas_app_create_dpuinfer (&resnet18_car_make_config, LOG_LEVEL_ERROR);

      input_queue =
          pipeline_ctx->resnet18_out_queue[CAR_CLASSIFICATION_TYPE_CAR_COLOR];
      output_queue =
          pipeline_ctx->resnet18_out_queue[CAR_CLASSIFICATION_TYPE_CAR_MAKE];
    }
      break;

    case CAR_CLASSIFICATION_TYPE_CAR_TYPE:{
      resnet18_handle =
          vvas_app_create_dpuinfer (&resnet18_car_type_config, LOG_LEVEL_ERROR);

      input_queue =
          pipeline_ctx->resnet18_out_queue[CAR_CLASSIFICATION_TYPE_CAR_MAKE];
      output_queue =
          pipeline_ctx->resnet18_out_queue[CAR_CLASSIFICATION_TYPE_CAR_TYPE];
    }
      break;


    default:{
      is_error = true;
      goto error;
    }
      break;
  }

  if (!resnet18_handle) {
    VVAS_APP_ERROR_LOG ("failed to create resnet18_handle handle");
    is_error = true;
    goto error;
  }

  VVAS_APP_DEBUG_LOG ("Resnet18_%d thread up now", classification_type);

  while (true) {
    VvasVideoFrame *resnet18_dpu_inputs[MAX_NUM_OBJECT] = { NULL };
    VvasInferPrediction *resnet18_pred[MAX_NUM_OBJECT] = { NULL };
    VvasPipelineBuffer *output_buffers[MAX_NUM_OBJECT] = { NULL };
    VvasPipelineBuffer *stream_eos_buffers[MAX_STREAMS_PER_PIPELINE] = { NULL };
    VvasReturnType vret = VVAS_RET_SUCCESS;

    uint32_t crop_buf_len = 0;
    uint8_t current_batch_size = 0, num_output_buffers = 0;
    uint8_t stream_eos_buf_count = 0;

    VVAS_APP_DEBUG_LOG ("Preparing batch of size: %u", batch_size);

    while ((current_batch_size < batch_size)) {

      if (partial_buffer && num_partial_buffers) {
        uint32_t offset = 0;
        /* we have partial buffer from the previous batch */
        crop_buf_len =
            vvas_list_length (partial_buffer->level_2_cropped_buffers);

        /* In this partial buffer we have already processed
         * (crop_buf_len - num_partial_buffers) number of buffers.
         */
        offset = crop_buf_len - num_partial_buffers;

        VVAS_APP_DEBUG_LOG ("We have partial buffer: %p, "
            "num_partial_buffers: %u, current_batch_size: %u", partial_buffer,
            num_partial_buffers, current_batch_size);

        if (num_partial_buffers >= batch_size) {
          for (uint32_t i = 0; i < batch_size; i++) {
            VvasInferPrediction *resnet18_pred_node;
            VvasVideoBuffer *v_buffer = (VvasVideoBuffer *)
                vvas_list_nth_data (partial_buffer->level_2_cropped_buffers,
                offset + i);
            resnet18_pred_node = (VvasInferPrediction *) v_buffer->user_data;

            resnet18_dpu_inputs[current_batch_size] = v_buffer->video_frame;
            resnet18_pred[current_batch_size] = resnet18_pred_node;

            current_batch_size++;
            num_partial_buffers--;
          }
          /* As we have prepared the batch, break the batch preparing loop */
          VVAS_APP_DEBUG_LOG ("Batch prepared from the partial buffer");
          break;
        } else {
          for (uint32_t i = 0; i < num_partial_buffers; i++) {
            VvasInferPrediction *resnet18_pred_node;
            VvasVideoBuffer *v_buffer = (VvasVideoBuffer *)
                vvas_list_nth_data (partial_buffer->level_2_cropped_buffers,
                offset + i);
            resnet18_pred_node = (VvasInferPrediction *) v_buffer->user_data;

            resnet18_dpu_inputs[current_batch_size] = v_buffer->video_frame;
            resnet18_pred[current_batch_size] = resnet18_pred_node;

            current_batch_size++;
          }
          output_buffers[num_output_buffers++] = partial_buffer;
          partial_buffer = NULL;
          num_partial_buffers = 0;
        }
      }

      if (!is_eos) {
        if (resnet18_batch_timeout) {
          pipeline_buf = (VvasPipelineBuffer *)
              vvas_queue_dequeue_timeout (input_queue,
              (resnet18_batch_timeout * SECOND_IN_MS));

          if (!pipeline_buf) {
            if (current_batch_size) {
              VVAS_APP_DEBUG_LOG
                  ("timeout... pushing current batch to inference");
              break;
            } else {
              continue;
            }
          }
        } else {
          pipeline_buf =
              (VvasPipelineBuffer *) vvas_queue_dequeue (input_queue);
        }

        VVAS_APP_DEBUG_LOG ("Got Buffer from %hhu: %p",
            pipeline_buf->stream_id, pipeline_buf);
      } else {
        /* We had got EOS in last iteration, and we have processed partial
         * buffers (if any) also, need to quit the batch processing loop */
        break;
      }

      if (VVAS_PIPELINE_EOS == pipeline_buf->eos_type) {
        VVAS_APP_DEBUG_LOG ("Got EOS");
        is_eos = true;
        /* If there is any partial buffer, we need to process them in current
         * batch */
        continue;
      }

      if (VVAS_STREAM_EOS == pipeline_buf->eos_type) {
        /* This stream is EOS now, we can't send it downstream now, because
         * this batch may contain the buffers from this stream, first process
         * the current batch, then push this stream EOS after pushing processed
         * frames. It is possible that while preparing current batch, more than
         * one stream went EOS, hence storing all of them */
        VVAS_APP_DEBUG_LOG ("Got Stream EOS from %hhu",
            pipeline_buf->stream_id);
        stream_eos_buffers[stream_eos_buf_count++] = pipeline_buf;
        pipeline_buf = NULL;
        continue;
      }

      if (pipeline_buf->level_2_cropped_buffers) {
        crop_buf_len = vvas_list_length (pipeline_buf->level_2_cropped_buffers);

        VVAS_APP_DEBUG_LOG ("Crop_buf_len: %u, "
            "current_batch_size: %u", crop_buf_len, current_batch_size);

        if ((crop_buf_len + current_batch_size) <= batch_size) {

          /* We can process this buffer completely in this batch */
          for (uint32_t idx = 0; idx < crop_buf_len; idx++) {
            VvasInferPrediction *resnet18_pred_node;
            VvasVideoBuffer *v_buffer = (VvasVideoBuffer *)
                vvas_list_nth_data (pipeline_buf->level_2_cropped_buffers, idx);
            resnet18_pred_node = (VvasInferPrediction *) v_buffer->user_data;

            resnet18_dpu_inputs[current_batch_size] = v_buffer->video_frame;
            resnet18_pred[current_batch_size] = resnet18_pred_node;
            current_batch_size++;
          }
          output_buffers[num_output_buffers++] = pipeline_buf;
          VVAS_APP_DEBUG_LOG ("We can process %p buffer "
              "completely", pipeline_buf);
          pipeline_buf = NULL;
        } else {

          /* we can't process this buffer fully, need to process it partially
           * in this batch, remaining crop buffers/child buffs will be
           * processed in next batch. */
          uint32_t current_buf_process_count = batch_size - current_batch_size;

          num_partial_buffers = crop_buf_len - current_buf_process_count;

          for (uint32_t idx = 0; idx < current_buf_process_count; idx++) {
            VvasInferPrediction *resnet18_pred_node;
            VvasVideoBuffer *v_buffer = (VvasVideoBuffer *)
                vvas_list_nth_data (pipeline_buf->level_2_cropped_buffers, idx);
            resnet18_pred_node = (VvasInferPrediction *) v_buffer->user_data;

            resnet18_dpu_inputs[current_batch_size] = v_buffer->video_frame;
            resnet18_pred[current_batch_size] = resnet18_pred_node;

            current_batch_size++;
          }
          VVAS_APP_DEBUG_LOG ("%p buffer will be processed "
              "partially", pipeline_buf);
          partial_buffer = pipeline_buf;
          pipeline_buf = NULL;
        }
      } else {
        /* Current buffer doesn't have level 2 cropped buffers, this means
         * that there were no objects detected in this buffer */
        VVAS_APP_DEBUG_LOG ("Buffer doesn't have crop bufs, "
            "%p", pipeline_buf);

        if (current_batch_size) {
          /* As batch preparation has started and current batch has old buffers,
           * we need to hold this buffer and forward it only when old buffers
           * are sent downstream.
           */
          output_buffers[num_output_buffers++] = pipeline_buf;

          if (num_output_buffers >= batch_size) {
            /* We can't wait indefinitely for the batch to be prepared, there
             * is possibility that there are no detection of objects from the
             * Yolov3 and hence no 2nd level crop buffers for large number of
             * buffers, we can't wait indefinitely. Hence breaking the batch
             * preparation loop, we will process all that we have in current
             * batch. */
            VVAS_APP_DEBUG_LOG ("Breaking batch preparation");
            break;
          }

        } else {
          /* There is no old frames in the current batch, we can send this
           * buffer now to downstream .
           */
          VVAS_APP_DEBUG_LOG ("Pushing buffer downstream: %p", pipeline_buf);
          vvas_queue_enqueue (output_queue, pipeline_buf);
        }
        pipeline_buf = NULL;
        continue;
      }
    }

    VVAS_APP_DEBUG_LOG ("Processing batch of size: %u", current_batch_size);

    /* Batch prepared, do inferencing now */
    vret =
        vvas_dpuinfer_process_frames (resnet18_handle, resnet18_dpu_inputs,
        resnet18_pred, current_batch_size);
    if (VVAS_IS_ERROR (vret)) {
      VVAS_APP_ERROR_LOG ("failed to do resnet18 make inference = %d", vret);
      is_error = true;
      for (uint8_t idx = 0; idx < num_output_buffers; idx++) {
        vvas_app_free_pipeline_buffer (output_buffers[idx]);
        output_buffers[idx] = NULL;
      }
      if (partial_buffer) {
        vvas_app_free_pipeline_buffer (partial_buffer);
        partial_buffer = NULL;
      }
      break;
    }

    for (int idx = 0; idx < num_output_buffers; idx++) {
      VvasPipelineBuffer *out_bufer = output_buffers[idx];
      if (CAR_CLASSIFICATION_TYPE_LAST == classification_type) {

        /* This is the last classification level in 2nd stage inferencing,
         * release cropped buffer to the pool */
        uint32_t buf_len =
            vvas_list_length (out_bufer->level_2_cropped_buffers);

        for (uint32_t i = 0; i < buf_len; i++) {
          VvasVideoBuffer *v_buffer =
              (VvasVideoBuffer *)
              vvas_list_nth_data (out_bufer->level_2_cropped_buffers, i);

          /* Prediction node will be freed from overlay thread */
          vvas_buffer_pool_release_buffer (v_buffer);
        }

        vvas_list_free (out_bufer->level_2_cropped_buffers);
        out_bufer->level_2_cropped_buffers = NULL;
      }

      VVAS_APP_DEBUG_LOG ("Pushing buffer downstream: %p", out_bufer);
      vvas_queue_enqueue (output_queue, out_bufer);
      output_buffers[idx] = NULL;
    }

    num_output_buffers = 0;
    current_batch_size = 0;

    for (uint8_t idx = 0; idx < stream_eos_buf_count; idx++) {
      /* Batch processed and sent downstream, Send stream EOS buffers now */
      VVAS_APP_DEBUG_LOG ("Pushing STREAM EOS buffer %p",
          stream_eos_buffers[idx]);
      vvas_queue_enqueue (output_queue, stream_eos_buffers[idx]);
    }

    if (is_eos) {
      /* pipeline_buf is having EOS, we can send it downstream */
      break;
    }
    pipeline_buf = NULL;
  }                             /* End of while loop */

error:
  if (is_error) {
    /* We got Error in this thread, notify main streaming thread */
    vvas_app_make_pipeline_exit (pipeline_ctx);

    if (!is_eos && pipeline_buf) {
      vvas_app_free_pipeline_buffer (pipeline_buf);
    }

    while (!is_eos) {
      /* Wait for EOS from upstream */
      pipeline_buf = (VvasPipelineBuffer *) vvas_queue_dequeue (input_queue);

      if (VVAS_PIPELINE_EOS == pipeline_buf->eos_type) {
        /* Got EOS break the loop and send EOS downstream */
        is_eos = true;
      } else {
        /* As we ran into error, we can't process this buffer and send it to
         * downstream, hence freeing it */
        vvas_app_free_pipeline_buffer (pipeline_buf);
      }
    }
  }

  VVAS_APP_DEBUG_LOG ("Pushing EOS downstream");
  vvas_queue_enqueue (output_queue, pipeline_buf);

  if (resnet18_handle) {
    vvas_dpuinfer_destroy (resnet18_handle);
    VVAS_APP_DEBUG_LOG ("Resnet18 instance Destroyed");
  }

  VVAS_APP_DEBUG_LOG ("Exiting Resnet18 thread");

  free (resnet18_thread_data);

  pthread_exit (NULL);
}

/**
 *  vvas_crop_scale_thread() - Crop Scaler Thread Routine
 *
 *  @args: Crop Scaler thread arguments
 *
 *  Return: NULL
 *
 */
static void *
vvas_crop_scale_thread (void *args)
{
  ThreadData *crop_scale_thread_data = (ThreadData *) args;
  PipelineContext *pipeline_ctx = crop_scale_thread_data->pipeline_ctx;
  VvasContext *vvas_ctx = NULL;
  VvasScaler *vvas_scaler = NULL;
  VvasBufferPool *scaler_buf_pool = NULL;
  VvasBufferPoolConfig pool_config = { 0 };
  VvasPipelineBuffer *pipeline_buf = NULL;
  VvasScalerPpe scaler_ppe = { 0 };
  int32_t num_planes = 0;
  bool is_error = false;
  const VvasDpuInferConf *model_config;
  const VvasModelConf *model_requirement;
  VvasQueue *input_queue, *output_queue;
  char scaler_ip_name[100];

  model_config = &resnet18_car_make_config;
  model_requirement = &resnet18_make_type_color_req;
  input_queue = pipeline_ctx->yolov3_out_queue;
  output_queue = pipeline_ctx->crop_scaler_out_queue;

  num_planes = get_num_planes (model_config->model_format);
  if (!num_planes) {
    VVAS_APP_ERROR_LOG ("Unsupported video format");
    is_error = true;
    goto error;
  }

  pool_config.alloc_flag = VVAS_ALLOC_FLAG_NONE;
  pool_config.alloc_type = VVAS_ALLOC_TYPE_CMA;
  pool_config.block_on_empty = true;
  pool_config.mem_bank_idx = 0;
  pool_config.minimum_buffers = (model_config->batch_size * 3) * 3;
  pool_config.maximum_buffers = 0;

  pool_config.video_info.n_planes = num_planes;
  pool_config.video_info.width = model_requirement->model_width;
  pool_config.video_info.height = model_requirement->model_height;
  pool_config.video_info.fmt = model_config->model_format;
  pool_config.video_info.alignment.padding_top = 0;
  pool_config.video_info.alignment.padding_left = 0;
  pool_config.video_info.alignment.padding_right =
      ALIGN (model_requirement->model_width, SCALER_STRIDE_ALIGNMENT) -
      model_requirement->model_width;
  pool_config.video_info.alignment.padding_bottom =
      ALIGN (model_requirement->model_height, 2) -
      model_requirement->model_height;

  for (int32_t idx = 0; idx < num_planes; idx++) {
    pool_config.video_info.alignment.stride_align[idx] =
        SCALER_STRIDE_ALIGNMENT - 1;
  }

  scaler_ppe.mean_r = model_requirement->mean_r;
  scaler_ppe.mean_g = model_requirement->mean_g;
  scaler_ppe.mean_b = model_requirement->mean_b;
  scaler_ppe.scale_r = model_requirement->scale_r;
  scaler_ppe.scale_g = model_requirement->scale_g;
  scaler_ppe.scale_b = model_requirement->scale_b;

  VVAS_APP_DEBUG_LOG ("Crop Scaler Thread Started");
  VVAS_APP_DEBUG_LOG ("Crop Scaler Output: %d x %d, format: %d",
      pool_config.video_info.width, pool_config.video_info.height,
      pool_config.video_info.fmt);

  vvas_ctx = vvas_app_create_vvas_context (pipeline_ctx, xclbin_location);
  if (!vvas_ctx) {
    VVAS_APP_ERROR_LOG ("Failed to create VVAS Context");
    is_error = true;
    goto error;
  }

  scaler_buf_pool = vvas_buffer_pool_create (vvas_ctx, &pool_config,
      LOG_LEVEL_WARNING);
  if (!scaler_buf_pool) {
    VVAS_APP_ERROR_LOG ("Couldn't allocate buffer pool for scaler");
    is_error = true;
    goto error;
  }

  VVAS_APP_DEBUG_LOG ("Created buffer pool: %p", scaler_buf_pool);

#ifdef XLNX_U30_PLATFORM
  /* U30 platform */
  vvas_scaler =
      vvas_scaler_create (vvas_ctx, SCALER_IP_NAME, LOG_LEVEL_WARNING);
#else
  /* V70 platform */
  if (pipeline_ctx->instance_id <= 1) {
    sprintf (scaler_ip_name, SCALER_IP_NAME, 1);
  } else {
    sprintf (scaler_ip_name, SCALER_IP_NAME, 2);
  }

  VVAS_APP_DEBUG_LOG ("Creating Scaler: %s", scaler_ip_name);

  /* Create scaler context */
  vvas_scaler = vvas_scaler_create (vvas_ctx, scaler_ip_name,
      LOG_LEVEL_WARNING);
#endif

  if (!vvas_scaler) {
    VVAS_APP_ERROR_LOG ("failed to create scaler context");
    is_error = true;
    goto error;
  }

  while (true) {
    VvasReturnType vret = VVAS_RET_SUCCESS;

    pipeline_buf = (VvasPipelineBuffer *)
        vvas_queue_dequeue (input_queue);

    VVAS_APP_DEBUG_LOG ("Got Buffer from %hhu: %p",
        pipeline_buf->stream_id, pipeline_buf);

    if (VVAS_PIPELINE_EOS == pipeline_buf->eos_type) {
      VVAS_APP_DEBUG_LOG ("Got EOS");
      break;
    }

    if (pipeline_buf->main_buffer && pipeline_buf->main_buffer->user_data) {
      /* User data (Inference meta data is there, need to crop all the detected
       * objects */

      VvasInferPrediction *cur_yolov3_pred;
      cur_yolov3_pred =
          (VvasInferPrediction *) pipeline_buf->main_buffer->user_data;

      CropScalerData crop_scale_data = { 0 };
      crop_scale_data.buffer_pool = scaler_buf_pool;
      crop_scale_data.vvas_scaler = vvas_scaler;
      crop_scale_data.src_frame = pipeline_buf->main_buffer->video_frame;
      crop_scale_data.ppe = &scaler_ppe;
      crop_scale_data.pipeline_buf = pipeline_buf;
      crop_scale_data.model_config = model_requirement;

      /* prepare list of output frames to crop from main buffer */
      vvas_treenode_traverse (cur_yolov3_pred->node, PRE_ORDER,
          TRAVERSE_ALL, RESNET18_INFER_LEVEL,
          vvas_app_scaler_crop_each_bbox, &crop_scale_data);

      /* scale input frame */
      vret = vvas_scaler_process_frame (vvas_scaler);
      if (VVAS_IS_ERROR (vret)) {
        VVAS_APP_ERROR_LOG ("failed to process scaler");
        is_error = true;
        break;
      }
    }

    /* Send this buffer downstream */
    VVAS_APP_DEBUG_LOG ("Pushing buffer downstream: %p", pipeline_buf);

    vvas_queue_enqueue (output_queue, pipeline_buf);
    pipeline_buf = NULL;
  }                             /* End of while loop */

error:
  if (is_error) {
    /* We got Error in this thread, notify main streaming thread */
    vvas_app_make_pipeline_exit (pipeline_ctx);

    if (pipeline_buf) {
      vvas_app_free_pipeline_buffer (pipeline_buf);
    }

    while (true) {
      /* Wait for EOS from upstream */
      pipeline_buf = (VvasPipelineBuffer *)
          vvas_queue_dequeue (input_queue);

      if (VVAS_PIPELINE_EOS == pipeline_buf->eos_type) {
        /* Got EOS break the loop and send EOS downstream */
        break;
      } else {
        /* As we ran into error, we can't process this buffer and send it to
         * downstream, hence freeing it */
        vvas_app_free_pipeline_buffer (pipeline_buf);
      }
    }
  }

  VVAS_APP_DEBUG_LOG ("Pushing EOS downstream");
  vvas_queue_enqueue (output_queue, pipeline_buf);

  if (vvas_scaler) {
    vvas_scaler_destroy (vvas_scaler);
    VVAS_APP_DEBUG_LOG ("Crop Scaler Destroyed");
  }

  if (scaler_buf_pool) {
    VVAS_APP_DEBUG_LOG ("Freeing crop_scaler buffer pool");
    vvas_buffer_pool_free (scaler_buf_pool);
  }

  if (vvas_ctx) {
    vvas_context_destroy (vvas_ctx);
  }

  VVAS_APP_DEBUG_LOG ("Exiting Crop Scaler thread");
  free (crop_scale_thread_data);
  pthread_exit (NULL);
}

/**
 *  vvas_yolov3_thread() - Yolov3 Thread Routine
 *
 *  @args: Yolov3 thread arguments
 *
 *  Return: NULL
 *
 */
static void *
vvas_yolov3_thread (void *args)
{
  ThreadData *yolov3_thread_data = (ThreadData *) args;
  PipelineContext *pipeline_ctx = yolov3_thread_data->pipeline_ctx;
  VvasDpuInfer *yolov3_handle = NULL;
  VvasPipelineBuffer *pipeline_buf = NULL;
  uint32_t yolov3_batch_timeout = batch_timeout;
  bool is_error = false, is_eos = false;

  /* create Yolov3 handle */
  yolov3_handle = vvas_app_create_dpuinfer (&yolov3_config, LOG_LEVEL_ERROR);

  if (!yolov3_handle) {
    VVAS_APP_ERROR_LOG ("failed to create yolov3 handle");
    is_error = true;
    goto error;
  }

  VVAS_APP_DEBUG_LOG ("YOLOV3 Thread started");

  while (true) {
    VvasVideoFrame *yolov3_dpu_inputs[MAX_NUM_OBJECT] = { NULL };
    VvasInferPrediction *yolov3_pred[MAX_NUM_OBJECT] = { NULL };
    VvasPipelineBuffer *pipline_buffers[MAX_NUM_OBJECT] = { NULL };
    VvasPipelineBuffer *stream_eos_buffers[MAX_STREAMS_PER_PIPELINE] = { NULL };
    VvasReturnType vret = VVAS_RET_SUCCESS;
    uint8_t current_batch_size = 0;
    uint8_t stream_eos_buf_count = 0;

    VVAS_APP_DEBUG_LOG ("Preparing batch of size : %hhu",
        yolov3_config.batch_size);

    /* Prepare batch */
    while (current_batch_size < yolov3_config.batch_size) {

      if (yolov3_batch_timeout) {
        pipeline_buf = (VvasPipelineBuffer *)
            vvas_queue_dequeue_timeout (pipeline_ctx->funnel_out_queue,
            (yolov3_batch_timeout * SECOND_IN_MS));

        if (!pipeline_buf) {
          if (current_batch_size) {
            VVAS_APP_DEBUG_LOG
                ("timeout... pushing current batch to inference");
            break;
          } else {
            continue;
          }
        }
      } else {
        pipeline_buf = (VvasPipelineBuffer *)
            vvas_queue_dequeue (pipeline_ctx->funnel_out_queue);
      }
      VVAS_APP_DEBUG_LOG ("Got Buffer from %hhu: %p",
          pipeline_buf->stream_id, pipeline_buf);

      if (VVAS_PIPELINE_EOS == pipeline_buf->eos_type) {
        VVAS_APP_DEBUG_LOG ("Got EOS");
        is_eos = true;
        break;
      }

      if ((!pipeline_buf->level_1_scaled_buffer) &&
          !(VVAS_STREAM_EOS == pipeline_buf->eos_type)) {
        VVAS_APP_ERROR_LOG ("No Level-1 scaled buffer");
        is_error = true;
        break;
      }

      if (VVAS_STREAM_EOS == pipeline_buf->eos_type) {
        /* This stream is EOS now, we can't send it downstream now, because
         * this batch may contain the buffers from this stream, first process
         * the current batch, then push this stream EOS after pushing processed
         * frames. It is possible that while preparing current batch, more than
         * one stream went EOS, hence storing all of them */
        VVAS_APP_DEBUG_LOG ("Got Stream EOS from %hhu",
            pipeline_buf->stream_id);
        stream_eos_buffers[stream_eos_buf_count++] = pipeline_buf;
        continue;
      }

      if (pipeline_buf->level_1_scaled_buffer) {
        yolov3_dpu_inputs[current_batch_size] =
            pipeline_buf->level_1_scaled_buffer->video_frame;
        pipline_buffers[current_batch_size] = pipeline_buf;
        pipeline_buf = NULL;
        current_batch_size++;
      }
    }

    if (is_error) {
      for (uint8_t idx = 0; idx < current_batch_size; idx++) {
        vvas_app_free_pipeline_buffer (pipline_buffers[idx]);
      }
      break;
    }

    if (current_batch_size) {

      VVAS_APP_DEBUG_LOG ("Processing batch of %u", current_batch_size);

      vret = vvas_dpuinfer_process_frames (yolov3_handle, yolov3_dpu_inputs,
          yolov3_pred, current_batch_size);
      if (VVAS_IS_ERROR (vret)) {
        VVAS_APP_ERROR_LOG ("failed to do yolov3 inference = %d", vret);
        for (uint8_t idx = 0; idx < current_batch_size; idx++) {
          vvas_app_free_pipeline_buffer (pipline_buffers[idx]);
        }
        is_error = true;
        break;
      }

      for (uint8_t idx = 0; idx < current_batch_size; idx++) {
        VvasPipelineBuffer *buf = pipline_buffers[idx];

        if (yolov3_pred[idx]) {
          /* Objects detected */
          PredictionScaleData data;
          VvasVideoFrame *main_buffer = buf->main_buffer->video_frame;
          memset (&data, 0, sizeof (PredictionScaleData));

          vvas_video_frame_get_videoinfo (yolov3_dpu_inputs[idx], &data.from);
          vvas_video_frame_get_videoinfo (main_buffer, &data.to);

          /* scale metadata to original buffer (i.e. decoder output buffer) */
          vvas_treenode_traverse (yolov3_pred[idx]->node, IN_ORDER,
              TRAVERSE_ALL, -1, (vvas_treenode_traverse_func) node_scale_ip,
              &data);

          /* Store scaled metadata into main buffer's user_data */
          buf->main_buffer->user_data = yolov3_pred[idx];
        }

        /* As level_1 scaled buffer is used for YOLOV3 detection, release it */
        vvas_buffer_pool_release_buffer (buf->level_1_scaled_buffer);
        buf->level_1_scaled_buffer = NULL;

        VVAS_APP_DEBUG_LOG ("Pushing buffer downstream: %p", buf);
        vvas_queue_enqueue (pipeline_ctx->yolov3_out_queue, buf);
      }
    }

    for (uint8_t idx = 0; idx < stream_eos_buf_count; idx++) {
      /* Batch processed and sent downstream, Send stream EOS buffers now */
      VVAS_APP_DEBUG_LOG ("Pushing STREAM EOS buffer %p",
          stream_eos_buffers[idx]);
      vvas_queue_enqueue (pipeline_ctx->yolov3_out_queue,
          stream_eos_buffers[idx]);
    }

    if (is_eos) {
      /* pipeline_buf is having EOS, we can send it downstream */
      break;
    }
  }                             /* End of while loop */

error:
  if (is_error) {
    /* We got Error in this thread, notify main streaming thread */
    vvas_app_make_pipeline_exit (pipeline_ctx);

    if (pipeline_buf) {
      vvas_app_free_pipeline_buffer (pipeline_buf);
    }

    while (true) {
      /* Wait for EOS from upstream */
      pipeline_buf = (VvasPipelineBuffer *)
          vvas_queue_dequeue (pipeline_ctx->funnel_out_queue);

      if (VVAS_PIPELINE_EOS == pipeline_buf->eos_type) {
        /* Got EOS break the loop and send EOS downstream */
        break;
      } else {
        /* As we ran into error, we can't process this buffer and send it to
         * downstream, hence freeing it */
        vvas_app_free_pipeline_buffer (pipeline_buf);
      }
    }
  }

  VVAS_APP_DEBUG_LOG ("Pushing EOS downstream");
  vvas_queue_enqueue (pipeline_ctx->yolov3_out_queue, pipeline_buf);

  if (yolov3_handle) {
    vvas_dpuinfer_destroy (yolov3_handle);
    VVAS_APP_DEBUG_LOG ("YOLOV3 instance Destroyed");
  }

  VVAS_APP_DEBUG_LOG ("Exiting YOLOV3 thread");

  free (yolov3_thread_data);

  pthread_exit (NULL);
}

/**
 *  vvas_scaler_thread() - Scaler Thread Routine
 *
 *  @args: Scaler thread arguments
 *
 *  Return: NULL
 *
 */
static void *
vvas_scaler_thread (void *args)
{
  ThreadData *scaler_thread_data = (ThreadData *) args;
  PipelineContext *pipeline_ctx = scaler_thread_data->pipeline_ctx;
  VvasContext *vvas_ctx = NULL;
  VvasScaler *vvas_scaler = NULL;
  VvasBufferPool *scaler_buf_pool = NULL;
  VvasBufferPoolConfig pool_config = { 0 };
  VvasPipelineBuffer *pipeline_buf = NULL;
  VvasScalerPpe scaler_ppe = { 0 };
  char scaler_ip_name[100];
  int32_t num_planes, idx;
  uint8_t instance_num = scaler_thread_data->instance_id;
  bool is_error = false;

  num_planes = get_num_planes (resnet18_car_make_config.model_format);
  if (!num_planes) {
    VVAS_APP_ERROR_LOG ("Unsupported video format");
    is_error = true;
    goto error;
  }

  pool_config.alloc_flag = VVAS_ALLOC_FLAG_NONE;
  pool_config.alloc_type = VVAS_ALLOC_TYPE_CMA;
  pool_config.block_on_empty = true;
  pool_config.mem_bank_idx = 0;
  pool_config.minimum_buffers = yolov3_config.batch_size * 3;
  pool_config.maximum_buffers = 0;

  pool_config.video_info.n_planes = num_planes;
  pool_config.video_info.width = yolov3_model_requirement.model_width;
  pool_config.video_info.height = yolov3_model_requirement.model_height;
  pool_config.video_info.fmt = yolov3_config.model_format;
  pool_config.video_info.alignment.padding_top = 0;
  pool_config.video_info.alignment.padding_left = 0;
  pool_config.video_info.alignment.padding_right =
      ALIGN (yolov3_model_requirement.model_width, SCALER_STRIDE_ALIGNMENT) -
      yolov3_model_requirement.model_width;
  pool_config.video_info.alignment.padding_bottom =
      ALIGN (yolov3_model_requirement.model_height, 2) -
      yolov3_model_requirement.model_height;

  for (idx = 0; idx < num_planes; idx++) {
    pool_config.video_info.alignment.stride_align[idx] =
        SCALER_STRIDE_ALIGNMENT - 1;
  }

  scaler_ppe.mean_r = yolov3_model_requirement.mean_r;
  scaler_ppe.mean_g = yolov3_model_requirement.mean_g;
  scaler_ppe.mean_b = yolov3_model_requirement.mean_b;
  scaler_ppe.scale_r = yolov3_model_requirement.scale_r;
  scaler_ppe.scale_g = yolov3_model_requirement.scale_g;
  scaler_ppe.scale_b = yolov3_model_requirement.scale_b;

  VVAS_APP_DEBUG_LOG ("Scaler Thread_%hhu started", instance_num);
  VVAS_APP_DEBUG_LOG ("Scaler Output: %d x %d, format: %d",
      pool_config.video_info.width, pool_config.video_info.height,
      pool_config.video_info.fmt);

  vvas_ctx = vvas_app_create_vvas_context (pipeline_ctx, xclbin_location);
  if (!vvas_ctx) {
    VVAS_APP_ERROR_LOG ("Failed to create VVAS Context");
    is_error = true;
    goto error;
  }

#ifdef XLNX_U30_PLATFORM
  /* U30 platform */
  vvas_scaler = vvas_scaler_create (vvas_ctx, SCALER_IP_NAME,
      LOG_LEVEL_WARNING);
#else
  /* V70 platform */
  if (pipeline_ctx->instance_id <= 1) {
    sprintf (scaler_ip_name, SCALER_IP_NAME, 1);
  } else {
    sprintf (scaler_ip_name, SCALER_IP_NAME, 2);
  }

  VVAS_APP_DEBUG_LOG ("Creating Scaler: %s", scaler_ip_name);

  /* Create scaler context */
  vvas_scaler = vvas_scaler_create (vvas_ctx, scaler_ip_name,
      LOG_LEVEL_WARNING);
#endif

  if (!vvas_scaler) {
    VVAS_APP_ERROR_LOG ("failed to create scaler context");
    is_error = true;
    goto error;
  }

  VVAS_APP_DEBUG_LOG ("Created Scaler: %p", vvas_scaler);

  scaler_buf_pool = vvas_buffer_pool_create (vvas_ctx, &pool_config,
      LOG_LEVEL_WARNING);
  if (!scaler_buf_pool) {
    VVAS_APP_ERROR_LOG ("Couldn't allocate buffer pool for scaler");
    is_error = true;
    goto error;
  }

  VVAS_APP_DEBUG_LOG ("Created Pool: %p", scaler_buf_pool);

  while (true) {
    VvasScalerRect src_rect = { 0 }, dst_rect = { 0 };
    VvasVideoInfo in_vinfo;
    VvasVideoBuffer *scaler_out_buf = NULL;
    VvasReturnType vret = VVAS_RET_SUCCESS;

    pipeline_buf = (VvasPipelineBuffer *)
        vvas_queue_dequeue (pipeline_ctx->decoder_out_queue[instance_num]);
    VVAS_APP_DEBUG_LOG ("Got Buffer: %p", pipeline_buf);

    if (VVAS_PIPELINE_EOS == pipeline_buf->eos_type) {
      VVAS_APP_DEBUG_LOG ("Got EOS");
      break;
    }

    /* Got buffer from upstream element, get output buffer from the pool */
    VVAS_APP_DEBUG_LOG ("Going to acquire buffer");
    scaler_out_buf = vvas_buffer_pool_acquire_buffer (scaler_buf_pool);

    VVAS_APP_DEBUG_LOG ("Acquired buffer");
    vvas_video_frame_get_videoinfo (pipeline_buf->main_buffer->video_frame,
        &in_vinfo);

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = in_vinfo.width;
    src_rect.height = in_vinfo.height;
    src_rect.frame = pipeline_buf->main_buffer->video_frame;

    dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.width = pool_config.video_info.width;
    dst_rect.height = pool_config.video_info.height;
    dst_rect.frame = scaler_out_buf->video_frame;

    vret = vvas_scaler_channel_add (vvas_scaler, &src_rect, &dst_rect,
        &scaler_ppe, NULL);
    if (VVAS_IS_ERROR (vret)) {
      VVAS_APP_ERROR_LOG ("failed to add processing channel in scaler");
      is_error = true;
      vvas_buffer_pool_release_buffer (scaler_out_buf);
      break;
    }

    VVAS_APP_DEBUG_LOG ("channel_add done");
    /* scale input frame */
    vret = vvas_scaler_process_frame (vvas_scaler);
    if (VVAS_IS_ERROR (vret)) {
      VVAS_APP_ERROR_LOG ("failed to process");
      is_error = true;
      vvas_buffer_pool_release_buffer (scaler_out_buf);
      break;
    }

    /* Scaler operation successful, scaler_out_buf has scaled buffer */
    pipeline_buf->level_1_scaled_buffer = scaler_out_buf;

    VVAS_APP_DEBUG_LOG ("Pushing buffer downstream: %p", pipeline_buf);

    vvas_queue_enqueue (pipeline_ctx->scaler_out_queue[instance_num],
        pipeline_buf);
    pipeline_buf = NULL;
  }                             /* End of while loop */

error:
  if (is_error) {
    /* We got Error in this thread, notify main streaming thread */
    vvas_app_make_stream_exit (pipeline_ctx, instance_num);

    if (pipeline_buf) {
      vvas_app_free_pipeline_buffer (pipeline_buf);
    }

    while (true) {
      /* Wait for EOS from upstream */
      pipeline_buf = (VvasPipelineBuffer *)
          vvas_queue_dequeue (pipeline_ctx->decoder_out_queue[instance_num]);

      if (VVAS_PIPELINE_EOS == pipeline_buf->eos_type) {
        /* Got EOS break the loop and send EOS downstream */
        break;
      } else {
        /* As we ran into error, we can't process this buffer and send it to
         * downstream, hence freeing it */
        vvas_app_free_pipeline_buffer (pipeline_buf);
      }
    }
  }

  VVAS_APP_DEBUG_LOG ("Pushing EOS downstream");
  vvas_queue_enqueue (pipeline_ctx->scaler_out_queue[instance_num],
      pipeline_buf);

  if (vvas_scaler) {
    vvas_scaler_destroy (vvas_scaler);
    VVAS_APP_DEBUG_LOG ("Scaler Destroyed");
  }

  if (scaler_buf_pool) {
    VVAS_APP_DEBUG_LOG ("Freeing scaler buffer pool");
    vvas_buffer_pool_free (scaler_buf_pool);
  }

  if (vvas_ctx) {
    vvas_context_destroy (vvas_ctx);
  }

  VVAS_APP_DEBUG_LOG ("Exiting Scaler thread");

  free (scaler_thread_data);
  pthread_exit (NULL);
}

/**
 *  vvas_decoder_thread() - Decoder Thread Routine
 *
 *  @args: Decoder thread arguments
 *
 *  Return: NULL
 *
 */
static void *
vvas_decoder_thread (void *args)
{
  ThreadData *decoder_thread_data = (ThreadData *) args;
  PipelineContext *pipeline_ctx = decoder_thread_data->pipeline_ctx;
  VvasContext *vvas_ctx = NULL;
  uint8_t instance_num = decoder_thread_data->instance_id;

  DecoderContext decoder_ctx = { 0 };
  VvasReturnType vret = VVAS_RET_SUCCESS;
  bool configure_decoder = true;
  VvasList *dec_outbuf_list = NULL;
  VvasList *dec_pending_buffers = NULL;
  ParserBuffer *parsed_buffer = NULL;
  VvasPipelineBuffer *out_pipeline_buf = NULL;
  uint64_t decoder_resubmit_time = 0;
  uint8_t decoder_instance_num;
  uint8_t decoder_kernel_name[128] = { 0 };
  uint8_t hw_instance_id = 0;
  bool is_error = false, is_parser_eos = false;

  VVAS_APP_DEBUG_LOG ("Decoder Thread_%hhu started", instance_num);

  pthread_mutex_init (&decoder_ctx.decoder_mutex, NULL);
  pthread_cond_init (&decoder_ctx.free_buffer_cond, NULL);

  /* Currently V70 supports only 2 decoder_instances, and each decoder can
   * process 4Kp60, there are 8 instances for this 0 - 7, each instance capable
   * of processing 1Kp30. kernel_vcu_decoder:{kernel_vcu_decoder_0} to
   * kernel_vcu_decoder:{kernel_vcu_decoder_7}.
   * Same for device 1 also. In our case each pipeline will have 4 1K30, hence
   * pipeline 0 and 1 will run on HW 0 and pipeline 2 and 3 on HW 1.
   */

  decoder_instance_num = (pipeline_ctx->instance_id * MAX_STREAMS_PER_PIPELINE)
      + decoder_thread_data->instance_id;

  vvas_ctx = vvas_app_create_vvas_context (pipeline_ctx, xclbin_location);
  if (!vvas_ctx) {
    VVAS_APP_ERROR_LOG ("Failed to create VVAS Context");
    is_error = true;
    goto error;
  }

#ifndef XLNX_U30_PLATFORM
  /* V70 platform */
  if (pipeline_ctx->instance_id <= 1) {
    hw_instance_id = 0;
  } else {
    hw_instance_id = 1;
  }

  sprintf ((char *) decoder_kernel_name, DECODER_IP_NAME, decoder_instance_num);
  VVAS_APP_INFO_LOG ("Decoder kernel_name: %s, HW instance id: %hhu",
      decoder_kernel_name, hw_instance_id);

  /* Create decoder context */
  decoder_ctx.vvas_decoder = vvas_decoder_create (vvas_ctx,
      decoder_kernel_name, pipeline_ctx->codec_type[instance_num],
      hw_instance_id, LOG_LEVEL_WARNING);
#else
  /* U30 platform */
  /* Create decoder context */

  VVAS_APP_INFO_LOG ("Creating decoder: %s, instance_num: %hhu",
      DECODER_IP_NAME, decoder_instance_num);

  decoder_ctx.vvas_decoder = vvas_decoder_create (vvas_ctx,
      DECODER_IP_NAME, pipeline_ctx->codec_type[instance_num],
      decoder_instance_num, LOG_LEVEL_WARNING);

#endif

  if (!decoder_ctx.vvas_decoder) {
    VVAS_APP_ERROR_LOG ("Failed to create decoder context");
    is_error = true;
    goto error;
  }

  while (true) {
    VvasMemory *au_frame = NULL;
    VvasVideoFrame *decoed_vframe = NULL;
    bool send_again = false;

    if (!is_parser_eos) {

      VVAS_APP_DEBUG_LOG ("Getting buffer from Parser");

      parsed_buffer =
          (ParserBuffer *)
          vvas_queue_dequeue (pipeline_ctx->parser_out_queue[instance_num]);

      VVAS_APP_DEBUG_LOG ("Got Buffer: %p", parsed_buffer);

      if (VVAS_PIPELINE_EOS == parsed_buffer->eos_type) {
        VVAS_APP_DEBUG_LOG ("Got EOS from upstream");
        is_parser_eos = true;
        if (is_stream_interrupted (pipeline_ctx, instance_num)) {
          /* Stream has been interrupted either due to Ctrl + C or due to
           * some error, we'll not drain in this case.
           */
          VVAS_APP_DEBUG_LOG ("Stream interrupted, closing");
          break;
        }
      }

      if (parsed_buffer->dec_cfg && configure_decoder) {
        vvas_app_configure_decoder (vvas_ctx, &decoder_ctx,
            parsed_buffer->dec_cfg, &dec_outbuf_list);
        if (VVAS_IS_ERROR (vret)) {
          VVAS_APP_ERROR_LOG ("failed to configure decoder");
          is_error = true;
          break;
        }

        decoder_resubmit_time =
            calculate_decoder_resubmit_time (parsed_buffer->dec_cfg);

        configure_decoder = false;
      }

      if (parsed_buffer->dec_cfg) {
        free (parsed_buffer->dec_cfg);
        parsed_buffer->dec_cfg = NULL;
      }
    }

  try_again:
    pthread_mutex_lock (&decoder_ctx.decoder_mutex);
    if (decoder_ctx.are_free_buffers_available) {
      decoder_ctx.are_free_buffers_available = false;
      pthread_mutex_unlock (&decoder_ctx.decoder_mutex);
      /* Free buffers available in the pool */
      while (get_decoder_output_buffers (&decoder_ctx, &dec_outbuf_list));
    } else {
      pthread_mutex_unlock (&decoder_ctx.decoder_mutex);
    }

    uint32_t list_len = vvas_list_length (dec_outbuf_list);
    if (list_len) {
      VVAS_APP_DEBUG_LOG ("Feeding %u free buffers to the decoder", list_len);
      for (uint32_t idx = 0; idx < list_len; idx++) {
        VvasVideoFrame *vframe = vvas_list_nth_data (dec_outbuf_list, idx);
        if (!vvas_list_find (dec_pending_buffers, vframe)) {
          dec_pending_buffers = vvas_list_append (dec_pending_buffers, vframe);
        }
      }
    }

    au_frame = parsed_buffer ? parsed_buffer->parsed_frame : NULL;
    VVAS_APP_DEBUG_LOG ("auf_frame: %p", au_frame);

    /* submit input frame and free output frames for decoding */
    vret = vvas_decoder_submit_frames (decoder_ctx.vvas_decoder, au_frame,
        dec_outbuf_list);
    if (VVAS_IS_ERROR (vret)) {
      VVAS_APP_ERROR_LOG ("submit_frames() failed ret = %d", vret);
      is_error = true;
      break;
    } else if (vret == VVAS_RET_SEND_AGAIN) {
      VVAS_APP_DEBUG_LOG ("input buffer is not consumed. send again");
      send_again = true;
    } else if ((vret == VVAS_RET_SUCCESS) && parsed_buffer) {
      VVAS_APP_DEBUG_LOG ("input buffer consumed, freeing parser buf: %p",
          parsed_buffer);
      vvas_app_free_parser_buffer (parsed_buffer);
      parsed_buffer = NULL;
    }

    VVAS_APP_DEBUG_LOG ("submit_frames ret: %d", vret);

    /* list will be freed but not the output video frames */
    vvas_list_free (dec_outbuf_list);
    dec_outbuf_list = NULL;

    /* get output frame from decoder */
    vret = vvas_decoder_get_decoded_frame (decoder_ctx.vvas_decoder,
        &decoed_vframe);

    VVAS_APP_DEBUG_LOG ("get_decoded_frame ret: %d", vret);

    if (vret == VVAS_RET_SUCCESS) {

      VvasVideoBuffer *pool_buf = (VvasVideoBuffer *)
          vvas_hash_table_lookup (decoder_ctx.out_buf_hash_table,
          decoed_vframe);
      if (!pool_buf) {
        VVAS_APP_ERROR_LOG ("Couldn't get pool buf for "
            "decoded buffer: %p", decoed_vframe);
        is_error = true;
        break;
      }

      dec_pending_buffers =
          vvas_list_remove (dec_pending_buffers, decoed_vframe);

      out_pipeline_buf =
          (VvasPipelineBuffer *) calloc (1, sizeof (VvasPipelineBuffer));

      out_pipeline_buf->stream_id = instance_num;
      out_pipeline_buf->main_buffer = pool_buf;
      out_pipeline_buf->eos_type = VVAS_EOS_NONE;

      VVAS_APP_DEBUG_LOG ("Pushing buffer downstream: %p", out_pipeline_buf);

      vvas_queue_enqueue (pipeline_ctx->decoder_out_queue[instance_num],
          out_pipeline_buf);
      out_pipeline_buf = NULL;

    } else if (vret == VVAS_RET_EOS) {
      VVAS_APP_DEBUG_LOG ("Got EOS from Decoder");
      break;
    }

    if (send_again) {
      bool can_try_again = true;
      bool free_frames_avaialble;
      /* Decoder didn't consume the encoded frame, may be because there are no
         room for output buffer. Encoded frames required to be sent again once
         frames are free'ed up */
      /* Wait for sometime */

      send_again = false;

      pthread_mutex_lock (&decoder_ctx.decoder_mutex);
      free_frames_avaialble = decoder_ctx.are_free_buffers_available;
      pthread_mutex_unlock (&decoder_ctx.decoder_mutex);

      if (free_frames_avaialble) {
        /* Some frames become free, let's go and give them to decoder */
        goto try_again;
      } else {
        struct timespec ts;
        /* Let's wait for sometime */
        VVAS_APP_DEBUG_LOG ("Send Again, waiting for %lu or till signaled",
            decoder_resubmit_time);
        clock_gettime (CLOCK_REALTIME, &ts);
        ts.tv_nsec += decoder_resubmit_time * 1000;

        pthread_mutex_lock (&decoder_ctx.decoder_mutex);
        pthread_cond_timedwait (&decoder_ctx.free_buffer_cond,
            &decoder_ctx.decoder_mutex, &ts);
        pthread_mutex_unlock (&decoder_ctx.decoder_mutex);
      }

      /* It is possible that while we were busy in try again, there may be some
       * stream error or user might have done Ctrl + C, hence we need to break
       * this try again loop
       */
      can_try_again = !is_stream_interrupted (pipeline_ctx, instance_num);

      if (can_try_again) {
        goto try_again;
      } else {
        /* We can't send this buffer again, free it */
        vvas_app_free_parser_buffer (parsed_buffer);
        parsed_buffer = NULL;
      }
    }
  }                             /* End of while loop */

error:
  if (is_error) {
    /* We got Error in this thread, notify main streaming thread */
    vvas_app_make_stream_exit (pipeline_ctx, instance_num);

    if (parsed_buffer) {
      vvas_app_free_parser_buffer (parsed_buffer);
    }

    while (true) {
      /* Wait for EOS from upstream */
      parsed_buffer = (ParserBuffer *)
          vvas_queue_dequeue (pipeline_ctx->parser_out_queue[instance_num]);

      if (VVAS_PIPELINE_EOS == parsed_buffer->eos_type) {
        /* Got EOS break the loop and send EOS downstream */
        break;
      } else {
        /* As we ran into error, we can't process this buffer and send it to
         * downstream, hence freeing it */
        vvas_app_free_parser_buffer (parsed_buffer);
      }
    }
  }

  if (!out_pipeline_buf) {
    out_pipeline_buf =
        (VvasPipelineBuffer *) calloc (1, sizeof (VvasPipelineBuffer));
  }

  out_pipeline_buf->stream_id = instance_num;
  out_pipeline_buf->eos_type = VVAS_PIPELINE_EOS;

  VVAS_APP_DEBUG_LOG ("Pushing EOS downstream");
  vvas_queue_enqueue (pipeline_ctx->decoder_out_queue[instance_num],
      out_pipeline_buf);

  if (dec_pending_buffers) {
    VVAS_APP_DEBUG_LOG ("Releasing Decoder pending buffers...");

    vvas_list_foreach (dec_pending_buffers, release_frames,
        decoder_ctx.out_buf_hash_table);

    vvas_list_free (dec_pending_buffers);
  }

  if (decoder_ctx.vvas_decoder) {
    vvas_decoder_destroy (decoder_ctx.vvas_decoder);
    VVAS_APP_DEBUG_LOG ("Decoder Destroyed");
  }

  if (decoder_ctx.out_buf_hash_table) {
    vvas_hash_table_unref (decoder_ctx.out_buf_hash_table);
  }

  if (decoder_ctx.buffer_pool) {
    VVAS_APP_DEBUG_LOG ("Freeing decoder buffer pool");
    vvas_buffer_pool_free (decoder_ctx.buffer_pool);
  }

  pthread_mutex_destroy (&decoder_ctx.decoder_mutex);
  pthread_cond_destroy (&decoder_ctx.free_buffer_cond);

  if (vvas_ctx) {
    vvas_context_destroy (vvas_ctx);
  }

  /* Parser uses pipeline_ctx->parser_vvas_ctx[instance_num] to create
   * AU frames(VvasMemory) which it sends to decoder thread, At the end of
   * stream parser thread will inform Decoder thread about EOS and exit.
   * It is possible that there are still AU frames in the parser output queue,
   * And if parser destroys this vvas_context then while using the AU frame
   * we may encounter error as vvas_context for that memory is destroyed.
   * Hence destroying pipeline_ctx->parser_vvas_ctx[instance_num] here.
   */
  if (pipeline_ctx->parser_vvas_ctx[instance_num]) {
    vvas_context_destroy (pipeline_ctx->parser_vvas_ctx[instance_num]);
    pipeline_ctx->parser_vvas_ctx[instance_num] = NULL;
  }

  free (decoder_thread_data);

  VVAS_APP_DEBUG_LOG ("Exiting Decoder thread");
  pthread_exit (NULL);
}

/**
 *  vvas_parser_thread() - Parser Thread Routine
 *
 *  @args: Parser thread arguments
 *
 *  Return: NULL
 *
 */
static void *
vvas_parser_thread (void *args)
{
  ThreadData *parser_thread_data = (ThreadData *) args;
  PipelineContext *pipeline_ctx = parser_thread_data->pipeline_ctx;
  uint8_t instance_num = parser_thread_data->instance_id;

  ParserContext parser_ctx = { 0 };
  FILE *infp = NULL;
  VvasReturnType vret = VVAS_RET_SUCCESS;
  VvasMemory *es_buf = NULL;
  VvasMemoryMapInfo es_buf_info;
  VvasMemory *au_frame = NULL;
  VvasDecoderInCfg *dec_in_cfg = NULL;
  ParserBuffer *parsed_buf = NULL;
  uint32_t repeat_count = 0, buf_count = 0;

  VVAS_APP_DEBUG_LOG ("Parser Thread_%hhu started", instance_num);

  VVAS_APP_INFO_LOG ("Input file: %s, codec: %d",
      pipeline_ctx->input_file[instance_num],
      pipeline_ctx->codec_type[instance_num]);

  /* Create SW VVAS Context, Decoder thread[instance_num] will free this
   * context */
  pipeline_ctx->parser_vvas_ctx[instance_num] =
      vvas_app_create_vvas_context (pipeline_ctx, NULL);
  if (!pipeline_ctx->parser_vvas_ctx[instance_num]) {
    VVAS_APP_ERROR_LOG ("Failed to create VVAS Context");
    goto exit;
  }

  /* Create parser context */
  parser_ctx.vvas_parser =
      vvas_parser_create (pipeline_ctx->parser_vvas_ctx[instance_num],
      pipeline_ctx->codec_type[instance_num], LOG_LEVEL_WARNING);
  if (!parser_ctx.vvas_parser) {
    VVAS_APP_ERROR_LOG ("Failed to create parser context");
    goto exit;
  }

  VVAS_APP_DEBUG_LOG ("Parser created: %p", parser_ctx.vvas_parser);

  /* open input elementary stream for reading */
  infp = fopen (pipeline_ctx->input_file[instance_num], "r");
  if (!infp) {
    VVAS_APP_ERROR_LOG ("failed to open file %s",
        pipeline_ctx->input_file[instance_num]);
    goto exit;
  }

  /* Allocate memory for elementary stream buffer */
  es_buf = vvas_memory_alloc (pipeline_ctx->parser_vvas_ctx[instance_num],
      VVAS_ALLOC_TYPE_NON_CMA,
      VVAS_ALLOC_FLAG_NONE, 0, DEFAULT_READ_SIZE, &vret);
  if (vret != VVAS_RET_SUCCESS) {
    VVAS_APP_ERROR_LOG ("Failed to alloc vvas_memory for elementary"
        " stream buffer");
    goto exit;
  }

  /* map es_buf to get user space address */
  vret = vvas_memory_map (es_buf, VVAS_DATA_MAP_WRITE, &es_buf_info);
  if (vret != VVAS_RET_SUCCESS) {
    VVAS_APP_ERROR_LOG ("ERROR: Failed to map the es_buf "
        "for write vret = %d", vret);
    goto exit;
  }

  parser_ctx.parser_offset = 0;
  parser_ctx.read_again = 1;

  while (true) {
    bool is_eos;
    is_eos = is_stream_interrupted (pipeline_ctx, instance_num);

    if (is_eos) {
      VVAS_APP_DEBUG_LOG ("Forcing EOS");
      break;
    }

    vret = vvas_app_get_es_frame (&parser_ctx, infp, es_buf,
        &es_buf_info, &au_frame, &dec_in_cfg, &is_eos);
    if (VVAS_IS_ERROR (vret)) {
      VVAS_APP_ERROR_LOG ("failed to get elementary stream buffer");
      break;
    }

    parsed_buf = (ParserBuffer *) calloc (1, sizeof (ParserBuffer));
    if (!parsed_buf) {
      break;
    }

    parsed_buf->dec_cfg = dec_in_cfg;
    parsed_buf->parsed_frame = au_frame;
    parsed_buf->eos_type = VVAS_EOS_NONE;

    VVAS_APP_DEBUG_LOG ("Pushing buffer downstream: %p", parsed_buf);
    vvas_queue_enqueue (pipeline_ctx->parser_out_queue[instance_num],
        parsed_buf);
    parsed_buf = NULL;

    if (VVAS_RET_EOS == vret) {

      repeat_count++;

      if (repeat_count >= stream_repeat_count) {
        VVAS_APP_INFO_LOG ("Parser EOS");
        break;

      } else {
        VVAS_APP_LOG ("Parser[%hhu.%hhu] Repeat_count: %u/%u",
            pipeline_ctx->instance_id, instance_num,
            repeat_count, stream_repeat_count);

        parser_ctx.parser_offset = 0;
        parser_ctx.read_again = 1;

        if (fseek (infp, 0, SEEK_SET)) {
          /* fseek failed, break main loop */
          break;
        }

        vvas_parser_destroy (parser_ctx.vvas_parser);

        parser_ctx.vvas_parser =
            vvas_parser_create (pipeline_ctx->parser_vvas_ctx[instance_num],
            pipeline_ctx->codec_type[instance_num], LOG_LEVEL_WARNING);
        if (!parser_ctx.vvas_parser) {
          VVAS_APP_ERROR_LOG ("Failed to create parser context");
          break;
        }
      }
    }
    buf_count++;
  }

exit:
  if (!parsed_buf) {
    parsed_buf = (ParserBuffer *) calloc (1, sizeof (ParserBuffer));
  }
  parsed_buf->dec_cfg = NULL;
  parsed_buf->parsed_frame = NULL;
  parsed_buf->eos_type = VVAS_PIPELINE_EOS;

  VVAS_APP_DEBUG_LOG ("Pushing EOS downstream");
  vvas_queue_enqueue (pipeline_ctx->parser_out_queue[instance_num], parsed_buf);

  if (infp) {
    fclose (infp);
  }

  /* Free input file */
  free (pipeline_ctx->input_file[instance_num]);

  if (es_buf) {
    vvas_memory_free (es_buf);
  }

  if (parser_ctx.vvas_parser) {
    VVAS_APP_DEBUG_LOG ("Destroying Parser %p", parser_ctx.vvas_parser);
    vvas_parser_destroy (parser_ctx.vvas_parser);
  }

  free (parser_thread_data);

  VVAS_APP_DEBUG_LOG ("Exiting Parser Thread");
  pthread_exit (NULL);
}

/**
 *  vvas_app_destroy_pipeline_queues() - Destroy all the pipeline queues
 *
 *  @pipeline_ctx: Pipeline context
 *
 *  Return: None
 *
 */
static void
vvas_app_destroy_pipeline_queues (PipelineContext * pipeline_ctx)
{
  for (uint8_t idx = 0; idx < pipeline_ctx->num_streams; idx++) {
    if (pipeline_ctx->parser_out_queue[idx]) {
      VVAS_APP_DEBUG_LOG ("Freeing Parser Queue[%u], %d",
          idx, vvas_queue_get_length (pipeline_ctx->parser_out_queue[idx]));
      vvas_queue_free (pipeline_ctx->parser_out_queue[idx]);
      pipeline_ctx->parser_out_queue[idx] = NULL;
    }

    if (pipeline_ctx->decoder_out_queue[idx]) {
      VVAS_APP_DEBUG_LOG ("Freeing Decoder Queue[%u], %d",
          idx, vvas_queue_get_length (pipeline_ctx->decoder_out_queue[idx]));
      vvas_queue_free (pipeline_ctx->decoder_out_queue[idx]);
      pipeline_ctx->decoder_out_queue[idx] = NULL;
    }

    if (pipeline_ctx->scaler_out_queue[idx]) {
      VVAS_APP_DEBUG_LOG ("Freeing Scaler Queue[%u], %d",
          idx, vvas_queue_get_length (pipeline_ctx->scaler_out_queue[idx]));
      vvas_queue_free (pipeline_ctx->scaler_out_queue[idx]);
      pipeline_ctx->scaler_out_queue[idx] = NULL;
    }

    if (pipeline_ctx->defunnel_out_queue[idx]) {
      VVAS_APP_DEBUG_LOG ("Freeing De-funnel Queue[%u], %d",
          idx, vvas_queue_get_length (pipeline_ctx->defunnel_out_queue[idx]));
      vvas_queue_free (pipeline_ctx->defunnel_out_queue[idx]);
      pipeline_ctx->defunnel_out_queue[idx] = NULL;
    }

    if (pipeline_ctx->overlay_out_queue[idx]) {
      VVAS_APP_DEBUG_LOG ("Freeing Overlay Queue[%u], %d",
          idx, vvas_queue_get_length (pipeline_ctx->overlay_out_queue[idx]));
      vvas_queue_free (pipeline_ctx->overlay_out_queue[idx]);
      pipeline_ctx->overlay_out_queue[idx] = NULL;
    }
  }

  if (pipeline_ctx->funnel_out_queue) {
    VVAS_APP_DEBUG_LOG ("Freeing Funnel Queue, %d",
        vvas_queue_get_length (pipeline_ctx->funnel_out_queue));
    vvas_queue_free (pipeline_ctx->funnel_out_queue);
    pipeline_ctx->funnel_out_queue = NULL;
  }

  if (pipeline_ctx->yolov3_out_queue) {
    VVAS_APP_DEBUG_LOG ("Freeing Yolov3 Queue, %d",
        vvas_queue_get_length (pipeline_ctx->yolov3_out_queue));
    vvas_queue_free (pipeline_ctx->yolov3_out_queue);
    pipeline_ctx->yolov3_out_queue = NULL;
  }

  if (pipeline_ctx->crop_scaler_out_queue) {
    VVAS_APP_DEBUG_LOG ("Freeing CropScaler Queue, %d",
        vvas_queue_get_length (pipeline_ctx->crop_scaler_out_queue));
    vvas_queue_free (pipeline_ctx->crop_scaler_out_queue);
    pipeline_ctx->crop_scaler_out_queue = NULL;
  }

  for (uint8_t idx = 0; idx < CAR_CLASSIFICATION_TYPE_MAX; idx++) {
    if (pipeline_ctx->resnet18_out_queue[idx]) {
      VVAS_APP_DEBUG_LOG ("Freeing Resnet18_%d Queue, %d",
          idx, vvas_queue_get_length (pipeline_ctx->resnet18_out_queue[idx]));
      vvas_queue_free (pipeline_ctx->resnet18_out_queue[idx]);
      pipeline_ctx->resnet18_out_queue[idx] = NULL;
    }
  }
}

/**
 *  vvas_app_join_stream_threads() - Join all the streams threads of pipeline.
 *
 *  @pipeline_ctx: Pipeline context
 *  @stream_id: Stream ID of the pipeline
 *
 *  Return: None
 *
 */
static void
vvas_app_join_stream_threads (PipelineContext * pipeline_ctx, uint8_t stream_id)
{
  pthread_join (pipeline_ctx->sink_thread[stream_id], NULL);
  VVAS_APP_DEBUG_LOG ("Sink thread[%hhu] joined", stream_id);

  pthread_join (pipeline_ctx->overlay_thread[stream_id], NULL);
  VVAS_APP_DEBUG_LOG ("Overlay thread[%hhu] joined", stream_id);

  pthread_join (pipeline_ctx->scaler_thread[stream_id], NULL);
  VVAS_APP_DEBUG_LOG ("Scaler thread[%hhu] joined", stream_id);

  pthread_join (pipeline_ctx->decoder_thread[stream_id], NULL);
  VVAS_APP_DEBUG_LOG ("Decoder thread[%hhu] joined", stream_id);

  pthread_join (pipeline_ctx->parser_thread[stream_id], NULL);
  VVAS_APP_DEBUG_LOG ("Parser thread[%hhu] joined", stream_id);
}

/**
 *  vvas_app_join_pipeline_threads() - Join all the pipeline threads
 *
 *  @pipeline_ctx: Pipeline context
 *
 *  Return: None
 *
 */
static void
vvas_app_join_pipeline_threads (PipelineContext * pipeline_ctx)
{
  /* Join per stream threads */
  for (uint8_t idx = 0; idx < pipeline_ctx->num_streams; idx++) {
    vvas_app_join_stream_threads (pipeline_ctx, idx);
  }

  /* Join common threads */
  for (uint8_t idx = 0; idx < CAR_CLASSIFICATION_TYPE_MAX; idx++) {
    pthread_join (pipeline_ctx->resnet18_threads[idx], NULL);
    VVAS_APP_DEBUG_LOG ("Resnet18 thread[%hhu] joined", idx);
  }

  pthread_join (pipeline_ctx->defunnel_thread, NULL);
  VVAS_APP_DEBUG_LOG ("DeFunnel thread joined");

  pthread_join (pipeline_ctx->yolov3_thread, NULL);
  VVAS_APP_DEBUG_LOG ("YoloV3 thread joined");

  pthread_join (pipeline_ctx->crop_scaler_thread, NULL);
  VVAS_APP_DEBUG_LOG ("CropScaler thread joined");

  pthread_join (pipeline_ctx->funnel_thread, NULL);
  VVAS_APP_DEBUG_LOG ("Funnel thread joined");
}

/**
 *  vvas_app_create_pipeline_queues() - Create all the pipeline queues
 *
 *  @pipeline_ctx: Pipeline context
 *
 *  Return: Return True on success, False on failure.
 *
 */
static bool
vvas_app_create_pipeline_queues (PipelineContext * pipeline_ctx)
{
  bool ret = false;
  /* Create all thread coupling queues */
  do {
    uint8_t idx;

    VVAS_APP_DEBUG_LOG ("Creating Parser Out Queue");
    for (idx = 0; idx < pipeline_ctx->num_streams; idx++) {
      /*
       * Parser thread reads from the file and feeds the decoder after
       * parsing, hence this thread will be very fast compared to the
       * decoder thread as decoder thread is decoding using HW,
       * hence limiting this thread by limiting its queue size.
       */
      pipeline_ctx->parser_out_queue[idx] = vvas_queue_new (5);
      if (!pipeline_ctx->parser_out_queue[idx]) {
        VVAS_APP_DEBUG_LOG ("Couldn't create parser_out_queue[%hhu]", idx);
        break;
      }
    }

    VVAS_APP_DEBUG_LOG ("Creating Decoder Out Queue");
    for (idx = 0; idx < pipeline_ctx->num_streams; idx++) {
      pipeline_ctx->decoder_out_queue[idx] = vvas_queue_new (-1);
      if (!pipeline_ctx->decoder_out_queue[idx]) {
        VVAS_APP_DEBUG_LOG ("Couldn't create decoder_output_queue");
        break;
      }
    }

    VVAS_APP_DEBUG_LOG ("Creating Scaler Out Queue");
    for (idx = 0; idx < pipeline_ctx->num_streams; idx++) {
      pipeline_ctx->scaler_out_queue[idx] = vvas_queue_new (2);
      if (!pipeline_ctx->scaler_out_queue[idx]) {
        VVAS_APP_DEBUG_LOG ("Couldn't create scaler_out_queue");
        break;
      }
    }

    VVAS_APP_DEBUG_LOG ("Creating Funnel Out Queue");
    pipeline_ctx->funnel_out_queue = vvas_queue_new (-1);
    if (!pipeline_ctx->funnel_out_queue) {
      VVAS_APP_DEBUG_LOG ("Couldn't create funnel out queue");
      break;
    }

    VVAS_APP_DEBUG_LOG ("Creating YOLOV3 Out Queue");
    pipeline_ctx->yolov3_out_queue = vvas_queue_new (-1);
    if (!pipeline_ctx->yolov3_out_queue) {
      VVAS_APP_DEBUG_LOG ("Couldn't create yolov3_out_queue");
      break;
    }

    VVAS_APP_DEBUG_LOG ("Creating CropScaler Out Queue");
    pipeline_ctx->crop_scaler_out_queue = vvas_queue_new (-1);
    if (!pipeline_ctx->crop_scaler_out_queue) {
      VVAS_APP_DEBUG_LOG ("Couldn't create crop_scaler_out_queue");
      break;
    }

    VVAS_APP_DEBUG_LOG ("Creating Resnet18 Out Queues");
    for (idx = 0; idx < CAR_CLASSIFICATION_TYPE_MAX; idx++) {
      pipeline_ctx->resnet18_out_queue[idx] = vvas_queue_new (-1);
      if (!pipeline_ctx->resnet18_out_queue[idx]) {
        VVAS_APP_DEBUG_LOG ("Couldn't create resnet18_out_queue[%hhu]", idx);
        break;
      }
    }

    VVAS_APP_DEBUG_LOG ("Creating DeFunnel Out Queues");
    for (idx = 0; idx < pipeline_ctx->num_streams; idx++) {
      pipeline_ctx->defunnel_out_queue[idx] = vvas_queue_new (-1);
      if (!pipeline_ctx->defunnel_out_queue[idx]) {
        VVAS_APP_DEBUG_LOG ("Couldn't create DeFunnel out queue[%hhu]", idx);
        break;
      }
    }

    VVAS_APP_DEBUG_LOG ("Creating Overlay Out Queue");
    for (idx = 0; idx < pipeline_ctx->num_streams; idx++) {
      pipeline_ctx->overlay_out_queue[idx] = vvas_queue_new (-1);
      if (!pipeline_ctx->overlay_out_queue[idx]) {
        VVAS_APP_DEBUG_LOG ("Couldn't create overlay_out_queue[%hhu]", idx);
        break;
      }
    }
    ret = true;
  } while (0);

  return ret;
}

/**
 *  vvas_app_create_pipeline_threads() - Create all the pipeline threads
 *
 *  @pipeline_ctx: Pipeline context
 *
 *  Return: Return True on success, False on failure.
 *
 */
static bool
vvas_app_create_pipeline_threads (PipelineContext * pipeline_ctx)
{
  bool ret = false;

  do {
    uint8_t idx;
    char thread_name[16] = { 0 };

    /* Start Sink threads */
    for (idx = 0; idx < pipeline_ctx->num_streams; idx++) {
      ThreadData *sink_thread_data;
      sink_thread_data = (ThreadData *) calloc (1, sizeof (ThreadData));
      if (!sink_thread_data) {
        break;
      }
      sink_thread_data->pipeline_ctx = pipeline_ctx;
      sink_thread_data->instance_id = idx;
      snprintf (thread_name, 16, "%s_%hhu.%hhu", SINK_THREAD_NAME,
          pipeline_ctx->instance_id, sink_thread_data->instance_id);

      if (!vvas_app_create_thread (thread_name, vvas_sink_thread,
              &pipeline_ctx->sink_thread[idx], sink_thread_data)) {
        break;
      }
      pipeline_ctx->is_sink_thread_alive[idx] = true;
    }

    /* Start Overlay threads */
    for (idx = 0; idx < pipeline_ctx->num_streams; idx++) {
      ThreadData *overlay_thread_data;
      overlay_thread_data = (ThreadData *) calloc (1, sizeof (ThreadData));
      if (!overlay_thread_data) {
        break;
      }
      overlay_thread_data->pipeline_ctx = pipeline_ctx;
      overlay_thread_data->instance_id = idx;
      snprintf (thread_name, 16, "%s_%hhu.%hhu", OVERLAY_THREAD_NAME,
          pipeline_ctx->instance_id, overlay_thread_data->instance_id);

      if (!vvas_app_create_thread (thread_name, vvas_overlay_thread,
              &pipeline_ctx->overlay_thread[idx], overlay_thread_data)) {
        break;
      }
    }

    /* Create de-funnel thread */
    ThreadData *defunnel_thread_data;
    defunnel_thread_data = (ThreadData *) calloc (1, sizeof (ThreadData));
    if (!defunnel_thread_data) {
      break;
    }
    defunnel_thread_data->pipeline_ctx = pipeline_ctx;
    defunnel_thread_data->instance_id = pipeline_ctx->instance_id;
    snprintf (thread_name, 16, "%s_%hhu", DEFUNNEL_THREAD_NAME,
        pipeline_ctx->instance_id);

    if (!vvas_app_create_thread (thread_name, vvas_defunnel_thread,
            &pipeline_ctx->defunnel_thread, defunnel_thread_data)) {
      break;
    }

    /* Start Resnet18 classification threads */
    for (int idx = 0; idx < CAR_CLASSIFICATION_TYPE_MAX; idx++) {
      ThreadData *resnet18_thread_data;
      resnet18_thread_data = (ThreadData *) calloc (1, sizeof (ThreadData));
      if (!resnet18_thread_data) {
        break;
      }
      resnet18_thread_data->pipeline_ctx = pipeline_ctx;
      /* instance_id will be used to identify classification type */
      resnet18_thread_data->instance_id = idx;
      snprintf (thread_name, 16, "%s_%hhu.%hhu", RESNET18_THREAD_NAME,
          pipeline_ctx->instance_id, idx);

      if (!vvas_app_create_thread (thread_name, vvas_resnet18_thread,
              &pipeline_ctx->resnet18_threads[idx], resnet18_thread_data)) {
        break;
      }
    }

    /* Create CropScaler thread */
    ThreadData *crop_scaler_thread_data;
    crop_scaler_thread_data = (ThreadData *) calloc (1, sizeof (ThreadData));
    if (!crop_scaler_thread_data) {
      break;
    }
    crop_scaler_thread_data->pipeline_ctx = pipeline_ctx;
    crop_scaler_thread_data->instance_id = 0;
    snprintf (thread_name, 16, "%s_%hhu", CROP_SCALER_THREAD_NAME,
        pipeline_ctx->instance_id);

    if (!vvas_app_create_thread (thread_name, vvas_crop_scale_thread,
            &pipeline_ctx->crop_scaler_thread, crop_scaler_thread_data)) {
      break;
    }

    /* Create Yolov3 detection thread */
    ThreadData *yolov3_thread_data;
    yolov3_thread_data = (ThreadData *) calloc (1, sizeof (ThreadData));
    if (!yolov3_thread_data) {
      break;
    }
    yolov3_thread_data->pipeline_ctx = pipeline_ctx;
    yolov3_thread_data->instance_id = 0;
    snprintf (thread_name, 16, "%s_%hhu", YOLOV3_THREAD_NAME,
        pipeline_ctx->instance_id);

    if (!vvas_app_create_thread (thread_name, vvas_yolov3_thread,
            &pipeline_ctx->yolov3_thread, yolov3_thread_data)) {
      break;
    }

    /* Start Funnel thread */
    ThreadData *funnel_thread_data;
    funnel_thread_data = (ThreadData *) calloc (1, sizeof (ThreadData));
    if (!funnel_thread_data) {
      break;
    }
    funnel_thread_data->pipeline_ctx = pipeline_ctx;
    funnel_thread_data->instance_id = 0;
    snprintf (thread_name, 16, "%s_%hhu", FUNNEL_THREAD_NAME,
        pipeline_ctx->instance_id);

    if (!vvas_app_create_thread (thread_name, vvas_funnel_thread,
            &pipeline_ctx->funnel_thread, funnel_thread_data)) {
      break;
    }

    /* Start Scaler Thread */
    for (idx = 0; idx < pipeline_ctx->num_streams; idx++) {
      ThreadData *scaler_thread_data;
      scaler_thread_data = (ThreadData *) calloc (1, sizeof (ThreadData));
      if (!scaler_thread_data) {
        break;
      }
      scaler_thread_data->pipeline_ctx = pipeline_ctx;
      scaler_thread_data->instance_id = idx;
      snprintf (thread_name, 16, "%s_%hhu.%hhu", SCALER_THREAD_NAME,
          pipeline_ctx->instance_id, scaler_thread_data->instance_id);

      if (!vvas_app_create_thread (thread_name, vvas_scaler_thread,
              &pipeline_ctx->scaler_thread[idx], scaler_thread_data)) {
        break;
      }
    }

    /* Start Decoder thread */
    for (idx = 0; idx < pipeline_ctx->num_streams; idx++) {
      ThreadData *decoder_thread_data;
      decoder_thread_data = (ThreadData *) calloc (1, sizeof (ThreadData));
      if (!decoder_thread_data) {
        break;
      }
      decoder_thread_data->pipeline_ctx = pipeline_ctx;
      decoder_thread_data->instance_id = idx;
      snprintf (thread_name, 16, "%s_%hhu.%hhu", DECODER_THREAD_NAME,
          pipeline_ctx->instance_id, decoder_thread_data->instance_id);

      if (!vvas_app_create_thread (thread_name, vvas_decoder_thread,
              &pipeline_ctx->decoder_thread[idx], decoder_thread_data)) {
        break;
      }
    }

    /* Start Parser thread */
    for (idx = 0; idx < pipeline_ctx->num_streams; idx++) {
      ThreadData *parser_thread_data;
      parser_thread_data = (ThreadData *) calloc (1, sizeof (ThreadData));
      if (!parser_thread_data) {
        break;
      }
      parser_thread_data->pipeline_ctx = pipeline_ctx;
      parser_thread_data->instance_id = idx;
      snprintf (thread_name, 16, "%s_%hhu.%hhu", PARSER_THREAD_NAME,
          pipeline_ctx->instance_id, parser_thread_data->instance_id);

      if (!vvas_app_create_thread (thread_name, vvas_parser_thread,
              &pipeline_ctx->parser_thread[idx], parser_thread_data)) {
        break;
      }
    }

    /* All threads are started */
    ret = true;
  } while (0);

  return ret;
}

/**
 *  all_sink_dead() - Check is all sinks are thread
 *
 *  @pipeline_ctx: Pipeline context
 *
 *  Return: Return True if all sinks are dead, False if any one sink is alive.
 *
 */
static bool
all_sink_dead (PipelineContext * pipeline_ctx)
{
  bool all_dead = true;
  pthread_mutex_lock (&pipeline_ctx->pipeline_lock);
  for (uint8_t idx = 0; idx < pipeline_ctx->num_streams; idx++) {
    if (pipeline_ctx->is_sink_thread_alive[idx]) {
      all_dead = false;
    }
  }
  pthread_mutex_unlock (&pipeline_ctx->pipeline_lock);
  return all_dead;
}

/**
 *  vvas_pipeline_launcher_thread() - Pipeline launcher thread routine
 *  This thread creates following pipeline
 *
 *  parser -> decoder -> scaler ---                                                                                             -> overlay -> sink
 *                                  \                                                                                         /
 *  parser -> decoder -> scaler ---  \                                                                                       / --> overlay -> sink
 *                                    -> funnel -> yolov3 -> crop_scaler -> resnet_0 -> resnet_1 -> resnet_2 -> defunnel ----
 *  parser -> decoder -> scaler ---  /                                                                                       \ --> overlay -> sink
 *                                  /                                                                                         \
 *  parser -> decoder -> scaler ---                                                                                             -> overlay -> sink
 *
 *  @pipeline_ctx: Thread argument
 *
 *  Return: NULL
 *
 */
static void *
vvas_pipeline_launcher_thread (void *args)
{
  PipelineContext *pipeline_ctx = (PipelineContext *) args;
  VvasClockTime last_ts[MAX_STREAMS_PER_PIPELINE] = { 0 },
      sink_start_ts[MAX_STREAMS_PER_PIPELINE] = { 0 };

  VvasClockTime fps_display_intrvl = fps_display_interval * SECOND_IN_MS;

  uint64_t last_frame_rendered[MAX_STREAMS_PER_PIPELINE] = { 0 };
  bool fps_calculatation_stated[MAX_STREAMS_PER_PIPELINE] = { false };
  bool got_interrupt = false;

  if (0 == pipeline_ctx->num_streams) {
    VVAS_APP_ERROR_LOG ("No Stream, exiting");
    pthread_exit (NULL);
  }

  if (pipeline_ctx->num_streams > MAX_STREAMS_PER_PIPELINE) {
    VVAS_APP_ERROR_LOG ("No. of streams > %d, only playing %d streams",
        MAX_STREAMS_PER_PIPELINE, MAX_STREAMS_PER_PIPELINE);
    pipeline_ctx->num_streams = MAX_STREAMS_PER_PIPELINE;
  }

  for (uint8_t i = 0; i < pipeline_ctx->num_streams; i++) {
    /* Here not validating the coded type as it is already validated in
     * vvas_app_parse_master_json() function.
     */
    pipeline_ctx->codec_type[i] =
        get_video_codec_type (pipeline_ctx->input_file[i]);

    /* Initialize mutex lock for protecting number of frames rendered variable */
    pthread_mutex_init (&pipeline_ctx->frame_rate_lock[i], NULL);
  }

  /* Initialize pipeline mutext lock for protecting concurrent access to
   * pipeline context variable */
  pthread_mutex_init (&pipeline_ctx->pipeline_lock, NULL);

  pthread_mutex_lock (&global_mutex);
  /* Got interrupt before doing anything, exiting */
  got_interrupt = is_interrupt;
  pthread_mutex_unlock (&global_mutex);

  if (got_interrupt) {
    VVAS_APP_INFO_LOG ("Got interrupt before starting the pipeline");
    goto error;
  }

  /* Create all thread coupling queues */
  if (!vvas_app_create_pipeline_queues (pipeline_ctx)) {
    goto error;
  }
  VVAS_APP_DEBUG_LOG ("All Queues are created");

  /* Start all pipeline threads */
  if (!vvas_app_create_pipeline_threads (pipeline_ctx)) {
    goto error;
  }
  VVAS_APP_DEBUG_LOG ("All threads are created");

  /* Loop untill all the sinks are playing, calculate and display their FPS */
  while (true) {
    bool break_loop = false;

    for (uint8_t idx = 0; idx < pipeline_ctx->num_streams; idx++) {
      bool is_sink_alive;
      uint64_t frames_renedered;

      pthread_mutex_lock (&pipeline_ctx->pipeline_lock);
      is_sink_alive = pipeline_ctx->is_sink_thread_alive[idx];
      pthread_mutex_unlock (&pipeline_ctx->pipeline_lock);

      /* If all sinks are dead, break the loop */
      if (all_sink_dead (pipeline_ctx)) {
        VVAS_APP_DEBUG_LOG ("All sinks are dead now");
        break_loop = true;
        break;                  /* Breaking for loop */
      }

      /* This sink is dead now, no need to calculate its FPS */
      if (!is_sink_alive) {
        continue;
      }

      pthread_mutex_lock (&pipeline_ctx->frame_rate_lock[idx]);
      frames_renedered = pipeline_ctx->sink_frame_render_count[idx];
      pthread_mutex_unlock (&pipeline_ctx->frame_rate_lock[idx]);

      /* Calculate FPS */
      if (frames_renedered) {
        if (!fps_calculatation_stated[idx]) {
          /* Starting FPS calculation, note down the time when sink thread
           * started rendering frames. */
          sink_start_ts[idx] = pipeline_ctx->start_ts[idx];
          last_ts[idx] = sink_start_ts[idx];
          fps_calculatation_stated[idx] = true;
        } else {
          VvasClockTime now;
          double diff_time, elapsed_time;
          double current_fps, average_fps;

          now = vvas_get_clocktime ();
          if ((now - last_ts[idx]) > fps_display_intrvl) {
            /* Calculate and display FPS */
            diff_time = (double) (now - last_ts[idx]) / SECOND_IN_NS;
            elapsed_time = (double) (now - sink_start_ts[idx]) / SECOND_IN_NS;

            current_fps = ((double) frames_renedered - last_frame_rendered[idx])
                / diff_time;
            average_fps = (double) frames_renedered / elapsed_time;

            VVAS_APP_LOG ("[Sink_%hhu.%hhu] rendered: %lu, current FPS: %.2f,"
                " average FPS: %.2f",
                pipeline_ctx->instance_id, idx, frames_renedered,
                current_fps, average_fps);

            last_frame_rendered[idx] = frames_renedered;
            last_ts[idx] = now;
          }
        }
      }
    }                           /* End of for loop */
    if (break_loop) {
      break;                    /* Breaking main while loop */
    }
    usleep (fps_display_intrvl * 1000);
  }                             /* End of while loop */

error:

  /* All threads must have exited, let's join them */
  vvas_app_join_pipeline_threads (pipeline_ctx);
  VVAS_APP_DEBUG_LOG ("All threads joined");

  /* Free all thread coupling queues */
  vvas_app_destroy_pipeline_queues (pipeline_ctx);
  VVAS_APP_DEBUG_LOG ("All queues destroyed, exiting");

  /* Destroy mutexes */
  for (uint8_t idx = 0; idx < pipeline_ctx->num_streams; idx++) {
    pthread_mutex_destroy (&pipeline_ctx->frame_rate_lock[idx]);
  }
  pthread_mutex_destroy (&pipeline_ctx->pipeline_lock);

  VVAS_APP_DEBUG_LOG ("Exiting launcher thread");

  free (pipeline_ctx);
  pthread_exit (NULL);
}

/**
 *  vvas_app_free_global_contexts() - Free global context
 *
 *  Return: None
 *
 */
static void
vvas_app_free_global_contexts ()
{
  if (xclbin_location) {
    free (xclbin_location);
  }

  if (yolov3_json_path) {
    free (yolov3_json_path);
  }

  if (car_make_json_path) {
    free (car_make_json_path);
  }

  if (car_type_json_path) {
    free (car_type_json_path);
  }

  if (car_color_json_path) {
    free (car_color_json_path);
  }

  if (metaconvert_json_path) {
    free (metaconvert_json_path);
  }

  if (metaconvert_cfg.allowed_labels) {
    if (metaconvert_cfg.allowed_labels_count) {
      for (uint32_t idx = 0; idx < metaconvert_cfg.allowed_labels_count; idx++) {
        free (metaconvert_cfg.allowed_labels[idx]);
        metaconvert_cfg.allowed_labels[idx] = NULL;
      }
    }
    free (metaconvert_cfg.allowed_labels);
    metaconvert_cfg.allowed_labels = NULL;
  }

  if (metaconvert_cfg.allowed_classes) {
    if (metaconvert_cfg.allowed_classes_count) {
      for (uint32_t idx = 0; idx < metaconvert_cfg.allowed_classes_count; idx++) {
        free (metaconvert_cfg.allowed_classes[idx]);
        metaconvert_cfg.allowed_classes[idx] = NULL;
      }
    }
    free (metaconvert_cfg.allowed_classes);
    metaconvert_cfg.allowed_classes = NULL;
  }

  free_dpuinfer_conf (&yolov3_config);
  free_dpuinfer_conf (&resnet18_car_make_config);
  free_dpuinfer_conf (&resnet18_car_type_config);
  free_dpuinfer_conf (&resnet18_car_color_config);

  pthread_mutex_destroy (&print_mutex);
  pthread_mutex_destroy (&global_mutex);

  if (input_files) {
    vvas_list_free (input_files);
  }
}

int
main (int argc, char *argv[])
{
  Thread launcher_threads[256] = { 0 };
  uint8_t launcher_thread_counter = 0;
  char *mjson_path = NULL;
  int32_t ret = -1;
  uint32_t total_streams, num_full_pipeline, num_partial_pipeline_streams;
  uint32_t num_pipelines;
  int opt;

  /* Note start time of the application, others logs are with reference to
   * this time. */
  app_start_time = vvas_get_clocktime ();

  /* Add SIGINT handler to handle Ctrl + C */
  struct sigaction sighact = { 0 };
  sighact.sa_handler = vvas_app_siginterrupt_handler;
  sigaction (SIGINT, &sighact, NULL);

  while ((opt = getopt (argc, argv, "j:h")) != -1) {
    switch (opt) {
      case 'j':
        mjson_path = strdup (optarg);
        break;
      case 'h':
        print_help_text (argv[0]);
        return 0;
    }
  }

  if (!mjson_path) {
    print_help_text (argv[0]);
    return 0;
  }

  /* Parse Master Josn files given by the user */
  if (vvas_app_parse_master_json (mjson_path)) {
    VVAS_APP_ERROR_LOG ("failed to parse master json file %s", mjson_path);
    goto error;
  }

  /* Parse MetaConvert json file */
  if (!parse_metaconvert_json (metaconvert_json_path, &metaconvert_cfg)) {
    VVAS_APP_ERROR_LOG ("failed to parse metaconvert json file");
    goto error;
  }

  /*
   * Parse DPU json (Yolov3, Car Make, Type and Color) and get their
   * model requirements.
   */
  if (!vvas_app_prepare_dpu_configuration ()) {
    VVAS_APP_ERROR_LOG ("Couldn't prepare DPU configuration");
    goto error;
  }

  /*
   * Each pipeline can has MAX_STREAMS_PER_PIPELINE number of streams per
   * pipeline, based on number of input streams given by user calculate how
   * many pipeline will be there.
   */
  total_streams = vvas_list_length (input_files);
  num_full_pipeline = total_streams / MAX_STREAMS_PER_PIPELINE;
  num_partial_pipeline_streams = total_streams % MAX_STREAMS_PER_PIPELINE;
  num_pipelines = num_full_pipeline;
  num_pipelines += (num_partial_pipeline_streams) ? 1 : 0;

  VVAS_APP_DEBUG_LOG ("Number of total streams: %u, "
      "num_pipelines: %u, num_partial_pipeline_streams: %u", total_streams,
      num_pipelines, num_partial_pipeline_streams);

  /* Create pipeline thread for processing MAX_STREAMS_PER_PIPELINE number of
   * streams. */
  for (uint32_t idx = 0; idx < num_full_pipeline; idx++) {
    /* Create pipeline context for launcher thread */
    PipelineContext *pipeline_ctx;
    char thread_name[16];
    uint32_t offset = idx * MAX_STREAMS_PER_PIPELINE;

    pipeline_ctx = (PipelineContext *) calloc (1, sizeof (PipelineContext));
    if (!pipeline_ctx) {
      goto error;
    }

    pipeline_ctx->instance_id = launcher_thread_counter;

    for (int32_t i = 0; i < MAX_STREAMS_PER_PIPELINE; i++) {
      char *in_file = (char *) vvas_list_nth_data (input_files, offset + i);
      pipeline_ctx->input_file[i] = in_file;
      pipeline_ctx->num_streams++;
    }

    snprintf (thread_name, 16, "%s_%d", LAUNCHER_THREAD_NAME,
        pipeline_ctx->instance_id);

    if (!vvas_app_create_thread (thread_name, vvas_pipeline_launcher_thread,
            &launcher_threads[launcher_thread_counter], pipeline_ctx)) {
      goto error;
    }
    launcher_thread_counter++;
  }

  /* Create pipeline thread for processing num_partial_pipeline_streams number of
   * streams. */
  if (num_partial_pipeline_streams) {
    PipelineContext *pipeline_ctx;
    char thread_name[16];
    uint32_t offset = num_full_pipeline * MAX_STREAMS_PER_PIPELINE;

    pipeline_ctx = (PipelineContext *) calloc (1, sizeof (PipelineContext));
    if (!pipeline_ctx) {
      goto error;
    }

    pipeline_ctx->instance_id = launcher_thread_counter;

    for (uint32_t i = 0; i < num_partial_pipeline_streams; i++) {
      char *in_file = (char *) vvas_list_nth_data (input_files, offset + i);
      pipeline_ctx->input_file[i] = in_file;
      pipeline_ctx->num_streams++;
    }

    snprintf (thread_name, 16, "%s_%d", LAUNCHER_THREAD_NAME,
        pipeline_ctx->instance_id);

    if (!vvas_app_create_thread (thread_name, vvas_pipeline_launcher_thread,
            &launcher_threads[launcher_thread_counter], pipeline_ctx)) {
      goto error;
    }
    launcher_thread_counter++;
  }

  /* Wait for luncher threads to exit */
  for (uint32_t idx = 0; idx < launcher_thread_counter; idx++) {
    pthread_join (launcher_threads[idx], NULL);
  }

  VVAS_APP_LOG ("Done");

  ret = 0;

error:
  if (mjson_path) {
    free (mjson_path);
  }

  /* Free all global contexts */
  vvas_app_free_global_contexts ();

  return ret;
}
