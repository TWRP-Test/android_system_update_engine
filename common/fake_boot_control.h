//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef UPDATE_ENGINE_COMMON_FAKE_BOOT_CONTROL_H_
#define UPDATE_ENGINE_COMMON_FAKE_BOOT_CONTROL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/time/time.h>

#include "update_engine/common/boot_control_interface.h"
#include "update_engine/common/dynamic_partition_control_stub.h"

namespace chromeos_update_engine {

// Implements a fake bootloader control interface used for testing.
class FakeBootControl : public BootControlInterface {
 public:
  FakeBootControl() {
    SetNumSlots(num_slots_);
    // The current slot should be bootable.
    is_bootable_[current_slot_] = true;

    dynamic_partition_control_.reset(new DynamicPartitionControlStub());
  }

  void SetDynamicPartitionControl(
      std::unique_ptr<DynamicPartitionControlInterface> dynamic_control) {
    dynamic_partition_control_ = std::move(dynamic_control);
  }

  // BootControlInterface overrides.
  unsigned int GetNumSlots() const override { return num_slots_; }
  BootControlInterface::Slot GetCurrentSlot() const override {
    return current_slot_;
  }

  bool GetPartitionDevice(const std::string& partition_name,
                          BootControlInterface::Slot slot,
                          bool not_in_payload,
                          std::string* device,
                          bool* is_dynamic) const override {
    auto dev =
        GetPartitionDevice(partition_name, slot, current_slot_, not_in_payload);
    if (!dev.has_value()) {
      return false;
    }
    if (is_dynamic) {
      *is_dynamic = dev->is_dynamic;
    }
    if (device) {
      *device = dev->rw_device_path;
    }
    return true;
  }

  bool GetPartitionDevice(const std::string& partition_name,
                          BootControlInterface::Slot slot,
                          std::string* device) const override {
    return GetPartitionDevice(partition_name, slot, false, device, nullptr);
  }

  bool IsSlotBootable(BootControlInterface::Slot slot) const override {
    return slot < num_slots_ && is_bootable_[slot];
  }

  bool MarkSlotUnbootable(BootControlInterface::Slot slot) override {
    if (slot >= num_slots_)
      return false;
    is_bootable_[slot] = false;
    return true;
  }

  bool SetActiveBootSlot(Slot slot) override { return true; }
  Slot GetActiveBootSlot() override { return kInvalidSlot; }

  bool MarkBootSuccessfulAsync(base::Callback<void(bool)> callback) override {
    // We run the callback directly from here to avoid having to setup a message
    // loop in the test environment.
    is_marked_successful_[GetCurrentSlot()] = true;
    callback.Run(true);
    return true;
  }

  bool IsSlotMarkedSuccessful(Slot slot) const override {
    return slot < num_slots_ && is_marked_successful_[slot];
  }

  // Setters
  void SetNumSlots(unsigned int num_slots) {
    num_slots_ = num_slots;
    is_bootable_.resize(num_slots_, false);
    is_marked_successful_.resize(num_slots_, false);
    devices_.resize(num_slots_);
  }

  void SetCurrentSlot(BootControlInterface::Slot slot) { current_slot_ = slot; }

  void SetPartitionDevice(const std::string& partition_name,
                          BootControlInterface::Slot slot,
                          const std::string& device) {
    DCHECK(slot < num_slots_);
    devices_[slot][partition_name] = device;
  }

  void SetSlotBootable(BootControlInterface::Slot slot, bool bootable) {
    DCHECK(slot < num_slots_);
    is_bootable_[slot] = bootable;
  }

  DynamicPartitionControlInterface* GetDynamicPartitionControl() override {
    return dynamic_partition_control_.get();
  }

  std::optional<PartitionDevice> GetPartitionDevice(
      const std::string& partition_name,
      uint32_t slot,
      uint32_t current_slot,
      bool not_in_payload = false) const override {
    if (slot >= devices_.size()) {
      return {};
    }
    auto device_path = devices_[slot].find(partition_name);
    if (device_path == devices_[slot].end()) {
      return {};
    }
    PartitionDevice device;
    device.is_dynamic = false;
    device.rw_device_path = device_path->second;
    device.readonly_device_path = device.rw_device_path;
    return device;
  }

 private:
  BootControlInterface::Slot num_slots_{2};
  BootControlInterface::Slot current_slot_{0};

  std::vector<bool> is_bootable_;
  std::vector<bool> is_marked_successful_;
  std::vector<std::map<std::string, std::string>> devices_;

  std::unique_ptr<DynamicPartitionControlInterface> dynamic_partition_control_;

  DISALLOW_COPY_AND_ASSIGN(FakeBootControl);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_COMMON_FAKE_BOOT_CONTROL_H_
