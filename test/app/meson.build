
if not core_dpuinfer_dep.found()
  subdir_done()
endif


jansson_dep = dependency('jansson', version : '>= 2.7', required: true)
src = ['test_cascade_yolov3_3xresnet.cpp']

exe = executable('test_video_ml', src,
                 cpp_args : [vvas_core_args, '-std=c++17'],
                 include_directories : [configinc, core_common_inc, core_utils_inc, core_parser_inc, core_decoder_inc, core_scaler_inc, core_overlay_inc, core_dpuinfer_inc, core_metaconvert_inc],
                 dependencies : [core_common_dep, core_utils_dep, parser_dep, decoder_dep, core_scaler_dep, core_overlay_dep, core_dpuinfer_dep, core_metaconvert_dep, jansson_dep],
                 install : true)

install_data (['cascade_jsons/cascade_yolov3_3xresnet18.json',
               'cascade_jsons/yolov3_voc_tf.json',
               'cascade_jsons/resnet18_vehicle_make.json',
               'cascade_jsons/resnet18_vehicle_type.json',
               'cascade_jsons/resnet18_vehicle_color.json',
               'cascade_jsons/metaconvert_config.json'], install_dir : get_option('datadir')/'cascade_yolov3_3xresnet18')

src = ['test_cascade_yolov3_3xresnet_mt.cpp']

exe = executable('test_video_ml_mt', src,
                 cpp_args : [vvas_core_args, '-std=c++17'],
                 include_directories : [configinc, core_common_inc, core_utils_inc, core_parser_inc, core_decoder_inc, core_scaler_inc, core_overlay_inc, core_dpuinfer_inc, core_metaconvert_inc, test_utils_inc],
                 dependencies : [core_common_dep, core_utils_dep, parser_dep, decoder_dep, core_scaler_dep, core_overlay_dep, core_dpuinfer_dep, core_metaconvert_dep, jansson_dep, core_pool_dep, pthread_dep],
                 install : true)
