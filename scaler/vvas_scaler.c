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

#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#include "vvas_core/vvas_log.h"
#include "vvas_core/vvas_scaler.h"
#include "vvas_core/vvas_scaler_interface.h"

#ifdef XLNX_PCIe_PLATFORM
/** @def SCALER_LIB_DIR
 *  @brief Directory where scaler implementation libs will be kept.
 */
#define SCALER_LIB_DIR "/opt/xilinx/vvas/lib/vvas_core/"
#else
/** @def SCALER_LIB_DIR
 *  @brief Directory where scaler implementation libs will be kept.
 */
#define SCALER_LIB_DIR "/usr/lib/vvas_core/"
#endif

#define DEFAULT_LOG_LEVEL LOG_LEVEL_WARNING

/** @def KERNEL_NAME_LENGTH
 *  @brief Kernel name must not be more than this.
 */
#define KERNEL_NAME_LENGTH      256

#define CONF_STR_MAX_LENGTH 1024

typedef struct
{
  const char *name;
  VvasVideoFormat value;
} VvasColorFmtMap;

VvasColorFmtMap fmt_map[] = {
  {"VVAS_VIDEO_FORMAT_Y_UV8_420", VVAS_VIDEO_FORMAT_Y_UV8_420},
  {"VVAS_VIDEO_FORMAT_RGBx", VVAS_VIDEO_FORMAT_RGBx},
  {"VVAS_VIDEO_FORMAT_r210", VVAS_VIDEO_FORMAT_r210},
  {"VVAS_VIDEO_FORMAT_Y410", VVAS_VIDEO_FORMAT_Y410},
  {"VVAS_VIDEO_FORMAT_BGRx", VVAS_VIDEO_FORMAT_BGRx},
  {"VVAS_VIDEO_FORMAT_BGRA", VVAS_VIDEO_FORMAT_BGRA},
  {"VVAS_VIDEO_FORMAT_RGBA", VVAS_VIDEO_FORMAT_RGBA},
  {"VVAS_VIDEO_FORMAT_YUY2", VVAS_VIDEO_FORMAT_YUY2},
  {"VVAS_VIDEO_FORMAT_NV16", VVAS_VIDEO_FORMAT_NV16},
  {"VVAS_VIDEO_FORMAT_RGB", VVAS_VIDEO_FORMAT_RGB},
  {"VVAS_VIDEO_FORMAT_v308", VVAS_VIDEO_FORMAT_v308},
  {"VVAS_VIDEO_FORMAT_BGR", VVAS_VIDEO_FORMAT_BGR},
  {"VVAS_VIDEO_FORMAT_I422_10LE", VVAS_VIDEO_FORMAT_I422_10LE},
  {"VVAS_VIDEO_FORMAT_NV12_10LE32", VVAS_VIDEO_FORMAT_NV12_10LE32},
  {"VVAS_VIDEO_FORMAT_GRAY8", VVAS_VIDEO_FORMAT_GRAY8},
  {"VVAS_VIDEO_FORMAT_GRAY10_LE32", VVAS_VIDEO_FORMAT_GRAY10_LE32},
  {"VVAS_VIDEO_FORMAT_I420", VVAS_VIDEO_FORMAT_I420}
};

/** @struct VvasScalerPrivate
 *  @brief Scaler Private instance
 */
typedef struct
{
  /** Handle to dynamically loaded library */
  void *lib_handle;
  /** Scaler interface */
  VvasScalerInterface *scaler_interface;
  /** Vvas Scaler instance */
  VvasScalerInstace *scaler_instance;
} VvasScalerPrivate;

/**
 *  @fn static void parse_scaler_conf_file (const char *conf_path, VvasScalerProp *prop)
 *  @param [in] conf_path - Scaler static configuration file path
 *  @param [in] prop - Scaler properties @ref VvasScalerProp
 *  @return VvasReturnType
 *  @brief Parses configuration file and populates \p prop structure with supported color formats
 */
