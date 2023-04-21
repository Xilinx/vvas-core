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

#include <sys/stat.h>
#include <iomanip>
#include <numeric>
#include <google/protobuf/text_format.h>
#include <jansson.h>

#include <vitis/ai/facedetect.hpp>
#include <vitis/ai/nnpp/facedetect.hpp>
#include <vitis/ai/yolov3.hpp>
#include <vitis/ai/nnpp/yolov3.hpp>

#include <vvas_core/vvas_postprocessor.hpp>

using namespace vitis::ai;
using namespace cv;
using namespace std;

inline bool
fileexists (const string & name)
{
  struct stat buffer;
  return (stat (name.c_str (), &buffer) == 0);
}

static std::string
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

static vitis::ai::proto::DpuModelParam
get_config (const std::string & config_file, xir::Attrs * attrs)
{
  auto text = slurp (config_file.c_str ());
  vitis::ai::proto::DpuModelParamList mlist;
  auto ok = google::protobuf::TextFormat::ParseFromString (text, &mlist);
  CHECK (ok) << "cannot parse config file. config_file=" << config_file;
  CHECK_EQ (mlist.model_size (), 1)
      << "only support one model per config file." << "config_file " << config_file << " "      //
      << "content: " << mlist.DebugString () << " "     //
      ;
  const auto &model = mlist.model (0);
  if (model.use_graph_runner ()) {
    attrs->set_attr ("use_graph_runner", true);
  }

  vitis::ai::proto::DpuModelParam temp = mlist.model (0);
  return temp;
}

/* Add tensor format argument (NHWC / NCHW) */
static vitis::ai::library::OutputTensor
convert_tensor_buffer_to_output_tensor (vart::TensorBuffer * tb,
    vart::Runner::TensorFormat fmt, int8_t scale)
{
  auto ret = vitis::ai::library::OutputTensor { };
  auto tensor = tb->get_tensor ();
  auto dim_num = tensor->get_shape ().size ();
  auto batch = dim_num <= 0 ? 1 : tensor->get_shape ().at (0);
  ret.batch = batch;

  ret.size = tensor->get_element_num () *
      std::ceil (tensor->get_data_type ().bit_width / 8.f);
  /* Store the params as per format */
  if (fmt == vart::Runner::TensorFormat::NHWC) {
    if (dim_num == 2) {
      ret.height = 1;
      ret.width = 1;
      ret.channel = tensor->get_shape ().at (1);
    } else {
      ret.height = dim_num <= 1 ? 1 : tensor->get_shape ().at (1);
      ret.width = dim_num <= 2 ? 1 : tensor->get_shape ().at (2);
      ret.channel = dim_num <= 3 ? 1 : tensor->get_shape ().at (3);
    }
    if (tensor->get_data_type ().type == xir::DataType::XINT) {
      ret.dtype = library::DT_INT8;
      ret.fixpos = tensor->template get_attr < int >("fix_point");
    } else if (tensor->get_data_type ().type == xir::DataType::FLOAT) {
      ret.fixpos = 0;
      ret.dtype = library::DT_FLOAT;
    } else {
      LOG (FATAL) << "unsupported";
    }
  } else {
#ifdef ENABLE_DPUCADX8G_RUNNER
    /* DPUV1 has datatype float */
    ret.size = tensor->get_element_num () *
        std::ceil (tensor->get_data_type ().bit_width / 32.f);
    ret.fixpos = 0;
#else
    ret.fixpos = -(int8_t) log2f (scale);
#endif
    ret.height = dim_num <= 2 ? 1 : tensor->get_shape ().at (2);
    ret.width = dim_num <= 3 ? 1 : tensor->get_shape ().at (3);
    ret.channel = dim_num <= 1 ? 1 : tensor->get_shape ().at (1);
    ret.dtype = library::DT_FLOAT;
  }
  ret.name = tensor->get_name ();
  auto dims = tensor->get_shape ();
  auto size = 0ul;
  auto tb_ext = dynamic_cast < vart::TensorBufferExt * >(tb);
  for (auto batch_idx = 0; batch_idx < dims[0]; ++batch_idx) {
    auto idx = std::vector < int32_t > (dims.size ());
    idx[0] = batch_idx;
    auto data = tb->data (idx);
    ret.get_data (batch_idx) = (void *) data.first;
    size = data.second;

    CHECK_GE (size, ret.height * ret.width * ret.channel);
    ret.xcl_bo[batch_idx] = vitis::ai::library::XclBoInfo {
    0, nullptr, 0u};
    if (tb_ext) {
      auto bo = tb_ext->get_xcl_bo (batch_idx);
      ret.xcl_bo[batch_idx].xcl_handle = bo.xcl_handle;
      ret.xcl_bo[batch_idx].bo_handle = bo.bo_handle;
      ret.xcl_bo[batch_idx].offset =
          (unsigned int) tensor->template get_attr < int >("ddr_addr");
    }
  }
  return ret;
}

