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

#include <vvas_core/vvas_overlay.h>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <vvas_core/vvas_video_priv.h>
using namespace cv;

#include <vvas_core/vvas_log.h>
#define LOG_LEVEL     (LOG_LEVEL_INFO)

#define LOG_E(...)    (LOG_MESSAGE(LOG_LEVEL_ERROR, LOG_LEVEL,  __VA_ARGS__))
#define LOG_W(...)    (LOG_MESSAGE(LOG_LEVEL_WARNING, LOG_LEVEL,  __VA_ARGS__))
#define LOG_I(...)    (LOG_MESSAGE(LOG_LEVEL_INFO, LOG_LEVEL,  __VA_ARGS__))
#define LOG_D(...)    (LOG_MESSAGE(LOG_LEVEL_DEBUG, LOG_LEVEL,  __VA_ARGS__))

#define MAX_META_TEXT 10
#define MAX_STRING_SIZE 256

/**
 *  @fn  static void convert_rgb_to_yuv_clrs (VvasOverlayColorData  clr, uint8_t *y, uint16_t *uv)
 *  @param [in] clr  - reference of VvasOverlayColorData 
 *  @param [out] *y -  y component is updated here
 *  @param [out] *uv -  uv component is updated here
 *  @return none  
 *  @brief   
 *  @details This funciton retrieves y and uv color components corresponding to 
 *   givne RGB color
 */
static void
convert_rgb_to_yuv_clrs (VvasOverlayColorData & clr, uint8_t * y, uint16_t * uv)
{
  Mat YUVmat;
  Mat BGRmat (2, 2, CV_8UC3, Scalar (clr.red, clr.green, clr.blue));
  cvtColor (BGRmat, YUVmat, cv::COLOR_BGR2YUV_I420);
  *y = YUVmat.at < uchar > (0, 0);
  *uv = YUVmat.at < uchar > (2, 0) << 8 | YUVmat.at < uchar > (2, 1);
  return;
}

/**
 *  @fn static void vvas_overlay_draw_rgb_clock ( Mat &img, VvasOverlayClockInfo *pclkInfo) 
 *  @param [in] img  - Reference of img object to which clock needs to be drawn.
 *  @param [in] *pclkInfo - Address of the VvasOverlayClockInfo object.
 *  @return none  
 *  @brief   
 *  @details This funciton draws clock on the image.
 */
static void
vvas_overlay_draw_rgb_clock (Mat & img, VvasOverlayClockInfo * pclkInfo)
{
  if (NULL == pclkInfo) {
    LOG_E ("pclkInfo null received");
    return;
  }

  if (pclkInfo->display_clock) {

    time_t curtime;
    char clock_time_string[256];
    uint32_t v1, v2, v3, val;
    memset (clock_time_string, 0, sizeof (clock_time_string));

    time (&curtime);
    sprintf (clock_time_string, "%s", ctime (&curtime));

    val = pclkInfo->clock_font_color;
    val = val >> 8;

    v3 = val & 0xff;
    val = val >> 8;
    v2 = val & 0xff;
    val = val >> 8;
    v1 = val & 0xff;

    putText (img, clock_time_string,
        Point (pclkInfo->clock_x_offset, pclkInfo->clock_y_offset),
        pclkInfo->clock_font_name, pclkInfo->clock_font_scale, Scalar (v1,
            v2, v3), 1, 1);

  }
}

/**
 *  @fn static void vvas_overlay_draw_grey_clock ( Mat &img, VvasOverlayClockInfo *pclkInfo) 
 *  @param [in] img  - Reference of img object to which clock needs to be drawn.
 *  @param [in] *pclkInfo - Address of the VvasOverlayClockInfo object.
 *  @return none  
 *  @brief   
 *  @details This funciton draws clock on the image.
 */
static void
vvas_overlay_draw_gray_clock (Mat & img, VvasOverlayClockInfo * pclkInfo)
{
  if (NULL == pclkInfo) {
    LOG_E ("pclkInfo null received");
    return;
  }

  if (pclkInfo->display_clock) {

    time_t curtime;
    char clock_time_string[256];
    uint32_t v1, v2, v3, val, gray_val;

    memset (clock_time_string, 0, sizeof (clock_time_string));

    time (&curtime);
    sprintf (clock_time_string, "%s", ctime (&curtime));

    val = pclkInfo->clock_font_color;
    val = val >> 8;
    v3 = val & 0xff;
    val = val >> 8;
    v2 = val & 0xff;
    val = val >> 8;
    v1 = val & 0xff;

    gray_val = (v1 + v2 + v3) / 3;

    putText (img, clock_time_string,
        Point (pclkInfo->clock_x_offset, pclkInfo->clock_y_offset),
        pclkInfo->clock_font_name, pclkInfo->clock_font_scale,
        Scalar (gray_val), 1, 1);
  }
}

/**
 *  @fn static void vvas_overlay_draw_nv12_clock ( Mat &img_y,  Mat &img_uv, 
 *                                                            VvasOverlayClockInfo *pclkInfo) 
 *  @param [in] img  - Reference of img object to which clock needs to be drawn (Y plane).
 *  @param [in] img  - Reference of img object to which clock needs to be drawn (UV plane).
 *  @param [in] *pclkInfo - Address of the VvasOverlayClockInfo object.
 *  @return none  
 *  @brief   
 *  @details This funciton draws clock on the image.
 */
static void
vvas_overlay_draw_nv12_clock (Mat & img_y, Mat & img_uv,
    VvasOverlayClockInfo * pclkInfo)
{
  if (NULL == pclkInfo) {
    LOG_E ("pclkInfo null received");
    return;
  }

  if (pclkInfo->display_clock) {
    char clock_time_string[256];
    int32_t xmin, ymin;
    uint32_t v1, v2, v3, val;
    uint8_t yScalar;
    uint16_t uvScalar;
    VvasOverlayColorData clr;
    memset (clock_time_string, 0, sizeof (clock_time_string));

    val = pclkInfo->clock_font_color;

    val = val >> 8;
    v3 = val & 0xff;
    val = val >> 8;
    v2 = val & 0xff;
    val = val >> 8;
    v1 = val & 0xff;

    clr.red = v1;
    clr.green = v2;
    clr.blue = v3;

    convert_rgb_to_yuv_clrs (clr, &yScalar, &uvScalar);

    xmin = floor (pclkInfo->clock_x_offset / 2) * 2;
    ymin = floor (pclkInfo->clock_y_offset / 2) * 2;

    putText (img_y, clock_time_string, Point (xmin, ymin),
        pclkInfo->clock_font_name, pclkInfo->clock_font_scale,
        Scalar (yScalar), 1, 1);

    putText (img_uv, clock_time_string, Point (xmin / 2, ymin / 2),
        pclkInfo->clock_font_name, pclkInfo->clock_font_scale / 2,
        Scalar (uvScalar), 1, 1);
  }
}

/**
 *  @fn  static void vvas_overlay_rgb_draw_rect( Mat &img, VvasOverlayFrameInfo *pFrameInfo
 *                                               VvasVideoFrameMapInfo *info) 
 *  @param [in] *img  - image container.
 *  @param [in] *pFrameInfo - contains complete overlay information.
 *  @param [in] *Info - VvasVideoFrameMapInfo address.
 *  @return none  
 *  @brief   
 *  @details This funciton performs drawing on the given frame
 *
 */
