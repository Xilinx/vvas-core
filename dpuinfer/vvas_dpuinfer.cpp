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

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <fstream>

#include <vitis/ai/bounded_queue.hpp>
#include <vitis/ai/env_config.hpp>
#include <xir/graph/graph.hpp>
#include <google/protobuf/text_format.h>
#include <jansson.h>

#include <vvas_core/vvas_common.h>
#include <vvas_core/vvas_dpuinfer.hpp>
#include "vvas_dpumodels.hpp"
#include "vvas_dpupriv.hpp"

#ifdef ENABLE_CLASSIFICATION
#include "vvas_classification.hpp"
#endif
#ifdef ENABLE_VEHICLECLASSIFICATION
#include "vvas_vehicleclassification.hpp"
#endif
#ifdef ENABLE_YOLOV3
#include "vvas_yolov3.hpp"
#endif
#ifdef ENABLE_FACEDETECT
#include "vvas_facedetect.hpp"
#endif
#ifdef ENABLE_REID
#include "vvas_reid.hpp"
#endif
#ifdef ENABLE_SSD
#include "vvas_ssd.hpp"
#endif
#ifdef ENABLE_REFINEDET
#include "vvas_refinedet.hpp"
#endif
#ifdef ENABLE_TFSSD
#include "vvas_tfssd.hpp"
#endif
#ifdef ENABLE_YOLOV2
#include "vvas_yolov2.hpp"
#endif
#ifdef ENABLE_SEGMENTATION
#include "vvas_segmentation.hpp"
#endif
#ifdef ENABLE_PLATEDETECT
#include "vvas_platedetect.hpp"
#endif
#ifdef ENABLE_PLATENUM
#include "vvas_platenum.hpp"
#endif
#ifdef ENABLE_POSEDETECT
#include "vvas_posedetect.hpp"
#endif
#ifdef ENABLE_BCC
#include "vvas_bcc.hpp"
#endif
#ifdef ENABLE_EFFICIENTDETD2
#include "vvas_efficientdetd2.hpp"
#endif
#ifdef ENABLE_FACEFEATURE
#include "vvas_facefeature.hpp"
#endif
#ifdef ENABLE_FACELANDMARK
#include "vvas_facelandmark.hpp"
#endif
#ifdef ENABLE_ROADLINE
#include "vvas_roadline.hpp"
#endif
#ifdef ENABLE_ULTRAFAST
#include "vvas_ultrafast.hpp"
#endif
#ifdef ENABLE_RAWTENSOR
#include "vvas_rawtensor.hpp"
#endif

using namespace cv;
using namespace std;

static VvasMutex model_create_lock;
int
vvas_xclass_to_num (char *name)
{
  int nameslen = 0;
  while (vvas_xmodelclass[nameslen] != NULL) {
    if (!strcmp (vvas_xmodelclass[nameslen], name))
      return nameslen;
    nameslen++;
  }
  return VVAS_XCLASS_NOTFOUND;
}

vvas_dpumodel::~vvas_dpumodel ()
{
}

inline bool
fileexists (const string & name)
{
  struct stat buffer;
  return (stat (name.c_str (), &buffer) == 0);
}

static
std::vector < float >
get_means (const vitis::ai::proto::DpuKernelParam & c)
{
  return std::vector < float >(c.mean ().begin (), c.mean ().end ());
}

static
std::vector < float >
get_scales (const vitis::ai::proto::DpuKernelParam & c)
{
  return std::vector < float >(c.scale ().begin (), c.scale ().end ());
}

static
    std::vector < const
xir::Subgraph * >
get_dpu_subgraph (const xir::Graph * graph)
{
  auto root = graph->get_root_subgraph ();
  auto children = root->children_topological_sort ();
  auto ret = std::vector < const xir::Subgraph * >();
for (auto c:children) {
    auto device = c->get_attr < std::string > ("device");
    if (device == "DPU") {
      ret.emplace_back (c);
    }
  }
  return ret;
}

static float
get_innerscale_value (const std::string & model_name)
{
  auto graph = xir::Graph::deserialize (model_name);
  auto subgraphs = get_dpu_subgraph (graph.get ());
  auto intensors = subgraphs[0]->get_input_tensors ();
  float inner_scale =
      exp2f ((*intensors.begin ())->get_attr < int >("fix_point"));
  return inner_scale;
}

