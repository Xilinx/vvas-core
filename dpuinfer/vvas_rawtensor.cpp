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

#include "vvas_rawtensor.hpp"

#include "vart/batch_tensor_buffer_view.hpp"
#include "vart/runner_helper.hpp"
#include "vitis/ai/collection_helper.hpp"
#include "vitis/ai/env_config.hpp"
#include "vitis/ai/path_util.hpp"
#include "xir/graph/graph.hpp"

class vvas_tensorbuf
{
  std::vector < std::unique_ptr <
      vart::TensorBuffer >> new_tensor_buffers_per_image;
  std::vector < std::unique_ptr <
      vart::TensorBuffer >> tensor_buffers_per_image;
  std::vector < vart::TensorBuffer * >outputsPtr;

  /* map batch of tensors to single image tensor */
  std::vector < std::unique_ptr <
      vart::TensorBuffer >> map_to_single_batch (const std::vector <
      vart::TensorBuffer * >&tensor_buffers, size_t batch_index, size_t batch)
  {
    return vitis::ai::vec_map (tensor_buffers,
        [batch_index, batch] (const vart::TensorBuffer * tensor_buffer) {
        return std::unique_ptr < vart::TensorBuffer > (std::make_unique <
              vart::BatchTensorBufferView > (const_cast <
                  vart::TensorBuffer * >(tensor_buffer), batch_index, batch));}
    );
  }

public:
  /* Create new holder for tensor */
  vvas_tensorbuf (const std::vector <
      vart::TensorBuffer * >&output_tensor_buffers,
      unsigned int long cur_batch, int log_level) {
    tensor_buffers_per_image =
        map_to_single_batch (output_tensor_buffers, cur_batch, 1);

    /* Create new tensors, so that it can be pass to next plugin */
    for (auto i = 0u; i < tensor_buffers_per_image.size (); i++)
      new_tensor_buffers_per_image.emplace_back
          (vart::alloc_cpu_flat_tensor_buffer (tensor_buffers_per_image
              [i]->get_tensor ()));

    /* Store unique pointer */
    for (auto i = 0u; i < new_tensor_buffers_per_image.size (); ++i) {
      outputsPtr.push_back (new_tensor_buffers_per_image[i].get ());
    }
    if (log_level >= LOG_LEVEL_INFO)
      printf
          ("Tensor index\t name\t num of elements\t num of data \tbatch \twidth \theight \tchannel type\n");

    /* Deep copy tesnsors buffers to new pointes */
    for (auto i = 0u; i < new_tensor_buffers_per_image.size (); ++i) {
      vart::TensorBuffer::
          copy_tensor_buffer (tensor_buffers_per_image[i].get (),
          outputsPtr[i]);
      if (log_level >= LOG_LEVEL_INFO) {
        int height;
        int width;
        int channel;
        auto tensor = outputsPtr[i]->get_tensor ();
        auto dim_num = tensor->get_shape ().size ();
        auto batch = dim_num <= 0 ? 1 : tensor->get_shape ().at (0);
        const char *datatype =
            tensor->get_data_type ().type ==
            xir::DataType::XINT ? "XINT" : "FLOAT";
        if (dim_num == 2) {
          height = 1;
          width = 1;
          channel = tensor->get_shape ().at (1);
        } else {
          height = dim_num <= 1 ? 1 : tensor->get_shape ().at (1);
          width = dim_num <= 2 ? 1 : tensor->get_shape ().at (2);
          channel = dim_num <= 3 ? 1 : tensor->get_shape ().at (3);
        }
        printf ("%d\t\t %s\t\t %d\t\t \t%d \t%d \t%d \t%d \t%d \t%s\n", i,
            (tensor->get_name ()).c_str (), tensor->get_element_num (),
            tensor->get_data_size (), batch, width, height, channel, datatype);
      }
    }
  }
  std::vector < vart::TensorBuffer * >get_output_tensor_buffers () {
    return outputsPtr;
  }
  std::vector < std::unique_ptr <
      vart::TensorBuffer >> *get_output_tensor_buffers_ptr () {
    return &new_tensor_buffers_per_image;
  }
};

void
copy_vvas_tb (void **frm, void **to)
{
  TensorBuf **frm_tb = (TensorBuf **) frm;
  TensorBuf **to_tb = (TensorBuf **) to;

  if (!(*frm_tb)) {
    *to_tb = NULL;
    return;
  }

  /** increase ref count*/
  atomic_fetch_add (&(*frm_tb)->ref_count, 1);
  *to_tb = *frm_tb;
}