void
tensor_buffer_datatype_transform (vart::TensorBuffer * tb_from,
    vart::TensorBuffer * tb_to, float scale)
{
  auto tensor_from = tb_from->get_tensor ();
  auto tensor_to = tb_to->get_tensor ();
  auto from_batch_size = tensor_from->get_shape ()[0];
  auto to_batch_size = tensor_to->get_shape ()[0];
  size_t batch_size = std::min (from_batch_size, to_batch_size);
  std::int32_t from_dim_num = tensor_from->get_shape ().size ();
  auto to_dim_num = tensor_to->get_shape ().size ();
  CHECK_EQ (from_dim_num, to_dim_num);
  for (auto i = 1; i < from_dim_num; ++i) {
    CHECK_EQ (tensor_from->get_shape ().at (i), tensor_to->get_shape ().at (i))
        << "dim size is not same at dim " << i;
  }
  auto dim = std::vector < int32_t > (from_dim_num);
  auto view_from = std::pair < uint64_t, size_t > (0u, 0u);
  auto view_to = std::pair < uint64_t, size_t > (0u, 0u);
  auto from_data_type = tensor_from->get_data_type ().type;
  auto to_data_type = tensor_to->get_data_type ().type;
  size_t size_from = tensor_from->get_element_num () / from_batch_size;
  size_t size_to = tensor_to->get_element_num () / to_batch_size;
  CHECK_EQ (size_from, size_to) << "element numbers is not same";
  for (auto batch = 0u; batch < batch_size; ++batch) {
    dim[0] = (int) batch;
    view_from = tb_from->data (dim);
    view_to = tb_to->data (dim);
    for (auto i = 0u; i < size_from; ++i) {
      if (from_data_type == xir::DataType::FLOAT &&
          to_data_type == xir::DataType::XINT) {
        auto from_value = ((float *) view_from.first)[i];
        auto to_value = (int8_t) (from_value * scale);
        if (i < 3)
          printf ("frm %f to %d scale = %f\n", from_value, to_value, scale);
        ((int8_t *) view_to.first)[i] = to_value;
      } else if (from_data_type == xir::DataType::XINT &&
          to_data_type == xir::DataType::FLOAT) {
        auto from_value = ((int8_t *) view_from.first)[i];
        auto to_value = ((float) from_value) * scale;
        ((float *) view_to.first)[i] = to_value;
      } else {
        LOG (FATAL) << "unsupported data type conversion: from "
            << (int) from_data_type << " to " << (int) to_data_type;
      }
    }
  }
}

static int
get_fix_point (const xir::Tensor * tensor)
{
  CHECK (tensor->has_attr ("fix_point"))
      << "get tensor fix_point error! has no fix_point attr, tensor name is "
      << tensor->get_name ();

  return tensor->template get_attr < int >("fix_point");
}

const std::vector < std::string >
charactor_0 = {
  "unknown", "jing", "hu", "jin", "yu", "ji", "jin", "meng", "liao", "ji",
  "hei", "su", "zhe", "wan", "min", "gan",
  "lu", "yu", "e", "xiang", "yue", "gui", "qiong", "chuan", "gui", "yun",
  "zang", "shan", "gan", "qing", "ning", "xin"
};

const std::vector < std::string >
charactor_1 = {
  "unknown", "A", "B", "C", "D", "E", "F", "G", "H", "J", "K", "L",
  "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"
};

const std::vector < std::string >
charactor_2 = {
  "unknown", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B",
  "C", "D", "E",
  "F", "G", "H", "J", "K", "L", "M", "N", "P", "Q", "R", "S", "T", "U",
  "V", "W", "X", "Y", "Z"
};

const std::vector < std::string >
color = { "Blue", "Yellow" };