static void
vvas_overlay_rgb_draw_rect (Mat & img, VvasOverlayFrameInfo * pFrameInfo,
    VvasVideoFrameMapInfo * info)
{

  if (NULL == pFrameInfo) {
    return;
  }

  uint32_t v1 = 0;
  uint32_t v2 = 0;
  uint32_t v3 = 0;
  uint32_t num_rects = pFrameInfo->shape_info.num_rects;
  uint32_t thickness = 0;
  VvasOverlayColorData ol_color;
  VvasList *head = NULL;

  memset (&ol_color, 0, sizeof (ol_color));

  //Drawing rectangles
  if (num_rects) {

    head = pFrameInfo->shape_info.rect_params;
    VvasOverlayRectParams *rect;
    while (head) {
      rect = (VvasOverlayRectParams *) head->data;
      if (rect->apply_bg_color) {
        thickness = FILLED;
        ol_color = rect->bg_color;
      } else {
        thickness = rect->thickness;
        ol_color = rect->rect_color;
      }

      if (VVAS_VIDEO_FORMAT_BGR == info->fmt) {
        v1 = ol_color.blue;
        v2 = ol_color.green;
        v3 = ol_color.red;
      } else {
        v1 = ol_color.red;
        v2 = ol_color.green;
        v3 = ol_color.blue;
      }

      rectangle (img, Rect (Point (rect->points.x,
                  rect->points.y), Size (rect->width, rect->height)),
          Scalar (v1, v2, v3), thickness, 1, 0);
      head = head->next;
    }
  }
}

 /**
 *  @fn  static void vvas_overlay_rgb_draw_text( Mat &img, VvasOverlayFrameInfo *pFrameInfo,
 *                                               VvasVideoFrameMapInfo *info)
 *  @param [in] *img  - image container.
 *  @param [in] *pFrameInfo - contains complete overlay information.
 *  @param [in] *Info - VvasVideoFrameMapInfo address.
 *  @return none  
 *  @brief   
 *  @details This funciton performs drawing on the given frame
 *
 */
static void
vvas_overlay_rgb_draw_text (Mat & img, VvasOverlayFrameInfo * pFrameInfo,
    VvasVideoFrameMapInfo * info)
{

  if (NULL == pFrameInfo) {
    return;
  }

  uint32_t v1 = 0;
  uint32_t v2 = 0;
  uint32_t v3 = 0;
  uint32_t v1_t = 0;
  uint32_t v2_t = 0;
  uint32_t v3_t = 0;
  uint32_t num_text = pFrameInfo->shape_info.num_text;
  VvasOverlayColorData ol_color;
  VvasList *head = NULL;
  char meta_str[MAX_META_TEXT][MAX_STRING_SIZE];
  Size text_size[MAX_META_TEXT];
  int base_line[MAX_META_TEXT];
  int str_cnt = 0;
  char *token;
  int tot_height;
  Point txt_start, txt_end;
  int thickness = 1;
  char *save_ptr = NULL;

  memset (&ol_color, 0, sizeof (ol_color));

  //Drawing text
  if (num_text) {
    VvasOverlayTextParams *text_info;
    head = pFrameInfo->shape_info.text_params;

    while (head) {
      text_info = (VvasOverlayTextParams *) head->data;
      str_cnt = 0;

      token = NULL;
      token = strtok_r (text_info->disp_text, "\n", &save_ptr);
      while (token != NULL) {
        
        /* Below code will print car detection and classification results in separate rows */
        strncpy (meta_str[str_cnt], token, MAX_STRING_SIZE);

        /* Terminate with '\0' charector */
        meta_str[str_cnt][MAX_STRING_SIZE -1] = '\0';
        
        str_cnt++;

        if (str_cnt >= MAX_META_TEXT)
          break;
        token = strtok_r (NULL, "\n", &save_ptr);
      }

      tot_height = 0;
      for (int i = 0; i < str_cnt; i++) {
        base_line[i] = 0;
        text_size[i] = getTextSize (meta_str[i], text_info->text_font.font_num,
            text_info->text_font.font_size, thickness, &base_line[i]);
        base_line[i] += thickness;
        base_line[i] = base_line[i] + 4;
        tot_height += (text_size[i].height + base_line[i]);
      }

      if (text_info->bottom_left_origin)
        txt_start = Point (text_info->points.x, text_info->points.y)
            + Point (0, -tot_height);
      else
        txt_start = Point (text_info->points.x, text_info->points.y);

      if (text_info->apply_bg_color) {
        ol_color = text_info->bg_color;

        if (VVAS_VIDEO_FORMAT_BGR == info->fmt) {
          v1 = ol_color.blue;
          v2 = ol_color.green;
          v3 = ol_color.red;
        } else {
          v1 = ol_color.red;
          v2 = ol_color.green;
          v3 = ol_color.blue;
        }
      }

      ol_color = text_info->text_font.font_color;

      if (VVAS_VIDEO_FORMAT_BGR == info->fmt) {
        v1_t = ol_color.blue;
        v2_t = ol_color.green;
        v3_t = ol_color.red;
      } else {
        v1_t = ol_color.red;
        v2_t = ol_color.green;
        v3_t = ol_color.blue;
      }

      for (int i = 0; i < str_cnt; i++) {
        txt_end = txt_start +
            Point (text_size[i].width, text_size[i].height + base_line[i]);
        if (text_info->apply_bg_color)
          rectangle (img, txt_start, txt_end, Scalar (v1, v2, v3), FILLED, 1,
              0);

        txt_start = txt_start + Point (0, text_size[i].height + 4);
        putText (img, meta_str[i], txt_start, text_info->text_font.font_num,
            text_info->text_font.font_size, Scalar (v1_t, v2_t, v3_t), 1);
        txt_start = txt_start + Point (0, (base_line[i] - 4));
      }

      head = head->next;
    }
  }
}

/**
 *  @fn  static void vvas_overlay_rgb_draw_line( Mat &img, VvasOverlayFrameInfo *pFrameInfo, 
 *                                               VvasVideoFrameMapInfo *info)
 *  @param [in] *img  - image container.
 *  @param [in] *pFrameInfo - contains complete overlay information.
 *  @param [in] *Info - VvasVideoFrameMapInfo address.
 *  @return none  
 *  @brief   
 *  @details This funciton performs drawing on the given frame
 *
 */
static void
vvas_overlay_rgb_draw_line (Mat & img, VvasOverlayFrameInfo * pFrameInfo,
    VvasVideoFrameMapInfo * info)
{
  if (NULL == pFrameInfo) {
    return;
  }

  uint32_t v1 = 0;
  uint32_t v2 = 0;
  uint32_t v3 = 0;
  uint32_t num_lines = pFrameInfo->shape_info.num_lines;
  VvasOverlayColorData ol_color;
  VvasList *head = NULL;
  memset (&ol_color, 0, sizeof (ol_color));

  //Drawing lines
  if (num_lines) {
    VvasOverlayLineParams *line_info;
    head = pFrameInfo->shape_info.line_params;
    while (head) {
      line_info = (VvasOverlayLineParams *) head->data;
      ol_color = line_info->line_color;

      if (VVAS_VIDEO_FORMAT_BGR == info->fmt) {
        v1 = ol_color.blue;
        v2 = ol_color.green;
        v3 = ol_color.red;
      } else {
        v1 = ol_color.red;
        v2 = ol_color.green;
        v3 = ol_color.blue;
      }
      line (img, Point (line_info->start_pt.x,
              line_info->start_pt.y), Point (line_info->end_pt.x,
              line_info->end_pt.y),
          Scalar (v1, v2, v3), line_info->thickness, 1, 0);
      head = head->next;
    }
  }
}

/**
 *  @fn  static void vvas_overlay_rgb_draw_arrow( Mat &img, VvasOverlayFrameInfo *pFrameInfo, 
 *                                               VvasVideoFrameMapInfo *info)
 *  @param [in] *img  - image container.
 *  @param [in] *pFrameInfo - contains complete overlay information.
 *  @param [in] *Info - VvasVideoFrameMapInfo address.
 *  @return none  
 *  @brief   
 *  @details This funciton performs drawing on the given frame
 *
 */
