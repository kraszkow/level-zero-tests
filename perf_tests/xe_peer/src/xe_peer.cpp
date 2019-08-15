/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2016 - 2019 Intel Corporation. All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material contains trade secrets and proprietary
 * and confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and
 * treaty provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or
 * disclosed in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 */

#include "xe_peer.h"

#include "common.hpp"
#include "xe_api.h"
#include "xe_app.hpp"

#include <assert.h>
#include <iomanip>
#include <iostream>

class XePeer {
public:
  XePeer() {
    benchmark = new XeApp("xe_peer_benchmarks.spv");

    device_group_count = benchmark->deviceGroupCount();
    assert(device_group_count > 0);

    /* Retrieve device group number 1 */
    device_group_count = 1;
    benchmark->deviceGroupGet(&device_group_count, &device_group);

    /* Obtain device count */
    device_count = benchmark->deviceCount(device_group);
    devices = new std::vector<xe_device_handle_t>(device_count);

    /* Retrieve array of devices in device group */
    benchmark->deviceGroupGetDevices(device_group, device_count,
                                     devices->data());

    device_contexts = new std::vector<device_context_t>(device_count);

    for (uint32_t i = 0; i < device_count; i++) {
      xe_device_handle_t device;
      xe_module_handle_t module;
      xe_command_queue_handle_t command_queue;
      device_context_t *device_context = &device_contexts->at(i);

      device = devices->at(i);
      benchmark->moduleCreate(device, &module);
      benchmark->commandQueueCreate(device, 0 /* command_queue_id */,
                                    &command_queue);
      device_context->device = device;
      device_context->module = module;
      device_context->command_queue = command_queue;
      benchmark->commandListCreate(device, &device_context->command_list);
    }
  }

  ~XePeer() {
    for (uint32_t i = 0; i < device_count; i++) {
      device_context_t *device_context = &device_contexts->at(i);
      benchmark->moduleDestroy(device_context->module);
      benchmark->commandQueueDestroy(device_context->command_queue);
      benchmark->commandListDestroy(device_context->command_list);
    }

    delete benchmark;
    delete device_contexts;
    delete devices;
  }

  void bandwidth(bool bidirectional, peer_transfer_t transfer_type);
  void latency(bool bidirectional, peer_transfer_t transfer_type);

private:
  XeApp *benchmark;
  xe_device_group_handle_t device_group;
  uint32_t device_group_count;
  uint32_t device_count;
  std::vector<device_context_t> *device_contexts;
  std::vector<xe_device_handle_t> *devices;

  void _copy_function_setup(xe_module_handle_t module,
                            xe_function_handle_t &function,
                            const char *function_name, uint32_t globalSizeX,
                            uint32_t globalSizeY, uint32_t globalSizeZ,
                            uint32_t &group_size_x, uint32_t &group_size_y,
                            uint32_t &group_size_z);
  void _copy_function_cleanup(xe_function_handle_t function);
};

void XePeer::_copy_function_setup(xe_module_handle_t module,
                                  xe_function_handle_t &function,
                                  const char *function_name,
                                  uint32_t globalSizeX, uint32_t globalSizeY,
                                  uint32_t globalSizeZ, uint32_t &group_size_x,
                                  uint32_t &group_size_y,
                                  uint32_t &group_size_z) {
  group_size_x = 0;
  group_size_y = 0;
  group_size_z = 0;

  benchmark->functionCreate(module, &function, function_name);

  SUCCESS_OR_TERMINATE(xeFunctionSuggestGroupSize(
      function, globalSizeX, globalSizeY, globalSizeZ, &group_size_x,
      &group_size_y, &group_size_z));
  SUCCESS_OR_TERMINATE(xeFunctionSetGroupSize(function, group_size_x,
                                              group_size_y, group_size_z));
}

void XePeer::_copy_function_cleanup(xe_function_handle_t function) {
  benchmark->functionDestroy(function);
}