void
free_vvas_tb (void **ptr)
{
  TensorBuf **tb = (TensorBuf **) ptr;
  if (*tb) {
    vvas_tensorbuf *vvas_tb = (vvas_tensorbuf *) (*tb)->priv;
    if (vvas_tb) {
      /** decrease ref count*/
      atomic_fetch_sub (&(*tb)->ref_count, 1);
      if (!(((*tb)->ref_count))) {
        delete vvas_tb;
        free (*tb);
        *tb = NULL;
      }
    }
  }
}

static std::vector < std::int32_t >
get_index_zeros (const xir::Tensor * tensor)
{
  auto ret = tensor->get_shape ();
  std::fill (ret.begin (), ret.end (), 0);
  return ret;
}

//# Templatized the input data type
template < typename T > static void
cpy_image_to_tensor_buffer (T * data, long unsigned int rows,
    long unsigned int cols, long unsigned int channels, int stride,
    const uint8_t * input)
{
  for (long unsigned int row = 0; row < rows; ++row) {
    memcpy (data + row * cols * channels, input + row * stride,
        cols * channels);
  }
}

vvas_rawtensor::vvas_rawtensor (void *handle, const std::string & xmodel)
{
  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *) handle;
  log_level = kpriv->log_level;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter");
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "model : %s",
      xmodel.c_str ());

  /** create graph runner */
  graph = xir::Graph::deserialize (xmodel);
  attrs = xir::Attrs::create ();
  runner =
      vitis::ai::GraphRunner::create_graph_runner (graph.get (), attrs.get ());

  /** get input/output tensor buffers */
  input_tensor_buffers = runner->get_inputs ();
  output_tensor_buffers = runner->get_outputs ();
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level,
      "input_tensor_buffers ptr = %p", input_tensor_buffers[0]);
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level,
      "output_tensor_buffers ptr = %p", output_tensor_buffers[0]);
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "runner %p created",
      runner.get ());
}