static std::vector < std::int32_t >
get_index_zeros (const xir::Tensor * tensor)
{
  auto ret = tensor->get_shape ();
  std::fill (ret.begin (), ret.end (), 0);
  return ret;
}

static
std::vector < std::pair < int, float >>
topk (void *data1, size_t size, int K)
{
  const float *score = (const float *) data1;
  auto indices = std::vector < int >(size);
  std::iota (indices.begin (), indices.end (), 0);
  std::partial_sort (indices.begin (), indices.begin () + K, indices.end (),
      [&score] (int a, int b) {
      return score[a] > score[b];}
  );
  auto ret = std::vector < std::pair < int, float >>(K);
  std::transform (indices.begin (), indices.begin () + K, ret.begin (),
      [&score] (int index) {
      return std::make_pair (index, score[index]);}
  );
  return ret;
}

static size_t
find_tensor_index (const char *tensor_name,
    const std::vector < vart::TensorBuffer * >&outputs)
{
  auto it = std::find_if (outputs.begin (), outputs.end (),
      [&tensor_name] (const vart::TensorBuffer * tb) {
        return tb->get_tensor ()->get_name () == tensor_name;
      }
  );
  CHECK (it !=
      outputs.end ()) << "cannot find tensorbuffer. tensor_name=" <<
      tensor_name;
  return it - outputs.begin ();
}

static void
postprocess_platenum (const std::vector <
    vart::TensorBuffer * >&output_tensor_buffers,
    VvasInferPrediction ** dst, int log_level, char *model_name)
{
  VvasInferPrediction *parent_predict = NULL;
  VvasBoundingBox child_bbox;
  VvasInferPrediction *child_predict;

  auto output_tensor = output_tensor_buffers[0]->get_tensor ();
  auto batch = output_tensor->get_shape ().at (0);
  auto size = output_tensor_buffers.size ();
  CHECK_EQ (size, 8) << "output_tensor_buffers.size() must be 8";
  for (auto i = 1u; i < size; ++i) {
    CHECK_EQ (output_tensor_buffers[i]->get_tensor ()->get_shape ().at (0),
        batch)
        << "all output_tensor_buffer batch number must be equal";
  }

  std::vector < std::pair < int, float >>ret;
  for (int batch_index = 0; batch_index < batch; ++batch_index) {
    for (auto tb_index = 0u; tb_index < size; ++tb_index) {
      uint64_t data_out = 0u;
      size_t size_out = 0u;
      auto idx =
          get_index_zeros (output_tensor_buffers[tb_index]->get_tensor ());
      idx[0] = (int) batch_index;
      std::tie (data_out, size_out) =
          output_tensor_buffers[tb_index]->data (idx);
      auto elem_num =
          output_tensor_buffers[tb_index]->get_tensor ()->get_element_num () /
          batch;
      auto tb_top1 = topk ((void *) data_out, elem_num, 1)[0];
      ret.push_back (tb_top1);
    }
  }

  for (int batch_index = 0; batch_index < batch; ++batch_index) {
    std::string plate_number = "";
    std::string plate_color = "";
    /* output_tensor_buffers maybe out of order, need find correct output_tensor_buffer result by tensor name */
    plate_number +=
        charactor_0[ret[batch_index * size + find_tensor_index ("prob1",
                output_tensor_buffers)].first];
    plate_number +=
        charactor_1[ret[batch_index * size + find_tensor_index ("prob2",
                output_tensor_buffers)].first];
    plate_number +=
        charactor_2[ret[batch_index * size + find_tensor_index ("prob3",
                output_tensor_buffers)].first];
    plate_number +=
        charactor_2[ret[batch_index * size + find_tensor_index ("prob4",
                output_tensor_buffers)].first];
    plate_number +=
        charactor_2[ret[batch_index * size + find_tensor_index ("prob5",
                output_tensor_buffers)].first];
    plate_number +=
        charactor_2[ret[batch_index * size + find_tensor_index ("prob6",
                output_tensor_buffers)].first];
    plate_number +=
        charactor_2[ret[batch_index * size + find_tensor_index ("prob7",
                output_tensor_buffers)].first];
    plate_color =
        color[ret[batch_index * size + find_tensor_index ("prob8",
                output_tensor_buffers)].first];

    if (log_level >= LOG_LEVEL_INFO) {
      LOG_MESSAGE (LOG_LEVEL_INFO, log_level,
          "RESULT: batch_index: %d plate_color: %s plate_number: %s",
          batch_index, plate_color.c_str (), plate_number.c_str ());
    }

    if (!parent_predict) {
      parent_predict = vvas_inferprediction_new ();
    }

    child_bbox.x = 0;
    child_bbox.y = 0;
    child_bbox.width = 0;
    child_bbox.height = 0;
    child_predict = vvas_inferprediction_new ();
    child_predict->bbox = child_bbox;

    VvasInferClassification *c = NULL;
    c = vvas_inferclassification_new ();
    c->class_id = -1;
    c->class_prob = 1;
    c->class_label = strdup (plate_number.c_str ());
    c->num_classes = 0;
    child_predict->classifications = vvas_list_append (child_predict->classifications, c);
    child_predict->model_class = VVAS_XCLASS_PLATENUM;
    child_predict->model_name = strdup(model_name);
    vvas_inferprediction_append (parent_predict, child_predict);
  }

  *dst = parent_predict;
}

