/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2010-2011, Willow Garage, Inc.
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FAATPCL_RECOGNITION_OCCLUSION_REASONING_H_
#define FAATPCL_RECOGNITION_OCCLUSION_REASONING_H_

#include <pcl/common/common.h>
#include <pcl/common/transforms.h>
#include <pcl/common/io.h>

namespace v4r
{
  namespace occlusion_reasoning
  {
    /**
     * \brief Class to reason about occlusions
     * \author Aitor Aldoma
     */

    template<typename ModelT, typename SceneT>
      class ZBuffering
      {
      private:
        float f_;
        int cx_, cy_;
        float * depth_;

      public:

        ZBuffering ();
        ZBuffering (int resx, int resy, float f);
        ~ZBuffering ();
        void
        computeDepthMap (typename pcl::PointCloud<SceneT>::ConstPtr & scene, bool compute_focal = false, bool smooth = false, int wsize = 3);
        void
        filter (typename pcl::PointCloud<ModelT>::ConstPtr & model, typename pcl::PointCloud<ModelT>::Ptr & filtered, float thres = 0.01);
        void filter (typename pcl::PointCloud<ModelT>::ConstPtr & model, std::vector<int> & indices, float thres = 0.01);
      };

      template<typename SceneT, typename ModelT> std::vector<bool>
      computeOccludedPoints (const typename pcl::PointCloud<SceneT> & organized_cloud,
                             const typename pcl::PointCloud<ModelT> & to_be_filtered,
                             float f = 525.f,
                             float threshold = 0.01f,
                             bool is_occluded_out_fov = true)
      {
        const float cx = (static_cast<float> (organized_cloud.width) / 2.f - 0.5f);
        const float cy = (static_cast<float> (organized_cloud.height) / 2.f - 0.5f);
        std::vector<bool> is_occluded (to_be_filtered.points.size(), false);

        for (size_t i = 0; i < to_be_filtered.points.size (); i++)
        {
          if ( !pcl::isFinite(to_be_filtered.points[i]) )
               continue;

          const float x = to_be_filtered.points[i].x;
          const float y = to_be_filtered.points[i].y;
          const float z = to_be_filtered.points[i].z;
          const int u = static_cast<int> (f * x / z + cx);
          const int v = static_cast<int> (f * y / z + cy);

          // points out of the field of view in the first frame
          if ((u >= static_cast<int> (organized_cloud.width)) || (v >= static_cast<int> (organized_cloud.height)) || (u < 0) || (v < 0))
          {
              is_occluded[i] = is_occluded_out_fov;
              continue;
          }

          // Check for invalid depth
          if ( !pcl::isFinite (organized_cloud.at (u, v)) )
          {
              is_occluded[i] = true;
              continue;
          }


          //Check if point depth (distance to camera) is greater than the (u,v)
          if ( ( z - organized_cloud.at(u, v).z ) > threshold)
          {
              is_occluded[i] = true;
          }
        }
        return is_occluded;
      }



      template<typename SceneT, typename ModelT> std::vector<bool>
      computeOccludedPoints (const typename pcl::PointCloud<SceneT> & organized_cloud,
                             const typename pcl::PointCloud<ModelT> & to_be_filtered,
                             const Eigen::Matrix4f &transform_2to1,
                             float f = 525.f,
                             float threshold = 0.01f,
                             bool is_occluded_out_fov = true)
        {
            typename pcl::PointCloud<ModelT> cloud_trans;
            pcl::transformPointCloud(to_be_filtered, cloud_trans, transform_2to1);
            return computeOccludedPoints(organized_cloud, cloud_trans, f, threshold, is_occluded_out_fov);
        }


