/**
 * Copyright (c) 2022-2023, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include <string>
#include <unordered_map>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wpedantic"
#include "gxf/core/entity.hpp"
#include "gxf/core/gxf.h"
#include "gxf/multimedia/camera.hpp"
#include "gxf/std/timestamp.hpp"
#pragma GCC diagnostic pop

#include "isaac_ros_nitros_camera_info_type/nitros_camera_info.hpp"
#include "isaac_ros_nitros/types/type_adapter_nitros_context.hpp"

#include "rclcpp/rclcpp.hpp"


constexpr char RAW_CAMERA_MODEL_GXF_NAME[] = "intrinsics";
constexpr char RECT_CAMERA_MODEL_GXF_NAME[] = "target_camera";
constexpr char EXTRINSICS_GXF_NAME[] = "extrinsics";
constexpr char TARGET_EXTRINSICS_DELTA_GXF_NAME[] = "target_extrinsics_delta";

namespace
{
using DistortionType = nvidia::gxf::DistortionType;
const std::unordered_map<std::string, DistortionType> g_ros_to_gxf_distortion_model({
    {"pinhole", DistortionType::Perspective},
    {"plumb_bob", DistortionType::Brown},
    {"rational_polynomial", DistortionType::Polynomial},
    {"equidistant", DistortionType::FisheyeEquidistant}
  });

const std::unordered_map<DistortionType, std::string> g_gxf_to_ros_distortion_model({
    {DistortionType::Polynomial, "rational_polynomial"},
    {DistortionType::FisheyeEquidistant, "equidistant"},
    {DistortionType::Perspective, "pinhole"},
    {DistortionType::Brown, "plumb_bob"}
  });
}  // namespace


void rclcpp::TypeAdapter<
  nvidia::isaac_ros::nitros::NitrosCameraInfo,
  sensor_msgs::msg::CameraInfo>::convert_to_ros_message(
  const custom_type & source, ros_message_type & destination)
{
  nvidia::isaac_ros::nitros::nvtxRangePushWrapper(
    "NitrosCameraInfo::convert_to_ros_message",
    nvidia::isaac_ros::nitros::CLR_PURPLE);

  RCLCPP_DEBUG(
    rclcpp::get_logger("NitrosCameraInfo"),
    "[convert_to_ros_message] Conversion started for handle=%ld", source.handle);

  auto context = nvidia::isaac_ros::nitros::GetTypeAdapterNitrosContext().getContext();
  auto msg_entity = nvidia::gxf::Entity::Shared(context, source.handle);

  auto raw_gxf_camera_model = msg_entity->get<nvidia::gxf::CameraModel>(RAW_CAMERA_MODEL_GXF_NAME);
  if (!raw_gxf_camera_model) {
    std::stringstream error_msg;
    error_msg <<
      "[convert_to_ros_message] Failed to get the Raw CameraModel object: " <<
      GxfResultStr(raw_gxf_camera_model.error());
    RCLCPP_ERROR(
      rclcpp::get_logger("NitrosCameraInfo"), error_msg.str().c_str());
    throw std::runtime_error(error_msg.str().c_str());
  }
  auto rect_gxf_camera_model =
    msg_entity->get<nvidia::gxf::CameraModel>(RECT_CAMERA_MODEL_GXF_NAME);
  // Fallback to raw intrinsics incase rect camera model is not available
  if (!rect_gxf_camera_model) {
    RCLCPP_WARN_ONCE(
      rclcpp::get_logger(
        "NitrosCameraInfo"),
      "[convert_to_ros_message] Failed to get the Rectified CameraModel object: "
      "Falling back to raw camera model");
    rect_gxf_camera_model = raw_gxf_camera_model;
  }

  // Setting camera info from gxf camera model
  destination.height = raw_gxf_camera_model.value()->dimensions.y;
  destination.width = raw_gxf_camera_model.value()->dimensions.x;

  const auto distortion = g_gxf_to_ros_distortion_model.find(
    raw_gxf_camera_model.value()->distortion_type);
  if (distortion == std::end(g_gxf_to_ros_distortion_model)) {
    std::stringstream error_msg;
    error_msg <<
      "[convert_to_ros_message] Unsupported distortion model from gxf [" <<
      static_cast<int>(raw_gxf_camera_model.value()->distortion_type) << "].";
    RCLCPP_ERROR(
      rclcpp::get_logger("NitrosCameraInfo"), error_msg.str().c_str());
    throw std::runtime_error(error_msg.str().c_str());
  } else {
    destination.distortion_model = distortion->second;
  }

  // Resize d buffer to the right size
  destination.d.resize(
    sizeof(raw_gxf_camera_model.value()->distortion_coefficients) /
    sizeof(float));

  if (raw_gxf_camera_model.value()->distortion_type == DistortionType::Polynomial) {
    destination.d[0] = raw_gxf_camera_model.value()->distortion_coefficients[0];
    destination.d[1] = raw_gxf_camera_model.value()->distortion_coefficients[1];
    destination.d[2] = raw_gxf_camera_model.value()->distortion_coefficients[6];
    destination.d[3] = raw_gxf_camera_model.value()->distortion_coefficients[7];
    destination.d[4] = raw_gxf_camera_model.value()->distortion_coefficients[2];
    destination.d[5] = raw_gxf_camera_model.value()->distortion_coefficients[3];
    destination.d[6] = raw_gxf_camera_model.value()->distortion_coefficients[4];
    destination.d[7] = raw_gxf_camera_model.value()->distortion_coefficients[5];
  } else {
    std::copy(
      std::begin(raw_gxf_camera_model.value()->distortion_coefficients),
      std::end(raw_gxf_camera_model.value()->distortion_coefficients), std::begin(destination.d));
  }

  destination.k[0] = raw_gxf_camera_model.value()->focal_length.x;
  destination.k[1] = 0;
  destination.k[2] = raw_gxf_camera_model.value()->principal_point.x;
  destination.k[3] = 0;
  destination.k[4] = raw_gxf_camera_model.value()->focal_length.y;
  destination.k[5] = raw_gxf_camera_model.value()->principal_point.y;
  destination.k[6] = 0;
  destination.k[7] = 0;
  destination.k[8] = 1;

  // Setting extrinsic info from gxf pose 3D
  auto extrinsics_gxf_pose_3d = msg_entity->get<nvidia::gxf::Pose3D>(EXTRINSICS_GXF_NAME);
  if (!extrinsics_gxf_pose_3d) {
    std::stringstream error_msg;
    error_msg <<
      "[convert_to_ros_message] Failed to get the extrinsics Pose3D object: " <<
      GxfResultStr(extrinsics_gxf_pose_3d.error());
    RCLCPP_ERROR(
      rclcpp::get_logger("NitrosCameraInfo"), error_msg.str().c_str());
    throw std::runtime_error(error_msg.str().c_str());
  }
  auto target_extrinsics_delta_gxf_pose_3d = msg_entity->get<nvidia::gxf::Pose3D>(
    TARGET_EXTRINSICS_DELTA_GXF_NAME);

  // Fallback to extrinsics Pose3D object
  if (!target_extrinsics_delta_gxf_pose_3d) {
    std::stringstream warn_msg;
    warn_msg <<
      "[convert_to_ros_message] Failed to get the target extrinsics delta Pose3D object: " <<
      GxfResultStr(target_extrinsics_delta_gxf_pose_3d.error()) <<
      "Falling back to extrinsics Pose3D object";
    RCLCPP_WARN_ONCE(
      rclcpp::get_logger("NitrosCameraInfo"), warn_msg.str().c_str());

    target_extrinsics_delta_gxf_pose_3d = extrinsics_gxf_pose_3d;
  }

  std::copy(
    std::begin(target_extrinsics_delta_gxf_pose_3d.value()->rotation),
    std::end(target_extrinsics_delta_gxf_pose_3d.value()->rotation),
    std::begin(destination.r));

  // The left 3*3 portion of the P-matrix specifies the intrinsic of rectified image
  // The right 1*3 vector specifies the tranlsation vector
  destination.p[0] = rect_gxf_camera_model.value()->focal_length.x;
  destination.p[1] = 0;
  destination.p[2] = rect_gxf_camera_model.value()->principal_point.x;
  destination.p[3] = extrinsics_gxf_pose_3d.value()->translation[0] * destination.p[0];
  destination.p[4] = 0;
  destination.p[5] = rect_gxf_camera_model.value()->focal_length.y;
  destination.p[6] = rect_gxf_camera_model.value()->principal_point.y;
  destination.p[7] = extrinsics_gxf_pose_3d.value()->translation[1];
  destination.p[8] = 0;
  destination.p[9] = 0;
  destination.p[10] = 1;
  destination.p[11] = extrinsics_gxf_pose_3d.value()->translation[2];

  destination.binning_x = 1;    // No subsampling
  destination.binning_y = 1;    // No subsampling
  destination.roi.height = raw_gxf_camera_model.value()->dimensions.y;    // Full resolution
  destination.roi.width = raw_gxf_camera_model.value()->dimensions.x;    // Full resolution

  // Populate timestamp information back into ROS header
  auto input_timestamp = msg_entity->get<nvidia::gxf::Timestamp>("timestamp");
  if (!input_timestamp) {    // Fallback to label 'timestamp'
    input_timestamp = msg_entity->get<nvidia::gxf::Timestamp>();
  }
  if (input_timestamp) {
    destination.header.stamp.sec = static_cast<int32_t>(
      input_timestamp.value()->acqtime / static_cast<uint64_t>(1e9));
    destination.header.stamp.nanosec = static_cast<uint32_t>(
      input_timestamp.value()->acqtime % static_cast<uint64_t>(1e9));
  }

  // Set frame ID
  destination.header.frame_id = source.frame_id;

  RCLCPP_DEBUG(
    rclcpp::get_logger("NitrosCameraInfo"),
    "[convert_to_ros_message] Conversion completed");

  nvidia::isaac_ros::nitros::nvtxRangePopWrapper();
}

void rclcpp::TypeAdapter<
  nvidia::isaac_ros::nitros::NitrosCameraInfo,
  sensor_msgs::msg::CameraInfo>::convert_to_custom(
  const ros_message_type & source,
  custom_type & destination)
{
  nvidia::isaac_ros::nitros::nvtxRangePushWrapper(
    "NitrosCameraInfo::convert_to_custom",
    nvidia::isaac_ros::nitros::CLR_PURPLE);

  RCLCPP_DEBUG(
    rclcpp::get_logger("NitrosCameraInfo"),
    "[convert_to_custom] Conversion started");

  auto context = nvidia::isaac_ros::nitros::GetTypeAdapterNitrosContext().getContext();

  auto message = nvidia::gxf::Entity::New(context);
  if (!message) {
    std::stringstream error_msg;
    error_msg <<
      "[convert_to_custom] Error initializing new message entity: " <<
      GxfResultStr(message.error());
    RCLCPP_ERROR(
      rclcpp::get_logger("NitrosCameraInfo"), error_msg.str().c_str());
    throw std::runtime_error(error_msg.str().c_str());
  }

  auto raw_gxf_camera_model = message->add<nvidia::gxf::CameraModel>(RAW_CAMERA_MODEL_GXF_NAME);

  raw_gxf_camera_model.value()->dimensions = {source.width, source.height};
  raw_gxf_camera_model.value()->focal_length = {
    static_cast<float>(source.k[0]), static_cast<float>(source.k[4])};
  raw_gxf_camera_model.value()->principal_point = {
    static_cast<float>(source.k[2]), static_cast<float>(source.k[5])};

  const auto distortion = g_ros_to_gxf_distortion_model.find(source.distortion_model);
  if (distortion == std::end(g_ros_to_gxf_distortion_model)) {
    std::stringstream error_msg;
    error_msg <<
      "[convert_to_custom] Unsupported distortion model from ROS [" <<
      source.distortion_model.c_str() << "].";
    RCLCPP_ERROR(
      rclcpp::get_logger("NitrosCameraInfo"), error_msg.str().c_str());
    throw std::runtime_error(error_msg.str().c_str());
  } else {
    raw_gxf_camera_model.value()->distortion_type = distortion->second;
  }

  if (raw_gxf_camera_model.value()->distortion_type == DistortionType::Polynomial) {
    // prevents distortion parameters array access if its empty
    // simulators may send empty distortion parameter array since images are already rectified
    if (!source.d.empty()) {
      // distortion parameters in GXF: k1, k2, k3, k4, k5, k6, p1, p2
      // distortion parameters in ROS message: k1, k2, p1, p2, k3 ...
      raw_gxf_camera_model.value()->distortion_coefficients[0] = source.d[0];
      raw_gxf_camera_model.value()->distortion_coefficients[1] = source.d[1];

      for (uint16_t index = 2; index < source.d.size() - 2; index++) {
        raw_gxf_camera_model.value()->distortion_coefficients[index] = source.d[index + 2];
      }
      raw_gxf_camera_model.value()->distortion_coefficients[6] = source.d[2];
      raw_gxf_camera_model.value()->distortion_coefficients[7] = source.d[3];
    }
  } else {
    std::copy(
      std::begin(source.d), std::end(source.d),
      std::begin(raw_gxf_camera_model.value()->distortion_coefficients));
  }

  // add rectified image camera model
  auto rect_gxf_camera_model = message->add<nvidia::gxf::CameraModel>(RECT_CAMERA_MODEL_GXF_NAME);
  rect_gxf_camera_model.value()->dimensions = {source.width, source.height};
  rect_gxf_camera_model.value()->focal_length = {
    static_cast<float>(source.p[0]), static_cast<float>(source.p[5])};
  rect_gxf_camera_model.value()->principal_point = {
    static_cast<float>(source.p[2]), static_cast<float>(source.p[6])};
  rect_gxf_camera_model.value()->distortion_type = DistortionType::Perspective;
  memset(
    rect_gxf_camera_model.value()->distortion_coefficients, 0,
    rect_gxf_camera_model.value()->kMaxDistortionCoefficients * sizeof(float));
  // this pose3D entity is used to passthrough the translation between the two cameras
  // this translation data is not used by the tensorops rectification library
  // Note: The rotation of this EXTRINSICS_GXF_NAME pose3D entity is set identity
  // since the information cannot be calculated from a single camera info msg
  auto extrinsics_gxf_pose_3d = message->add<nvidia::gxf::Pose3D>(EXTRINSICS_GXF_NAME);
  // this pose3D entity is used to send the rectification matrix
  // Note: translation of this TARGET_EXTRINSICS_DELTA_GXF_NAME pose3D entity is always zero
  auto target_extrinsics_delta_gxf_pose_3d = message->add<nvidia::gxf::Pose3D>(
    TARGET_EXTRINSICS_DELTA_GXF_NAME);

  // populate rectification rotation matrix into the TARGET_EXTRINSICS_DELTA_GXF_NAME
  std::copy(
    std::begin(source.r), std::end(source.r),
    std::begin(target_extrinsics_delta_gxf_pose_3d.value()->rotation));

  // Based on the comments for the camera info msg type, p[0] == 0 means the topic
  // contains no calibration data, ie, its an uncalibrated camera
  if (source.p[0] == 0.0f) {
    RCLCPP_WARN(
      rclcpp::get_logger("NitrosCameraInfo"),
      "[convert_to_custom] Received an uncalibrated camera info msg.");
    extrinsics_gxf_pose_3d.value()->translation = {
      0,
      0,
      0};
  } else {
    // populate extrinsics translation the EXTRINSICS_GXF_NAME
    extrinsics_gxf_pose_3d.value()->translation = {
      static_cast<float>(source.p[3]) / static_cast<float>(source.p[0]),
      static_cast<float>(source.p[7]),
      static_cast<float>(source.p[11])};
  }

  // Add timestamp to the message
  uint64_t input_timestamp =
    source.header.stamp.sec * static_cast<uint64_t>(1e9) +
    source.header.stamp.nanosec;
  auto output_timestamp = message->add<nvidia::gxf::Timestamp>("timestamp");
  if (!output_timestamp) {
    std::stringstream error_msg;
    error_msg << "[convert_to_custom] Failed to add a timestamp component to message: " <<
      GxfResultStr(output_timestamp.error());
    RCLCPP_ERROR(
      rclcpp::get_logger("NitrosCameraInfo"), error_msg.str().c_str());
    throw std::runtime_error(error_msg.str().c_str());
  }
  output_timestamp.value()->acqtime = input_timestamp;

  // Set frame ID
  destination.frame_id = source.header.frame_id;

  // Set Entity Id
  destination.handle = message->eid();
  GxfEntityRefCountInc(context, message->eid());

  RCLCPP_DEBUG(
    rclcpp::get_logger("NitrosCameraInfo"),
    "[convert_to_custom] Conversion completed");

  nvidia::isaac_ros::nitros::nvtxRangePopWrapper();
}
