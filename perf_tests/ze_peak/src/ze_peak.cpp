/*
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "../include/ze_peak.h"

#include <algorithm>

#define ONE_KB (1 * 1024ULL)
#define EIGHT_KB (8 * ONE_KB)
#define ONE_MB (1 * 1024ULL * ONE_KB)
#define FOUR_GB (4 * 1024ULL * ONE_MB)

//---------------------------------------------------------------------
// Utility function to load the binary spv file from a path
// and return the file as a vector for use by L0.
//---------------------------------------------------------------------
std::vector<uint8_t> L0Context::load_binary_file(const std::string &file_path) {
  if (verbose)
    std::cout << "File path: " << file_path << "\n";
  std::ifstream stream(file_path, std::ios::in | std::ios::binary);

  std::vector<uint8_t> binary_file;
  if (!stream.good()) {
    std::cerr << "Failed to load binary file: " << file_path << "\n";
    return binary_file;
  }

  size_t length = 0;
  stream.seekg(0, stream.end);
  length = static_cast<size_t>(stream.tellg());
  stream.seekg(0, stream.beg);
  if (verbose)
    std::cout << "Binary file length: " << length << "\n";

  binary_file.resize(length);
  stream.read(reinterpret_cast<char *>(binary_file.data()), length);
  if (verbose)
    std::cout << "Binary file loaded"
              << "\n";

  return binary_file;
}

//---------------------------------------------------------------------
// Utility function to reset the Command List.
//---------------------------------------------------------------------
void L0Context::reset_commandlist(ze_command_list_handle_t cmd_list) {
  ze_result_t result = ZE_RESULT_SUCCESS;

  result = zeCommandListReset(cmd_list);
  if (result) {
    throw std::runtime_error("zeCommandListReset failed: " +
                             std::to_string(result));
  }
  if (verbose)
    std::cout << "Command list reset\n";
}

//---------------------------------------------------------------------
// Utility function to create the L0 module from a binary file.
// If successful, this function will set the context's module
// handle to a valid value for use in future calls.
// On error, an exception will be thrown describing the failure.
//---------------------------------------------------------------------
void L0Context::create_module(std::vector<uint8_t> binary_file) {
  ze_result_t result = ZE_RESULT_SUCCESS;
  ze_module_desc_t module_description = {};
  module_description.stype = ZE_STRUCTURE_TYPE_MODULE_DESC;

  module_description.pNext = nullptr;
  module_description.format = ZE_MODULE_FORMAT_IL_SPIRV;
  module_description.inputSize = static_cast<uint32_t>(binary_file.size());
  module_description.pInputModule =
      reinterpret_cast<const uint8_t *>(binary_file.data());
  module_description.pBuildFlags = nullptr;

  result =
      zeModuleCreate(context, device, &module_description, &module, nullptr);
  if (result) {
    throw std::runtime_error("zeDeviceCreateModule failed: " +
                             std::to_string(result));
  }
  if (verbose)
    std::cout << "Module created\n";
}

#define MAX_UUID_STRING_SIZE 49

static char hexdigit(int i) { return (i > 9) ? 'a' - 10 + i : '0' + i; }

static void generic_uuid_to_string(const uint8_t *id, int bytes, char *s) {
  int i;

  for (i = bytes - 1; i >= 0; --i) {
    *s++ = hexdigit(id[i] / 0x10);
    *s++ = hexdigit(id[i] % 0x10);
    if (i >= 6 && i <= 12 && (i & 1) == 0) {
      *s++ = '-';
    }
  }
  *s = '\0';
}

//---------------------------------------------------------------------
// Utility function to print the device properties from zeDeviceGetProperties.
//---------------------------------------------------------------------
void L0Context::print_ze_device_properties(
    const ze_device_properties_t &props) {

  ze_device_uuid_t uuid = props.uuid;
  char id[MAX_UUID_STRING_SIZE];
  generic_uuid_to_string(uuid.id, ZE_MAX_DEVICE_UUID_SIZE, id);

  std::cout << "Device : \n"
            << " * name : " << props.name << "\n"
            << " * vendorId : " << props.vendorId << "\n"
            << " * deviceId : " << props.deviceId << "\n"
            << " * subdeviceId : " << props.subdeviceId << "\n"
            << " * isSubdevice : "
            << ((props.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE) ? "TRUE"
                                                                  : "FALSE")
            << "\n"
            << " * UUID : " << id << "\n"
            << " * coreClockRate : " << props.coreClockRate << "\n"
            << std::endl;
}

//---------------------------------------------------------------------
// Utility function to initialize the ze driver, device, command list,
// command queue, & device property information.
// On error, an exception will be thrown describing the failure.
//---------------------------------------------------------------------
void L0Context::init_xe(uint32_t specified_platform,
                        uint32_t specified_device) {
  ze_command_list_desc_t command_list_description{};
  ze_command_queue_desc_t command_queue_description{};
  ze_result_t result = ZE_RESULT_SUCCESS;

  result = zeInit(0);
  if (result) {
    throw std::runtime_error("zeDriverInit failed: " + std::to_string(result));
  }
  if (verbose)
    std::cout << "Driver initialized\n";

  std::cout << "zeDriverGet...\n";
  uint32_t driver_count = 0;
  result = zeDriverGet(&driver_count, nullptr);
  if (result || driver_count == 0) {
    throw std::runtime_error("zeDriverGet failed: " + std::to_string(result));
  }

  /* Retrieve only one driver */
  driver_count = 1;
  result = zeDriverGet(&driver_count, &driver);
  if (result) {
    throw std::runtime_error("zeDriverGet failed: " + std::to_string(result));
  }

  /* Create a context to manage resources */
  ze_context_desc_t context_desc = {};
  context_desc.stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC;
  result = zeContextCreate(driver, &context_desc, &context);
  if (ZE_RESULT_SUCCESS != result) {
    throw std::runtime_error("zeContextCreate failed: " +
                             std::to_string(result));
  }

  device_count = 0;
  result = zeDeviceGet(driver, &device_count, nullptr);
  if (result || device_count == 0) {
    throw std::runtime_error("zeDeviceGet failed: " + std::to_string(result));
  }
  if (verbose)
    std::cout << "Device count retrieved: " << device_count << "\n";

  std::vector<ze_device_handle_t> devices(device_count);
  result = zeDeviceGet(driver, &device_count, devices.data());
  if (result) {
    throw std::runtime_error("zeDeviceGet failed: " + std::to_string(result));
  }
  if (verbose)
    std::cout << "Device retrieved\n";

  device = devices[0];
  if (specified_device >= device_count)
    std::cout << "Specified device " << specified_device
              << " is not valid, will default to the first device" << std::endl;
  else
    device = devices[specified_device];

  device_property.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
  device_property.pNext = nullptr;
  result = zeDeviceGetProperties(device, &device_property);
  if (result) {
    throw std::runtime_error("zeDeviceGetProperties failed: " +
                             std::to_string(result));
  }
  if (verbose)
    std::cout << "Device Properties retrieved\n";

  print_ze_device_properties(device_property);

  device_compute_property.stype = ZE_STRUCTURE_TYPE_DEVICE_COMPUTE_PROPERTIES;
  result = zeDeviceGetComputeProperties(device, &device_compute_property);
  if (result) {
    throw std::runtime_error("zeDeviceGetComputeProperties failed: " +
                             std::to_string(result));
  }
  if (verbose)
    std::cout << "Device Compute Properties retrieved\n";

  command_list_description.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
  command_list_description.pNext = nullptr;

  result = zeCommandListCreate(context, device, &command_list_description,
                               &command_list);
  if (result) {
    throw std::runtime_error("zeDeviceCreateCommandList failed: " +
                             std::to_string(result));
  }
  if (verbose)
    std::cout << "command_list created\n";

  command_queue_description.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
  command_queue_description.pNext = nullptr;
  command_queue_description.ordinal = command_queue_id;
  command_queue_description.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

  result = zeCommandQueueCreate(context, device, &command_queue_description,
                                &command_queue);
  if (result) {
    throw std::runtime_error("zeDeviceCreateCommandQueue failed: " +
                             std::to_string(result));
  }
  if (verbose)
    std::cout << "Command queue created\n";

  /* If device has copy engine, create corresponding resources */
  uint32_t command_queue_group_count = 0;
  zeDeviceGetCommandQueueGroupProperties(device, &command_queue_group_count,
                                         nullptr);

  std::vector<ze_command_queue_group_properties_t>
      command_queue_group_properties(command_queue_group_count);

  zeDeviceGetCommandQueueGroupProperties(device, &command_queue_group_count,
                                         command_queue_group_properties.data());

  uint32_t copy_ordinal = command_queue_group_count;
  for (uint32_t i = 0; i < command_queue_group_count; ++i) {
    if ((command_queue_group_properties[i].flags &
         ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY) &&
        !(command_queue_group_properties[i].flags &
          ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) &&
        (command_queue_group_properties[i].numQueues > 0)) {

      copy_ordinal = i;
      break;
    }
  }

  if (copy_ordinal == command_queue_group_count) {
    std::cout
        << "No async copy engines detected, disabling blitter benchmark\n";
  } else {
    std::cout << "Async copy engine detected, enabling blitter benchmark\n";

    command_list_description.commandQueueGroupOrdinal = copy_ordinal;

    result = zeCommandListCreate(context, device, &command_list_description,
                                 &copy_command_list);
    if (result) {
      throw std::runtime_error("zeCommandListCreate failed: " +
                               std::to_string(result));
    }
    if (verbose)
      std::cout << "copy command_list created\n";

    command_queue_description.ordinal = copy_ordinal;

    result = zeCommandQueueCreate(context, device, &command_queue_description,
                                  &copy_command_queue);
    if (result) {
      if (verbose) {
        std::cout << "Could not create copy-only command queue";
      }
    }
  }
}

