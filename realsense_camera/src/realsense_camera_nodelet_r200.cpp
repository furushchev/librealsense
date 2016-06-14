/******************************************************************************
 Copyright (c) 2016, Intel Corporation
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 3. Neither the name of the copyright holder nor the names of its contributors
 may be used to endorse or promote products derived from this software without
 specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *******************************************************************************/

#include "realsense_camera_nodelet_r200.h"

PLUGINLIB_EXPORT_CLASS (realsense_camera::RealsenseNodeletR200, nodelet::Nodelet)

namespace realsense_camera
{
  /*
   * Public Methods.
   */

  /*
   * Initialize the realsense camera
   */
  void RealsenseNodeletR200::onInit()
  {
    // set member vars used in base class
    nodelet_name_ = getName();
    num_streams_ = NUM_STREAMS_R200;
    nh_ = getNodeHandle();
    pnh_ = getPrivateNodeHandle();

    // create dynamic reconfigure server - this must be done before calling base class onInit()
    // onInit() calls setStaticCameraOptions() which relies on this being set already
    dynamic_reconf_server_.reset(new dynamic_reconfigure::Server<realsense_camera::camera_params_r200Config>(pnh_));

    // call base class onInit() method
    RealsenseNodelet::onInit();

    // Set up the IR2 frame and topics
    frame_id_[RS_STREAM_INFRARED2] = ir2_frame_id_;
    image_transport::ImageTransport it (nh_);
    camera_publisher_[RS_STREAM_INFRARED2] = it.advertiseCamera(IR2_TOPIC, 1);

    // setCallback can only be called after rs_device_ is set by base class connectToCamera()
    dynamic_reconf_server_->setCallback(boost::bind(&RealsenseNodeletR200::configCallback, this, _1, _2));
  }

  /*
   *Protected Methods.
   */
  void RealsenseNodeletR200::enableDepthStream()
  {
    // call the base class method first
    RealsenseNodelet::enableDepthStream();
    // enable IR2 stream
    enableInfrared2Stream();
  }

  void RealsenseNodeletR200::disableDepthStream()
  {
    // call the base class method first
    RealsenseNodelet::disableDepthStream();
    // disable IR2 stream
    disableInfrared2Stream();
  }

  void RealsenseNodeletR200::enableInfrared2Stream()
  {
    // Enable streams.
    if (mode_.compare ("manual") == 0)
    {
      ROS_INFO_STREAM(nodelet_name_ << " - Enabling Infrared2 stream: manual mode");
      rs_enable_stream(rs_device_, RS_STREAM_INFRARED2, depth_width_, depth_height_, IR2_FORMAT, depth_fps_, &rs_error_);
      checkError();
    }
    else
    {
      ROS_INFO_STREAM(nodelet_name_ << " - Enabling Infrared2 stream: preset mode");
      rs_enable_stream_preset(rs_device_, RS_STREAM_INFRARED2, RS_PRESET_BEST_QUALITY, &rs_error_);
      checkError();
    }

    uint32_t stream_index = (uint32_t) RS_STREAM_INFRARED2;
    if (camera_info_[stream_index] == NULL)
    {
      prepareStreamCalibData (RS_STREAM_INFRARED2);
    }
  }

  void RealsenseNodeletR200::disableInfrared2Stream()
  {
    ROS_INFO_STREAM(nodelet_name_ << " - Disabling Infrared2 stream");
    rs_disable_stream(rs_device_, RS_STREAM_INFRARED2, &rs_error_);
    checkError();
  }