static VvasReturnType
parse_scaler_conf_file (const char *conf_path, VvasScalerProp * prop)
{
  FILE *fp = NULL;
  char buff[CONF_STR_MAX_LENGTH];
  uint8_t have_fmt_group = 0;

  prop->n_fmts = 0;
  memset (prop->supported_fmts, 0x0,
      sizeof (VvasVideoFormat) * VVAS_SCALER_MAX_SUPPORT_FMT);

  fp = fopen (conf_path, "r");
  if (!fp) {
    LOG_WARNING (LOG_LEVEL_ERROR, LOG_LEVEL_ERROR,
        "failed to open config file %s. reason : %s", strerror (errno));
    return VVAS_RET_ERROR;
  }

  memset (buff, 0x0, CONF_STR_MAX_LENGTH);

  while (!feof (fp)) {
    char *sret __attribute__((unused));

    sret = fgets (buff, CONF_STR_MAX_LENGTH, fp);

    if (!strncmp (buff, "[color-formats]", strlen ("[color-formats]"))) {
      have_fmt_group = 1;
      memset (buff, 0x0, CONF_STR_MAX_LENGTH);
      continue;
    }
    if (have_fmt_group) {
      int i, n_fmt;

      n_fmt = sizeof (fmt_map) / sizeof (fmt_map[0]);
      for (i = 0; i < n_fmt; i++) {
        if (!strncmp (buff, fmt_map[i].name, strlen (fmt_map[i].name))) {
          prop->supported_fmts[prop->n_fmts++] = fmt_map[i].value;
        }
      }
    }
    memset (buff, 0x0, CONF_STR_MAX_LENGTH);
  }
  fclose (fp);
  return VVAS_RET_SUCCESS;
}

/**
 *  @fn void * vvas_scaler_load_scaler_lib (VvasScalerPrivate * self, const char * kernel_name, VvasLogLevel log_level)
 *  @param [in] self            - VvasScalerPrivate handle created using @ref vvas_context_create
 *  @param [in] kernel_name     - Scaler kernel name
 *  @param [in] log_level       - Log level for VvasScaler
 *  @return Scaler instance
 *  @brief  This API finds the scaler library which implements the given kernel_name, this API looks for scaler
 *          library in @ref SCALER_LIB_DIR or environment variable @ref VVAS_CORE_LIB_PATH_ENV_VAR if set by user.
 */
static void *
vvas_scaler_load_scaler_lib (VvasScalerPrivate * self, const char *kernel_name,
    VvasLogLevel log_level)
{
  DIR *lib_dir;
  struct dirent *dir_content;
  void *lib_handle = NULL;
  const char *lib_dir_path = NULL;
  char *lib_name = NULL, *ptr;
  char k_name[KERNEL_NAME_LENGTH] = { '\0' };
  uint16_t k_name_len;

  ptr = strchr (kernel_name, ':');
  if (!ptr) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, log_level, "Kernel name must have : in it");
    return NULL;
  }

  k_name_len = ptr - kernel_name;
  if (!k_name_len || (k_name_len >= KERNEL_NAME_LENGTH)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, log_level, "Kernel name length before : must"
        " be of max length %d ", KERNEL_NAME_LENGTH);
    return NULL;
  }

  strncpy (k_name, kernel_name, k_name_len);

  lib_dir_path = getenv (VVAS_CORE_LIB_PATH_ENV_VAR);
  if (!lib_dir_path) {
    lib_dir_path = SCALER_LIB_DIR;
  }

  LOG_MESSAGE (LOG_LEVEL_INFO, log_level, "Looking for scaler library in "
      "directory: %s", lib_dir_path);

  lib_dir = opendir (lib_dir_path);
  if (!lib_dir) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, log_level, "Couldn't open %s dir, error: %s",
        lib_dir_path, strerror (errno));
    return NULL;
  }

  while ((dir_content = readdir (lib_dir))) {

    int32_t lib_name_length;

    if (!strcmp (dir_content->d_name, ".") ||
        !strcmp (dir_content->d_name, "..") ||
        !strstr (dir_content->d_name, ".so")) {
      /* skip current directory, parent directory and any file whose name
       * doesn't have .so in it */
      continue;
    }

    /* +2 for '/' and '\0' character */
    lib_name_length = strlen (lib_dir_path) + strlen (dir_content->d_name) + 2;

    lib_name = (char *) calloc (1, lib_name_length);
    if (!lib_name) {
      lib_handle = NULL;
      goto error;
    }

    sprintf (lib_name, "%s/%s", lib_dir_path, dir_content->d_name);

    /* Load library */
    lib_handle = dlopen (lib_name, RTLD_LAZY);
    if (!lib_handle) {
      free (lib_name);
      continue;
    }

    /* Library loaded, resolve symbol */
    self->scaler_interface =
        (VvasScalerInterface *) dlsym (lib_handle, VVAS_SCALER_INTERFACE);
    if (self->scaler_interface) {
      if (!strcmp (self->scaler_interface->kernel_name, k_name)) {
        /* Found the library containing user given kernel */
        LOG_MESSAGE (LOG_LEVEL_INFO, log_level, "Scaler Library: %s", lib_name);
        free (lib_name);
        break;
      } else {
        self->scaler_interface = NULL;
      }
    }
    free (lib_name);
    dlclose (lib_handle);
    lib_handle = NULL;
  }

