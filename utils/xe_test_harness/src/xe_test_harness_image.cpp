/*
 * INTEL CONFIDENTIAL
 *
 * Copyright 2019 Intel Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code (Material) are owned by Intel Corporation or its
 * suppliers or licensors.
 *
 * Title to the Material remains with Intel Corporation or its suppliers and
 * licensors. The Material contains trade secrets and proprietary and
 * confidential information of Intel or its suppliers and licensors.
 * The Material is protected by worldwide copyright and trade secret laws and
 * treaty provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed,
 * or disclosed in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure
 * or delivery of the Materials, either expressly, by implication, inducement,
 * estoppel or otherwise. Any license under such intellectual property rights
 * must be express and approved by Intel in writing.
 */

#include "xe_test_harness/xe_test_harness.hpp"
#include "gtest/gtest.h"
#include "logging/logging.hpp"
#include "xe_utils/xe_utils.hpp"

namespace cs = compute_samples;

namespace compute_samples {

void create_xe_image(xe_image_handle_t &image,
                     xe_image_desc_t *image_descriptor) {
  EXPECT_EQ(XE_RESULT_SUCCESS,
            xeImageCreate(cs::xeDevice::get_instance()->get_device(),
                          image_descriptor, &image));
  EXPECT_NE(nullptr, image);
}

void create_xe_image(xe_image_handle_t &image) {
  xe_image_desc_t descriptor;
  descriptor.version = XE_IMAGE_DESC_VERSION_CURRENT;

  create_xe_image(image, &descriptor);
}

void destroy_xe_image(xe_image_handle_t image) {
  EXPECT_EQ(XE_RESULT_SUCCESS, xeImageDestroy(image));
}

void generate_xe_image_creation_flags_list(
    std::vector<xe_image_flag_t> &image_creation_flags_list) {
  for (auto image_rw_flag_a : cs::image_creation_rw_flags) {
    for (auto image_rw_flag_b : cs::image_creation_rw_flags) {
      for (auto image_cached_flag : cs::image_creation_cached_flags) {
        xe_image_flag_t image_creation_flags =
            static_cast<xe_image_flag_t>(static_cast<int>(image_rw_flag_a) |
                                         static_cast<int>(image_rw_flag_b) |
                                         static_cast<int>(image_cached_flag));
        image_creation_flags_list.push_back(image_creation_flags);
      }
    }
  }
}

xeImageCreateCommon::xeImageCreateCommon() : dflt_host_image_(128, 128) {
  cs::generate_xe_image_creation_flags_list(image_creation_flags_list_);
  xe_image_desc_t image_desc;
  image_desc.version = XE_IMAGE_DESC_VERSION_CURRENT;
  image_desc.format.layout = XE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
  image_desc.flags = XE_IMAGE_FLAG_PROGRAM_READ;
  image_desc.type = XE_IMAGE_TYPE_2D;
  image_desc.format.type = XE_IMAGE_FORMAT_TYPE_UNORM;
  image_desc.format.x = XE_IMAGE_FORMAT_SWIZZLE_R;
  image_desc.format.y = XE_IMAGE_FORMAT_SWIZZLE_G;
  image_desc.format.z = XE_IMAGE_FORMAT_SWIZZLE_B;
  image_desc.format.w = XE_IMAGE_FORMAT_SWIZZLE_A;
  image_desc.width = dflt_host_image_.width();
  image_desc.height = dflt_host_image_.height();
  image_desc.depth = 1;

  create_xe_image(dflt_device_image_, &image_desc);
  create_xe_image(dflt_device_image_2_, &image_desc);
}

xeImageCreateCommon::~xeImageCreateCommon() {
  destroy_xe_image(dflt_device_image_);
  destroy_xe_image(dflt_device_image_2_);
}

void print_image_format_descriptor(const xe_image_format_desc_t descriptor) {
  LOG_DEBUG << "   LAYOUT = " << descriptor.layout
            << "   TYPE = " << descriptor.type << "   X = " << descriptor.x
            << "   Y = " << descriptor.y << "   Z = " << descriptor.z
            << "   w = " << descriptor.w;
}

void print_image_descriptor(const xe_image_desc_t descriptor) {
  LOG_DEBUG << "VERSION = " << descriptor.version
            << "   FLAGS = " << descriptor.flags
            << "   TYPE = " << descriptor.type;
  print_image_format_descriptor(descriptor.format);
  LOG_DEBUG << "   WIDTH = " << descriptor.width
            << "   HEIGHT = " << descriptor.height
            << "   DEPTH = " << descriptor.depth
            << "   ARRAYLEVELS = " << descriptor.arraylevels
            << "   MIPLEVELS = " << descriptor.miplevels;
}

}; // namespace compute_samples