static
    std::string
slurp (const char *filename)
{
  std::ifstream in;
  in.open (filename, std::ifstream::in);
  CHECK (in.good ()) << "failed to read config file. filename=" << filename;
  std::stringstream sstr;
  sstr << in.rdbuf ();
  in.close ();
  return sstr.str ();
}

static bool
parse_prototxt_file (VvasDpuInferPrivate * kpriv,
    const std::string & prototxt_file)
{
  vitis::ai::proto::DpuModelParamList mlist;
  bool ret = true;
  auto text = slurp (prototxt_file.c_str ());
  auto ok = google::protobuf::TextFormat::ParseFromString (text, &mlist);
  CHECK (ok) << "cannot parse config file. prototxt_file=" << prototxt_file;
  CHECK_EQ (mlist.model_size (), 1)
      << "only support one model per config file."
      << "prototxt_file " << prototxt_file << " "
      << "content: " << mlist.DebugString () << " ";
  auto & model = mlist.model (0);
  auto mean = get_means (model.kernel (0));
  auto scale = get_scales (model.kernel (0));

  switch (kpriv->modelfmt) {
    /** prototxt file has mean/scale values always in BGR format
     *  irrespective of the model required input format.
     */
    case VVAS_VIDEO_FORMAT_RGB:
    case VVAS_VIDEO_FORMAT_BGR:{
      kpriv->pp_config.mean_r = mean[2];
      kpriv->pp_config.mean_g = mean[1];
      kpriv->pp_config.mean_b = mean[0];
      kpriv->pp_config.scale_r = scale[2];
      kpriv->pp_config.scale_g = scale[1];
      kpriv->pp_config.scale_b = scale[0];

    }
      break;

    default:
    case VVAS_VIDEO_FORMAT_GRAY8:{
      LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
          "This format is not supported");
      ret = false;
    }
      break;
  }
  return ret;
}

static
    std::string
modelexists (VvasDpuInferPrivate * kpriv)
{
  auto elf_name =
      kpriv->modelpath + "/" + kpriv->modelname + "/" + kpriv->modelname +
      ".elf";
  auto xmodel_name =
      kpriv->modelpath + "/" + kpriv->modelname + "/" + kpriv->modelname +
      ".xmodel";
  auto prototxt_name =
      kpriv->modelpath + "/" + kpriv->modelname + "/" + kpriv->modelname +
      ".prototxt";

  if (kpriv->modelclass != VVAS_XCLASS_RAWTENSOR && !fileexists (prototxt_name)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level, "%s not found",
        prototxt_name.c_str ());
    elf_name = "";
    return elf_name;
  }

  if (!kpriv->need_preprocess) {
    parse_prototxt_file (kpriv, prototxt_name);
  }
  if (fileexists (xmodel_name))
    return xmodel_name;
  else if (fileexists (elf_name))
    return elf_name;
  else {
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
        "xmodel or elf file not found");
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level, "%s", elf_name.c_str ());
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level, "%s", xmodel_name.c_str ());
    elf_name = "";
  }

  return elf_name;
}