error:
  closedir (lib_dir);
  return lib_handle;
}

/**
 *  @fn VvasScaler * vvas_scaler_create (VvasContext* ctx, const char * kernel_name, VvasLogLevel log_level)
 *  @param [in] ctx         - VvasContext handle created using @ref vvas_context_create
 *  @param [in] kernel_name - Scaler kernel name
 *  @param [in] log_level   - Log level for VvasScaler
 *  @return Scaler instance
 *  @brief  This API allocates Scaler instance.
 *  @note   This instance must be freed using @ref vvas_scaler_destroy
 */
VvasScaler *
vvas_scaler_create (VvasContext * ctx, const char *kernel_name,
    VvasLogLevel log_level)
{
  VvasScalerPrivate *self;

  if (!ctx) {
    return NULL;
  }

  if (!kernel_name) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, ctx->log_level, "kernel name can't be NULL");
    return NULL;
  }

  self = (VvasScalerPrivate *) calloc (1, sizeof (VvasScalerPrivate));

  if (!self) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, ctx->log_level, "Couldn't allocate scaler");
    return NULL;
  }

  self->lib_handle = vvas_scaler_load_scaler_lib (self, kernel_name, log_level);
  if (!self->lib_handle) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, log_level,
        "Couldn't find scaler library which implements %s kernel", kernel_name);
    goto error;
  }

  if (self->scaler_interface) {
    if (self->scaler_interface->vvas_scaler_create_impl) {
      self->scaler_instance =
          self->scaler_interface->vvas_scaler_create_impl (ctx, kernel_name,
          log_level);
      if (!self->scaler_instance) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, log_level,
            "Couldn't create scaler instance");
        goto error;
      }
    } else {
      LOG_ERROR (DEFAULT_LOG_LEVEL,
          "create is not implemented by the scaler library");
      goto error;
    }
  } else {
    LOG_ERROR (DEFAULT_LOG_LEVEL,
        "couldn't find scaler_impl library interface");
    goto error;
  }

  return (VvasScaler *) self;

error:
  if (self) {
    if (self->lib_handle) {
      dlclose (self->lib_handle);
    }
    free (self);
  }
  return NULL;
}

/**
 *  @fn VvasReturnType vvas_scaler_channel_add (VvasScaler * hndl,
 *                                              VvasScalerRect * src_rect,
 *                                              VvasScalerRect * dst_rect,
 *                                              VvasScalerPpe * ppe)
 *  @param [in] hndl        - VvasScaler handle pointer created using @ref vvas_scaler_create
 *  @param [in] src_rect    - Source Rect @ref VvasScalerRect
 *  @param [in] dst_rect    - Destination Rect @ref VvasScalerRect.
 *  @param [in] ppe         - Pre processing parameters @ref VvasScalerPpe\n NULL if no PPE is needed
 *  @return VvasReturnType
 *  @brief  This API adds one processing channel configuration.
 */
