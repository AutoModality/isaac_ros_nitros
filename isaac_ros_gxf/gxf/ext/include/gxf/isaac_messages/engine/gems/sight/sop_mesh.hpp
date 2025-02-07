/*
Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.

NVIDIA CORPORATION and its licensors retain all intellectual property
and proprietary rights in and to this software, related documentation
and any modifications thereto. Any use, reproduction, disclosure or
distribution of this software and related documentation without an express
license agreement from NVIDIA CORPORATION is strictly prohibited.
*/
#pragma once

#include <string>
#include <vector>

#include "third_party/nlohmann/json.hpp"

#include "engine/core/math/types.hpp"

namespace isaac {
namespace sight {

// Sight Operation for a mesh.
class SopMesh {
 public:
  // Delete copy constructor
  SopMesh(const SopMesh&) = delete;
  SopMesh(SopMesh&&) = default;

  // Creates a new SopMesh, `append` indicates whether or not it's an update or if we should restart
  // from scratch.
  SopMesh(bool append = true);

  // Adds a new block to the mesh.
  // Parameters:
  // - name: the name of the block.
  // - num_points: the number of points (N)
  // - points: a vector of coordinates, size 3N, contains [x0,y0,z0, x1,y1,z1, ...,]
  // - num_triangles: the number of triangles, (M)
  // - triangles: indices of points that form a triangle, size 3M, the first three indices
  //              correspond to the first triangle, the next three to the second triangle, and so on
  // - colors: if not null, it contains the color of each point in order. The format is RGB.
  // - normals: if not null, it contains the normal vector of each point,  [nx0,ny0,nz0, ...]
  void add(const std::string& name, int num_points, const float* points, int num_triangles,
           const uint16_t* triangles, const uint8_t* colors = nullptr,
           const float* normals = nullptr);
  void add(const std::string& name, const std::vector<float>& points,
           const std::vector<uint16_t>& triangles, const std::vector<uint8_t>& colors,
           const std::vector<float>& normals);

 private:
  friend const nlohmann::json& ToJson(const SopMesh&);
  friend nlohmann::json ToJson(SopMesh&&);
  nlohmann::json json_;
};

// Returns the json of a SopMesh
const nlohmann::json& ToJson(const SopMesh&);
nlohmann::json ToJson(SopMesh&&);

}  // namespace sight
}  // namespace isaac