static const char *
lookup (int index)
{
  static const char *table[] = {
#include "word_list.inc"
  };

  if (index < 0) {
    return "";
  } else {
    return table[index];
  }
};

static void
print_topk (const std::vector < std::pair < int, float >>&topk,
    VvasInferPrediction ** dst, int log_level, char *model_name)
{
  VvasInferPrediction *parent_predict = NULL;
  VvasBoundingBox child_bbox = { 0 };
  VvasInferPrediction *child_predict;

  for (const auto & v:topk) {

    if (log_level >= LOG_LEVEL_INFO)
      std::cout << std::setiosflags (std::ios::left) << std::setw (11)
          << "score[" + std::to_string (v.first) + "]"
          << " =  " << std::setw (12) << v.
          second << " text: " << lookup (v.first)
          << std::resetiosflags (std::ios::left) << std::endl;

    LOG_MESSAGE (LOG_LEVEL_INFO, log_level, "RESULT: %s", lookup (v.first));

    if (!parent_predict) {
      parent_predict = vvas_inferprediction_new ();
    }

    child_bbox.x = 0;
    child_bbox.y = 0;
    child_bbox.width = 0;
    child_bbox.height = 0;
    child_predict = vvas_inferprediction_new ();
    child_predict->bbox = child_bbox;

    VvasInferClassification *c = NULL;
    c = vvas_inferclassification_new ();
    c->class_id = v.first;
    c->class_prob = v.second;
    c->class_label = strdup (lookup (v.first));
    c->num_classes = 0;

    child_predict->classifications = vvas_list_append (child_predict->classifications, c);
    child_predict->model_class = VVAS_XCLASS_CLASSIFICATION;
    child_predict->model_name = strdup (model_name);
    vvas_inferprediction_append (parent_predict, child_predict);
  }
  *dst = parent_predict;
}

static void
postprocess_resnet_v1_50_tf (const std::vector <
    vart::TensorBuffer * >&output_tensor_buffers,
    VvasInferPrediction ** dst, int log_level, char *model_name)
{
  auto output_tensor = output_tensor_buffers[0]->get_tensor ();
  auto batch = output_tensor->get_shape ().at (0);
  auto size = output_tensor_buffers.size ();
  CHECK_EQ (size, 1) << "output_tensor_buffers.size() must be 1";
  for (int batch_index = 0; batch_index < batch; ++batch_index) {
    uint64_t data_out = 0u;
    size_t size_out = 0u;
    auto idx = get_index_zeros (output_tensor_buffers[0]->get_tensor ());
    idx[0] = (int) batch_index;
    std::tie (data_out, size_out) = output_tensor_buffers[0]->data (idx);
    auto elem_num =
        output_tensor_buffers[0]->get_tensor ()->get_element_num () / batch;
    auto tb_top1 = topk ((void *) data_out, elem_num, 1);
    if (log_level > LOG_LEVEL_INFO)
      std::cout << "batch_index: " << batch_index << std::endl;
    print_topk (tb_top1, dst, log_level, model_name);
  }
}

typedef struct {
  std::string modelpath;
  std::string modelname;
  std::string xmodel_name;
  std::string prototxt;
  std::vector <std::string> labels;
  std::unique_ptr <xir::Attrs> default_attrs_;
  VvasLogLevel log_level;
  int32_t fixpoint_node_flip;
} VvasPostProcessPriv;