//---------------------------------------------------------------------
// Utility function to close the command list & command queue.
// On error, an exception will be thrown describing the failure.
//---------------------------------------------------------------------
void L0Context::clean_xe() {
  ze_result_t result = ZE_RESULT_SUCCESS;

  result = zeCommandQueueDestroy(command_queue);
  if (result) {
    throw std::runtime_error("zeCommandQueueDestroy failed: " +
                             std::to_string(result));
  }
  if (verbose)
    std::cout << "Command queue destroyed\n";

  result = zeCommandListDestroy(command_list);
  if (result) {
    throw std::runtime_error("zeCommandListDestroy failed: " +
                             std::to_string(result));
  }
  if (verbose)
    std::cout << "command_list destroyed\n";

  /* Destroy Copy Resources */
  if (copy_command_queue) {
    result = zeCommandQueueDestroy(copy_command_queue);
    if (result) {
      throw std::runtime_error("zeCommandQueueDestroy failed: " +
                               std::to_string(result));
    }
    if (verbose)
      std::cout << "Copy command queue destroyed\n";
  }

  if (copy_command_list) {
    result = zeCommandListDestroy(copy_command_list);
    if (result) {
      throw std::runtime_error("zeCommandListDestroy failed: " +
                               std::to_string(result));
    }
    if (verbose)
      std::cout << "Copy command_list destroyed\n";
  }

  result = zeContextDestroy(context);
  if (result) {
    throw std::runtime_error("zeContextDestroy failed: " +
                             std::to_string(result));
  }
  if (verbose)
    std::cout << "Context destroyed";
}