    template<typename ModelT, typename SceneT> typename pcl::PointCloud<ModelT>::Ptr
    filter (typename pcl::PointCloud<SceneT>::ConstPtr & organized_cloud, typename pcl::PointCloud<ModelT>::ConstPtr & to_be_filtered, float f,
            float threshold)
    {
      const float cx = (static_cast<float> (organized_cloud->width) / 2.f - 0.5f);
      const float cy = (static_cast<float> (organized_cloud->height) / 2.f - 0.5f);
      typename pcl::PointCloud<ModelT>::Ptr filtered (new pcl::PointCloud<ModelT> ());

      std::vector<int> indices_to_keep;
      indices_to_keep.resize (to_be_filtered->points.size ());

      int keep = 0;
      for (size_t i = 0; i < to_be_filtered->points.size (); i++)
      {
        const float x = to_be_filtered->points[i].x;
        const float y = to_be_filtered->points[i].y;
        const float z = to_be_filtered->points[i].z;
        const int u = static_cast<int> (f * x / z + cx);
        const int v = static_cast<int> (f * y / z + cy);

        //Not out of bounds
        if ((u >= static_cast<int> (organized_cloud->width)) || (v >= static_cast<int> (organized_cloud->height)) || (u < 0) || (v < 0))
          continue;

        //Check for invalid depth
        if (!pcl_isfinite (organized_cloud->at (u, v).x) || !pcl_isfinite (organized_cloud->at (u, v).y)
            || !pcl_isfinite (organized_cloud->at (u, v).z))
          continue;

        float z_oc = organized_cloud->at (u, v).z;

        //Check if point depth (distance to camera) is greater than the (u,v)
        if ((z - z_oc) > threshold)
          continue;

        indices_to_keep[keep] = static_cast<int> (i);
        keep++;
      }

      indices_to_keep.resize (keep);
      pcl::copyPointCloud (*to_be_filtered, indices_to_keep, *filtered);
      return filtered;
    }

    template<typename ModelT, typename SceneT> typename pcl::PointCloud<ModelT>::Ptr
    filter (typename pcl::PointCloud<SceneT>::ConstPtr & organized_cloud, typename pcl::PointCloud<ModelT>::ConstPtr & to_be_filtered, float f,
            float threshold, std::vector<int> & indices_to_keep)
    {
      float cx = (static_cast<float> (organized_cloud->width) / 2.f - 0.5f);
      float cy = (static_cast<float> (organized_cloud->height) / 2.f - 0.5f);
      typename pcl::PointCloud<ModelT>::Ptr filtered (new pcl::PointCloud<ModelT> ());

      //std::vector<int> indices_to_keep;
      indices_to_keep.resize (to_be_filtered->points.size ());

      pcl::PointCloud<float> filtered_points_depth;
      pcl::PointCloud<int> closest_idx_points;
      filtered_points_depth.points.resize (organized_cloud->points.size ());
      closest_idx_points.points.resize (organized_cloud->points.size ());

      filtered_points_depth.width = closest_idx_points.width = organized_cloud->width;
      filtered_points_depth.height = closest_idx_points.height = organized_cloud->height;
      for (size_t i = 0; i < filtered_points_depth.points.size (); i++)
      {
        filtered_points_depth.points[i] = std::numeric_limits<float>::quiet_NaN ();
        closest_idx_points.points[i] = -1;
      }

      int keep = 0;
      for (size_t i = 0; i < to_be_filtered->points.size (); i++)
      {
        float x = to_be_filtered->points[i].x;
        float y = to_be_filtered->points[i].y;
        float z = to_be_filtered->points[i].z;
        int u = static_cast<int> (f * x / z + cx);
        int v = static_cast<int> (f * y / z + cy);

        //Not out of bounds
        if ((u >= static_cast<int> (organized_cloud->width)) || (v >= static_cast<int> (organized_cloud->height)) || (u < 0) || (v < 0))
          continue;

        //Check for invalid depth
        if (!pcl_isfinite (organized_cloud->at (u, v).x) || !pcl_isfinite (organized_cloud->at (u, v).y)
            || !pcl_isfinite (organized_cloud->at (u, v).z))
          continue;

        float z_oc = organized_cloud->at (u, v).z;

        //Check if point depth (distance to camera) is greater than the (u,v)
        if ((z - z_oc) > threshold)
          continue;

        if (pcl_isnan(filtered_points_depth.at (u, v)) || (z < filtered_points_depth.at (u, v)))
        {
          closest_idx_points.at (u, v) = static_cast<int> (i);
          filtered_points_depth.at (u, v) = z;
        }

        //indices_to_keep[keep] = static_cast<int> (i);
        //keep++;
      }

      for (size_t i = 0; i < closest_idx_points.points.size (); i++)
      {
        if(closest_idx_points[i] != -1)
        {
          indices_to_keep[keep] = closest_idx_points[i];
          keep++;
        }
      }

      indices_to_keep.resize (keep);
      pcl::copyPointCloud (*to_be_filtered, indices_to_keep, *filtered);
      return filtered;
    }