static bool readlabels (VvasPostProcessPriv *kpriv, char *json_file)
{
  json_t *root = NULL, *karray, *label, *value;
  json_error_t error;
  unsigned int num_labels;

  /* get root json object */
  root = json_load_file (json_file, JSON_DECODE_ANY, &error);
  if (!root) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
        "failed to load json file(%s) reason %s", json_file, error.text);
    goto error;
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

    value = json_object_get (label, "name");
    if (!json_is_string (value)) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
          "name is not found for array %d", index);
      goto error;
    } else {
      LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "name %s",
          json_string_value (value));
    }
    value = json_object_get (label, "display_name");
    if (!json_is_string (value)) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
          "display name is not found for array %d", index);
      goto error;
    } else {
      kpriv->labels.push_back (std::string (json_string_value (value)));
      LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "display_name %s",
          json_string_value (value));
    }
  }

  if (root)
    json_decref (root);

  return TRUE;

error:
  if (root)
    json_decref (root);
  return FALSE;
}

VvasPostProcessor * vvas_postprocess_create (VvasPostProcessConf * postproc_conf, VvasLogLevel log_level)
{
  VvasPostProcessPriv *kpriv = new VvasPostProcessPriv;
  if (!kpriv) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, log_level,
        "Failed to allocate memory");
    return NULL;
  }

  LOG_MESSAGE (LOG_LEVEL_WARNING, log_level, "This is an example library to demonstrate post-processing");
  LOG_MESSAGE (LOG_LEVEL_WARNING, log_level, "All models are not supported and the library is not optimized");

  kpriv->modelname = postproc_conf->model_name;
  kpriv->modelpath = postproc_conf->model_path;
  kpriv->log_level = log_level;
  kpriv->fixpoint_node_flip = 0;

  if (!fileexists (kpriv->modelpath)) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
        "Model path %s does not exist", kpriv->modelpath.c_str ());
    goto error;
  }

  kpriv->xmodel_name = kpriv->modelpath + "/" + kpriv->modelname + "/" + kpriv->modelname + ".xmodel";
  kpriv->prototxt = kpriv->modelpath + "/" + kpriv->modelname + "/" + kpriv->modelname + ".prototxt";

  if (!(strcmp (postproc_conf->model_name, "yolov3_voc_tf")) || !(strcmp (postproc_conf->model_name, "yolov3_voc"))) {
    std::string label_file = kpriv->modelpath + "/" + kpriv->modelname + "/" + "label.json";
    if (!fileexists (label_file)) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
          "label file %s does not exist", label_file.c_str ());
      goto error;
    }
    if (!readlabels (kpriv, (char *)label_file.c_str ())) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
          "Failed to read label file %s", label_file.c_str ());
      goto error;
    }
  }

  kpriv->default_attrs_ = xir::Attrs::create ();
  return (VvasPostProcessor *)kpriv;

error:
  if (kpriv)
    delete kpriv;
  return NULL;
}

VvasReturnType vvas_postprocess_destroy (VvasPostProcessor * postproc_handle)
{
  VvasPostProcessPriv *kpriv = (VvasPostProcessPriv *)postproc_handle;

  if (!kpriv) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "Invalid handle");
    return VVAS_RET_ERROR;
  }

  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "destroying");

  if (kpriv)
    delete kpriv;

  return VVAS_RET_SUCCESS;
}