//---------------------------------------------------------------------
// Utility function to execute the command list & synchronize
// the command queue. This function will reset the command list once the
// queue has been synchronized indicating that the commands in the command
// list have been completed.
// On error, an exception will be thrown describing the failure.
//---------------------------------------------------------------------
void L0Context::execute_commandlist_and_sync(bool use_copy_only_queue) {
  ze_result_t result = ZE_RESULT_SUCCESS;

  auto cmd_list = use_copy_only_queue ? copy_command_list : command_list;
  auto cmd_q = use_copy_only_queue ? copy_command_queue : command_queue;

  result = zeCommandListClose(cmd_list);
  if (result) {
    throw std::runtime_error("zeCommandListClose failed: " +
                             std::to_string(result));
  }
  if (verbose)
    std::cout << "Command list closed\n";

  result = zeCommandQueueExecuteCommandLists(cmd_q, 1, &cmd_list, nullptr);
  if (result) {
    throw std::runtime_error("zeCommandQueueExecuteCommandLists failed: " +
                             std::to_string(result));
  }
  if (verbose)
    std::cout << "Command list enqueued\n";

  result = zeCommandQueueSynchronize(cmd_q, UINT64_MAX);
  if (result) {
    throw std::runtime_error("zeCommandQueueSynchronize failed: " +
                             std::to_string(result));
  }
  if (verbose)
    std::cout << "Command queue synchronized\n";

  reset_commandlist(cmd_list);
}

//---------------------------------------------------------------------
// Utility function to total the current work items that would be
// executed given x,y,z sizes and x,y,z counts for the workgroups.
//---------------------------------------------------------------------
uint64_t total_current_work_items(uint64_t group_size_x, uint64_t group_count_x,
                                  uint64_t group_size_y, uint64_t group_count_y,
                                  uint64_t group_size_z,
                                  uint64_t group_count_z) {
  return (group_size_x * group_count_x * group_size_y * group_count_y *
          group_size_z * group_count_z);
}