static void
vvas_overlay_rgb_draw_arrow (Mat & img, VvasOverlayFrameInfo * pFrameInfo,
    VvasVideoFrameMapInfo * info)
{
  if (NULL == pFrameInfo) {
    return;
  }

  int32_t mid_x = 0;
  int32_t mid_y = 0;
  uint32_t v1 = 0;
  uint32_t v2 = 0;
  uint32_t v3 = 0;
  uint32_t thickness = 0;
  uint32_t num_arrows = pFrameInfo->shape_info.num_arrows;
  VvasOverlayColorData ol_color;
  VvasList *head = NULL;
  memset (&ol_color, 0, sizeof (ol_color));

  //Drawing arrows
  if (num_arrows) {
    VvasOverlayArrowParams *arrow_info;
    head = pFrameInfo->shape_info.arrow_params;

    while (head) {
      arrow_info = (VvasOverlayArrowParams *) head->data;

      ol_color = arrow_info->line_color;

      if (VVAS_VIDEO_FORMAT_BGR == info->fmt) {
        v1 = ol_color.blue;
        v2 = ol_color.green;
        v3 = ol_color.red;
      } else {
        v1 = ol_color.red;
        v2 = ol_color.green;
        v3 = ol_color.blue;
      }
      thickness = arrow_info->thickness;
      switch (arrow_info->arrow_direction) {
        case ARROW_DIRECTION_START:{
          arrowedLine (img, Point (arrow_info->end_pt.x,
                  arrow_info->end_pt.y), Point (arrow_info->start_pt.x,
                  arrow_info->start_pt.y), Scalar (v1, v2, v3),
              thickness, 1, 0, arrow_info->tipLength);
        }
          break;
        case ARROW_DIRECTION_END:{
          arrowedLine (img, Point (arrow_info->start_pt.x,
                  arrow_info->start_pt.y), Point (arrow_info->end_pt.x,
                  arrow_info->end_pt.y), Scalar (v1, v2, v3),
              thickness, 1, 0, arrow_info->tipLength);
        }
          break;
        case ARROW_DIRECTION_BOTH_ENDS:{
          if (arrow_info->end_pt.x >= arrow_info->start_pt.x) {
            mid_x = arrow_info->start_pt.x + (arrow_info->end_pt.x -
                arrow_info->start_pt.x) / 2;
          } else {
            mid_x = arrow_info->end_pt.x + (arrow_info->start_pt.x -
                arrow_info->end_pt.x) / 2;
          }

          if (arrow_info->end_pt.y >= arrow_info->start_pt.y) {

            mid_y = arrow_info->start_pt.y + (arrow_info->end_pt.y -
                arrow_info->start_pt.y) / 2;
          } else {
            mid_y = arrow_info->end_pt.y + (arrow_info->start_pt.y -
                arrow_info->end_pt.y) / 2;
          }

          arrowedLine (img, Point (mid_x, mid_y),
              Point (arrow_info->end_pt.x, arrow_info->end_pt.y),
              Scalar (v1, v2, v3), thickness, 1, 0, arrow_info->tipLength / 2);

          arrowedLine (img, Point (mid_x, mid_y),
              Point (arrow_info->start_pt.x, arrow_info->start_pt.y),
              Scalar (v1, v2, v3), thickness, 1, 0, arrow_info->tipLength / 2);
        }
          break;
        default:
          break;
      }
      head = head->next;
    }
  }
}

/**
 *  @fn  static void vvas_overlay_rgb_draw_circle( Mat &img, VvasOverlayFrameInfo *pFrameInfo) 
 *                                               VvasVideoFrameMapInfo *info)
 *  @param [in] *img  - image container.
 *  @param [in] *pFrameInfo - contains complete overlay information.
 *  @param [in] *Info - VvasVideoFrameMapInfo address.
 *  @return none  
 *  @brief   
 *  @details This funciton performs drawing on the given frame
 *
 */
static void
vvas_overlay_rgb_draw_circle (Mat & img, VvasOverlayFrameInfo * pFrameInfo,
    VvasVideoFrameMapInfo * info)
{

  if (NULL == pFrameInfo) {
    return;
  }

  uint32_t v1 = 0;
  uint32_t v2 = 0;
  uint32_t v3 = 0;
  uint32_t num_circles = pFrameInfo->shape_info.num_circles;
  VvasOverlayColorData ol_color;
  VvasList *head = NULL;
  memset (&ol_color, 0, sizeof (ol_color));

  /* check how many circles need to be drawn */
  if (num_circles) {
    VvasOverlayCircleParams *circle_info;
    head = pFrameInfo->shape_info.circle_params;

    while (head) {
      circle_info = (VvasOverlayCircleParams *) head->data;
      ol_color = circle_info->circle_color;

      if (VVAS_VIDEO_FORMAT_BGR == info->fmt) {
        v1 = ol_color.blue;
        v2 = ol_color.green;
        v3 = ol_color.red;
      } else {
        v1 = ol_color.red;
        v2 = ol_color.green;
        v3 = ol_color.blue;
      }

      circle (img, Point (circle_info->center_pt.x,
              circle_info->center_pt.y), circle_info->radius,
          Scalar (v1, v2, v3), circle_info->thickness, 1, 0);
      head = head->next;
    }
  }

}

 /**
 *  @fn  static void vvas_overlay_rgb_draw_polygon( Mat &img, 
 *                                                  VvasOverlayFrameInfo *pFrameInfo,
 *                                                  VvasVideoFrameMapInfo *info) 
 *  @param [in] *img  - image container.
 *  @param [in] *pFrameInfo - contains complete overlay information.
 *  @param [in] *Info - VvasVideoFrameMapInfo address.
 *  @return none  
 *  @brief   
 *  @details This funciton performs drawing on the given frame
 *
 */
static void
vvas_overlay_rgb_draw_polygon (Mat & img,
    VvasOverlayFrameInfo * pFrameInfo, VvasVideoFrameMapInfo * info)
{
  if (NULL == pFrameInfo) {
    return;
  }

  uint32_t v1 = 0;
  uint32_t v2 = 0;
  uint32_t v3 = 0;
  uint32_t num_polys = pFrameInfo->shape_info.num_polys;
  VvasOverlayColorData ol_color;
  VvasList *head = NULL, *pt_head = NULL;
  memset (&ol_color, 0, sizeof (ol_color));

  /* check how many polygons need to be drawn */
  if (num_polys) {
    VvasOverlayPolygonParams *poly_info;
    std::vector < Point > poly_pts;
    const Point *pts;
    head = pFrameInfo->shape_info.polygn_params;
    while (head) {
      poly_info = (VvasOverlayPolygonParams *) head->data;
      ol_color = poly_info->poly_color;

      if (VVAS_VIDEO_FORMAT_BGR == info->fmt) {
        v1 = ol_color.blue;
        v2 = ol_color.green;
        v3 = ol_color.red;
      } else {
        v1 = ol_color.red;
        v2 = ol_color.green;
        v3 = ol_color.blue;
      }

      poly_pts.clear ();
      pt_head = poly_info->poly_pts;
      VvasOverlayCoordinates *pt_info;
      while (pt_head) {
        pt_info = (VvasOverlayCoordinates *) pt_head->data;
        poly_pts.push_back (Point (pt_info->x, pt_info->y));
        pt_head = pt_head->next;
      }

      pts = (const Point *) Mat (poly_pts).data;

      /* draws poloygon on the image buffer */
      polylines (img, &pts, &poly_info->num_pts, 1, true,
          Scalar (v1, v2, v3), poly_info->thickness, 1, 0);
      head = head->next;
    }
  }
}