VvasReturnType
vvas_scaler_channel_add (VvasScaler * hndl,
    VvasScalerRect * src_rect, VvasScalerRect * dst_rect, VvasScalerPpe * ppe,
    VvasScalerParam * param)
{
  VvasScalerPrivate *self;
  VvasReturnType ret = VVAS_RET_ERROR;
  uint16_t dy;
  uint16_t dx;
  double in_ratio;
  double out_ratio;
  uint16_t in_scaleup_width;
  uint16_t in_scaleup_height;

  if (!hndl || !src_rect || !dst_rect || !src_rect->width || !dst_rect->width) {
    return VVAS_RET_INVALID_ARGS;
  }

  dy = dst_rect->height;
  dx = dst_rect->width;

  /* Update the source and dest rect param based on scaler type */
  if (param && param->type != VVAS_SCALER_DEFAULT) {
    if (param->type == VVAS_SCALER_LETTERBOX) {
      /* Calculate the input frame scale ratio */
      in_ratio = (double) src_rect->height / (double) src_rect->width;
      /*Calculate the output frame scale ratio */
      out_ratio = (double) dst_rect->height / (double) dst_rect->width;

      if (in_ratio < out_ratio) {
        /* update the height based on input scale ratio */
        dst_rect->height = (uint16_t) ((double) dst_rect->width * in_ratio);
      } else {
        /* update the width based on input scale ratio */
        dst_rect->width = (uint16_t) ((double) dst_rect->height / in_ratio);
      }
    } else if (param->type == VVAS_SCALER_ENVELOPE_CROPPED) {
      /* Calculate the scale ratio with respect to smallest side */
      in_ratio =
          (double) param->smallest_side_num / (double) (src_rect->height <
          src_rect->width ? src_rect->height : src_rect->width);
      /* Calculate the width and height based on scale ratio with respect to src frame */
      in_scaleup_width = (uint16_t) ((double) dst_rect->width / in_ratio);
      in_scaleup_height = (uint16_t) ((double) dst_rect->height / in_ratio);
      /* Cropping center image with calculated width and height from src image */
      src_rect->x = (src_rect->width - in_scaleup_width) / 2;
      src_rect->y = (src_rect->height - in_scaleup_height) / 2;
      src_rect->width = in_scaleup_width;
      src_rect->height = in_scaleup_height;
    } else {
      LOG_ERROR (DEFAULT_LOG_LEVEL, "Invalid Scaler Type");
      return VVAS_RET_INVALID_ARGS;
    }
    /* Update the horizontal alignment */
    switch (param->horz_align) {
      case VVAS_SCALER_HORZ_ALIGN_LEFT:
        break;
      case VVAS_SCALER_HORZ_ALIGN_RIGHT:
        dst_rect->x += (dx - dst_rect->width);
        break;
      case VVAS_SCALER_HORZ_ALIGN_CENTER:
        dst_rect->x += (dx - dst_rect->width)/2;
        break;
      default:
        LOG_ERROR (DEFAULT_LOG_LEVEL, "Invalid Scaler Horizontal Alignment Type");
        return VVAS_RET_INVALID_ARGS;
    }
    /* Update the vertical alignment */
    switch (param->vert_align) {
      case VVAS_SCALER_VERT_ALIGN_TOP:
        break;
      case VVAS_SCALER_VERT_ALIGN_BOTTOM:
        dst_rect->y += (dy - dst_rect->height);
        break;
      case VVAS_SCALER_VERT_ALIGN_CENTER:
        dst_rect->y += (dy - dst_rect->height)/2;
        break;
      default:
        LOG_ERROR (DEFAULT_LOG_LEVEL, "Invalid Scaler Vertical Alignment Type");
        return VVAS_RET_INVALID_ARGS;
    }
  }

  self = (VvasScalerPrivate *) hndl;

  if (self->scaler_interface->vvas_scaler_channel_add_impl) {
    ret =
        self->scaler_interface->vvas_scaler_channel_add_impl (self->
        scaler_instance, src_rect, dst_rect, ppe, param);
  } else {
    LOG_ERROR (DEFAULT_LOG_LEVEL,
        "channel add is not implemented by the scaler library");
  }

  return ret;
}

/**
 *  @fn VvasReturnType vvas_scaler_process_frame (VvasScaler * hndl)
 *  @param [in] hndl        - VvasScaler handle pointer created using @ref vvas_scaler_create
 *  @return VvasReturnType
 *  @brief  This API does processing of channels added using @ref vvas_scaler_channel_add
 */
VvasReturnType
vvas_scaler_process_frame (VvasScaler * hndl)
{
  VvasScalerPrivate *self;
  VvasReturnType ret = VVAS_RET_ERROR;

  if (!hndl) {
    return VVAS_RET_INVALID_ARGS;
  }

  self = (VvasScalerPrivate *) hndl;

  if (self->scaler_interface->vvas_scaler_process_frame_impl) {
    ret =
        self->scaler_interface->vvas_scaler_process_frame_impl (self->
        scaler_instance);
  } else {
    LOG_ERROR (DEFAULT_LOG_LEVEL,
        "Process frame is not implemented by the scaler library");
  }

  return ret;
}

/**
 *  @fn VvasReturnType vvas_scaler_destroy (VvasScaler * hndl)
 *  @param [in] hndl        - VvasScaler handle pointer created using @ref vvas_scaler_create
 *  @return VvasReturnType
 *  @brief  This API destroys the scaler instance created using @ref vvas_scaler_create
 */
VvasReturnType
vvas_scaler_destroy (VvasScaler * hndl)
{
  VvasScalerPrivate *self;
  VvasReturnType ret = VVAS_RET_ERROR;

  if (!hndl) {
    return VVAS_RET_INVALID_ARGS;
  }

  self = (VvasScalerPrivate *) hndl;

  if (self->scaler_interface->vvas_scaler_destroy_impl) {
    ret =
        self->scaler_interface->vvas_scaler_destroy_impl (self->
        scaler_instance);
  } else {
    LOG_ERROR (DEFAULT_LOG_LEVEL,
        "Destroy is not implemented by the scaler library");
  }

  /* Close loaded library */
  dlclose (self->lib_handle);

  /* Freeing myself */
  free (self);
  return ret;
}