int
vvas_rawtensor::run (void *handle, std::vector < cv::Mat > &images,
    VvasInferPrediction ** predictions)
{
  VvasDpuInferPrivate *kpriv = (VvasDpuInferPrivate *) handle;
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "enter");
  char *pstr;

  /** The height, width are always same for all the input_tensor in a batch */
  auto input_tensor = input_tensor_buffers[0]->get_tensor ();
  auto batch = input_tensor->get_shape ().at (0);
  auto height = input_tensor->get_shape ().at (1);
  auto width = input_tensor->get_shape ().at (2);
  int channels = 3;             /* CV_8UC3 */
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "Input w*h = %d*%d",
      images[0].cols, images[0].rows);
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "model required w*h = %d*%d",
      width, height);
  if (height != images[0].rows || width != images[0].cols) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level, "Wrong input image size");
    return false;
  }
  if (images.size () <= (unsigned) batch) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level,
        "batch required is %d, but no of inputs are %ld", batch,
        images.size ());

    /* Update the batch to no of inputs images */
    batch = images.size ();
  }

  /* Copy images to input tensor buffers */
  for (int index = 0; index < batch; ++index) {
    uint64_t data_in = 0u;
    size_t size_in = 0u;

    /* reset input tensor */
    auto idx = get_index_zeros (input_tensor);
    idx[0] = (int) index;
    std::tie (data_in, size_in) = input_tensor_buffers[0]->data (idx);
    cpy_image_to_tensor_buffer ((uint8_t *) data_in, height, width, channels,
        images[index].step, images[index].data);
  }

  /* sync input tensor buffers */
  for (auto & input:input_tensor_buffers) {
    input->sync_for_write (0,
        input->get_tensor ()->get_data_size () /
        input->get_tensor ()->get_shape ()[0]);
  }

  /* run graph runner */
  auto v = runner->execute_async (input_tensor_buffers, output_tensor_buffers);
  auto status = runner->wait ((int) v.first, -1);
  if (status != 0) {
    LOG_MESSAGE (LOG_LEVEL_ERROR, kpriv->log_level, "failed to run the graph");
    return false;
  }

  /* sync output tensor buffers */
  for (auto output:output_tensor_buffers) {
    output->sync_for_read (0,
        output->get_tensor ()->get_data_size () /
        output->get_tensor ()->get_shape ()[0]);
  }
  auto output_tensor = output_tensor_buffers[0]->get_tensor ();
  auto obatch = output_tensor->get_shape ().at (0);
  auto size = output_tensor_buffers.size ();
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level,
      "output_tensor_buffers.size() is %ld\n", size);
  LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level, "batch size %d\n", obatch);

  /* Copy Output to new buffer */
  auto output_tensors = runner->get_output_tensors ();
  if (images.size () <= (unsigned) obatch) {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level,
        "obatch required is %d, but no of inputs are %ld", obatch,
        images.size ());
    obatch = images.size ();
  }

  /* Attach output TensorBuffer of resp image to infermeta of that image */
  for (auto i = 0; i < obatch; i++) {
    VvasInferPrediction *parent_predict = NULL;
    {
      vvas_tensorbuf *vvas_tb =
          new vvas_tensorbuf (output_tensor_buffers, (unsigned int long) i,
          kpriv->log_level);
      std::vector < vart::TensorBuffer * >outputsPtr =
          vvas_tb->get_output_tensor_buffers ();

      VvasBoundingBox parent_bbox = { 0 };

      int cols = images[i].cols;
      int rows = images[i].rows;
      parent_bbox.x = parent_bbox.y = 0;
      parent_bbox.width = cols;
      parent_bbox.height = rows;
      parent_predict = predictions[i];
      if (!parent_predict) {
        parent_predict = vvas_inferprediction_new ();
        parent_predict->bbox = parent_bbox;
      }

      VvasInferPrediction *predict;
      predict = vvas_inferprediction_new ();
      TensorBuf *tb = (TensorBuf *) malloc (sizeof (TensorBuf));

      /* fill tensor data */
      {
        tb->priv = (void *) vvas_tb;
        tb->copy = copy_vvas_tb;
        tb->free = free_vvas_tb;
        tb->size = outputsPtr.size ();
        tb->width = cols;
        tb->height = rows;
        tb->fmt = (unsigned long int) runner->get_tensor_format ();
        tb->ref_count = 1;
        for (auto i = 0u; i < outputsPtr.size (); ++i) {
          tb->ptr[i] = (void *) outputsPtr[i];
        }
        predict->tb = tb;
      }
      if (log_level >= LOG_LEVEL_DEBUG) {
        for (auto j = 0u; j < outputsPtr.size (); j++) {
          int height;
          int width;
          int channel;
          auto tensor = outputsPtr[j]->get_tensor ();
          auto dim_num = tensor->get_shape ().size ();
          auto batch = dim_num <= 0 ? 1 : tensor->get_shape ().at (0);
          const char *datatype =
              tensor->get_data_type ().type ==
              xir::DataType::XINT ? "XINT" : "FLOAT";
          if (dim_num == 2) {
            height = 1;
            width = 1;
            channel = tensor->get_shape ().at (1);
          } else {
            height = dim_num <= 1 ? 1 : tensor->get_shape ().at (1);
            width = dim_num <= 2 ? 1 : tensor->get_shape ().at (2);
            channel = dim_num <= 3 ? 1 : tensor->get_shape ().at (3);
          }
          printf ("%d\t\t %s\t\t %d\t\t \t%d \t%d \t%d \t%d \t%d \t%s\n", i,
              (tensor->get_name ()).c_str (), tensor->get_element_num (),
              tensor->get_data_size (), batch, width, height, channel,
              datatype);
        }
      }

      /* add class and name in prediction node */
      predict->model_class = (VvasClass) kpriv->modelclass;
      predict->model_name = strdup (kpriv->modelname.c_str ());
      vvas_inferprediction_append (parent_predict, predict);
      if (kpriv->log_level >= LOG_LEVEL_DEBUG) {
        pstr = vvas_inferprediction_to_string (parent_predict);
        LOG_MESSAGE (LOG_LEVEL_DEBUG, kpriv->log_level,
          "prediction tree : \n%s", pstr);
        free (pstr);
      }
    }
    predictions[i] = parent_predict;
  }
  LOG_MESSAGE (LOG_LEVEL_INFO, kpriv->log_level, " ");
  return true;
}

int
vvas_rawtensor::requiredwidth (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  auto input_tensor = input_tensor_buffers[0]->get_tensor ();
  auto width = input_tensor->get_shape ().at (2);
  return width;
}

int
vvas_rawtensor::requiredheight (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  auto input_tensor = input_tensor_buffers[0]->get_tensor ();
  auto height = input_tensor->get_shape ().at (1);
  return height;
}

int
vvas_rawtensor::supportedbatchsz (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  auto input_tensor = input_tensor_buffers[0]->get_tensor ();
  auto batch = input_tensor->get_shape ().at (0);
  return batch;
}

int
vvas_rawtensor::close (void)
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
  return true;
}

vvas_rawtensor::~vvas_rawtensor ()
{
  LOG_MESSAGE (LOG_LEVEL_DEBUG, log_level, "enter");
}