/**
 *  @fn VvasReturnType vvas_overlay_rgb_draw(VvasOverlayFrameInfo *pFrameInfo,
 *                                          VvasVideoFrameMapInfo *info)
 *  @param [in] *pFrameInfo  OverlayFrameInformation.
 *  @param [in] *Info  Map info structure.
 *  @return On Success returns VVAS_RET_SUCCESS 
 *          On Failure returns VVAS_ERROR_*  
 *  @brief   
 *  @details This funciton performs drawing on the given frame
 *
 */
static VvasReturnType
vvas_overlay_rgb_draw (VvasOverlayFrameInfo * pFrameInfo,
    VvasVideoFrameMapInfo * info)
{
  VvasReturnType ret = VVAS_RET_SUCCESS;

  if (NULL == pFrameInfo) {
    ret = VVAS_RET_INVALID_ARGS;
    return ret;
  }

  uint32_t img_height = info->height;
  uint32_t img_width = info->width;
  uint32_t stride = info->planes[0].stride;
  uint8_t *in_plane1 = info->planes[0].data;

  Mat img (img_height, img_width, CV_8UC3, in_plane1, stride);

  /* draw clock info */
  vvas_overlay_draw_rgb_clock (img, &pFrameInfo->clk_info);

  /* draws rectangle pattern on the image */
  vvas_overlay_rgb_draw_rect (img, pFrameInfo, info);

  /* draws text information on image */
  vvas_overlay_rgb_draw_text (img, pFrameInfo, info);

  /* draws line pattern on image */
  vvas_overlay_rgb_draw_line (img, pFrameInfo, info);

  /* draws arrow pattern on image */
  vvas_overlay_rgb_draw_arrow (img, pFrameInfo, info);

  /* draws circle pattern on image */
  vvas_overlay_rgb_draw_circle (img, pFrameInfo, info);

  /* draws polygon pattern on image */
  vvas_overlay_rgb_draw_polygon (img, pFrameInfo, info);

  return ret;
}


/**
 *  @fn  static void vvas_overlay_nv12_draw_rect(Mat &img_y, Mat &img_uv, VvasOverlayFrameInfo *pFrameInfo) 
 *  @param [in] *img_y  - image container for luma.
 *  @param [in] *img_uv  - image container for chroma.
 *  @param [in] *pFrameInfo - contains complete overlay information.
 *  @return none  
 *  @brief   
 *  @details This funciton performs drawing on the given frame
 *
 */
static void
vvas_overlay_nv12_draw_rect (Mat & img_y, Mat & img_uv,
    VvasOverlayFrameInfo * pFrameInfo)
{
  if (NULL == pFrameInfo) {
    return;
  }

  int32_t xmin = 0;
  int32_t ymin = 0;
  int32_t xmax = 0;
  int32_t ymax = 0;
  uint32_t thickness = 0;
  uint32_t num_rects = pFrameInfo->shape_info.num_rects;
  uint8_t yScalar = 0;
  uint16_t uvScalar = 0;
  VvasList *head = NULL;

  VvasOverlayColorData ol_color;
  memset (&ol_color, 0, sizeof (ol_color));

  if (num_rects) {
    VvasOverlayRectParams *rect;
    head = pFrameInfo->shape_info.rect_params;

    while (head) {
      rect = (VvasOverlayRectParams *) head->data;
      xmin = floor (rect->points.x / 2) * 2;
      ymin = floor (rect->points.y / 2) * 2;
      xmax = floor ((rect->width + rect->points.x) / 2) * 2;
      ymax = floor ((rect->height + rect->points.y) / 2) * 2;

      if (rect->apply_bg_color) {
        convert_rgb_to_yuv_clrs (rect->bg_color, &yScalar, &uvScalar);
        rectangle (img_y, Rect (Point (xmin, ymin),
                Point (xmax, ymax)), Scalar (yScalar), FILLED, 1, 0);
        rectangle (img_uv, Rect (Point (xmin / 2, ymin / 2),
                Point (xmax / 2, ymax / 2)), Scalar (uvScalar), FILLED, 1, 0);
      } else {
        thickness = (rect->thickness * 2) / 2;
        convert_rgb_to_yuv_clrs (rect->rect_color, &yScalar, &uvScalar);
        rectangle (img_y, Rect (Point (xmin, ymin),
                Point (xmax, ymax)), Scalar (yScalar), thickness, 1, 0);

        rectangle (img_uv, Rect (Point (xmin / 2, ymin / 2),
                Point (xmax / 2, ymax / 2)), Scalar (uvScalar), thickness / 2,
            1, 0);
      }
      head = head->next;
    }
  }
}


/**
 *  @fn  static void vvas_overlay_nv12_draw_text(Mat &img_y, Mat &img_uv, VvasOverlayFrameInfo *pFrameInfo) 
 *  @param [in] *img_y  - image container for luma.
 *  @param [in] *img_uv  - image container for chroma.
 *  @param [in] *pFrameInfo - contains complete overlay information.
 *  @return none  
 *  @brief   
 *  @details This funciton performs drawing on the given frame
 *
 */
static void
vvas_overlay_nv12_draw_text (Mat & img_y, Mat & img_uv,
    VvasOverlayFrameInfo * pFrameInfo)
{

  if (NULL == pFrameInfo) {
    return;
  }

  int32_t xmin = 0;
  int32_t ymin = 0;
  uint32_t num_text = pFrameInfo->shape_info.num_text;
  uint8_t yScalar = 0;
  uint16_t uvScalar = 0;
  uint8_t bg_yScalar = 0;
  uint16_t bg_uvScalar = 0;
  VvasOverlayColorData ol_color;
  VvasList *head = NULL;
  char meta_str[MAX_META_TEXT][MAX_STRING_SIZE];
  Size text_size[MAX_META_TEXT];
  int base_line[MAX_META_TEXT];
  int str_cnt = 0;
  char *token;
  int tot_height;
  Point txt_start, txt_end;
  int thickness = 2;
  char *save_ptr = NULL;

  memset (&ol_color, 0, sizeof (ol_color));

  //Drawing text
  if (num_text) {
    VvasOverlayTextParams *text_info;
    head = pFrameInfo->shape_info.text_params;

    while (head) {
      text_info = (VvasOverlayTextParams *) head->data;
      xmin = floor (text_info->points.x / 2) * 2;
      ymin = floor (text_info->points.y / 2) * 2;
      str_cnt = 0;

      token = NULL;
      token = strtok_r (text_info->disp_text, "\n", &save_ptr);
      while (token != NULL) {

        /* Below code will print car detection and classification results in separate rows */
        strncpy (meta_str[str_cnt], token, MAX_STRING_SIZE);

        /* Terminate with '\0' charector */
        meta_str[str_cnt][MAX_STRING_SIZE -1] = '\0';

        str_cnt++;
        if (str_cnt >= MAX_META_TEXT)
          break;
        token = strtok_r (NULL, "\n", &save_ptr);
      }

      tot_height = 0;
      for (int i = 0; i < str_cnt; i++) {
        base_line[i] = 0;
        text_size[i] = getTextSize (meta_str[i], text_info->text_font.font_num,
            text_info->text_font.font_size, thickness, &base_line[i]);
        text_size[i].width = floor (text_size[i].width / 2) * 2;
        text_size[i].height = floor (text_size[i].height / 2) * 2;
        base_line[i] += thickness;
        base_line[i] = base_line[i] + 4;
        tot_height += (text_size[i].height + base_line[i]);
      }

      if (text_info->bottom_left_origin)
        txt_start = Point (xmin, ymin) + Point (0, -tot_height);
      else
        txt_start = Point (xmin, ymin);

      if (text_info->apply_bg_color)
        convert_rgb_to_yuv_clrs (text_info->bg_color,
            &bg_yScalar, &bg_uvScalar);

      convert_rgb_to_yuv_clrs (text_info->text_font.font_color,
          &yScalar, &uvScalar);

      for (int i = 0; i < str_cnt; i++) {
        txt_end = txt_start +
            Point (text_size[i].width, text_size[i].height + base_line[i]);
        if (text_info->apply_bg_color) {
          rectangle (img_y, txt_start, txt_end, Scalar (bg_yScalar), FILLED, 1,
              0);
          rectangle (img_uv, txt_start / 2, txt_end / 2, Scalar (bg_uvScalar),
              FILLED, 1, 0);
        }

        txt_start = txt_start + Point (0, text_size[i].height + 4);
        putText (img_y, meta_str[i], txt_start, text_info->text_font.font_num,
            text_info->text_font.font_size, Scalar (yScalar), 1);
        putText (img_uv, meta_str[i], txt_start / 2,
            text_info->text_font.font_num, text_info->text_font.font_size / 2,
            Scalar (uvScalar), 1);
        txt_start = txt_start + Point (0, (base_line[i] - 4));
      }
      head = head->next;
    }
  }
}

