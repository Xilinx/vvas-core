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
 *  DOC: VVAS Logging APIs
 *  This file contains logging function declaration and helper macros for core libraries to print log
 *
 */

#pragma once

#ifndef __VVAS_LOG_H__
#define __VVAS_LOG_H__

/* Update of this file by the user is not encouraged */
#include <stdio.h>
#include <stdint.h>

/* Defines maximum buffer size for environment variable */
#define VVAS_CORE_FILE_PATH_SIZE                  ( 2000u )

/* Defines environment variable to which file path is set to which logs are saved.
 */
#define VVAS_CORE_LOG_FILE_PATH    ( "VVAS_CORE_LOG_FILE_PATH" )

/* Defines environment variable value for logging to console */
#define VVAS_CORE_LOG_CONSOLE           "CONSOLE"

/**
 * enum VvasLogLevel - Log levels supported VVAS Core APIs
 * @LOG_LEVEL_ERROR: Prints ERROR logs
 * @LOG_LEVEL_WARNING: Prints WARNING and ERROR logs
 * @LOG_LEVEL_INFO: Prints INFO, WARNING & ERROR logs
 * @LOG_LEVEL_DEBUG: Prints DEBUG, INFO, WARNING & ERROR logs
 */
typedef enum
{
  LOG_LEVEL_ERROR = 0,
  LOG_LEVEL_WARNING,
  LOG_LEVEL_INFO,
  LOG_LEVEL_DEBUG
} VvasLogLevel;

#define NOT_FOUND -1

#ifndef TRUE
#define TRUE    (0==0)
#endif

#ifndef FALSE
#define FALSE   (!TRUE)
#endif

/*Macro to filename which is to be used in logs printing */
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define LOG_MESSAGE(level, set_level, ...)  vvas_log(level, set_level, __FILENAME__,__func__, __LINE__, __VA_ARGS__)

#define LOG_ERROR(set_level, ...)    vvas_log(LOG_LEVEL_ERROR, set_level, __FILENAME__,__func__, __LINE__, __VA_ARGS__)
#define LOG_WARNING(set_level, ...)  vvas_log(LOG_LEVEL_WARNING, set_level, __FILENAME__,__func__, __LINE__, __VA_ARGS__)
#define LOG_INFO(set_level, ...)     vvas_log(LOG_LEVEL_INFO, set_level, __FILENAME__,__func__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(set_level, ...)    vvas_log(LOG_LEVEL_DEBUG, set_level, __FILENAME__,__func__, __LINE__, __VA_ARGS__)

/*
 * Note: Do not use below logs macros, this is for future additions.  
 */
#define LOG_ERROR_OBJ(OBJ, set_level, ...)    vvas_log(LOG_LEVEL_ERROR, set_level, __FILENAME__,__func__, __LINE__, __VA_ARGS__)
#define LOG_WARNING_OBJ(OBJ, set_level, ...)  vvas_log(LOG_LEVEL_WARNING, set_level, __FILENAME__,__func__, __LINE__, __VA_ARGS__)
#define LOG_INFO_OBJ(OBJ, set_level, ...)     vvas_log(LOG_LEVEL_INFO, set_level, __FILENAME__,__func__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG_OBJ(OBJ, set_level, ...)    vvas_log(LOG_LEVEL_DEBUG, set_level, __FILENAME__,__func__, __LINE__, __VA_ARGS__)

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * vvas_log() - This function send logs to a destination based on environment variable value VVAS_CORE_LOG_FILE_PATH
 * @log_level: Log level
 * @set_log_level: Log level to filter logs
 * @filename: Source code filename from which this logging is triggered
 * @func: Source code function name
 * @line: Source code line number
 * @fmt: Format string passed for logging.
 *
 * This API dumps logs based on VVAS_CORE_LOG_FILE_PATH environment variable like below:
 *   1. if Valid path is set then logs will be stored in specified path.
 *   2. if "CONSOLE" is set then logs will be routed to console.
 *   3. if no value is set then logs will be routed to syslog
 *
 * Note : It is recommended use macros LOG_ERROR/LOG_WARNING/LOG_INFO/LOG_DEBUG instead of calling this function to avoid sending multiple arguments
 *
 * Return: None
 */
  void vvas_log (uint32_t log_level, uint32_t set_log_level,
      const char *filename, const char *func, uint32_t line, const char *fmt,
      ...);

#ifdef __cplusplus
}
#endif

#endif                          /* __VVAS_LOG_H__ */