  void RealsenseNodeletR200::configCallback(realsense_camera::camera_params_r200Config &config, uint32_t level)
  {
    // Set common options
    rs_set_device_option(rs_device_, RS_OPTION_COLOR_BACKLIGHT_COMPENSATION, config.color_backlight_compensation, 0);
    rs_set_device_option(rs_device_, RS_OPTION_COLOR_BRIGHTNESS, config.color_brightness, 0);
    rs_set_device_option(rs_device_, RS_OPTION_COLOR_CONTRAST, config.color_contrast, 0);
    rs_set_device_option(rs_device_, RS_OPTION_COLOR_GAIN, config.color_gain, 0);
    rs_set_device_option(rs_device_, RS_OPTION_COLOR_GAMMA, config.color_gamma, 0);
    rs_set_device_option(rs_device_, RS_OPTION_COLOR_HUE, config.color_hue, 0);
    rs_set_device_option(rs_device_, RS_OPTION_COLOR_SATURATION, config.color_saturation, 0);
    rs_set_device_option(rs_device_, RS_OPTION_COLOR_SHARPNESS, config.color_sharpness, 0);
    rs_set_device_option(rs_device_, RS_OPTION_COLOR_ENABLE_AUTO_WHITE_BALANCE, config.color_enable_auto_white_balance, 0);

    if (config.color_enable_auto_white_balance == 0)
    {
      rs_set_device_option(rs_device_, RS_OPTION_COLOR_WHITE_BALANCE, config.color_white_balance, 0);
    }

    if (config.enable_depth == false)
    {
      if (enable_color_ == false)
      {
        ROS_INFO_STREAM(nodelet_name_ << " - Color stream is also disabled. Cannot disable depth stream");
        config.enable_depth = true;
      }
      else
      {
        enable_depth_ = false;
      }
    }
    else
    {
      enable_depth_ = true;
    }

    //R200 camera specific options
    rs_set_device_option(rs_device_, RS_OPTION_R200_LR_AUTO_EXPOSURE_ENABLED, config.r200_lr_auto_exposure_enabled, 0);

    if (config.r200_lr_auto_exposure_enabled == 0)
    {
      rs_set_device_option(rs_device_, RS_OPTION_R200_LR_EXPOSURE, config.r200_lr_exposure, 0);
    }

    rs_set_device_option(rs_device_, RS_OPTION_R200_LR_GAIN, config.r200_lr_gain, 0);
    rs_set_device_option(rs_device_, RS_OPTION_R200_EMITTER_ENABLED, config.r200_emitter_enabled, 0);

    if (config.r200_lr_auto_exposure_enabled == 1)
    {
      if (config.r200_auto_exposure_top_edge >= depth_height_)
      {
        config.r200_auto_exposure_top_edge = depth_height_ - 1;
      }
      if (config.r200_auto_exposure_bottom_edge >= depth_height_)
      {
        config.r200_auto_exposure_bottom_edge = depth_height_ - 1;
      }
      if (config.r200_auto_exposure_left_edge >= depth_width_)
      {
        config.r200_auto_exposure_left_edge = depth_width_ - 1;
      }
      if (config.r200_auto_exposure_right_edge >= depth_width_)
      {
        config.r200_auto_exposure_right_edge = depth_width_ - 1;
      }
      edge_values_[0] = config.r200_auto_exposure_left_edge;
      edge_values_[1] = config.r200_auto_exposure_top_edge;
      edge_values_[2] = config.r200_auto_exposure_right_edge;
      edge_values_[3] = config.r200_auto_exposure_bottom_edge;

      rs_set_device_options(rs_device_, edge_options_, 4, edge_values_, 0);
    }
  }

  /*
   * Define buffer for images and prepare camera info for each enabled stream.
   */
  void RealsenseNodeletR200::allocateResources()
  {
    // call base class method first
    RealsenseNodelet::allocateResources();
    // set IR2 image buffer
    image_[(uint32_t) RS_STREAM_INFRARED2] = cv::Mat(depth_height_, depth_width_, CV_8UC1, cv::Scalar (0));
  }

  /*
   * Set the stream options based on input params.
   */
  void RealsenseNodeletR200::setStreamOptions()
  {
    // call base class method first
    RealsenseNodelet::setStreamOptions();
    // setup R200 specific frame
    pnh_.param("ir2_frame_id", ir2_frame_id_, DEFAULT_IR2_FRAME_ID);
  }

  /*
   * Populate the encodings for each stream.
   */
  void RealsenseNodeletR200::fillStreamEncoding()
  {
    // Call base class method first
    RealsenseNodelet::fillStreamEncoding();
    // Setup IR2 stream
    stream_encoding_[(uint32_t) RS_STREAM_INFRARED2] = sensor_msgs::image_encodings::TYPE_8UC1;
    stream_step_[(uint32_t) RS_STREAM_INFRARED2] = depth_width_ * sizeof (unsigned char);
  }

  /*
   * Set the static camera options.
   */
  void RealsenseNodeletR200::setStaticCameraOptions()
  {
    ROS_INFO_STREAM(nodelet_name_ << " - Setting camera options");

    // Get dynamic options from the dynamic reconfigure server.
    realsense_camera::camera_params_r200Config params_config;
    dynamic_reconf_server_->getConfigDefault(params_config);

    std::vector<realsense_camera::camera_params_r200Config::AbstractParamDescriptionConstPtr> param_desc = params_config.__getParamDescriptions__();

    // Iterate through the supported camera options
    for (CameraOptions o: camera_options_)
    {
      std::string opt_name = rs_option_to_string(o.opt);
      bool found = false;

      std::vector<realsense_camera::camera_params_r200Config::AbstractParamDescriptionConstPtr>::iterator it;
      for (realsense_camera::camera_params_r200Config::AbstractParamDescriptionConstPtr param_desc_ptr: param_desc)
      {
        std::transform(opt_name.begin(), opt_name.end(), opt_name.begin(), ::tolower);
        if (opt_name.compare((* param_desc_ptr).name) == 0)
        {
          found = true;
          break;
        }
      }
      // Skip the dynamic options and set only the static camera options.
      if (found == false)
      {
        std::string key;
        double val;

        if (pnh_.searchParam(opt_name, key))
        {
          double opt_val;
          pnh_.getParam(key, val);

          // Validate and set the input values within the min-max range
          if (val < o.min)
          {
            opt_val = o.min;
          }
          else if (val > o.max)
          {
            opt_val = o.max;
          }
          else
          {
            opt_val = val;
          }
          ROS_INFO_STREAM(nodelet_name_ << " - Static Options: " << opt_name << " = " << opt_val);
          rs_set_device_option(rs_device_, o.opt, opt_val, &rs_error_);
          checkError();
        }
      }
    }
  }
}  // end namespace