/**
 *  @fn  static void vvas_overlay_nv12_draw_line(Mat &img_y, Mat &img_uv, VvasOverlayFrameInfo *pFrameInfo) 
 *  @param [in] *img_y  - image container for luma.
 *  @param [in] *img_uv  - image container for chroma.
 *  @param [in] *pFrameInfo - contains complete overlay information.
 *  @return none  
 *  @brief   
 *  @details This funciton performs drawing on the given frame
 *
 */
static void
vvas_overlay_nv12_draw_line (Mat & img_y, Mat & img_uv,
    VvasOverlayFrameInfo * pFrameInfo)
{

  if (NULL == pFrameInfo) {
    return;
  }

  int32_t xmin = 0;
  int32_t ymin = 0;
  int32_t xmax = 0;
  int32_t ymax = 0;
  uint8_t yScalar = 0;
  uint16_t uvScalar = 0;
  VvasList *head = NULL;
  uint32_t num_lines = pFrameInfo->shape_info.num_lines;
  uint32_t thickness = 0;

  if (num_lines) {
    VvasOverlayLineParams *line_info;
    head = pFrameInfo->shape_info.line_params;
    while (head) {
      line_info = (VvasOverlayLineParams *) head->data;
      convert_rgb_to_yuv_clrs (line_info->line_color, &yScalar, &uvScalar);
      xmin = floor (line_info->start_pt.x / 2) * 2;
      ymin = floor (line_info->start_pt.y / 2) * 2;
      xmax = floor (line_info->end_pt.x / 2) * 2;
      ymax = floor (line_info->end_pt.y / 2) * 2;
      thickness = (line_info->thickness * 2) / 2;
      line (img_y, Point (xmin, ymin), Point (xmax, ymax),
          Scalar (yScalar), thickness, 1, 0);

      line (img_uv, Point (xmin / 2, ymin / 2), Point (xmax / 2, ymax / 2),
          Scalar (uvScalar), thickness / 2, 1, 0);
      head = head->next;
    }
  }
}


 /**
 *  @fn  static void vvas_overlay_nv12_draw_arrow(Mat &img_y, Mat &img_uv, VvasOverlayFrameInfo *pFrameInfo) 
 *  @param [in] *img_y  - image container for luma.
 *  @param [in] *img_uv  - image container for chroma.
 *  @param [in] *pFrameInfo - contains complete overlay information.
 *  @return none  
 *  @brief   
 *  @details This funciton performs drawing on the given frame
 *
 */
static void
vvas_overlay_nv12_draw_arrow (Mat & img_y, Mat & img_uv,
    VvasOverlayFrameInfo * pFrameInfo)
{
  if (NULL == pFrameInfo) {
    return;
  }

  int32_t xmin = 0;
  int32_t ymin = 0;
  int32_t xmax = 0;
  int32_t ymax = 0;
  int32_t mid_x = 0;
  int32_t mid_y = 0;
  uint8_t yScalar = 0;
  uint16_t uvScalar = 0;
  uint32_t thickness = 0;
  uint32_t num_arrows = pFrameInfo->shape_info.num_arrows;
  float tiplength = 0;
  VvasList *head = NULL;

  //Drawing arrows
  if (num_arrows) {
    VvasOverlayArrowParams *arrow_info;
    head = pFrameInfo->shape_info.arrow_params;

    while (head) {
      arrow_info = (VvasOverlayArrowParams *) head->data;
      convert_rgb_to_yuv_clrs (arrow_info->line_color, &yScalar, &uvScalar);
      xmin = floor (arrow_info->start_pt.x / 2) * 2;
      ymin = floor (arrow_info->start_pt.y / 2) * 2;
      xmax = floor (arrow_info->end_pt.x / 2) * 2;
      ymax = floor (arrow_info->end_pt.y / 2) * 2;
      tiplength = arrow_info->tipLength;

      thickness = (arrow_info->thickness * 2) / 2;
      switch (arrow_info->arrow_direction) {
        case ARROW_DIRECTION_START:{
          arrowedLine (img_y, Point (xmax, ymax), Point (xmin, ymin),
              Scalar (yScalar), thickness, 1, 0, tiplength);

          arrowedLine (img_uv, Point (xmax / 2, ymax / 2), Point (xmin / 2,
                  ymin / 2), Scalar (uvScalar), thickness / 2, 1, 0, tiplength);
        }
          break;
        case ARROW_DIRECTION_END:{
          arrowedLine (img_y, Point (xmin, ymin), Point (xmax, ymax),
              Scalar (yScalar), thickness, 1, 0, tiplength);

          arrowedLine (img_uv, Point (xmin / 2, ymin / 2), Point (xmax / 2,
                  ymax / 2), Scalar (uvScalar), thickness / 2, 1, 0, tiplength);
        }
          break;
        case ARROW_DIRECTION_BOTH_ENDS:{
          if (xmax >= xmin) {
            mid_x = floor ((xmin + (xmax - xmin) / 2) / 2) * 2;
          } else {
            mid_x = floor ((xmax + (xmin - xmax) / 2) / 2) * 2;
          }

          if (ymax >= ymin) {
            mid_y = floor ((ymin + (ymax - ymin) / 2) / 2) * 2;
          } else {
            mid_y = floor ((ymax + (ymin - ymax) / 2) / 2) * 2;
          }

          arrowedLine (img_y, Point (mid_x, mid_y),
              Point (xmax, ymax), Scalar (yScalar), thickness, 1, 0, tiplength);

          arrowedLine (img_y, Point (mid_x, mid_y),
              Point (xmin, ymin), Scalar (yScalar), thickness, 1, 0, tiplength);

          arrowedLine (img_uv, Point (mid_x / 2, mid_y / 2),
              Point (xmax / 2, ymax / 2), Scalar (uvScalar),
              thickness / 2, 1, 0, tiplength);

          arrowedLine (img_uv, Point (mid_x / 2, mid_y / 2),
              Point (xmin / 2, ymin / 2), Scalar (uvScalar),
              thickness / 2, 1, 0, tiplength);
        }
          break;
        default:
          break;
      }                         // end of switch case
      head = head->next;
    }                           // end of while loop
  }                             // end of if block

}


 /**
 *  @fn  static void vvas_overlay_nv12_draw_circle(Mat &img_y, Mat &img_uv, VvasOverlayFrameInfo *pFrameInfo) 
 *  @param [in] *img_y  - image container for luma.
 *  @param [in] *img_uv  - image container for chroma.
 *  @param [in] *pFrameInfo - contains complete overlay information.
 *  @return none  
 *  @brief   
 *  @details This funciton performs drawing on the given frame
 *
 */