void XePeer::bandwidth(bool bidirectional, peer_transfer_t transfer_type) {
  int number_iterations = 5;
  int warm_up_iterations = 5;
  int number_buffer_elements = 10000000;
  std::vector<void *> buffers(device_count, nullptr);
  size_t element_size = sizeof(unsigned long int);
  size_t buffer_size = element_size * number_buffer_elements;

  for (uint32_t i = 0; i < device_count; i++) {
    device_context_t *device_context = &device_contexts->at(i);

    benchmark->memoryAlloc(device_group, device_context->device, buffer_size,
                           &buffers[i]);
  }

  for (uint32_t i = 0; i < device_count; i++) {
    device_context_t *device_context_i = &device_contexts->at(i);
    xe_function_handle_t function_a = nullptr;
    xe_function_handle_t function_b = nullptr;
    void *buffer_i = buffers.at(i);
    xe_command_list_handle_t command_list_a = device_context_i->command_list;
    xe_command_queue_handle_t command_queue_a = device_context_i->command_queue;
    xe_thread_group_dimensions_t thread_group_dimensions;
    uint32_t group_size_x;
    uint32_t group_size_y;
    uint32_t group_size_z;

    _copy_function_setup(device_context_i->module, function_a,
                         "single_copy_peer_to_peer", number_buffer_elements, 1,
                         1, group_size_x, group_size_y, group_size_z);
    if (bidirectional) {
      _copy_function_setup(device_context_i->module, function_b,
                           "single_copy_peer_to_peer", number_buffer_elements,
                           1, 1, group_size_x, group_size_y, group_size_z);
    }

    const int group_count_x = number_buffer_elements / group_size_x;
    thread_group_dimensions.groupCountX = group_count_x;
    thread_group_dimensions.groupCountY = 1;
    thread_group_dimensions.groupCountZ = 1;

    for (uint32_t j = 0; j < device_count; j++) {
      int64_t total_time_usec;
      double total_time_s;
      double total_data_transfer;
      double total_bandwidth;
      Timer<std::chrono::microseconds> timer;
      void *buffer_j = buffers.at(j);

      if (bidirectional) {
        /* PEER_WRITE */
        SUCCESS_OR_TERMINATE(
            xeFunctionSetArgumentValue(function_a, 0, /* Destination buffer*/
                                       sizeof(buffer_j), &buffer_j));
        SUCCESS_OR_TERMINATE(
            xeFunctionSetArgumentValue(function_a, 1, /* Source buffer */
                                       sizeof(buffer_i), &buffer_i));
        /* PEER_READ */
        SUCCESS_OR_TERMINATE(
            xeFunctionSetArgumentValue(function_b, 0, /* Destination buffer*/
                                       sizeof(buffer_i), &buffer_i));
        SUCCESS_OR_TERMINATE(
            xeFunctionSetArgumentValue(function_b, 1, /* Source buffer */
                                       sizeof(buffer_j), &buffer_j));

        xeCommandListAppendLaunchFunction(command_list_a, function_a,
                                          &thread_group_dimensions, nullptr, 0,
                                          nullptr);
        xeCommandListAppendLaunchFunction(command_list_a, function_b,
                                          &thread_group_dimensions, nullptr, 0,
                                          nullptr);
      } else { /* unidirectional */
        if (transfer_type == PEER_WRITE) {
          SUCCESS_OR_TERMINATE(
              xeFunctionSetArgumentValue(function_a, 0, /* Destination buffer*/
                                         sizeof(buffer_j), &buffer_j));
          SUCCESS_OR_TERMINATE(
              xeFunctionSetArgumentValue(function_a, 1, /* Source buffer */
                                         sizeof(buffer_i), &buffer_i));
        } else if (transfer_type == PEER_READ) {
          SUCCESS_OR_TERMINATE(
              xeFunctionSetArgumentValue(function_a, 0, /* Destination buffer*/
                                         sizeof(buffer_i), &buffer_i));
          SUCCESS_OR_TERMINATE(
              xeFunctionSetArgumentValue(function_a, 1, /* Source buffer */
                                         sizeof(buffer_j), &buffer_j));
        } else {
          std::cerr
              << "ERROR: Bandwidth test - transfer type parameter is invalid"
              << std::endl;
          std::terminate();
        }

        xeCommandListAppendLaunchFunction(command_list_a, function_a,
                                          &thread_group_dimensions, nullptr, 0,
                                          nullptr);
      }
      benchmark->commandListClose(command_list_a);

      /* Warm up */
      for (int i = 0; i < warm_up_iterations; i++) {
        benchmark->commandQueueExecuteCommandList(command_queue_a, 1,
                                                  &command_list_a);
        benchmark->commandQueueSynchronize(command_queue_a);
      }

      timer.start();
      for (int i = 0; i < number_iterations; i++) {
        benchmark->commandQueueExecuteCommandList(command_queue_a, 1,
                                                  &command_list_a);
        benchmark->commandQueueSynchronize(command_queue_a);
      }
      timer.end();

      total_time_usec = timer.period_minus_overhead();
      total_time_s = total_time_usec / static_cast<double>(1e6);

      total_data_transfer = (buffer_size * number_iterations) /
                            static_cast<double>(1e9); /* Units in Gigabytes */
      if (bidirectional) {
        total_data_transfer = 2 * total_data_transfer;
      }
      total_bandwidth = total_data_transfer / total_time_s;

      if (bidirectional) {
        std::cout << std::setprecision(11) << std::setw(8) << " Device(" << i
                  << ")<->Device(" << j << "): "
                  << " GBPS " << total_bandwidth << std::endl;
      } else {
        if (transfer_type == PEER_WRITE) {
          std::cout << std::setprecision(11) << std::setw(8) << " Device(" << i
                    << ")->Device(" << j << "): "
                    << " GBPS " << total_bandwidth << std::endl;
        } else {
          std::cout << std::setprecision(11) << std::setw(8) << " Device(" << i
                    << ")<-Device(" << j << "): "
                    << " GBPS " << total_bandwidth << std::endl;
        }
      }
    }
    _copy_function_cleanup(function_a);
    if (bidirectional) {
      _copy_function_cleanup(function_b);
    }
  }

  for (void *buffer : buffers) {
    benchmark->memoryFree(device_group, buffer);
  }
}