//---------------------------------------------------------------------
// Utility function to set the workgroup dimensions based on the desired
// number of work items a user wants to execute.
// This will attempt to distribute the work items across the workgroup
// dimensions and get to as close to the work items requested as possible.
// Once the number of work items that would be executed is equal to or >
// the number of work items requested, then the workgroup information
// is set accordingly and the total work items that will execute is returned.
//---------------------------------------------------------------------
uint64_t ZePeak::set_workgroups(L0Context &context,
                                const uint64_t total_work_items_requested,
                                struct ZeWorkGroups *workgroup_info) {

  auto group_size_x =
      std::min(total_work_items_requested,
               uint64_t(context.device_compute_property.maxGroupSizeX));
  auto group_size_y = 1;
  auto group_size_z = 1;
  auto group_size = group_size_x * group_size_y * group_size_z;

  auto group_count_x = total_work_items_requested / group_size;
  group_count_x = std::min(
      group_count_x, uint64_t(context.device_compute_property.maxGroupCountX));
  auto remaining_items =
      total_work_items_requested - group_count_x * group_size;

  uint64_t group_count_y = remaining_items / (group_count_x * group_size);
  group_count_y = std::min(
      group_count_y, uint64_t(context.device_compute_property.maxGroupCountY));
  group_count_y = std::max(group_count_y, uint64_t(1));
  remaining_items =
      total_work_items_requested - group_count_x * group_count_y * group_size;

  uint64_t group_count_z =
      remaining_items / (group_count_x * group_count_y * group_size);
  group_count_z = std::min(
      group_count_z, uint64_t(context.device_compute_property.maxGroupCountZ));
  group_count_z = std::max(group_count_z, uint64_t(1));

  auto final_work_items =
      group_count_x * group_count_y * group_count_z * group_size;
  remaining_items = total_work_items_requested - final_work_items;

  if (verbose) {
    std::cout << "Group size x: " << group_size_x << "\n";
    std::cout << "Group size y: " << group_size_y << "\n";
    std::cout << "Group size z: " << group_size_z << "\n";
    std::cout << "Group count x: " << group_count_x << "\n";
    std::cout << "Group count y: " << group_count_y << "\n";
    std::cout << "Group count z: " << group_count_z << "\n";
  }

  if (verbose)
    std::cout << "total work items that will be executed: " << final_work_items
              << " requested: " << total_work_items_requested << "\n";

  workgroup_info->group_size_x = static_cast<uint32_t>(group_size_x);
  workgroup_info->group_size_y = static_cast<uint32_t>(group_size_y);
  workgroup_info->group_size_z = static_cast<uint32_t>(group_size_z);
  workgroup_info->thread_group_dimensions.groupCountX =
      static_cast<uint32_t>(group_count_x);
  workgroup_info->thread_group_dimensions.groupCountY =
      static_cast<uint32_t>(group_count_y);
  workgroup_info->thread_group_dimensions.groupCountZ =
      static_cast<uint32_t>(group_count_z);

  return final_work_items;
}

//---------------------------------------------------------------------
// Utility function to execute the command lists on the command queue.
// On error, an exception will be thrown describing the failure.
//---------------------------------------------------------------------
void ZePeak::run_command_queue(L0Context &context) {
  ze_result_t result = ZE_RESULT_SUCCESS;
  result = zeCommandQueueExecuteCommandLists(context.command_queue, 1,
                                             &context.command_list, nullptr);
  if (result) {
    throw std::runtime_error("zeCommandQueueExecuteCommandLists failed: " +
                             std::to_string(result));
  }
}

//---------------------------------------------------------------------
// Utility function to synchronize the command queue.
// On error, an exception will be thrown describing the failure.
//---------------------------------------------------------------------
void ZePeak::synchronize_command_queue(L0Context &context) {
  ze_result_t result = ZE_RESULT_SUCCESS;
  result = zeCommandQueueSynchronize(context.command_queue, UINT64_MAX);
  if (result) {
    throw std::runtime_error("zeCommandQueueSynchronize failed: " +
                             std::to_string(result));
  }
}