static void
vvas_overlay_nv12_draw_circle (Mat & img_y, Mat & img_uv,
    VvasOverlayFrameInfo * pFrameInfo)
{
  if (NULL == pFrameInfo) {
    return;
  }

  int32_t xmin = 0;
  int32_t ymin = 0;
  int32_t radius = 0;
  uint8_t yScalar = 0;
  uint16_t uvScalar = 0;
  VvasList *head = NULL;
  uint32_t num_circles = pFrameInfo->shape_info.num_circles;
  uint32_t thickness = 0;

  //Drawing cicles
  if (num_circles) {
    VvasOverlayCircleParams *circle_info;
    head = pFrameInfo->shape_info.circle_params;

    while (head) {
      circle_info = (VvasOverlayCircleParams *) head->data;
      convert_rgb_to_yuv_clrs (circle_info->circle_color, &yScalar, &uvScalar);
      xmin = floor (circle_info->center_pt.x / 2) * 2;
      ymin = floor (circle_info->center_pt.y / 2) * 2;
      radius = floor (circle_info->radius / 2) * 2;
      thickness = (circle_info->thickness * 2) / 2;

      circle (img_y, Point (xmin, ymin), radius,
          Scalar (yScalar), thickness, 1, 0);

      circle (img_uv, Point (xmin / 2, ymin / 2), radius / 2,
          Scalar (uvScalar), thickness / 2, 1, 0);
      head = head->next;
    }
  }
}

 /**
 *  @fn  static void vvas_overlay_nv12_draw_polygon(Mat &img_y, Mat &img_uv, VvasOverlayFrameInfo *pFrameInfo) 
 *  @param [in] *img_y  - image container for luma.
 *  @param [in] *img_uv  - image container for chroma.
 *  @param [in] *pFrameInfo - contains complete overlay information.
 *  @return none  
 *  @brief   
 *  @details This funciton performs drawing on the given frame
 *
 */
static void
vvas_overlay_nv12_draw_polygon (Mat & img_y, Mat & img_uv,
    VvasOverlayFrameInfo * pFrameInfo)
{
  if (NULL == pFrameInfo) {
    return;
  }

  int32_t xmin = 0;
  int32_t ymin = 0;
  uint8_t yScalar = 0;
  uint16_t uvScalar = 0;
  VvasList *head = NULL, *pt_head = NULL;

  uint32_t num_polys = pFrameInfo->shape_info.num_polys;
  uint32_t thickness = 0;

  //Drawing polygons
  if (num_polys) {
    VvasOverlayPolygonParams *poly_info;
    head = pFrameInfo->shape_info.polygn_params;

    std::vector < Point > poly_pts_y;
    std::vector < Point > poly_pts_uv;
    const Point *pts;
    while (head) {
      poly_info = (VvasOverlayPolygonParams *) head->data;

      convert_rgb_to_yuv_clrs (poly_info->poly_color, &yScalar, &uvScalar);

      poly_pts_y.clear ();
      poly_pts_uv.clear ();
      pt_head = poly_info->poly_pts;
      VvasOverlayCoordinates *pt_info;
      while (pt_head) {
        pt_info = (VvasOverlayCoordinates *) pt_head->data;
        xmin = floor (pt_info->x / 2) * 2;
        ymin = floor (pt_info->y / 2) * 2;
        poly_pts_y.push_back (Point (xmin, ymin));
        poly_pts_uv.push_back (Point (xmin / 2, ymin / 2));
        pt_head = pt_head->next;
      }

      thickness = (poly_info->thickness * 2) / 2;
      pts = (const Point *) Mat (poly_pts_y).data;
      polylines (img_y, &pts, &poly_info->num_pts, 1, true,
          Scalar (yScalar), thickness, 1, 0);

      pts = (const Point *) Mat (poly_pts_uv).data;
      polylines (img_uv, &pts, &poly_info->num_pts, 1, true,
          Scalar (uvScalar), thickness / 2, 1, 0);
      head = head->next;
    }                           //end of for loop
  }                             // end of if block

}

/**
 *  @fn VvasReturnType vvas_overlay_nv12_draw(VvasOverlayFrameInfo *pFrameInfo
 *                                           VvasVideoFrameMapInfo *info)
 *  @param [in] *pFrameInfo  - OverlayFrameInformation.
 *  @param [in] *Info  VvasVideoFrameMapInfo address.
 *  @return On Success returns VVAS_RET_SUCCESS 
 *          On Failure returns VVAS_ERROR_*  
 *  @brief   
 *  @details This funciton performs drawing on the given frame
 *
 */
static VvasReturnType
vvas_overlay_nv12_draw (VvasOverlayFrameInfo * pFrameInfo,
    VvasVideoFrameMapInfo * info)
{
  VvasReturnType ret = VVAS_RET_SUCCESS;

  if (NULL == pFrameInfo) {
    ret = VVAS_RET_INVALID_ARGS;
    return ret;
  }

  uint32_t img_height = info->height;
  uint32_t img_width = info->width;
  uint32_t stride = info->planes[0].stride;
  uint8_t *in_plane1 = info->planes[0].data;


  Mat img_y (img_height, img_width, CV_8UC1, in_plane1, stride);


  uint8_t *in_plane2 = info->planes[1].data;
  stride = info->planes[1].stride;

  Mat img_uv (img_height / 2, img_width / 2, CV_16UC1, in_plane2, stride);

  /* draw clock info */
  vvas_overlay_draw_nv12_clock (img_y, img_uv, &pFrameInfo->clk_info);

  /* draws rectangle pattern on the image */
  vvas_overlay_nv12_draw_rect (img_y, img_uv, pFrameInfo);

  /* draws text information on image */
  vvas_overlay_nv12_draw_text (img_y, img_uv, pFrameInfo);

  /* draws line pattern on image */
  vvas_overlay_nv12_draw_line (img_y, img_uv, pFrameInfo);

  /* draws arrow pattern on image */
  vvas_overlay_nv12_draw_arrow (img_y, img_uv, pFrameInfo);

  /* draws circle pattern on image */
  vvas_overlay_nv12_draw_circle (img_y, img_uv, pFrameInfo);

  /* draws polygon pattern on image */
  vvas_overlay_nv12_draw_polygon (img_y, img_uv, pFrameInfo);
  return ret;
}

/**
 *  @fn  static void vvas_overlay_gray_draw_rect(Mat &img, VvasOverlayFrameInfo *pFrameInfo) 
 *  @param [in] *img  - image container.
 *  @param [in] *pFrameInfo - contains complete overlay information.
 *  @return none  
 *  @brief   
 *  @details This funciton performs drawing on the given frame
 *
 */
static void
vvas_overlay_gray_draw_rect (Mat & img, VvasOverlayFrameInfo * pFrameInfo)
{

  if (NULL == pFrameInfo) {
    return;
  }

  uint32_t num_rects = pFrameInfo->shape_info.num_rects;
  uint32_t thickness = 0;
  uint32_t gray_val = 0;
  VvasList *head = NULL;

  //Drawing rectangles
  if (num_rects) {
    VvasOverlayRectParams *rect;
    head = pFrameInfo->shape_info.rect_params;
    while (head) {
      rect = (VvasOverlayRectParams *) head->data;
      if (rect->apply_bg_color) {
        thickness = FILLED;
        gray_val = (rect->bg_color.red +
            rect->bg_color.green + rect->bg_color.blue) / 3;
      } else {
        thickness = rect->thickness;
        gray_val = (rect->rect_color.red +
            rect->rect_color.green + rect->rect_color.blue) / 3;
      }
      rectangle (img, Rect (Point (rect->points.x,
                  rect->points.y), Size (rect->width, rect->height)),
          Scalar (gray_val), thickness, 1, 0);
      head = head->next;
    }
  }
}

/**
 *  @fn  static void vvas_overlay_gray_draw_text(Mat &img, VvasOverlayFrameInfo *pFrameInfo) 
 *  @param [in] *img  - image container.
 *  @param [in] *pFrameInfo - contains complete overlay information.
 *  @return none  
 *  @brief   
 *  @details This funciton performs drawing on the given frame
 *
 */
