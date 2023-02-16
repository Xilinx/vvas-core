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
#define _GNU_SOURCE
#include <vvas_core/vvas_log.h>
#include <vvas_core/vvas_common.h>
#include <syslog.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

/*
 * defines prefix string for loging
 */
static const char *prefix_log_string[4] = {
  "ERROR",
  "WARNING",
  "INFO",
  "DEBUG"
};

/*
 * defines sys log level
 */
static const int Sys_log_level[] = {
  LOG_ERR,
  LOG_WARNING,
  LOG_INFO,
  LOG_DEBUG
};

/**
 * @enum VvasCoreLogType
 * @brief Log levels supported VVAS Base APIs
 */
typedef enum
{
  /** Route Logs to Sys Log (Default Option) */
  CORE_LOG_TO_SYSLOG,

  /** Route to user Specified File */
  CORE_LOG_TO_FILE,

  /** Route to Console */
  CORE_LOG_TO_CONSOLE,

} VvasCoreLogType;

static void vvas_log_read_env (char *file_path, VvasCoreLogType * log_type);

/**
 * @fn void vvas_log_read_env (char *file_path, VvasCoreLogType * log_type)
 * @param[out] file_path - file_path will be updated to this variable.
 * @param[out] log_type - log_type will be updated to this variable.
 * @return  void 
 * @brief this function will read environment variable and based on 
 *        that will chose logging type (file/syslog/console)
 */
static void
vvas_log_read_env (char *file_path, VvasCoreLogType * log_type)
{

  if ((NULL == file_path) || (NULL == log_type)) {
    return;
  }

  /* Default Option */
  *log_type = CORE_LOG_TO_SYSLOG;
  const char *fp = (const char *) getenv (VVAS_CORE_LOG_FILE_PATH);

  /* 
   * Logs will be routed to user specified file/syslog/console.  
   */
  if (fp &&
      (snprintf (file_path, VVAS_CORE_FILE_PATH_SIZE, "%s",
              fp) < VVAS_CORE_FILE_PATH_SIZE)) {

    if (!strcmp (file_path, VVAS_CORE_LOG_CONSOLE)) {
      *log_type = CORE_LOG_TO_CONSOLE;
    } else {

      FILE *file = fopen (file_path, "a+");

      /*Check if file path is valid */
      if (file) {
        fclose (file);
        *log_type = CORE_LOG_TO_FILE;
      } else {
        printf ("File Path: %s is invalid!, logs will be routed to syslog",
            file_path);
      }
    }
  }
}

/**
 * When calling va_start API, we must use the function's final argument
 * as format (char *fmt). In this case, we cannot use the format directly,
 * we need to use a new format because we need to append file name, 
 * function name, and line number into our logging. But when we use the
 * new format in va_start API then the compiler is giving below warning.
 * 
 * warning: second parameter of 'va_start' not last named argument [-Wvarargs]
 * here compiler is expecting va_start(vlist, fmt)
 * but we are are calling va_start(vlist, new_fmt); 
 
 * This warning is unavoidable as we need to use new format. 
 * so suppressing warning.
 *
 * Ignore -Wvarargs warning start
 */
#pragma GCC diagnostic ignored "-Wvarargs"

/**
 * @fn void vvas_log(uint32_t log_level, uint32_t set_log_level, 
 *                     const char *func, uint32_t line, const char *fmt, ...)
 * @param[in] log_level - represents debug log level
 * @param[in] set_log_level - represents debug log level set 
 *            used for filtering
 * @param[in] filename represents filename name
 * @param[in] func - represents function name
 * @param[in] line - represents line number
 * @param[in] fmt  - string passed for logging.
 * @return  void
 * @brief This function is used to log based on environment variable value
 *        "VVAS_CORE_LOG_FILE_PATH" 
 *        1. if Valid path is set then logs will be stored in specified path. 
 *        2. if "CONSOLE" is set then logs will be routed to console.
 *        3. if no value is set then logs will be routed to syslog. 
 */
void
vvas_log (uint32_t log_level, uint32_t set_log_level, const char *filename,
    const char *func, uint32_t line, const char *fmt, ...)
{

  static char file_path[VVAS_CORE_FILE_PATH_SIZE];
  static VvasCoreLogType log_type = CORE_LOG_TO_SYSLOG;

  static bool env_read_complete = false;

  /* Read environment variable only at initial stage
   */
  if (false == env_read_complete) {
    vvas_log_read_env (file_path, &log_type);
    env_read_complete = true;
  }

  /*
   * if log level condition is not met then skip.
   */

  if (log_level > set_log_level) {
    return;
  }
  va_list vlist;
  switch (log_type) {

    case CORE_LOG_TO_CONSOLE:{
      printf ("[%s %s:%d] %s: ", filename, func, line,
          prefix_log_string[log_level]);

      va_start (vlist, fmt);
      vprintf (fmt, vlist);
      printf ("\n");
      va_end (vlist);
    }
      break;

    case CORE_LOG_TO_FILE:{
      FILE *fp = fopen (file_path, "a+");

      if (fp) {
        char *new_fmt = NULL;
        int32_t ret =
            asprintf (&new_fmt, "[%s %s:%d] %s: %s", filename, func, line,
            prefix_log_string[log_level], fmt);
        if ((ret > 0) && new_fmt) {
          va_start (vlist, new_fmt);
          vfprintf (fp, new_fmt, vlist);
          fprintf (fp, "\n");
          va_end (vlist);

          free (new_fmt);
        } else {
          va_start (vlist, fmt);

          /* Unable to append file/func name, so now push logdata directly
           * to file 
           */
          vfprintf (fp, fmt, vlist);
          va_end (vlist);
        }
        fclose (fp);
      }
    }
      break;

    case CORE_LOG_TO_SYSLOG:
    default:{
      char *new_fmt = NULL;

      int32_t ret =
          asprintf (&new_fmt, "[%s %s:%d] %s: %s", filename, func, line,
          prefix_log_string[log_level], fmt);


      if ((ret > 0) && (NULL != new_fmt)) {

        va_start (vlist, new_fmt);
        vsyslog (Sys_log_level[log_level], new_fmt, vlist);
        va_end (vlist);

        free (new_fmt);
      } else {
        va_start (vlist, fmt);

        /* Unable to append func name, now push log info to syslog */
        vsyslog (LOG_ERR, fmt, vlist);

        va_end (vlist);
      }
    }
      break;
  }

}

/* Ignore -Wvarargs warning end */
#pragma GCC diagnostic  warning  "-Wvarargs"
