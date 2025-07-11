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

#ifndef UPDATE_ENGINE_AOSP_BOOT_CONTROL_ANDROID_H_
#define UPDATE_ENGINE_AOSP_BOOT_CONTROL_ANDROID_H_

#include <memory>
#include <string>

#include <liblp/builder.h>
#include <gtest/gtest_prod.h>
#include <BootControlClient.h>

#include "update_engine/aosp/dynamic_partition_control_android.h"
#include "update_engine/common/boot_control_interface.h"
#include "update_engine/common/dynamic_partition_control_interface.h"

namespace chromeos_update_engine {

// The Android implementation of the BootControlInterface. This implementation
// uses the libhardware's boot_control HAL to access the bootloader.
class BootControlAndroid final : public BootControlInterface {
 public:
  BootControlAndroid() = default;
  ~BootControlAndroid() = default;

  // Load boot_control HAL implementation using libhardware and
  // initializes it. Returns false if an error occurred.
  bool Init();

  // BootControlInterface overrides.
  unsigned int GetNumSlots() const override;
  BootControlInterface::Slot GetCurrentSlot() const override;
  std::optional<PartitionDevice> GetPartitionDevice(
      const std::string& partition_name,
      uint32_t slot,
      uint32_t current_slot,
      bool not_in_payload = false) const override;
  bool GetPartitionDevice(const std::string& partition_name,
                          BootControlInterface::Slot slot,
                          bool not_in_payload,
                          std::string* device,
                          bool* is_dynamic) const override;
  bool GetPartitionDevice(const std::string& partition_name,
                          BootControlInterface::Slot slot,
                          std::string* device) const override;
  bool IsSlotBootable(BootControlInterface::Slot slot) const override;
  bool MarkSlotUnbootable(BootControlInterface::Slot slot) override;
  bool SetActiveBootSlot(BootControlInterface::Slot slot) override;
  bool MarkBootSuccessfulAsync(base::Callback<void(bool)> callback) override;
  bool IsSlotMarkedSuccessful(BootControlInterface::Slot slot) const override;
  Slot GetActiveBootSlot() override;
  DynamicPartitionControlInterface* GetDynamicPartitionControl() override;

 private:
  std::unique_ptr<android::hal::BootControlClient> module_;
  std::unique_ptr<DynamicPartitionControlAndroid> dynamic_control_;

  friend class BootControlAndroidTest;
  friend class UpdateAttempterAndroidIntegrationTest;

  DISALLOW_COPY_AND_ASSIGN(BootControlAndroid);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_AOSP_BOOT_CONTROL_ANDROID_H_