labels *
readlabel (VvasDpuInferPrivate * kpriv, char *json_file)
{
  json_t *root = NULL, *karray, *label, *value;
  json_error_t error;
  unsigned int num_labels;
  labels *labelptr = NULL;

  /* get root json object */
  root = json_load_file (json_file, JSON_DECODE_ANY, &error);
  if (!root) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
        "failed to load json file(%s) reason %s", json_file, error.text);
    return NULL;
  }

  value = json_object_get (root, "model-name");
  if (json_is_string (value)) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "label is for model %s",
        (char *) json_string_value (value));
  }

  value = json_object_get (root, "num-labels");
  if (!value || !json_is_integer (value)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
        "num-labels not found in %s", json_file);
    goto error;
  } else {
    num_labels = json_integer_value (value);
    labelptr = new labels[num_labels * sizeof (labels)];
    kpriv->max_labels = num_labels;
  }

  /* get kernels array */
  karray = json_object_get (root, "labels");
  if (!karray) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
        "failed to find key labels");
    goto error;
  }

  if (!json_is_array (karray)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
        "labels key is not of array type");
    goto error;
  }

  if (num_labels != json_array_size (karray)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
        "number of labels(%u) != karray size(%lu)\n", num_labels,
        json_array_size (karray));
    goto error;
  }

  for (unsigned int index = 0; index < num_labels; index++) {
    label = json_array_get (karray, index);
    if (!label) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
          "failed to get label object");
      goto error;
    }
    value = json_object_get (label, "label");
    if (!value || !json_is_integer (value)) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
          "label num found for array %d", index);
      goto error;
    }

    /*label is index of array */
    LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "label %d",
        (int) json_integer_value (value));
    labels *lptr = labelptr + (int) json_integer_value (value);
    lptr->label = (int) json_integer_value (value);

    value = json_object_get (label, "name");
    if (!json_is_string (value)) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
          "name is not found for array %d", index);
      goto error;
    } else {
      lptr->name = (char *) json_string_value (value);
      LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "name %s",
          lptr->name.c_str ());
    }
    value = json_object_get (label, "display_name");
    if (!json_is_string (value)) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
          "display name is not found for array %d", index);
      goto error;
    } else {
      lptr->display_name = (char *) json_string_value (value);
      LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "display_name %s",
          lptr->display_name.c_str ());
    }

  }
  kpriv->num_labels = num_labels;
  json_decref (root);
  return labelptr;
error:
  if (labelptr)
    delete[]labelptr;
  json_decref (root);
  return NULL;
}

long long
get_time ()
{
  struct timeval tv;
  gettimeofday (&tv, NULL);
  return ((long long) tv.tv_sec * 1000000 + tv.tv_usec) +
      42 * 60 * 60 * INT64_C (1000000);
}