static void
vvas_overlay_gray_draw_text (Mat & img, VvasOverlayFrameInfo * pFrameInfo)
{

  if (NULL == pFrameInfo) {
    return;
  }

  uint32_t num_text = pFrameInfo->shape_info.num_text;
  uint32_t gray_val = 0;
  uint32_t gray_val_t = 0;
  VvasList *head = NULL;
  char meta_str[MAX_META_TEXT][MAX_STRING_SIZE];
  Size text_size[MAX_META_TEXT];
  int base_line[MAX_META_TEXT];
  int str_cnt = 0;
  char *token;
  int tot_height;
  Point txt_start, txt_end;
  int thickness = 1;
  char *save_ptr = NULL;

  //Drawing text
  if (num_text) {
    VvasOverlayTextParams *text_info;
    head = pFrameInfo->shape_info.text_params;

    while (head) {
      text_info = (VvasOverlayTextParams *) head->data;
      str_cnt = 0;

      token = NULL;
      token = strtok_r (text_info->disp_text, "\n", &save_ptr);
      while (token != NULL) {
        
        /* Below code will print car detection and classification results in separate rows */
        strncpy (meta_str[str_cnt], token, MAX_STRING_SIZE);

        /* Terminate with '\0' charector */
        meta_str[str_cnt][MAX_STRING_SIZE -1] = '\0';
        
        str_cnt++;

        if (str_cnt >= MAX_META_TEXT)
          break;
        token = strtok_r (NULL, "\n", &save_ptr);
      }

      tot_height = 0;
      for (int i = 0; i < str_cnt; i++) {
        base_line[i] = 0;
        text_size[i] = getTextSize (meta_str[i], text_info->text_font.font_num,
            text_info->text_font.font_size, thickness, &base_line[i]);
        base_line[i] += thickness;
        base_line[i] = base_line[i] + 4;
        tot_height += (text_size[i].height + base_line[i]);
      }

      if (text_info->bottom_left_origin)
        txt_start = Point (text_info->points.x, text_info->points.y)
            + Point (0, -tot_height);
      else
        txt_start = Point (text_info->points.x, text_info->points.y);

      if (text_info->apply_bg_color)
        gray_val = (text_info->bg_color.red +
            text_info->bg_color.green + text_info->bg_color.blue) / 3;

      gray_val_t = (text_info->text_font.font_color.red +
          text_info->text_font.font_color.green +
          text_info->text_font.font_color.blue) / 3;

      for (int i = 0; i < str_cnt; i++) {
        txt_end = txt_start +
            Point (text_size[i].width, text_size[i].height + base_line[i]);
        if (text_info->apply_bg_color)
          rectangle (img, txt_start, txt_end, Scalar (gray_val), FILLED, 1, 0);

        txt_start = txt_start + Point (0, text_size[i].height + 4);
        putText (img, meta_str[i], txt_start, text_info->text_font.font_num,
            text_info->text_font.font_size, Scalar (gray_val_t), 1);
        txt_start = txt_start + Point (0, (base_line[i] - 4));
      }

      head = head->next;
    }
  }
}


/**
 *  @fn  static void vvas_overlay_gray_draw_lines(Mat &img, VvasOverlayFrameInfo *pFrameInfo) 
 *  @param [in] *img  - image container.
 *  @param [in] *pFrameInfo - contains complete overlay information.
 *  @return none  
 *  @brief   
 *  @details This funciton performs drawing on the given frame
 *
 */
static void
vvas_overlay_gray_draw_line (Mat & img, VvasOverlayFrameInfo * pFrameInfo)
{

  if (NULL == pFrameInfo) {
    return;
  }

  uint32_t num_lines = pFrameInfo->shape_info.num_lines;
  uint32_t gray_val = 0;
  VvasList *head = NULL;

  //Drawing lines
  if (num_lines) {
    VvasOverlayLineParams *line_info;
    head = pFrameInfo->shape_info.line_params;

    while (head) {
      line_info = (VvasOverlayLineParams *) head->data;
      gray_val = (line_info->line_color.red +
          line_info->line_color.green + line_info->line_color.blue) / 3;
      line (img, Point (line_info->start_pt.x,
              line_info->start_pt.y), Point (line_info->end_pt.x,
              line_info->end_pt.y),
          Scalar (gray_val), line_info->thickness, 1, 0);
      head = head->next;
    }
  }
}

 /**
 *  @fn  static void vvas_overlay_gray_draw_arrows(Mat &img, VvasOverlayFrameInfo *pFrameInfo) 
 *  @param [in] *img  - image container.
 *  @param [in] *pFrameInfo - contains complete overlay information.
 *  @return none  
 *  @brief   
 *  @details This funciton performs drawing on the given frame
 *
 */
static void
vvas_overlay_gray_draw_arrow (Mat & img, VvasOverlayFrameInfo * pFrameInfo)
{

  if (NULL == pFrameInfo) {
    return;
  }

  uint32_t mid_x = 0;
  uint32_t mid_y = 0;
  uint32_t num_arrows = pFrameInfo->shape_info.num_arrows;
  uint32_t gray_val = 0;
  uint32_t thickness = 0;
  VvasList *head = NULL;

  if (num_arrows) {
    VvasOverlayArrowParams *arrow_info;
    head = pFrameInfo->shape_info.arrow_params;

    while (head) {
      arrow_info = (VvasOverlayArrowParams *) head->data;
      gray_val = (arrow_info->line_color.red +
          arrow_info->line_color.green + arrow_info->line_color.blue) / 3;

      thickness = arrow_info->thickness;
      switch (arrow_info->arrow_direction) {
        case ARROW_DIRECTION_START:{
          arrowedLine (img, Point (arrow_info->end_pt.x,
                  arrow_info->end_pt.y), Point (arrow_info->start_pt.x,
                  arrow_info->start_pt.y), Scalar (gray_val),
              thickness, 1, 0, arrow_info->tipLength);
        }
          break;
        case ARROW_DIRECTION_END:{
          arrowedLine (img, Point (arrow_info->start_pt.x,
                  arrow_info->start_pt.y), Point (arrow_info->end_pt.x,
                  arrow_info->end_pt.y), Scalar (gray_val),
              thickness, 1, 0, arrow_info->tipLength);
        }
          break;
        case ARROW_DIRECTION_BOTH_ENDS:{
          if (arrow_info->end_pt.x >= arrow_info->start_pt.x) {
            mid_x = arrow_info->start_pt.x + (arrow_info->end_pt.x -
                arrow_info->start_pt.x) / 2;
          } else {
            mid_x = arrow_info->end_pt.x + (arrow_info->start_pt.x -
                arrow_info->end_pt.x) / 2;
          }

          if (arrow_info->end_pt.y >= arrow_info->start_pt.y) {
            mid_y = arrow_info->start_pt.y + (arrow_info->end_pt.y -
                arrow_info->start_pt.y) / 2;
          } else {
            mid_y = arrow_info->end_pt.y + (arrow_info->start_pt.y -
                arrow_info->end_pt.y) / 2;
          }

          arrowedLine (img, Point (mid_x, mid_y),
              Point (arrow_info->end_pt.x, arrow_info->end_pt.y),
              Scalar (gray_val), thickness, 1, 0, arrow_info->tipLength / 2);

          arrowedLine (img, Point (mid_x, mid_y),
              Point (arrow_info->start_pt.x, arrow_info->start_pt.y),
              Scalar (gray_val), thickness, 1, 0, arrow_info->tipLength / 2);
        }
          break;
        default:
          break;
      }
      head = head->next;
    }
  }
}

 /**
 *  @fn  static void vvas_overlay_gray_draw_circle(Mat &img, VvasOverlayFrameInfo *pFrameInfo) 
 *  @param [in] *img  - image container.
 *  @param [in] *pFrameInfo - contains complete overlay information.
 *  @return none  
 *  @brief   
 *  @details This funciton performs drawing on the given frame
 *
 */