    template<typename ModelT, typename SceneT> typename pcl::PointCloud<ModelT>::Ptr
    filter (typename pcl::PointCloud<SceneT>::Ptr & organized_cloud, typename pcl::PointCloud<ModelT>::Ptr & to_be_filtered, float f,
            float threshold, bool check_invalid_depth = true)
    {
      float cx = (static_cast<float> (organized_cloud->width) / 2.f - 0.5f);
      float cy = (static_cast<float> (organized_cloud->height) / 2.f - 0.5f);
      typename pcl::PointCloud<ModelT>::Ptr filtered (new pcl::PointCloud<ModelT> ());

      std::vector<int> indices_to_keep;
      indices_to_keep.resize (to_be_filtered->points.size ());

      int keep = 0;
      for (size_t i = 0; i < to_be_filtered->points.size (); i++)
      {
        float x = to_be_filtered->points[i].x;
        float y = to_be_filtered->points[i].y;
        float z = to_be_filtered->points[i].z;
        int u = static_cast<int> (f * x / z + cx);
        int v = static_cast<int> (f * y / z + cy);

        //Not out of bounds
        if ((u >= static_cast<int> (organized_cloud->width)) || (v >= static_cast<int> (organized_cloud->height)) || (u < 0) || (v < 0))
          continue;

        //Check for invalid depth
        if (check_invalid_depth)
        {
          if (!pcl_isfinite (organized_cloud->at (u, v).x) || !pcl_isfinite (organized_cloud->at (u, v).y)
              || !pcl_isfinite (organized_cloud->at (u, v).z))
            continue;
        }

        float z_oc = organized_cloud->at (u, v).z;

        //Check if point depth (distance to camera) is greater than the (u,v)
        if ((z - z_oc) > threshold)
          continue;

        indices_to_keep[keep] = static_cast<int> (i);
        keep++;
      }

      indices_to_keep.resize (keep);
      pcl::copyPointCloud (*to_be_filtered, indices_to_keep, *filtered);
      return filtered;
    }

    template<typename ModelT, typename SceneT> typename pcl::PointCloud<ModelT>::Ptr
    getOccludedCloud (typename pcl::PointCloud<SceneT>::Ptr & organized_cloud, typename pcl::PointCloud<ModelT>::Ptr & to_be_filtered, float f,
                      float threshold, bool check_invalid_depth = true)
    {
      float cx = (static_cast<float> (organized_cloud->width) / 2.f - 0.5f);
      float cy = (static_cast<float> (organized_cloud->height) / 2.f - 0.5f);
      typename pcl::PointCloud<ModelT>::Ptr filtered (new pcl::PointCloud<ModelT> ());

      std::vector<int> indices_to_keep;
      indices_to_keep.resize (to_be_filtered->points.size ());

      int keep = 0;
      for (size_t i = 0; i < to_be_filtered->points.size (); i++)
      {
        float x = to_be_filtered->points[i].x;
        float y = to_be_filtered->points[i].y;
        float z = to_be_filtered->points[i].z;
        int u = static_cast<int> (f * x / z + cx);
        int v = static_cast<int> (f * y / z + cy);

        //Out of bounds
        if ((u >= static_cast<int> (organized_cloud->width)) || (v >= static_cast<int> (organized_cloud->height)) || (u < 0) || (v < 0))
          continue;

        //Check for invalid depth
        if (check_invalid_depth)
        {
          if (!pcl_isfinite (organized_cloud->at (u, v).x) || !pcl_isfinite (organized_cloud->at (u, v).y)
              || !pcl_isfinite (organized_cloud->at (u, v).z))
            continue;
        }

        float z_oc = organized_cloud->at (u, v).z;

        //Check if point depth (distance to camera) is greater than the (u,v)
        if ((z - z_oc) > threshold)
        {
          indices_to_keep[keep] = static_cast<int> (i);
          keep++;
        }
      }

      indices_to_keep.resize (keep);
      pcl::copyPointCloud (*to_be_filtered, indices_to_keep, *filtered);
      return filtered;
    }
  }
}

#ifdef PCL_NO_PRECOMPILE
#include <faat_pcl/recognition/impl/hv/occlusion_reasoning.hpp>
#endif

#endif /* PCL_RECOGNITION_OCCLUSION_REASONING_H_ */