vvas_dpumodel *
vvas_xinitmodel (VvasDpuInferPrivate * kpriv, int modelclass)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter");
  vvas_dpumodel *model = NULL;
  kpriv->labelptr = NULL;
  kpriv->labelflags = VVAS_XLABEL_NOT_REQUIRED;

  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "Creating model %s",
      kpriv->modelname.c_str ());

  const auto labelfile =
      kpriv->modelpath + "/" + kpriv->modelname + "/" + "label.json";
  if (fileexists (labelfile)) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level,
        "Label file %s found\n", labelfile.c_str ());
    kpriv->labelptr = readlabel (kpriv, (char *) labelfile.c_str ());
  }

  /*
   * Vitis AI does not support concurrent model creation & destroy so we need to 
   * synchronize.
   * Note: For multi process applications, synchronization of Vitis AI model create
   * & destroy should be handled by application.
   */
  vvas_mutex_lock (&model_create_lock);
  switch (modelclass) {
#ifdef ENABLE_CLASSIFICATION
    case VVAS_XCLASS_CLASSIFICATION:
    {
      model =
          new vvas_classification (kpriv, kpriv->elfname,
          kpriv->need_preprocess);
      break;
    }
#endif
#ifdef ENABLE_VEHICLECLASSIFICATION
    case VVAS_XCLASS_VEHICLECLASSIFICATION:
    {
      model =
          new vvas_vehicleclassification (kpriv, kpriv->elfname,
          kpriv->need_preprocess);
      break;
    }
#endif

#ifdef ENABLE_YOLOV3
    case VVAS_XCLASS_YOLOV3:
    {
      model = new vvas_yolov3 (kpriv, kpriv->elfname, kpriv->need_preprocess);
      break;
    }
#endif
#ifdef ENABLE_FACEDETECT
    case VVAS_XCLASS_FACEDETECT:
    {
      model =
          new vvas_facedetect (kpriv, kpriv->elfname, kpriv->need_preprocess);
      break;
    }
#endif
#ifdef ENABLE_REID
    case VVAS_XCLASS_REID:
    {
      model = new vvas_reid (kpriv, kpriv->elfname, kpriv->need_preprocess);
      break;
    }
#endif
#ifdef ENABLE_SSD
    case VVAS_XCLASS_SSD:
    {
      model = new vvas_ssd (kpriv, kpriv->elfname, kpriv->need_preprocess);
      break;
    }
#endif
#ifdef ENABLE_REFINEDET
    case VVAS_XCLASS_REFINEDET:
    {
      model =
          new vvas_refinedet (kpriv, kpriv->elfname, kpriv->need_preprocess);
      break;
    }
#endif
#ifdef ENABLE_TFSSD
    case VVAS_XCLASS_TFSSD:
    {
      model = new vvas_tfssd (kpriv, kpriv->elfname, kpriv->need_preprocess);
      break;
    }
#endif
#ifdef ENABLE_YOLOV2
    case VVAS_XCLASS_YOLOV2:
    {
      model = new vvas_yolov2 (kpriv, kpriv->elfname, kpriv->need_preprocess);
      break;
    }
#endif
#ifdef ENABLE_SEGMENTATION
    case VVAS_XCLASS_SEGMENTATION:
    {
      model =
          new vvas_segmentation (kpriv, kpriv->elfname, kpriv->need_preprocess);
      break;
    }
#endif
#ifdef ENABLE_PLATEDETECT
    case VVAS_XCLASS_PLATEDETECT:
    {
      model =
          new vvas_platedetect (kpriv, kpriv->elfname, kpriv->need_preprocess);
      break;
    }
#endif
#ifdef ENABLE_PLATENUM
    case VVAS_XCLASS_PLATENUM:
    {
      model = new vvas_platenum (kpriv, kpriv->elfname, kpriv->need_preprocess);
      break;
    }
#endif
#ifdef ENABLE_POSEDETECT
    case VVAS_XCLASS_POSEDETECT:
    {
      model =
          new vvas_posedetect (kpriv, kpriv->elfname, kpriv->need_preprocess);
      break;
    }
#endif
#ifdef ENABLE_BCC
    case VVAS_XCLASS_BCC:
    {
      model = new vvas_bcc (kpriv, kpriv->elfname, kpriv->need_preprocess);
      break;
    }
#endif
#ifdef ENABLE_EFFICIENTDETD2
    case VVAS_XCLASS_EFFICIENTDETD2:
    {
      model =
          new vvas_efficientdetd2 (kpriv, kpriv->elfname,
          kpriv->need_preprocess);
      break;
    }
#endif
#ifdef ENABLE_FACEFEATURE
    case VVAS_XCLASS_FACEFEATURE:
    {
      model =
          new vvas_facefeature (kpriv, kpriv->elfname, kpriv->need_preprocess);
      break;
    }
#endif
#ifdef ENABLE_FACELANDMARK
    case VVAS_XCLASS_FACELANDMARK:
    {
      model =
          new vvas_facelandmark (kpriv, kpriv->elfname, kpriv->need_preprocess);
      break;
    }
#endif
#ifdef ENABLE_ROADLINE
    case VVAS_XCLASS_ROADLINE:
    {
      model = new vvas_roadline (kpriv, kpriv->elfname, kpriv->need_preprocess);
      break;
    }
#endif
#ifdef ENABLE_ULTRAFAST
    case VVAS_XCLASS_ULTRAFAST:
    {
      model =
          new vvas_ultrafast (kpriv, kpriv->elfname, kpriv->need_preprocess);
      break;
    }
#endif
#ifdef ENABLE_RAWTENSOR
    case VVAS_XCLASS_RAWTENSOR:
    {
      model = new vvas_rawtensor (kpriv, kpriv->elfname);
      break;
    }
#endif

    default:
    {
      /* Unlock before return */
      vvas_mutex_unlock (&model_create_lock);
      return NULL;
    }
  }
  vvas_mutex_unlock (&model_create_lock);

  if ((kpriv->labelflags & VVAS_XLABEL_REQUIRED)
      && (kpriv->labelflags & VVAS_XLABEL_NOT_FOUND)) {
    model->close ();
    delete model;
    kpriv->modelclass = VVAS_XCLASS_NOTFOUND;
    if (kpriv->labelptr != NULL)
      delete kpriv->labelptr;
    return NULL;
  }

  long int model_batch_size = model->supportedbatchsz ();
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level,
      "model supported batch size (%ld)"
      "batch size set by user (%d)", model_batch_size, kpriv->batch_size);

  kpriv->batch_size = model_batch_size;

  return model;
}