static void
vvas_overlay_gray_draw_circle (Mat & img, VvasOverlayFrameInfo * pFrameInfo)
{

  if (NULL == pFrameInfo) {
    return;
  }

  uint32_t num_circles = pFrameInfo->shape_info.num_circles;
  uint32_t gray_val = 0;
  VvasList *head = NULL;

  //Drawing cicles
  if (num_circles) {
    VvasOverlayCircleParams *circle_info;
    head = pFrameInfo->shape_info.circle_params;

    while (head) {
      circle_info = (VvasOverlayCircleParams *) head->data;
      gray_val = (circle_info->circle_color.red +
          circle_info->circle_color.green + circle_info->circle_color.blue) / 3;
      circle (img, Point (circle_info->center_pt.x,
              circle_info->center_pt.y), circle_info->radius,
          Scalar (gray_val), circle_info->thickness, 1, 0);
      head = head->next;
    }
  }
}

/**
 *  @fn  static void vvas_overlay_gray_draw_polygon(Mat &img, VvasOverlayFrameInfo *pFrameInfo) 
 *  @param [in] *img  - image container.
 *  @param [in] *pFrameInfo - contains complete overlay information.
 *  @return none  
 *  @brief   
 *  @details This funciton performs drawing on the given frame
 *
 */
static void
vvas_overlay_gray_draw_polygon (Mat & img, VvasOverlayFrameInfo * pFrameInfo)
{

  if (NULL == pFrameInfo) {
    return;
  }

  uint32_t num_polys = pFrameInfo->shape_info.num_polys;
  uint32_t gray_val = 0;
  VvasList *head = NULL, *pt_head = NULL;

  //Drawing polygons
  if (num_polys) {
    VvasOverlayPolygonParams *poly_info;
    head = pFrameInfo->shape_info.polygn_params;

    std::vector < Point > poly_pts;
    const Point *pts;
    while (head) {
      poly_info = (VvasOverlayPolygonParams *) head->data;
      gray_val = (poly_info->poly_color.red +
          poly_info->poly_color.green + poly_info->poly_color.blue) / 3;

      poly_pts.clear ();
      pt_head = poly_info->poly_pts;
      VvasOverlayCoordinates *pt_info;
      while (pt_head) {
        pt_info = (VvasOverlayCoordinates *) pt_head->data;
        poly_pts.push_back (Point (pt_info->x, pt_info->y));
        pt_head = pt_head->next;
      }

      pts = (const Point *) Mat (poly_pts).data;
      polylines (img, &pts, &poly_info->num_pts, 1, true,
          Scalar (gray_val), poly_info->thickness, 1, 0);
      head = head->next;
    }
  }
}

/**
 *  @fn VvasReturnType vvas_overlay_gray_draw(VvasOverlayFrameInfo *pFrameInfo)
 *                                           VvasVideoFrameMapInfo *info)
 *  @param [in] *pFrameInfo  - OverlayFrameInformation.
 *  @param [in] *info  - VvasVideoFrameMapInfo addresss.
 *  @return On Success returns VVAS_RET_SUCCESS 
 *          On Failure returns VVAS_ERROR_*  
 *  @brief   
 *  @details This funciton performs drawing on the given frame
 *
 */
static VvasReturnType
vvas_overlay_gray_draw (VvasOverlayFrameInfo * pFrameInfo,
    VvasVideoFrameMapInfo * info)
{
  VvasReturnType ret = VVAS_RET_SUCCESS;

  if (NULL == pFrameInfo) {
    ret = VVAS_RET_INVALID_ARGS;
    return ret;
  }
  uint32_t img_height = info->height;
  uint32_t img_width = info->width;
  uint32_t stride = info->planes[0].stride;
  uint8_t *in_plane1 = info->planes[0].data;

  Mat img (img_height, img_width, CV_8UC1, in_plane1, stride);

  /* draw clock on the image */
  vvas_overlay_draw_gray_clock (img, &pFrameInfo->clk_info);

  /* draws rectangle pattern on the image */
  vvas_overlay_gray_draw_rect (img, pFrameInfo);

  /* draws text information on image */
  vvas_overlay_gray_draw_text (img, pFrameInfo);

  /* draws line pattern on image */
  vvas_overlay_gray_draw_line (img, pFrameInfo);

  /* draws arrow pattern on image */
  vvas_overlay_gray_draw_arrow (img, pFrameInfo);

  /* draws circle pattern on image */
  vvas_overlay_gray_draw_circle (img, pFrameInfo);

  /* draws polygon pattern on image */
  vvas_overlay_gray_draw_polygon (img, pFrameInfo);

  return ret;
}

/**
 *  @fn VvasReturnType vvas_overlay_process_frame(VvasOverlayFrameInfo *pFrameInfo)
 *  @param [in] *pFrameInfo  - OverlayFrameInformation.
 *  @return On Success returns VVAS_RET_SUCCESS 
 *          On Failure returns VVAS_ERROR_*  
 *  @brief   
 *  @details This funciton performs drawing on the given frame
 *
 */
VvasReturnType
vvas_overlay_process_frame (VvasOverlayFrameInfo * pFrameInfo)
{
  VvasReturnType ret = VVAS_RET_ERROR;
  /* Validate input params */
  if ((NULL == pFrameInfo) ||
      ((NULL != pFrameInfo) && (NULL == pFrameInfo->frame_info))) {
    LOG_E ("NULL Frame info received.");
    ret = VVAS_RET_INVALID_ARGS;
    return ret;
  }

  VvasVideoFrameMapInfo info;
  memset (&info, 0, sizeof (VvasVideoFrameMapInfo));
  ret = vvas_video_frame_map (pFrameInfo->frame_info,
      (VvasDataMapFlags) (VVAS_DATA_MAP_READ | VVAS_DATA_MAP_WRITE), &info);

  if (VVAS_RET_ERROR == ret) {
    LOG_E ("failed to map memory");
    return VVAS_RET_ERROR;
  }

  VvasVideoFormat ColorFormat = info.fmt;

  switch (ColorFormat) {

    case VVAS_VIDEO_FORMAT_RGB:
    case VVAS_VIDEO_FORMAT_BGR:{
      ret = vvas_overlay_rgb_draw (pFrameInfo, &info);
      if (ret != VVAS_RET_SUCCESS) {
        LOG_E ("failed to draw");
        return ret;
      }
    }
      break;

    case VVAS_VIDEO_FORMAT_Y_UV8_420:{
      ret = vvas_overlay_nv12_draw (pFrameInfo, &info);
      if (ret != VVAS_RET_SUCCESS) {
        LOG_E ("failed to draw");
        return ret;
      }
    }
      break;

    case VVAS_VIDEO_FORMAT_GRAY8:{
      ret = vvas_overlay_gray_draw (pFrameInfo, &info);
      if (ret != VVAS_RET_SUCCESS) {
        LOG_E ("failed to draw");
        return ret;
      }
    }
      break;

    default:{
      ret = VVAS_RET_INVALID_ARGS;
      return ret;
    }
      break;
  }


  /* frame with overlay drawings is not required to sync to device memory
   * Therefore unsetting flag to avoid sync to device.
   */
  vvas_video_frame_unset_sync_flag (pFrameInfo->frame_info,
      VVAS_DATA_SYNC_TO_DEVICE);

  ret = vvas_video_frame_unmap (pFrameInfo->frame_info, &info);
  if (VVAS_IS_ERROR (ret)) {
    LOG_E ("failed to map memory\n");
    return ret;
  }


  return ret;
}
