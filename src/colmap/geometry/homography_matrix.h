// Copyright (c), ETH Zurich and UNC Chapel Hill.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
//     * Neither the name of ETH Zurich and UNC Chapel Hill nor the names of
//       its contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include "colmap/geometry/rigid3.h"
#include "colmap/util/eigen_alignment.h"
#include "colmap/util/types.h"

#include <vector>

#include <Eigen/Core>

namespace colmap {

// Decompose an homography matrix into the possible rotations, translations, and
// plane normal vectors, according to:
//
//    Malis, Ezio, and Manuel Vargas. "Deeper understanding of the homography
//    decomposition for vision-based control." (2007): 90.
//
// The first pose is assumed to be P = [I | 0]. Note that the homography is
// plane-induced if `cams2_from_cams1.size() == n.size() == 4`. If
// `cams2_from_cams1.size() == n.size() == 1` the homography is pure-rotational.
//
// @param H                 3x3 homography matrix.
// @param K                 3x3 calibration matrix.
// @param cams2_from_cams1  Possible relative camera transformations.
// @param normals           Possible normal vectors.
void DecomposeHomographyMatrix(const Eigen::Matrix3d& H,
                               const Eigen::Matrix3d& K1,
                               const Eigen::Matrix3d& K2,
                               std::vector<Rigid3d>* cams2_from_cams1,
                               std::vector<Eigen::Vector3d>* normals);

// Recover the most probable pose from the given homography matrix.
//
// The pose of the first image is assumed to be P = [I | 0].
//
// @param H               3x3 homography matrix.
// @param K1              3x3 calibration matrix of first camera.
// @param K2              3x3 calibration matrix of second camera.
// @param cam_rays1       First set of corresponding rays.
// @param cam_rays2       Second set of corresponding rays.
// @param inlier_mask     Only points with `true` in the inlier mask are
//                        considered in the cheirality test. Size of the
//                        inlier mask must match the number of points N.
// @param cam2_from_cam1  Most probable 3x1 translation vector.
// @param normal          Most probable 3x1 normal vector.
// @param points3D        Triangulated 3D points infront of camera
//                        (only if homography is not pure-rotational).
void PoseFromHomographyMatrix(const Eigen::Matrix3d& H,
                              const Eigen::Matrix3d& K1,
                              const Eigen::Matrix3d& K2,
                              const std::vector<Eigen::Vector3d>& cam_rays1,
                              const std::vector<Eigen::Vector3d>& cam_rays2,
                              Rigid3d* cam2_from_cam1,
                              Eigen::Vector3d* normal,
                              std::vector<Eigen::Vector3d>* points3D);

// Compose homography matrix from relative pose.
//
// @param K1      3x3 calibration matrix of first camera.
// @param K2      3x3 calibration matrix of second camera.
// @param R       Most probable 3x3 rotation matrix.
// @param t       Most probable 3x1 translation vector.
// @param n       Most probable 3x1 normal vector.
// @param d       Orthogonal distance from plane.
//
// @return        3x3 homography matrix.
Eigen::Matrix3d HomographyMatrixFromPose(const Eigen::Matrix3d& K1,
                                         const Eigen::Matrix3d& K2,
                                         const Eigen::Matrix3d& R,
                                         const Eigen::Vector3d& t,
                                         const Eigen::Vector3d& n,
                                         double d);

}  // namespace colmap