int
prepare_filter_labels (VvasDpuInferPrivate * kpriv, char **filter_labels,
    int num)
{
  for (int i = 0; i < num; i++) {
    if (!filter_labels[i]) {
      return -1;
    }
    kpriv->filter_labels.push_back (std::string (filter_labels[i]));
  }
  return 0;
}

/**
 *  @fn VvasDpuInfer * vvas_dpuinfer_create (VvasDpuInferConf * dpu_conf, VvasLogLevel log_level)
 *
 *  @param [in] dpu_conf @ref VvasDpuInferConf structure.
 *  @param [in] log_level @ref VvasLogLevel enum.
 *  @return On Success returns @ref VvasDpuInfer handle. \n
 *          On Failure returns NULL.
 *  @brief  Upon success initializes DPU with config parameters and allocates DpuInfer instance.
 *  @note   This instance must be freed using @ref vvas_dpuinfer_destroy
 */
VvasDpuInfer *
vvas_dpuinfer_create (VvasDpuInferConf * dpu_conf, VvasLogLevel log_level)
{
  VvasDpuInferPrivate *kpriv = NULL;

  kpriv = new VvasDpuInferPrivate;
  if (!kpriv) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, log_level, "Failed to allocate memory");
    return NULL;
  }

  kpriv->model = NULL;
  kpriv->labelptr = NULL;
  kpriv->log_level = log_level;
  kpriv->need_preprocess = dpu_conf->need_preprocess;
  kpriv->batch_size = dpu_conf->batch_size;
  kpriv->modelfmt = dpu_conf->model_format;
  if (kpriv->modelfmt == VVAS_VIDEO_FORMAT_UNKNOWN) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
        "SORRY NOT SUPPORTED MODEL FORMAT %d", dpu_conf->model_format);
    goto error;
  }
  kpriv->modelpath = dpu_conf->model_path;
  if (!fileexists (kpriv->modelpath)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
        "Model path %s does not exist", kpriv->modelpath.c_str ());
    goto error;
  }
  kpriv->modelclass = vvas_xclass_to_num (dpu_conf->modelclass);
  if (kpriv->modelclass == VVAS_XCLASS_SEGMENTATION) {
    kpriv->segoutfactor = dpu_conf->segoutfactor;
    kpriv->segoutfmt = dpu_conf->segoutfmt;
    if (kpriv->segoutfmt == VVAS_VIDEO_FORMAT_UNKNOWN ||
        !(kpriv->segoutfmt == VVAS_VIDEO_FORMAT_BGR ||
            kpriv->segoutfmt == VVAS_VIDEO_FORMAT_GRAY8)) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
          "SORRY NOT SUPPORTED SEGMENTATION OUTPUT FORMAT %d",
          dpu_conf->segoutfmt);
      goto error;
    }
  } else if (kpriv->modelclass == VVAS_XCLASS_RAWTENSOR
      && kpriv->need_preprocess) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
        "Please provide preprocessed data for rawtensor output");
    goto error;
  } else if (kpriv->modelclass == VVAS_XCLASS_NOTFOUND) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
        "Model Class %s not found", dpu_conf->modelclass);
    goto error;
  }

  kpriv->modelname = dpu_conf->model_name;
  kpriv->elfname = modelexists (kpriv);
  if (kpriv->elfname.empty ()) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
        "elfname %s does not exist", kpriv->elfname.c_str ());
    goto error;
  }
  kpriv->objs_detection_max = dpu_conf->objs_detection_max;
  kpriv->performance_test = dpu_conf->performance_test;
  kpriv->float_feature = dpu_conf->float_feature;

  kpriv->model = vvas_xinitmodel (kpriv, kpriv->modelclass);
  if (!kpriv->model) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
        "failed to init model %s", kpriv->modelname.c_str ());
    goto error;
  }
  kpriv->model_width = kpriv->model->requiredwidth ();
  kpriv->model_height = kpriv->model->requiredheight ();

  if (kpriv->need_preprocess) {
    /** No need of PP from VVAS */
    kpriv->pp_config.mean_r = 0;
    kpriv->pp_config.mean_g = 0;
    kpriv->pp_config.mean_b = 0;
    kpriv->pp_config.scale_r = 1;
    kpriv->pp_config.scale_g = 1;
    kpriv->pp_config.scale_b = 1;
  }

  if (dpu_conf->num_filter_labels) {
    if (prepare_filter_labels (kpriv, dpu_conf->filter_labels,
            dpu_conf->num_filter_labels) < 0) {
      goto error;
    }
  }

  return (VvasDpuInfer *) kpriv;