/**
 *  @fn VvasReturnType vvas_scaler_set_filter_coef (VvasScaler * hndl,
                                                    VvasScalerFilterCoefType coef_type,
                                                    const int16_t tbl[VVAS_SCALER_MAX_PHASES][VVAS_SCALER_FILTER_TAPS_12])
 *  @param [in] hndl        - VvasScaler handle pointer created using @ref vvas_scaler_create
 *  @param [in] coef_type   - coef_type @ref VvasScalerFilterCoefType
 *  @param [in] tbl         - tbl Reference of VVAS_SCALER_MAX_PHASESxVVAS_SCALER_FILTER_TAPS_12 array of short
 *  @return VvasReturnType
 *  @brief  This API can be used to over write default filter coefficients.
 */
VvasReturnType
vvas_scaler_set_filter_coef (VvasScaler * hndl,
    VvasScalerFilterCoefType coef_type,
    const int16_t tbl[VVAS_SCALER_MAX_PHASES][VVAS_SCALER_FILTER_TAPS_12])
{
  VvasScalerPrivate *self;
  VvasReturnType ret = VVAS_RET_ERROR;

  if (!hndl) {
    return VVAS_RET_INVALID_ARGS;
  }

  self = (VvasScalerPrivate *) hndl;

  if (self->scaler_interface->vvas_scaler_set_filter_coef_impl) {
    ret =
        self->scaler_interface->vvas_scaler_set_filter_coef_impl (self->
        scaler_instance, coef_type, tbl);
  } else {
    LOG_ERROR (DEFAULT_LOG_LEVEL,
        "Set filter coeff is not implemented by the scaler library");
  }

  return ret;
}

/**
 *  @fn VvasReturnType vvas_scaler_prop_get (VvasScaler * hndl, VvasScalerProp* prop)
 *  @param [in] hndl    - VvasScaler handle pointer created using @ref vvas_scaler_create
 *  @param [out] prop   - Scaler properties @ref VvasScalerProp
 *  @return VvasReturnType
 *  @brief  This API will fill Scaler properties in \p prop. This API returns the default
 *          properties if called before setting these properties.
 */
VvasReturnType
vvas_scaler_prop_get (VvasScaler * hndl, VvasScalerProp * prop)
{
  VvasScalerPrivate *self;
  VvasReturnType ret = VVAS_RET_ERROR;
  const char *conf_path = NULL;

  if (!prop) {
    return VVAS_RET_INVALID_ARGS;
  }

  if (hndl) {
    self = (VvasScalerPrivate *) hndl;

    if (self->scaler_interface->vvas_scaler_prop_get_impl) {
      ret =
          self->scaler_interface->
          vvas_scaler_prop_get_impl (self->scaler_instance, prop);
      if (ret != VVAS_RET_SUCCESS) {
        LOG_ERROR (DEFAULT_LOG_LEVEL,
            "Failed to get property from scaler library");
        return ret;
      }
    } else {
      LOG_ERROR (DEFAULT_LOG_LEVEL,
          "Get Property is not implemented by the scaler library");
    }
  }

  conf_path = getenv (VVAS_CORE_CFG_PATH_ENV_VAR);
  if (!conf_path) {
    conf_path = VVAS_SCALER_STATIC_CONFIG_FILE_PATH;
  }

  /* parse config file to get ABR scaler capabilities */
  ret = parse_scaler_conf_file (conf_path, prop);

  return ret;
}

/**
 *  @fn VvasReturnType vvas_scaler_prop_set (VvasScaler * hndl, VvasScalerProp * prop)
 *  @param [in] hndl    - VvasScaler handle pointer created using @ref vvas_scaler_create
 *  @param [in] prop    - Scaler properties @ref VvasScalerProp
 *  @return VvasReturnType
 *  @brief  This API is used to set properties of VvasScaler *
 */
VvasReturnType
vvas_scaler_prop_set (VvasScaler * hndl, VvasScalerProp * prop)
{
  VvasScalerPrivate *self;
  VvasReturnType ret = VVAS_RET_ERROR;

  if (!hndl || !prop) {
    return VVAS_RET_INVALID_ARGS;
  }

  self = (VvasScalerPrivate *) hndl;

  if (self->scaler_interface->vvas_scaler_prop_set_impl) {
    ret =
        self->scaler_interface->vvas_scaler_prop_set_impl (self->
        scaler_instance, prop);
  } else {
    LOG_ERROR (DEFAULT_LOG_LEVEL,
        "Set Property is not implemented by the scaler library");
  }

  return ret;
}
