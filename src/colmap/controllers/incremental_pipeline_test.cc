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

#include "colmap/controllers/incremental_pipeline.h"

#include "colmap/estimators/alignment.h"
#include "colmap/scene/synthetic.h"
#include "colmap/util/testing.h"

#include <gtest/gtest.h>

namespace colmap {
namespace {

void ExpectEqualReconstructions(const Reconstruction& gt,
                                const Reconstruction& computed,
                                const double max_rotation_error_deg,
                                const double max_proj_center_error,
                                const double num_obs_tolerance,
                                const bool align = true) {
  EXPECT_EQ(computed.NumCameras(), gt.NumCameras());
  EXPECT_EQ(computed.NumImages(), gt.NumImages());
  EXPECT_EQ(computed.NumRegImages(), gt.NumRegImages());
  EXPECT_GE(computed.ComputeNumObservations(),
            (1 - num_obs_tolerance) * gt.ComputeNumObservations());

  Sim3d gt_from_computed;
  if (align) {
    ASSERT_TRUE(
        AlignReconstructionsViaProjCenters(computed,
                                           gt,
                                           /*max_proj_center_error=*/0.1,
                                           &gt_from_computed));
  }

  const std::vector<ImageAlignmentError> errors =
      ComputeImageAlignmentError(computed, gt, gt_from_computed);
  EXPECT_EQ(errors.size(), gt.NumImages());
  for (const auto& error : errors) {
    EXPECT_LT(error.rotation_error_deg, max_rotation_error_deg);
    EXPECT_LT(error.proj_center_error, max_proj_center_error);
  }
}

TEST(IncrementalPipeline, WithoutNoise) {
  const std::string database_path = CreateTestDir() + "/database.db";

  Database database(database_path);
  Reconstruction gt_reconstruction;
  SyntheticDatasetOptions synthetic_dataset_options;
  synthetic_dataset_options.num_rigs = 2;
  synthetic_dataset_options.num_cameras_per_rig = 1;
  synthetic_dataset_options.num_frames_per_rig = 7;
  synthetic_dataset_options.num_points3D = 50;
  synthetic_dataset_options.point2D_stddev = 0;
  synthetic_dataset_options.camera_has_prior_focal_length = false;
  SynthesizeDataset(synthetic_dataset_options, &gt_reconstruction, &database);

  auto reconstruction_manager = std::make_shared<ReconstructionManager>();
  IncrementalPipeline mapper(std::make_shared<IncrementalPipelineOptions>(),
                             /*image_path=*/"",
                             database_path,
                             reconstruction_manager);
  mapper.Run();

  ASSERT_EQ(reconstruction_manager->Size(), 1);
  ExpectEqualReconstructions(gt_reconstruction,
                             *reconstruction_manager->Get(0),
                             /*max_rotation_error_deg=*/1e-2,
                             /*max_proj_center_error=*/1e-4,
                             /*num_obs_tolerance=*/0);
}

TEST(IncrementalPipeline, WithoutNoiseAndWithNonTrivialFrames) {
  const std::string database_path = CreateTestDir() + "/database.db";

  Database database(database_path);
  Reconstruction gt_reconstruction;
  SyntheticDatasetOptions synthetic_dataset_options;
  synthetic_dataset_options.num_rigs = 2;
  synthetic_dataset_options.num_cameras_per_rig = 2;
  synthetic_dataset_options.num_frames_per_rig = 7;
  synthetic_dataset_options.num_points3D = 100;
  synthetic_dataset_options.point2D_stddev = 0;
  synthetic_dataset_options.camera_has_prior_focal_length = false;
  synthetic_dataset_options.sensor_from_rig_translation_stddev = 0.05;
  synthetic_dataset_options.sensor_from_rig_rotation_stddev = 30;
  SynthesizeDataset(synthetic_dataset_options, &gt_reconstruction, &database);

  for (const bool refine_sensor_from_rig : {true, false}) {
    auto reconstruction_manager = std::make_shared<ReconstructionManager>();
    auto options = std::make_shared<IncrementalPipelineOptions>();
    options->ba_refine_sensor_from_rig = refine_sensor_from_rig;
    IncrementalPipeline mapper(options,
                               /*image_path=*/"",
                               database_path,
                               reconstruction_manager);
    mapper.Run();

    ASSERT_EQ(reconstruction_manager->Size(), 1);
    ExpectEqualReconstructions(gt_reconstruction,
                               *reconstruction_manager->Get(0),
                               /*max_rotation_error_deg=*/1e-2,
                               /*max_proj_center_error=*/1e-3,
                               /*num_obs_tolerance=*/0);
  }
}

TEST(IncrementalPipeline, WithoutNoiseAndWithPanoramicNonTrivialFrames) {
  const std::string database_path = CreateTestDir() + "/database.db";

  Database database(database_path);
  Reconstruction gt_reconstruction;
  SyntheticDatasetOptions synthetic_dataset_options;
  synthetic_dataset_options.num_rigs = 2;
  synthetic_dataset_options.num_cameras_per_rig = 3;
  synthetic_dataset_options.num_frames_per_rig = 7;
  synthetic_dataset_options.num_points3D = 100;
  synthetic_dataset_options.point2D_stddev = 0;
  synthetic_dataset_options.camera_has_prior_focal_length = false;
  synthetic_dataset_options.sensor_from_rig_translation_stddev = 0;
  synthetic_dataset_options.sensor_from_rig_rotation_stddev = 30;
  SynthesizeDataset(synthetic_dataset_options, &gt_reconstruction, &database);

  for (const bool refine_sensor_from_rig : {true, false}) {
    auto reconstruction_manager = std::make_shared<ReconstructionManager>();
    auto options = std::make_shared<IncrementalPipelineOptions>();
    options->ba_refine_sensor_from_rig = refine_sensor_from_rig;
    IncrementalPipeline mapper(options,
                               /*image_path=*/"",
                               database_path,
                               reconstruction_manager);
    mapper.Run();

    ASSERT_EQ(reconstruction_manager->Size(), 1);
    ExpectEqualReconstructions(gt_reconstruction,
                               *reconstruction_manager->Get(0),
                               /*max_rotation_error_deg=*/1e-2,
                               /*max_proj_center_error=*/1e-3,
                               /*num_obs_tolerance=*/0);
  }
}

TEST(IncrementalPipeline, WithPriorFocalLength) {
  const std::string database_path = CreateTestDir() + "/database.db";

  Database database(database_path);
  Reconstruction gt_reconstruction;
  SyntheticDatasetOptions synthetic_dataset_options;
  synthetic_dataset_options.num_rigs = 2;
  synthetic_dataset_options.num_cameras_per_rig = 1;
  synthetic_dataset_options.num_frames_per_rig = 7;
  synthetic_dataset_options.num_points3D = 50;
  synthetic_dataset_options.point2D_stddev = 0;
  synthetic_dataset_options.camera_has_prior_focal_length = true;
  SynthesizeDataset(synthetic_dataset_options, &gt_reconstruction, &database);

  auto reconstruction_manager = std::make_shared<ReconstructionManager>();
  IncrementalPipeline mapper(std::make_shared<IncrementalPipelineOptions>(),
                             /*image_path=*/"",
                             database_path,
                             reconstruction_manager);
  mapper.Run();

  ASSERT_EQ(reconstruction_manager->Size(), 1);
  ExpectEqualReconstructions(gt_reconstruction,
                             *reconstruction_manager->Get(0),
                             /*max_rotation_error_deg=*/1e-2,
                             /*max_proj_center_error=*/1e-4,
                             /*num_obs_tolerance=*/0);
}

TEST(IncrementalPipeline, WithNoise) {
  const std::string database_path = CreateTestDir() + "/database.db";

  Database database(database_path);
  Reconstruction gt_reconstruction;
  SyntheticDatasetOptions synthetic_dataset_options;
  synthetic_dataset_options.num_rigs = 2;
  synthetic_dataset_options.num_cameras_per_rig = 1;
  synthetic_dataset_options.num_frames_per_rig = 7;
  synthetic_dataset_options.num_points3D = 100;
  synthetic_dataset_options.point2D_stddev = 0.5;
  SynthesizeDataset(synthetic_dataset_options, &gt_reconstruction, &database);

  auto reconstruction_manager = std::make_shared<ReconstructionManager>();
  IncrementalPipeline mapper(std::make_shared<IncrementalPipelineOptions>(),
                             /*image_path=*/"",
                             database_path,
                             reconstruction_manager);
  mapper.Run();

  ASSERT_EQ(reconstruction_manager->Size(), 1);
  ExpectEqualReconstructions(gt_reconstruction,
                             *reconstruction_manager->Get(0),
                             /*max_rotation_error_deg=*/1e-1,
                             /*max_proj_center_error=*/1e-1,
                             /*num_obs_tolerance=*/0.02);
}

TEST(IncrementalPipeline, MultiReconstruction) {
  const std::string database_path = CreateTestDir() + "/database.db";

  Database database(database_path);
  Reconstruction gt_reconstruction1;
  Reconstruction gt_reconstruction2;
  SyntheticDatasetOptions synthetic_dataset_options;
  synthetic_dataset_options.num_rigs = 1;
  synthetic_dataset_options.num_cameras_per_rig = 1;
  synthetic_dataset_options.num_frames_per_rig = 5;
  synthetic_dataset_options.num_points3D = 50;
  synthetic_dataset_options.point2D_stddev = 0;
  SynthesizeDataset(synthetic_dataset_options, &gt_reconstruction1, &database);
  synthetic_dataset_options.num_frames_per_rig = 4;
  SynthesizeDataset(synthetic_dataset_options, &gt_reconstruction2, &database);

  auto reconstruction_manager = std::make_shared<ReconstructionManager>();
  auto mapper_options = std::make_shared<IncrementalPipelineOptions>();
  mapper_options->min_model_size = 4;
  IncrementalPipeline mapper(mapper_options,
                             /*image_path=*/"",
                             database_path,
                             reconstruction_manager);
  mapper.Run();

  ASSERT_EQ(reconstruction_manager->Size(), 2);
  Reconstruction* computed_reconstruction1 = nullptr;
  Reconstruction* computed_reconstruction2 = nullptr;
  if (reconstruction_manager->Get(0)->NumRegImages() == 5) {
    computed_reconstruction1 = reconstruction_manager->Get(0).get();
    computed_reconstruction2 = reconstruction_manager->Get(1).get();
  } else {
    computed_reconstruction1 = reconstruction_manager->Get(1).get();
    computed_reconstruction2 = reconstruction_manager->Get(0).get();
  }
  ExpectEqualReconstructions(gt_reconstruction1,
                             *computed_reconstruction1,
                             /*max_rotation_error_deg=*/1e-2,
                             /*max_proj_center_error=*/1e-4,
                             /*num_obs_tolerance=*/0);
  ExpectEqualReconstructions(gt_reconstruction2,
                             *computed_reconstruction2,
                             /*max_rotation_error_deg=*/1e-2,
                             /*max_proj_center_error=*/1e-4,
                             /*num_obs_tolerance=*/0);
}

TEST(IncrementalPipeline, FixExistingFrames) {
  const std::string database_path = CreateTestDir() + "/database.db";

  Database database(database_path);
  Reconstruction gt_reconstruction;
  SyntheticDatasetOptions synthetic_dataset_options;
  synthetic_dataset_options.num_rigs = 1;
  synthetic_dataset_options.num_cameras_per_rig = 1;
  synthetic_dataset_options.num_frames_per_rig = 7;
  synthetic_dataset_options.num_points3D = 50;
  synthetic_dataset_options.point2D_stddev = 0;
  synthetic_dataset_options.camera_has_prior_focal_length = false;
  SynthesizeDataset(synthetic_dataset_options, &gt_reconstruction, &database);

  auto reconstruction_manager = std::make_shared<ReconstructionManager>();
  auto options = std::make_shared<IncrementalPipelineOptions>();
  for (const bool fix_existing_frames : {false, true}) {
    if (fix_existing_frames) {
      ASSERT_EQ(reconstruction_manager->Size(), 1);
      Reconstruction& reconstruction = *reconstruction_manager->Get(0);
      // De-register a frame that expect to be re-registered in the second run.
      reconstruction.DeRegisterFrame(1);
      // Clear all the observations of one image but keep it registered. We do
      // not expect fixed images to be filtered (due to insufficient
      // observations).
      Image& image2 = reconstruction.Image(2);
      for (point2D_t point2D_idx = 0; point2D_idx < image2.NumPoints2D();
           ++point2D_idx) {
        if (image2.Point2D(point2D_idx).HasPoint3D()) {
          reconstruction.DeleteObservation(image2.ImageId(), point2D_idx);
        }
      }
    }
    options->fix_existing_frames = fix_existing_frames;
    IncrementalPipeline mapper(options,
                               /*image_path=*/"",
                               database_path,
                               reconstruction_manager);
    mapper.Run();

    ASSERT_EQ(reconstruction_manager->Size(), 1);
    ExpectEqualReconstructions(gt_reconstruction,
                               *reconstruction_manager->Get(0),
                               /*max_rotation_error_deg=*/1e-2,
                               /*max_proj_center_error=*/1e-4,
                               /*num_obs_tolerance=*/0);
  }
}

TEST(IncrementalPipeline, ChainedMatches) {
  const std::string database_path = CreateTestDir() + "/database.db";

  Database database(database_path);
  Reconstruction gt_reconstruction;
  SyntheticDatasetOptions synthetic_dataset_options;
  synthetic_dataset_options.match_config =
      SyntheticDatasetOptions::MatchConfig::CHAINED;
  synthetic_dataset_options.num_rigs = 1;
  synthetic_dataset_options.num_cameras_per_rig = 1;
  synthetic_dataset_options.num_frames_per_rig = 4;
  synthetic_dataset_options.num_points3D = 100;
  synthetic_dataset_options.point2D_stddev = 0;
  SynthesizeDataset(synthetic_dataset_options, &gt_reconstruction, &database);

  auto reconstruction_manager = std::make_shared<ReconstructionManager>();
  IncrementalPipeline mapper(std::make_shared<IncrementalPipelineOptions>(),
                             /*image_path=*/"",
                             database_path,
                             reconstruction_manager);
  mapper.Run();

  ASSERT_EQ(reconstruction_manager->Size(), 1);
  ExpectEqualReconstructions(gt_reconstruction,
                             *reconstruction_manager->Get(0),
                             /*max_rotation_error_deg=*/1e-2,
                             /*max_proj_center_error=*/1e-4,
                             /*num_obs_tolerance=*/0);
}

TEST(IncrementalPipeline, PriorBasedSfMWithoutNoise) {
  const std::string database_path = CreateTestDir() + "/database.db";

  Database database(database_path);
  Reconstruction gt_reconstruction;
  SyntheticDatasetOptions synthetic_dataset_options;
  synthetic_dataset_options.num_rigs = 2;
  synthetic_dataset_options.num_cameras_per_rig = 1;
  synthetic_dataset_options.num_frames_per_rig = 10;
  synthetic_dataset_options.num_points3D = 100;
  synthetic_dataset_options.point2D_stddev = 0.5;

  synthetic_dataset_options.use_prior_position = true;
  synthetic_dataset_options.prior_position_stddev = 0.0;
  SynthesizeDataset(synthetic_dataset_options, &gt_reconstruction, &database);

  std::shared_ptr<IncrementalPipelineOptions> mapper_options =
      std::make_shared<IncrementalPipelineOptions>();
  mapper_options->use_prior_position = true;

  auto reconstruction_manager = std::make_shared<ReconstructionManager>();
  IncrementalPipeline mapper(mapper_options,
                             /*image_path=*/"",
                             database_path,
                             reconstruction_manager);
  mapper.Run();

  ASSERT_EQ(reconstruction_manager->Size(), 1);

  // No noise on prior so do not align gt & computed (expected to be aligned
  // from PositionPriorBundleAdjustment)
  ExpectEqualReconstructions(gt_reconstruction,
                             *reconstruction_manager->Get(0),
                             /*max_rotation_error_deg=*/1e-1,
                             /*max_proj_center_error=*/1e-1,
                             /*num_obs_tolerance=*/0.02,
                             /*align=*/false);
}

TEST(IncrementalPipeline, PriorBasedSfMWithoutNoiseAndWithNonTrivialFrames) {
  const std::string database_path = CreateTestDir() + "/database.db";

  Database database(database_path);
  Reconstruction gt_reconstruction;
  SyntheticDatasetOptions synthetic_dataset_options;
  synthetic_dataset_options.num_rigs = 2;
  synthetic_dataset_options.num_cameras_per_rig = 2;
  synthetic_dataset_options.num_frames_per_rig = 7;
  synthetic_dataset_options.num_points3D = 100;
  synthetic_dataset_options.point2D_stddev = 0;
  synthetic_dataset_options.camera_has_prior_focal_length = false;

  synthetic_dataset_options.use_prior_position = true;
  synthetic_dataset_options.prior_position_stddev = 0.0;
  SynthesizeDataset(synthetic_dataset_options, &gt_reconstruction, &database);

  std::shared_ptr<IncrementalPipelineOptions> mapper_options =
      std::make_shared<IncrementalPipelineOptions>();

  mapper_options->use_prior_position = true;
  mapper_options->use_robust_loss_on_prior_position = true;

  auto reconstruction_manager = std::make_shared<ReconstructionManager>();
  IncrementalPipeline mapper(mapper_options,
                             /*image_path=*/"",
                             database_path,
                             reconstruction_manager);
  mapper.Run();

  ASSERT_EQ(reconstruction_manager->Size(), 1);
  ExpectEqualReconstructions(gt_reconstruction,
                             *reconstruction_manager->Get(0),
                             /*max_rotation_error_deg=*/1e-1,
                             /*max_proj_center_error=*/1e-1,
                             /*num_obs_tolerance=*/0.02,
                             /*align=*/true);
}

TEST(IncrementalPipeline, PriorBasedSfMWithNoise) {
  const std::string database_path = CreateTestDir() + "/database.db";

  Database database(database_path);
  Reconstruction gt_reconstruction;
  SyntheticDatasetOptions synthetic_dataset_options;
  synthetic_dataset_options.num_rigs = 2;
  synthetic_dataset_options.num_cameras_per_rig = 1;
  synthetic_dataset_options.num_frames_per_rig = 7;
  synthetic_dataset_options.num_points3D = 100;
  synthetic_dataset_options.point2D_stddev = 0.5;

  synthetic_dataset_options.use_prior_position = true;
  synthetic_dataset_options.prior_position_stddev = 1.5;
  SynthesizeDataset(synthetic_dataset_options, &gt_reconstruction, &database);

  std::shared_ptr<IncrementalPipelineOptions> mapper_options =
      std::make_shared<IncrementalPipelineOptions>();

  mapper_options->use_prior_position = true;
  mapper_options->use_robust_loss_on_prior_position = true;

  auto reconstruction_manager = std::make_shared<ReconstructionManager>();
  IncrementalPipeline mapper(mapper_options,
                             /*image_path=*/"",
                             database_path,
                             reconstruction_manager);
  mapper.Run();

  ASSERT_EQ(reconstruction_manager->Size(), 1);
  ExpectEqualReconstructions(gt_reconstruction,
                             *reconstruction_manager->Get(0),
                             /*max_rotation_error_deg=*/1e-1,
                             /*max_proj_center_error=*/1e-1,
                             /*num_obs_tolerance=*/0.02,
                             /*align=*/true);
}

TEST(IncrementalPipeline, GPSPriorBasedSfMWithNoise) {
  const std::string database_path = CreateTestDir() + "/database.db";

  Database database(database_path);
  Reconstruction gt_reconstruction;
  SyntheticDatasetOptions synthetic_dataset_options;
  synthetic_dataset_options.num_rigs = 2;
  synthetic_dataset_options.num_cameras_per_rig = 1;
  synthetic_dataset_options.num_frames_per_rig = 10;
  synthetic_dataset_options.num_points3D = 100;
  synthetic_dataset_options.point2D_stddev = 0.5;

  synthetic_dataset_options.use_prior_position = true;
  synthetic_dataset_options.use_geographic_coords_prior = true;
  synthetic_dataset_options.prior_position_stddev = 1.5;
  SynthesizeDataset(synthetic_dataset_options, &gt_reconstruction, &database);

  std::shared_ptr<IncrementalPipelineOptions> mapper_options =
      std::make_shared<IncrementalPipelineOptions>();

  mapper_options->use_prior_position = true;
  mapper_options->use_robust_loss_on_prior_position = true;

  auto reconstruction_manager = std::make_shared<ReconstructionManager>();
  IncrementalPipeline mapper(mapper_options,
                             /*image_path=*/"",
                             database_path,
                             reconstruction_manager);
  mapper.Run();

  ASSERT_EQ(reconstruction_manager->Size(), 1);
  ExpectEqualReconstructions(gt_reconstruction,
                             *reconstruction_manager->Get(0),
                             /*max_rotation_error_deg=*/1e-1,
                             /*max_proj_center_error=*/1e-1,
                             /*num_obs_tolerance=*/0.02,
                             /*align=*/true);
}

}  // namespace
}  // namespace colmap