error:
  if (kpriv->model)
    kpriv->model->close ();
  if (kpriv->labelptr)
    delete[]kpriv->labelptr;
  delete kpriv;
  return NULL;
}

/**
 *  @fn VvasReturnType vvas_dpuinfer_process_frames (VvasDpuInfer * dpu_handle, VvasVideoFrame *input[MAX_NUM_OBJECT], VvasInferPrediction *prediction[MAX_NUM_OBJECT], int batch_size)
 *
 *  @param [in] dpu_handle     VvasDpuInfer handle created using @vvas_dpuinfer_create
 *  @param [in] inputs         Array of @ref VvasVideoFrame
 *  @param [in,out] predictions         Array of @ref VvasInferPrediction
 *  @param [in] batch_size     Batch size.
 *  @return VvasReturnType
 *  @details Upon success initializes DPU with config parameters.
 *  @brief   This API returns VvasInferPrediction to each frame.
 *  @note    It is user's responsibility to free the VvasInferPrediction of each frame.
 */
VvasReturnType
vvas_dpuinfer_process_frames (VvasDpuInfer * dpu_handle,
    VvasVideoFrame * inputs[MAX_NUM_OBJECT],
    VvasInferPrediction * predictions[MAX_NUM_OBJECT], int batch_size)
{
  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *) dpu_handle;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter");
  std::vector < cv::Mat > images;
  vvas_perf *pf = &kpriv->pf;
  VvasReturnType vret;

  vvas_dpumodel *model = (vvas_dpumodel *) kpriv->model;
  VvasVideoFrame *cur_frame = NULL;
  VvasVideoFrameMapInfo vframe_info;

  if (batch_size > kpriv->batch_size) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
        "received more frames than batch size (%d) of the DPU",
        kpriv->batch_size);
    return VVAS_RET_ERROR;
  }

  for (auto i = 0; i < batch_size; i++) {
    cur_frame = inputs[i];
    if (!cur_frame) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
          "Input Frame %d is NULL", i + 1);
      return VVAS_RET_ERROR;
    }

    vret = vvas_video_frame_map (cur_frame, VVAS_DATA_MAP_READ, &vframe_info);
    if (vret != VVAS_RET_SUCCESS) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
          "Failed to map video frame");
      return vret;
    }

    if (vframe_info.fmt != kpriv->modelfmt) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
          "Video frame format %d not supported", vframe_info.fmt);
      return VVAS_RET_ERROR;
    }

    if (kpriv->model_width != vframe_info.width
        || kpriv->model_height != vframe_info.height) {
      LOG_MESSAGE (LOG_LEVEL_WARNING, kpriv->log_level,
          "Input height/width not match with model" "requirement");
      LOG_MESSAGE (LOG_LEVEL_WARNING, kpriv->log_level,
          "model required wxh is %dx%d", kpriv->model_width,
          kpriv->model_height);
      LOG_MESSAGE (LOG_LEVEL_WARNING, kpriv->log_level,
          "input image wxh is %dx%d", vframe_info.width, vframe_info.height);
    }


    uchar *data_ptr = (uchar *) vframe_info.planes[0].data;
    cv::Mat image (vframe_info.height, vframe_info.width, CV_8UC3,
        (void *) (data_ptr), vframe_info.planes[0].stride);

    vvas_video_frame_unmap (cur_frame, &vframe_info);
    images.push_back (image);
    LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "pushed Mat image %d",
        i + 1);
  }

  if (kpriv->performance_test && !kpriv->pf.test_started) {
    pf->timer_start = get_time ();
    pf->last_displayed_time = pf->timer_start;
    pf->test_started = 1;
  }

  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "Processing frame");
  if (model->run (kpriv, images, predictions) != true) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level, "Model run failed %s",
        kpriv->modelname.c_str ());
    return VVAS_RET_ERROR;
  }

  if (kpriv->performance_test && kpriv->pf.test_started) {
    pf->frames += batch_size;
    if (get_time () - pf->last_displayed_time >= 1000000.0) {
      long long current_time = get_time ();
      double time = (current_time - pf->last_displayed_time) / 1000000.0;
      pf->last_displayed_time = current_time;
      double fps =
          (time >
          0.0) ? ((pf->frames - pf->last_displayed_frame) / time) : 999.99;
      pf->last_displayed_frame = pf->frames;
      printf ("\rframe=%5lu fps=%6.*f        \r", pf->frames,
          (fps < 9.995) ? 3 : 2, fps);
      fflush (stdout);
    }
  }

  return VVAS_RET_SUCCESS;

}