void single_event_pool_create(L0Context &context,
                              ze_event_pool_handle_t *kernel_launch_event_pool,
                              ze_event_pool_flags_t flags) {
  ze_result_t result;
  ze_event_pool_desc_t kernel_launch_event_pool_desc = {};
  kernel_launch_event_pool_desc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;

  kernel_launch_event_pool_desc.count = 1;
  kernel_launch_event_pool_desc.flags = flags;

  kernel_launch_event_pool_desc.pNext = nullptr;

  result = zeEventPoolCreate(context.context, &kernel_launch_event_pool_desc, 1,
                             &context.device, kernel_launch_event_pool);
  if (result) {
    throw std::runtime_error("zeEventPoolCreate failed: " +
                             std::to_string(result));
  }
}

void single_event_create(ze_event_pool_handle_t event_pool,
                         ze_event_handle_t *event) {
  ze_result_t result;
  ze_event_desc_t event_desc = {};
  event_desc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;

  event_desc.index = 0;
  event_desc.signal = 0;
  event_desc.wait = 0;

  event_desc.pNext = nullptr;
  result = zeEventCreate(event_pool, &event_desc, event);
  if (result) {
    throw std::runtime_error("zeEventCreate failed: " + std::to_string(result));
  }
}
//---------------------------------------------------------------------
// Utility function to execute a kernel function for a set of iterations
// and measure the time elapsed based off the timing type.
// This function takes a pre-calculated workgroup distribution
// and will time the kernel executed given the timing type.
// The current timing types supported are:
//          BANDWIDTH -> Average time to execute the kernel for # iterations
//          BANDWIDTH_EVENT_TIMING -> Average time to execute the kernel for #
//                                    iterations using Level Zero Events
//          KERNEL_LAUNCH_LATENCY->Average time to execute the kernel on
//                                  the command list
//          KERNEL_COMPLETE_LATENCY - Average time to execute a given kernel
//                                  for # iterations.
// On success, the average time in microseconds is returned.
// On error, an exception will be thrown describing the failure.
//---------------------------------------------------------------------
long double ZePeak::run_kernel(L0Context context, ze_kernel_handle_t &function,
                               struct ZeWorkGroups &workgroup_info,
                               TimingMeasurement type,
                               bool reset_command_list) {
  ze_result_t result = ZE_RESULT_SUCCESS;
  long double timed = 0;

  result = zeKernelSetGroupSize(function, workgroup_info.group_size_x,
                                workgroup_info.group_size_y,
                                workgroup_info.group_size_z);
  if (result) {
    throw std::runtime_error("zeKernelSetGroupSize failed: " +
                             std::to_string(result));
  }
  if (verbose)
    std::cout << "Group size set\n";

  Timer timer;

  if (type == TimingMeasurement::BANDWIDTH) {
    result = zeCommandListAppendLaunchKernel(
        context.command_list, function, &workgroup_info.thread_group_dimensions,
        nullptr, 0, nullptr);
    if (result) {
      throw std::runtime_error("zeCommandListAppendLaunchKernel failed: " +
                               std::to_string(result));
    }
    if (verbose)
      std::cout << "Function launch appended\n";

    result = zeCommandListClose(context.command_list);
    if (result) {
      throw std::runtime_error("zeCommandListClose failed: " +
                               std::to_string(result));
    }
    if (verbose)
      std::cout << "Command list closed\n";

    for (uint32_t i = 0; i < warmup_iterations; i++) {
      run_command_queue(context);
    }

    synchronize_command_queue(context);

    timer.start();
    for (uint32_t i = 0; i < iters; i++) {
      run_command_queue(context);
    }
    synchronize_command_queue(context);
    timed = timer.stopAndTime();
  } else if (type == TimingMeasurement::BANDWIDTH_EVENT_TIMING) {
    ze_event_pool_handle_t event_pool;
    ze_event_handle_t function_event;

    single_event_pool_create(context, &event_pool,
                             ZE_EVENT_POOL_FLAG_HOST_VISIBLE |
                                 ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
    if (verbose)
      std::cout << "Event Pool Created\n";

    single_event_create(event_pool, &function_event);
    if (verbose)
      std::cout << "Event Created\n";

    result = zeCommandListAppendLaunchKernel(
        context.command_list, function, &workgroup_info.thread_group_dimensions,
        function_event, 0, nullptr);
    if (result) {
      throw std::runtime_error("zeCommandListAppendLaunchKernel failed: " +
                               std::to_string(result));
    }
    if (verbose)
      std::cout << "Function launch appended\n";

    result = zeCommandListClose(context.command_list);
    if (result) {
      throw std::runtime_error("zeCommandListClose failed: " +
                               std::to_string(result));
    }
    if (verbose)
      std::cout << "Command list closed\n";

    for (uint32_t i = 0; i < warmup_iterations; i++) {
      result = zeCommandQueueExecuteCommandLists(
          context.command_queue, 1, &context.command_list, nullptr);
      if (result) {
        throw std::runtime_error("zeCommandQueueExecuteCommandLists failed: " +
                                 std::to_string(result));
      }

      result = zeEventHostSynchronize(function_event, UINT64_MAX);
      if (result) {
        throw std::runtime_error("zeEventHostSynchronize failed: " +
                                 std::to_string(result));
      }

      result = zeCommandQueueSynchronize(context.command_queue, UINT64_MAX);
      if (result) {
        throw std::runtime_error("zeCommandQueueSynchronize failed: " +
                                 std::to_string(result));
      }

      result = zeEventHostReset(function_event);
      if (result) {
        throw std::runtime_error("zeEventHostReset failed: " +
                                 std::to_string(result));
      }
      if (verbose)
        std::cout << "Event Reset" << std::endl;
    }

    for (uint32_t i = 0; i < iters; i++) {
      result = zeCommandQueueExecuteCommandLists(
          context.command_queue, 1, &context.command_list, nullptr);
      if (result) {
        throw std::runtime_error("zeCommandQueueExecuteCommandLists failed: " +
                                 std::to_string(result));
      }

      result = zeEventHostSynchronize(function_event, UINT64_MAX);
      if (result) {
        throw std::runtime_error("zeEventHostSynchronize failed: " +
                                 std::to_string(result));
      }

      timed += context_time_in_us(context, function_event);

      result = zeCommandQueueSynchronize(context.command_queue, UINT64_MAX);
      if (result) {
        throw std::runtime_error("zeCommandQueueSynchronize failed: " +
                                 std::to_string(result));
      }
      if (verbose)
        std::cout << "Command queue synchronized\n";

      result = zeEventHostReset(function_event);
      if (result) {
        throw std::runtime_error("zeEventHostReset failed: " +
                                 std::to_string(result));
      }
      if (verbose)
        std::cout << "Event Reset\n";
    }
    zeEventDestroy(function_event);
    zeEventPoolDestroy(event_pool);
  } else if (type == TimingMeasurement::KERNEL_LAUNCH_LATENCY) {
    ze_event_handle_t kernel_launch_event;
    ze_event_pool_handle_t kernel_launch_event_pool;

    single_event_pool_create(context, &kernel_launch_event_pool,
                             ZE_EVENT_POOL_FLAG_HOST_VISIBLE);
    if (verbose)
      std::cout << "Event Pool Created\n";

    single_event_create(kernel_launch_event_pool, &kernel_launch_event);
    if (verbose)
      std::cout << "Event Created\n";
    result = zeCommandListAppendSignalEvent(context.command_list,
                                            kernel_launch_event);
    if (result) {
      throw std::runtime_error("zeCommandListAppendSignalEvent failed: " +
                               std::to_string(result));
    }
    if (verbose)
      std::cout << "Kernel Launch Event signal appended to command list\n";

    result = zeCommandListAppendLaunchKernel(
        context.command_list, function, &workgroup_info.thread_group_dimensions,
        nullptr, 0, nullptr);
    if (result) {
      throw std::runtime_error("zeCommandListAppendLaunchKernel failed: " +
                               std::to_string(result));
    }
    if (verbose)
      std::cout << "Function launch appended\n";

    result = zeCommandListClose(context.command_list);
    if (result) {
      throw std::runtime_error("zeCommandListClose failed: " +
                               std::to_string(result));
    }
    if (verbose)
      std::cout << "Command list closed\n";

    for (uint32_t i = 0; i < warmup_iterations; i++) {
      run_command_queue(context);
      synchronize_command_queue(context);
      result = zeEventHostSynchronize(kernel_launch_event, UINT64_MAX);
      if (result) {
        throw std::runtime_error("zeEventHostSynchronize failed: " +
                                 std::to_string(result));
      }
      result = zeEventHostReset(kernel_launch_event);
      if (result) {
        throw std::runtime_error("zeEventHostReset failed: " +
                                 std::to_string(result));
      }
      if (verbose)
        std::cout << "Event Reset\n";
    }

    for (uint32_t i = 0; i < iters; i++) {
      timer.start();
      result = zeCommandQueueExecuteCommandLists(
          context.command_queue, 1, &context.command_list, nullptr);
      if (result) {
        throw std::runtime_error("zeCommandQueueExecuteCommandLists failed: " +
                                 std::to_string(result));
      }

      result = zeEventHostSynchronize(kernel_launch_event, UINT64_MAX);
      if (result) {
        throw std::runtime_error("zeEventHostSynchronize failed: " +
                                 std::to_string(result));
      }
      timed += timer.stopAndTime();

      result = zeCommandQueueSynchronize(context.command_queue, UINT64_MAX);
      if (result) {
        throw std::runtime_error("zeCommandQueueSynchronize failed: " +
                                 std::to_string(result));
      }
      if (verbose)
        std::cout << "Command queue synchronized\n";

      result = zeEventHostReset(kernel_launch_event);
      if (result) {
        throw std::runtime_error("zeEventHostReset failed: " +
                                 std::to_string(result));
      }
      if (verbose)
        std::cout << "Event Reset\n";
    }
    zeEventDestroy(kernel_launch_event);
    zeEventPoolDestroy(kernel_launch_event_pool);

  } else if (type == TimingMeasurement::KERNEL_COMPLETE_RUNTIME) {
    ze_event_pool_handle_t event_pool;
    ze_event_handle_t kernel_duration_event;

    single_event_pool_create(context, &event_pool,
                             ZE_EVENT_POOL_FLAG_HOST_VISIBLE |
                                 ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
    if (verbose)
      std::cout << "Event Pool Created\n";

    single_event_create(event_pool, &kernel_duration_event);
    if (verbose)
      std::cout << "Event Created\n";

    result = zeCommandListAppendLaunchKernel(
        context.command_list, function, &workgroup_info.thread_group_dimensions,
        kernel_duration_event, 0, nullptr);

    if (result) {
      throw std::runtime_error("zeCommandListAppendLaunchKernel failed: " +
                               std::to_string(result));
    }
    if (verbose)
      std::cout << "Function launch appended\n";

    result = zeCommandListClose(context.command_list);
    if (result) {
      throw std::runtime_error("zeCommandListClose failed: " +
                               std::to_string(result));
    }
    if (verbose)
      std::cout << "Command list closed\n";

    for (uint32_t i = 0; i < warmup_iterations; i++) {
      run_command_queue(context);
      result = zeEventHostSynchronize(kernel_duration_event, UINT64_MAX);
      if (result) {
        throw std::runtime_error("zeEventHostSynchronize failed: " +
                                 std::to_string(result));
      }
      result = zeEventHostReset(kernel_duration_event);
      if (result) {
        throw std::runtime_error("zeEventHostReset failed: " +
                                 std::to_string(result));
      }
    }

    synchronize_command_queue(context);

    for (uint32_t i = 0; i < iters; i++) {
      run_command_queue(context);
      synchronize_command_queue(context);

      result = zeEventHostSynchronize(kernel_duration_event, UINT64_MAX);
      if (result) {
        throw std::runtime_error("zeEventHostSynchronize failed: " +
                                 std::to_string(result));
      }

      timed += context_time_in_us(context, kernel_duration_event);

      result = zeEventHostReset(kernel_duration_event);
      if (result) {
        throw std::runtime_error("zeEventHostReset failed: " +
                                 std::to_string(result));
      }
    }

    zeEventDestroy(kernel_duration_event);
    zeEventPoolDestroy(event_pool);
  }
  if (reset_command_list)
    context.reset_commandlist(context.command_list);

  return (timed / static_cast<long double>(iters));
}

//---------------------------------------------------------------------
// Utility function to setup a kernel function with an input & output argument.
// On error, an exception will be thrown describing the failure.
//---------------------------------------------------------------------
void ZePeak::setup_function(L0Context &context, ze_kernel_handle_t &function,
                            const char *name, void *input, void *output,
                            size_t outputSize) {
  ze_kernel_desc_t function_description = {};
  function_description.stype = ZE_STRUCTURE_TYPE_KERNEL_DESC;
  ze_result_t result = ZE_RESULT_SUCCESS;

  function_description.pNext = nullptr;
  function_description.flags = 0;
  function_description.pKernelName = name;

  result = zeKernelCreate(context.module, &function_description, &function);
  if (result) {
    throw std::runtime_error("zeModuleCreateFunction failed: " +
                             std::to_string(result));
  }
  if (verbose)
    std::cout << "Function created\n";

  result = zeKernelSetArgumentValue(function, 0, sizeof(input), &input);
  if (result) {
    throw std::runtime_error("zeKernelSetArgumentValue failed: " +
                             std::to_string(result));
  }
  if (verbose)
    std::cout << "Input buffer set as function argument\n";

  // some kernels require scalar to be used on argument 1
  if (outputSize) {
    result = zeKernelSetArgumentValue(function, 1, outputSize, output);
  } else {
    result = zeKernelSetArgumentValue(function, 1, sizeof(output), &output);
  }

  if (result) {
    throw std::runtime_error("zeKernelSetArgumentValue failed: " +
                             std::to_string(result));
  }
  if (verbose)
    std::cout << "Output buffer set as function argument\n";
}

//---------------------------------------------------------------------
// Utility function to calculate the max work items that the current
// device can execute simultaneously with a single kernel function enqueued.
//---------------------------------------------------------------------
uint64_t ZePeak::get_max_work_items(L0Context &context) {
  return context.device_property.numSlices *
         context.device_property.numSubslicesPerSlice *
         context.device_property.numEUsPerSubslice *
         context.device_compute_property.maxGroupSizeX;
}

//---------------------------------------------------------------------
// Utility function to print a standard string to end a test.
//---------------------------------------------------------------------
void ZePeak::print_test_complete() {
  std::cout << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n";
}

//---------------------------------------------------------------------
// Main function which calls the argument parsing and calls each
// test requested.
//---------------------------------------------------------------------
int main(int argc, char **argv) {
  ZePeak peak_benchmark;
  L0Context context;

  peak_benchmark.parse_arguments(argc, argv);
  context.verbose = peak_benchmark.verbose;

  context.init_xe(peak_benchmark.specified_platform,
                  peak_benchmark.specified_device);

  if (peak_benchmark.run_global_bw)
    peak_benchmark.ze_peak_global_bw(context);

  if (peak_benchmark.run_hp_compute)
    peak_benchmark.ze_peak_hp_compute(context);

  if (peak_benchmark.run_sp_compute)
    peak_benchmark.ze_peak_sp_compute(context);

  if (peak_benchmark.run_dp_compute)
    peak_benchmark.ze_peak_dp_compute(context);

  if (peak_benchmark.run_int_compute)
    peak_benchmark.ze_peak_int_compute(context);

  if (peak_benchmark.run_transfer_bw)
    peak_benchmark.ze_peak_transfer_bw(context);

  if (peak_benchmark.run_kernel_lat)
    peak_benchmark.ze_peak_kernel_latency(context);

  context.clean_xe();

  std::cout << std::flush;

  return 0;
}

#if defined(unix) || defined(__unix__) || defined(__unix)

#include <unistd.h>

unsigned long long int total_available_memory() {
  const long page_count = sysconf(_SC_PHYS_PAGES);
  const long page_size = sysconf(_SC_PAGE_SIZE);
  const unsigned long long total_bytes = page_count * page_size;

  return total_bytes;
}

#endif

#if defined(_WIN64) || defined(_WIN64) || defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

unsigned long long int total_available_memory() {
  MEMORYSTATUSEX stat;
  stat.dwLength = sizeof(stat);
  GlobalMemoryStatusEx(&stat);

  return stat.ullAvailVirtual;
}

#endif

TimingMeasurement ZePeak::is_bandwidth_with_event_timer(void) {
  if (use_event_timer) {
    return TimingMeasurement::BANDWIDTH_EVENT_TIMING;
  } else {
    return TimingMeasurement::BANDWIDTH;
  }
}

long double ZePeak::calculate_gbps(long double period,
                                   long double buffer_size) {
  return buffer_size / period / 1e3f;
}

long double ZePeak::context_time_in_us(L0Context &context,
                                       ze_event_handle_t &event) {
  ze_result_t result = ZE_RESULT_SUCCESS;
  long double context_time_ns = 0;
  ze_kernel_timestamp_result_t ts_result = {};

  result = zeEventQueryKernelTimestamp(event, &ts_result);
  if (result) {
    throw std::runtime_error("zeEventQueryKernelTimeStamp failed: " +
                             std::to_string(result));
  }

  const uint64_t timestamp_freq = context.device_property.timerResolution;
  const uint64_t timestamp_max_value =
      ~(-1 << context.device_property.kernelTimestampValidBits);
  context_time_ns =
      (ts_result.context.kernelEnd >= ts_result.context.kernelStart)
          ? (ts_result.context.kernelEnd - ts_result.context.kernelStart) *
                (double)timestamp_freq
          : ((timestamp_max_value - ts_result.context.kernelStart) +
             ts_result.context.kernelEnd + 1) *
                (double)timestamp_freq;

  return (context_time_ns / 1000); // time is returned in microseconds
}