VvasInferPrediction* vvas_postprocess_tensor (VvasPostProcessor *postproc_handle, VvasInferPrediction *src)
{
  VvasPostProcessPriv *kpriv = (VvasPostProcessPriv *)postproc_handle;
  VvasInferPrediction *parent_predict = NULL;
  VvasBoundingBox parent_bbox = { 0 };
  VvasBoundingBox child_bbox = { 0 };
  VvasInferPrediction *child_predict = NULL;
  VvasInferClassification *c = NULL;

  if (!kpriv) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, DEFAULT_VVAS_LOG_LEVEL, "Invalid handle");
    return parent_predict;
  }

  if (!src)
  {
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level, "Source Prediction is NULL");
    return parent_predict;
  }

  if (!src->tb)
  {
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level, "Source Prediction has no tensors");
    return parent_predict;
  }

  if (src->model_class != VVAS_XCLASS_RAWTENSOR) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level, "Model Class is not RAWTENSOR");
    return parent_predict;
  }

  std::vector < vart::TensorBuffer * >outputsPtr;
  TensorBuf *tb = src->tb;
  for (auto i = 0; i < tb->size; ++i) {
    outputsPtr.push_back ((vart::TensorBuffer *) tb->ptr[i]);
  }

  if (!strcmp (src->model_name, "plate_num")) {
    postprocess_platenum (outputsPtr, &parent_predict, kpriv->log_level, src->model_name);
  } else if (!strcmp (src->model_name, "resnet_v1_50_tf")) {
    postprocess_resnet_v1_50_tf (outputsPtr, &parent_predict, kpriv->log_level, src->model_name);
  } else if (!strcmp (src->model_name, "yolov3_voc_tf") ||
             !strcmp (src->model_name, "yolov3_voc") ||
	     !strcmp (src->model_name, "densebox_320_320")) {

    if (!fileexists (kpriv->prototxt)) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level, "Prototxt file %s not found", kpriv->prototxt.c_str ());
      return parent_predict;
    }
    const vitis::ai::proto::DpuModelParam & config =
        get_config (kpriv->prototxt, kpriv->default_attrs_.get ());
    auto det_threshold_ = config.dense_box_param ().det_threshold ();

    /* create new input TensorBuffer */
    auto input_tensors =
        std::vector < std::vector < vitis::ai::library::InputTensor >> ();
    input_tensors.reserve (1);
    /* Get the current format */
    auto input_tb = std::vector < vitis::ai::library::InputTensor > { };
    input_tb.reserve (1);
    for (int i = 0; i < 1; i++) {
      auto ret1 = vitis::ai::library::InputTensor { };
      ret1.width = tb->width;
      ret1.height = tb->height;
      ret1.batch = 1;
      input_tb.emplace_back (ret1);
    }
    input_tensors.emplace_back (input_tb);

    /* create new output TensorBuffer */
    std::vector < std::unique_ptr < vart::TensorBuffer >> xint_tensor_buffers;
    auto output_tensors =
        std::vector < std::vector < vitis::ai::library::OutputTensor >> ();
    output_tensors.reserve (1);
    /* Get the current format */
    auto output_tb = std::vector < vitis::ai::library::OutputTensor > { };
    output_tb.reserve (outputsPtr.size ());
    int i = 0;
    for (auto & t:outputsPtr) {
      auto tensor_from = t->get_tensor ();
      auto new_tensor_ = xir::Tensor::create (tensor_from->get_name (),
          tensor_from->get_shape (), { xir::DataType::XINT, 8 });

      new_tensor_->set_attrs (tensor_from->get_attrs ());
      if (!strcmp (src->model_name, "yolov3_voc_tf") || !strcmp (src->model_name, "yolov3_voc")) {
        new_tensor_->set_attr < int >("fix_point", (int) 2);
      } else if (!strcmp (src->model_name, "densebox_320_320")) {
        if (kpriv->fixpoint_node_flip == 0) {
          new_tensor_->set_attr < int >("fix_point", (int) 0);
          kpriv->fixpoint_node_flip = 1;
        } else {
          new_tensor_->set_attr < int >("fix_point", (int) 4);
          kpriv->fixpoint_node_flip = 0;
        }
      } else {
	LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level, "Unsupported model");
        return parent_predict;
      }

      xint_tensor_buffers.emplace_back (vart::
          alloc_cpu_flat_tensor_buffer (new_tensor_.get ()));
      vart::TensorBuffer::copy_tensor_buffer (t, xint_tensor_buffers[i].get ());
      auto tensor = xint_tensor_buffers[i]->get_tensor ();
      int height;
      int width;
      int channel;
      auto dim_num = tensor->get_shape ().size ();
      auto batch = dim_num <= 0 ? 1 : tensor->get_shape ().at (0);
      const char *datatype =
          tensor->get_data_type ().type ==
          xir::DataType::XINT ? "XINT" : "FLOAT";
      height = dim_num <= 1 ? 1 : tensor->get_shape ().at (1);
      width = dim_num <= 2 ? 1 : tensor->get_shape ().at (2);
      channel = dim_num <= 3 ? 1 : tensor->get_shape ().at (3);

      if (LOG_LEVEL_INFO < kpriv->log_level) {
        printf ("%d\t\t %s\t\t %d\t\t \t%d \t%d \t%d \t%d \t%d \t%s\n", 0,
            (tensor->get_name ()).c_str (),
            tensor->get_element_num (),
            tensor->get_data_size (), batch, width, height, channel, datatype);
      }

        output_tb.emplace_back (convert_tensor_buffer_to_output_tensor
            (xint_tensor_buffers[i].get (), (vart::Runner::TensorFormat) tb->fmt,
            get_fix_point (new_tensor_.get ())));
        i++;
    }

    output_tensors.emplace_back (output_tb);

    if (!(strcmp (src->model_name, "yolov3_voc_tf")) || !(strcmp (src->model_name, "yolov3_voc"))) {
      auto results =
          vitis::ai::yolov3_post_process (input_tensors[0], output_tensors[0],
          config, tb->width, tb->height);

      for (auto & box:results.bboxes) {
        int label = box.label;
        float xmin = box.x * tb->width + 1;
        float ymin = box.y * tb->height + 1;
        float xmax = xmin + box.width * tb->width;
        float ymax = ymin + box.height * tb->height;
        if (xmin < 0.)
          xmin = 1.;
        if (ymin < 0.)
          ymin = 1.;
        if (xmax > tb->width)
          xmax = tb->width;
        if (ymax > tb->height)
          ymax = tb->height;
        float confidence = box.score;

        if (!parent_predict) {
          parent_predict = vvas_inferprediction_new ();
        }

	child_bbox.x = xmin;
        child_bbox.y = ymin;
        child_bbox.width = xmax - xmin;
        child_bbox.height = ymax - ymin;
	child_predict = vvas_inferprediction_new ();
	child_predict->bbox = child_bbox;
	c = vvas_inferclassification_new ();
        c->class_id = label;
        c->class_prob = confidence;
        c->class_label = strdup (kpriv->labels[label].c_str());
        c->num_classes = 0;
        child_predict->classifications = vvas_list_append (child_predict->classifications, c);

        child_predict->model_class = VVAS_XCLASS_YOLOV3;
        child_predict->model_name = strdup (src->model_name);
        vvas_inferprediction_append (parent_predict, child_predict);

        LOG_MESSAGE (LOG_LEVEL_INFO, kpriv->log_level,
            "RESULT: (%d) %f %f %f %f (%f)",
            label, xmin, ymin, xmax, ymax, confidence);
      }
      LOG_MESSAGE (LOG_LEVEL_INFO, kpriv->log_level, "\n");
    } else if (!strcmp (src->model_name, "densebox_320_320")) {
      auto results =
          vitis::ai::face_detect_post_process (input_tensors, output_tensors,
          config, det_threshold_);
      for (auto batch_idx = 0u; batch_idx < results.size (); batch_idx++) {
        for (auto & box:results[batch_idx].rects) {
          float xmin = box.x * tb->width + 1;
          float ymin = box.y * tb->height + 1;
          float xmax = xmin + box.width * tb->width;
          float ymax = ymin + box.height * tb->height;
          if (xmin < 0.)
            xmin = 1.;
          if (ymin < 0.)
            ymin = 1.;
          if (xmax > tb->width)
            xmax = tb->width;
          if (ymax > tb->height)
            ymax = tb->height;
          float confidence = box.score;

	  if (!parent_predict) {
            parent_predict = vvas_inferprediction_new ();
          }

          child_bbox.x = xmin;
          child_bbox.y = ymin;
          child_bbox.width = xmax - xmin;
          child_bbox.height = ymax - ymin;
          child_predict = vvas_inferprediction_new ();
          child_predict->bbox = child_bbox;
          c = vvas_inferclassification_new ();
          c->class_id = -1;
          c->class_prob = confidence;
          c->num_classes = 0;
          child_predict->classifications = vvas_list_append (child_predict->classifications, c);

          child_predict->model_class = VVAS_XCLASS_FACEDETECT;
          child_predict->model_name = strdup (src->model_name);
          vvas_inferprediction_append (parent_predict, child_predict);

          LOG_MESSAGE (LOG_LEVEL_INFO, kpriv->log_level,
              "RESULT: %f %f %f %f (%f)", xmin, ymin, xmax, ymax, confidence);
        }
      }
    } else {
      LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level,
          "unsupported postprocessing for mode %s", src->model_name);
    }
  }

  /* Update root prediction with image size */
  if (parent_predict) {
    parent_bbox.x = parent_bbox.y = 0;
    parent_bbox.width = src->tb->width;
    parent_bbox.height = src->tb->height;
    parent_predict->bbox = parent_bbox;
  }

  return parent_predict;
}