void XePeer::latency(bool bidirectional, peer_transfer_t transfer_type) {
  int number_iterations = 100;
  int warm_up_iterations = 5;
  int number_buffer_elements = 1;
  std::vector<void *> buffers(device_count, nullptr);
  size_t element_size = sizeof(unsigned long int);
  size_t buffer_size = element_size * number_buffer_elements;

  for (uint32_t i = 0; i < device_count; i++) {
    device_context_t *device_context = &device_contexts->at(i);

    benchmark->memoryAlloc(device_group, device_context->device, buffer_size,
                           &buffers[i]);
  }

  for (uint32_t i = 0; i < device_count; i++) {
    device_context_t *device_context_i = &device_contexts->at(i);
    xe_function_handle_t function_a = nullptr;
    xe_function_handle_t function_b = nullptr;
    void *buffer_i = buffers.at(i);
    xe_command_list_handle_t command_list_a = device_context_i->command_list;
    xe_command_queue_handle_t command_queue_a = device_context_i->command_queue;
    xe_thread_group_dimensions_t thread_group_dimensions;
    uint32_t group_size_x;
    uint32_t group_size_y;
    uint32_t group_size_z;

    _copy_function_setup(device_context_i->module, function_a,
                         "single_copy_peer_to_peer", number_buffer_elements, 1,
                         1, group_size_x, group_size_y, group_size_z);
    if (bidirectional) {
      _copy_function_setup(device_context_i->module, function_b,
                           "single_copy_peer_to_peer", number_buffer_elements,
                           1, 1, group_size_x, group_size_y, group_size_z);
    }

    const int group_count_x = number_buffer_elements / group_size_x;
    thread_group_dimensions.groupCountX = group_count_x;
    thread_group_dimensions.groupCountY = 1;
    thread_group_dimensions.groupCountZ = 1;

    for (uint32_t j = 0; j < device_count; j++) {
      double total_time_usec;
      Timer<std::chrono::microseconds> timer;
      void *buffer_j = buffers.at(j);

      if (bidirectional) {
        /* PEER_WRITE */
        SUCCESS_OR_TERMINATE(
            xeFunctionSetArgumentValue(function_a, 0, /* Destination buffer*/
                                       sizeof(buffer_j), &buffer_j));
        SUCCESS_OR_TERMINATE(
            xeFunctionSetArgumentValue(function_a, 1, /* Source buffer */
                                       sizeof(buffer_i), &buffer_i));
        /* PEER_READ */
        SUCCESS_OR_TERMINATE(
            xeFunctionSetArgumentValue(function_b, 0, /* Destination buffer*/
                                       sizeof(buffer_i), &buffer_i));
        SUCCESS_OR_TERMINATE(
            xeFunctionSetArgumentValue(function_b, 1, /* Source buffer */
                                       sizeof(buffer_j), &buffer_j));

        xeCommandListAppendLaunchFunction(command_list_a, function_a,
                                          &thread_group_dimensions, nullptr, 0,
                                          nullptr);
        xeCommandListAppendLaunchFunction(command_list_a, function_b,
                                          &thread_group_dimensions, nullptr, 0,
                                          nullptr);
      } else { /* unidirectional */
        if (transfer_type == PEER_WRITE) {
          SUCCESS_OR_TERMINATE(
              xeFunctionSetArgumentValue(function_a, 0, /* Destination buffer*/
                                         sizeof(buffer_j), &buffer_j));
          SUCCESS_OR_TERMINATE(
              xeFunctionSetArgumentValue(function_a, 1, /* Source buffer */
                                         sizeof(buffer_i), &buffer_i));
        } else if (transfer_type == PEER_READ) {
          SUCCESS_OR_TERMINATE(
              xeFunctionSetArgumentValue(function_a, 0, /* Destination buffer*/
                                         sizeof(buffer_i), &buffer_i));
          SUCCESS_OR_TERMINATE(
              xeFunctionSetArgumentValue(function_a, 1, /* Source buffer */
                                         sizeof(buffer_j), &buffer_j));
        } else {
          std::cerr
              << "ERROR: Latency test - transfer type parameter is invalid"
              << std::endl;
          std::terminate();
        }

        xeCommandListAppendLaunchFunction(command_list_a, function_a,
                                          &thread_group_dimensions, nullptr, 0,
                                          nullptr);
      }
      benchmark->commandListClose(command_list_a);

      /* Warm up */
      for (int i = 0; i < warm_up_iterations; i++) {
        benchmark->commandQueueExecuteCommandList(command_queue_a, 1,
                                                  &command_list_a);
        benchmark->commandQueueSynchronize(command_queue_a);
      }

      timer.start();
      for (int i = 0; i < number_iterations; i++) {
        benchmark->commandQueueExecuteCommandList(command_queue_a, 1,
                                                  &command_list_a);
        benchmark->commandQueueSynchronize(command_queue_a);
      }
      timer.end();

      total_time_usec = timer.period_minus_overhead() /
                        static_cast<double>(number_iterations);

      if (bidirectional) {
        std::cout << std::setprecision(11) << std::setw(8) << " Device(" << i
                  << ")<->Device(" << j << "): " << total_time_usec << " uS"
                  << std::endl;
      } else {
        if (transfer_type == PEER_WRITE) {
          std::cout << std::setprecision(11) << std::setw(8) << " Device(" << i
                    << ")->Device(" << j << "): " << total_time_usec << " uS"
                    << std::endl;
        } else {
          std::cout << std::setprecision(11) << std::setw(8) << " Device(" << i
                    << ")<-Device(" << j << "): " << total_time_usec << " uS"
                    << std::endl;
        }
      }
    }
    _copy_function_cleanup(function_a);
    if (bidirectional) {
      _copy_function_cleanup(function_b);
    }
  }

  for (void *buffer : buffers) {
    benchmark->memoryFree(device_group, buffer);
  }
}

int main(int argc, char **argv) {
  XePeer peer;

  std::cout << "Unidirectional Bandwidth P2P Write" << std::endl;
  peer.bandwidth(false /* unidirectional */, PEER_WRITE);
  std::cout << std::endl;

  std::cout << "Unidirectional Bandwidth P2P Read" << std::endl;
  peer.bandwidth(false /* unidirectional */, PEER_READ);
  std::cout << std::endl;

  std::cout << "Bidirectional Bandwidth P2P Write" << std::endl;
  peer.bandwidth(true /* bidirectional */, PEER_NONE);
  std::cout << std::endl;

  std::cout << "Unidirectional Bandwidth P2P Write" << std::endl;
  peer.latency(false /* unidirectional */, PEER_WRITE);
  std::cout << std::endl;

  std::cout << "Unidirectional Bandwidth P2P Read" << std::endl;
  peer.latency(false /* unidirectional */, PEER_READ);
  std::cout << std::endl;

  std::cout << "Bidirectional Bandwidth P2P Write" << std::endl;
  peer.latency(true /* bidirectional */, PEER_NONE);
  std::cout << std::endl;

  return 0;
}