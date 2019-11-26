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

#include "test_harness/test_harness.hpp"

#include "ze_api.h"
#include "utils/utils.hpp"

namespace lzt = level_zero_tests;

namespace level_zero_tests {

uint32_t get_freq_handle_count(ze_device_handle_t device, uint32_t pCount) {
  zet_sysman_handle_t hSysmanDevice = lzt::get_sysman_handle(device);
  EXPECT_EQ(ZE_RESULT_SUCCESS,
            zetSysmanFrequencyGet(hSysmanDevice, &pCount, nullptr));
  EXPECT_GT(pCount, 0);
  return pCount;
}

std::vector<zet_sysman_freq_handle_t>
get_freq_handles(ze_device_handle_t device) {
  uint32_t pCount = get_freq_handle_count(device);
  zet_sysman_handle_t hSysmanDevice = lzt::get_sysman_handle(device);
  std::vector<zet_sysman_freq_handle_t> pFreqHandles(pCount);
  EXPECT_EQ(ZE_RESULT_SUCCESS,
            zetSysmanFrequencyGet(hSysmanDevice, &pCount, pFreqHandles.data()));
  return pFreqHandles;
}

}; // namespace level_zero_tests