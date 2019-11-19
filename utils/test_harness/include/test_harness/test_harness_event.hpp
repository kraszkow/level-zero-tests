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

#ifndef level_zero_tests_ZE_TEST_HARNESS_EVENT_HPP
#define level_zero_tests_ZE_TEST_HARNESS_EVENT_HPP

#include "test_harness/test_harness.hpp"
#include "ze_api.h"

namespace lzt = level_zero_tests;

namespace level_zero_tests {

class zeEventPool {
public:
  zeEventPool();
  ~zeEventPool();

  // By default, an event pool is created with 32 events and default flags
  // during the first call to create_events().  To change the default behavior
  // call InitEventPool() with any other values BEFORE calling
  // create_events().
  void InitEventPool();
  void InitEventPool(uint32_t count);
  void InitEventPool(uint32_t count, ze_event_pool_flag_t flags);
  void InitEventPool(ze_event_pool_desc_t desc);
  void InitEventPool(ze_event_pool_desc_t desc,
                     std::vector<ze_device_handle_t> devices);

  void create_event(ze_event_handle_t &event);
  void create_event(ze_event_handle_t &event, ze_event_scope_flag_t signal,
                    ze_event_scope_flag_t wait);
  void create_event(ze_event_handle_t &event, ze_event_desc_t desc);

  void create_events(std::vector<ze_event_handle_t> &events,
                     size_t event_count);
  void create_events(std::vector<ze_event_handle_t> &events, size_t event_count,
                     ze_event_scope_flag_t signal, ze_event_scope_flag_t wait);

  void destroy_event(ze_event_handle_t event);
  void destroy_events(std::vector<ze_event_handle_t> &events);

  void get_ipc_handle(ze_ipc_event_pool_handle_t *hIpc);

  ze_event_pool_handle_t event_pool_ = nullptr;
  std::vector<bool> pool_indexes_available_;
  std::map<ze_event_handle_t, uint32_t> handle_to_index_map_;
};

void signal_event_from_host(ze_event_handle_t hEvent);

ze_event_pool_handle_t create_event_pool(ze_event_pool_desc_t desc);
ze_event_pool_handle_t
create_event_pool(ze_event_pool_desc_t desc,
                  std::vector<ze_device_handle_t> devices);
void open_ipc_event_handle(ze_ipc_event_pool_handle_t hIpc,
                           ze_event_pool_handle_t *eventPool);
void close_ipc_event_handle(ze_event_pool_handle_t eventPool);

class zeEventPoolTests : public ::testing::Test {
protected:
  zeEventPool ep;
};

}; // namespace level_zero_tests

#endif