/**
 *  @fn VvasReturnType vvas_dpuinfer_get_config (VvasDpuInfer * dpu_handle, VvasModelConf *model_conf)
 *
 *  @param [in] dpu_handle VvasDpuInfer handle created using @ref vvas_dpuinfer_create.
 *  @param [in,out] model_conf @ref VvasModelConf structure
 *  @return VvasReturnType
 *  @brief  Returns the VvasModelConf structure with all fields populated.
 *  @note It is user's responsibility to allocate memory to this structure
 */
VvasReturnType
vvas_dpuinfer_get_config (VvasDpuInfer * dpu_handle, VvasModelConf * model_conf)
{

  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *) dpu_handle;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter");
  if (model_conf) {
    if (!kpriv->need_preprocess) {
      float inner_scale_factor = get_innerscale_value (kpriv->elfname);
      LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "inner scale %f",
          inner_scale_factor);
      kpriv->pp_config.scale_r *= inner_scale_factor;
      kpriv->pp_config.scale_g *= inner_scale_factor;
      kpriv->pp_config.scale_b *= inner_scale_factor;
    }

    model_conf->model_width = kpriv->model_width;
    model_conf->model_height = kpriv->model_height;
    model_conf->batch_size = kpriv->batch_size;
    model_conf->mean_r = kpriv->pp_config.mean_r;
    model_conf->mean_g = kpriv->pp_config.mean_g;
    model_conf->mean_b = kpriv->pp_config.mean_b;
    model_conf->scale_r = kpriv->pp_config.scale_r;
    model_conf->scale_g = kpriv->pp_config.scale_g;
    model_conf->scale_b = kpriv->pp_config.scale_b;
  } else {
    return VVAS_RET_ERROR;
  }

  return VVAS_RET_SUCCESS;
}

/**
 *  @fn VvasReturnType vvas_dpuinfer_destroy (VvasDpuInfer * dpu_handle)
 *
 *  @param [in] dpu_handle VvasDpuInfer handle created using @ref vvas_dpuinfer_create.
 *  @return VvasReturnType
 *  @brief  De-initialises the model and free all other resources allocated
 */
VvasReturnType
vvas_dpuinfer_destroy (VvasDpuInfer * dpu_handle)
{
  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *) dpu_handle;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter");
  vvas_perf *pf = &kpriv->pf;

  if (kpriv->performance_test && kpriv->pf.test_started) {
    double time = (get_time () - pf->timer_start) / 1000000.0;
    double fps = (time > 0.0) ? (pf->frames / time) : 999.99;
    printf ("\rframe=%5lu fps=%6.*f        \n", pf->frames,
        (fps < 9.995) ? 3 : 2, fps);
  }
  pf->test_started = 0;
  pf->frames = 0;
  pf->last_displayed_frame = 0;
  pf->timer_start = 0;
  pf->last_displayed_time = 0;

  kpriv->modelclass = VVAS_XCLASS_NOTFOUND;

  /*
   * Vitis AI model destroy is not concurrent, serializing destructor call.
   */
  vvas_mutex_lock (&model_create_lock);
  if (kpriv->model != NULL) {
    kpriv->model->close ();
    delete kpriv->model;
    kpriv->model = NULL;
  }
  vvas_mutex_unlock (&model_create_lock);

  if (kpriv->labelptr)
    delete[]kpriv->labelptr;

  delete kpriv;
  return VVAS_RET_SUCCESS;
}
