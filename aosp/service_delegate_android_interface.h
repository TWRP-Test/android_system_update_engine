//
// Copyright (C) 2016 The Android Open Source Project
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

#ifndef UPDATE_ENGINE_AOSP_SERVICE_DELEGATE_ANDROID_INTERFACE_H_
#define UPDATE_ENGINE_AOSP_SERVICE_DELEGATE_ANDROID_INTERFACE_H_

#include <memory>
#include <string>
#include <vector>

#include "update_engine/common/error.h"

namespace chromeos_update_engine {

// See ServiceDelegateAndroidInterface.CleanupSuccessfulUpdate
// Wraps a IUpdateEngineCallback binder object used specifically for
// CleanupSuccessfulUpdate.
class CleanupSuccessfulUpdateCallbackInterface {
 public:
  virtual ~CleanupSuccessfulUpdateCallbackInterface() {}
  virtual void OnCleanupProgressUpdate(double progress) = 0;
  virtual void OnCleanupComplete(int32_t error_code) = 0;
  // Call RegisterForDeathNotifications on the internal binder object.
  virtual void RegisterForDeathNotifications(
      const std::function<void()>& unbind) = 0;
};

// This class defines the interface exposed by the Android version of the
// daemon service. This interface only includes the method calls that such
// daemon exposes. For asynchronous events initiated by a class implementing
// this interface see the ServiceObserverInterface class.
class ServiceDelegateAndroidInterface {
 public:
  virtual ~ServiceDelegateAndroidInterface() = default;

  // Start an update attempt to download an apply the provided |payload_url| if
  // no other update is running. The extra |key_value_pair_headers| will be
  // included when fetching the payload. Returns whether the update was started
  // successfully, which means that no other update was running and the passed
  // parameters were correct, but not necessarily that the update finished
  // correctly.
  virtual bool ApplyPayload(
      const std::string& payload_url,
      int64_t payload_offset,
      int64_t payload_size,
      const std::vector<std::string>& key_value_pair_headers,
      Error* error) = 0;

  virtual bool ApplyPayload(
      int fd,
      int64_t payload_offset,
      int64_t payload_size,
      const std::vector<std::string>& key_value_pair_headers,
      Error* error) = 0;

  virtual bool TriggerPostinstall(const std::string& partition,
                                  Error* error) = 0;

  // Suspend an ongoing update. Returns true if there was an update ongoing and
  // it was suspended. In case of failure, it returns false and sets |error|
  // accordingly.
  virtual bool SuspendUpdate(Error* error) = 0;

  // Resumes an update suspended with SuspendUpdate(). The update can't be
  // suspended after it finished and this method will fail in that case.
  // Returns whether the resume operation was successful, which only implies
  // that there was a suspended update. In case of error, returns false and sets
  // |error| accordingly.
  virtual bool ResumeUpdate(Error* error) = 0;

  // Cancel the ongoing update. The update could be running or suspended, but it
  // can't be canceled after it was done. In case of error, returns false and
  // sets |error| accordingly.
  virtual bool CancelUpdate(Error* error) = 0;

  // Reset the already applied update back to an idle state. This method can
  // only be called when no update attempt is going on, and it will reset the
  // status back to idle, deleting the currently applied update if any. In case
  // of error, returns false and sets |error| accordingly.
  virtual bool ResetStatus(Error* error) = 0;

  // Verifies whether a payload (delegated by the payload metadata) can be
  // applied to the current device. Returns whether the payload is applicable.
  // In case of error, returns false and sets |error| accordingly.
  virtual bool VerifyPayloadApplicable(const std::string& metadata_filename,
                                       Error* error) = 0;
  // Sets the A/B slot switch for the next boot after applying an ota update.
  // If applyPayload hasn't switched the slot by itself, the client can call
  // this API to switch the slot and apply the update on next boot. Returns
  // true on success.
  virtual bool setShouldSwitchSlotOnReboot(const std::string& metadata_filename,
                                           Error* error) = 0;

  // Resets the boot slot to the source/current slot, without cancelling the
  // update progress. This can be called after the update is installed, and to
  // prevent the device from accidentally taking the update when it reboots.
  virtual bool resetShouldSwitchSlotOnReboot(Error* error) = 0;

  // Allocates space for a payload.
  // Returns 0 if space is successfully preallocated.
  // Return non-zero if not enough space is not available; returned value is
  // the total space required (in bytes) to be free on the device for this
  // update to be applied, and |error| is unset.
  // In case of error, returns 0, and sets |error| accordingly.
  //
  // This function may block for several minutes in the worst case.
  virtual uint64_t AllocateSpaceForPayload(
      const std::string& metadata_filename,
      const std::vector<std::string>& key_value_pair_headers,
      Error* error) = 0;

  // Wait for merge to complete, then clean up merge after an update has been
  // successful.
  //
  // This function returns immediately. Progress updates are provided in
  // |callback|.
  virtual void CleanupSuccessfulUpdate(
      std::unique_ptr<CleanupSuccessfulUpdateCallbackInterface> callback,
      Error* error) = 0;

 protected:
  ServiceDelegateAndroidInterface() = default;
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_AOSP_SERVICE_DELEGATE_ANDROID_INTERFACE_H_
