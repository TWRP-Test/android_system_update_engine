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

#include <xz.h>

#include <string>
#include <vector>

#include <base/command_line.h>
#include <base/strings/string_split.h>
#include <android-base/stringprintf.h>
#include <brillo/asynchronous_signal_handler.h>
#include <brillo/flag_helper.h>
#include <brillo/message_loops/base_message_loop.h>
#include <brillo/streams/file_stream.h>
#include <brillo/streams/stream.h>

#include "update_engine/aosp/update_attempter_android.h"
#include "update_engine/common/boot_control.h"
#include "update_engine/common/error_code_utils.h"
#include "update_engine/common/hardware.h"
#include "update_engine/common/logging.h"
#include "update_engine/common/prefs.h"
#include "update_engine/common/subprocess.h"
#include "update_engine/common/terminator.h"
#include "update_engine/common/utils.h"

using std::string;
using std::vector;
using update_engine::UpdateEngineStatus;
using update_engine::UpdateStatus;

namespace chromeos_update_engine {
namespace {

class SideloadDaemonState : public DaemonStateInterface,
                            public ServiceObserverInterface {
 public:
  explicit SideloadDaemonState(brillo::StreamPtr status_stream)
      : status_stream_(std::move(status_stream)) {
    // Add this class as the only observer.
    observers_.insert(this);
  }
  ~SideloadDaemonState() override = default;

  // DaemonStateInterface overrides.
  bool StartUpdater() override { return true; }
  void AddObserver(ServiceObserverInterface* observer) override {}
  void RemoveObserver(ServiceObserverInterface* observer) override {}
  const std::set<ServiceObserverInterface*>& service_observers() override {
    return observers_;
  }

  // ServiceObserverInterface overrides.
  void SendStatusUpdate(
      const UpdateEngineStatus& update_engine_status) override {
    UpdateStatus status = update_engine_status.status;
    double progress = update_engine_status.progress;
    if (status_ != status && (status == UpdateStatus::DOWNLOADING ||
                              status == UpdateStatus::FINALIZING)) {
      // Split the progress bar in two parts for the two stages DOWNLOADING and
      // FINALIZING.
      ReportStatus(android::base::StringPrintf(
          "ui_print Step %d/2", status == UpdateStatus::DOWNLOADING ? 1 : 2));
      ReportStatus(android::base::StringPrintf("progress 0.5 0"));
    }
    if (status_ != status || fabs(progress - progress_) > 0.005) {
      ReportStatus(android::base::StringPrintf("set_progress %.2lf", progress));
    }
    progress_ = progress;
    status_ = status;
  }

  void SendPayloadApplicationComplete(ErrorCode error_code) override {
    if (error_code != ErrorCode::kSuccess) {
      ReportStatus(android::base::StringPrintf(
          "ui_print Error applying update: %d (%s)",
          error_code,
          utils::ErrorCodeToString(error_code).c_str()));
    }
    error_code_ = error_code;
    brillo::MessageLoop::current()->BreakLoop();
  }

  // Getters.
  UpdateStatus status() { return status_; }
  ErrorCode error_code() { return error_code_; }

 private:
  // Report a status message in the status_stream_, if any. These messages
  // should conform to the specification defined in the Android recovery.
  void ReportStatus(const string& message) {
    if (!status_stream_)
      return;
    string status_line = message + "\n";
    status_stream_->WriteAllBlocking(
        status_line.data(), status_line.size(), nullptr);
  }

  std::set<ServiceObserverInterface*> observers_;
  brillo::StreamPtr status_stream_;

  // The last status and error code reported.
  UpdateStatus status_{UpdateStatus::IDLE};
  ErrorCode error_code_{ErrorCode::kSuccess};
  double progress_{-1.};
};

// Apply an update payload directly from the given payload URI.
ErrorCode ApplyUpdatePayload(const string& payload,
                        int64_t payload_offset,
                        int64_t payload_size,
                        const vector<string>& headers,
                        int64_t status_fd) {
  brillo::BaseMessageLoop loop;
  loop.SetAsCurrent();

  // Setup the subprocess handler.
  brillo::AsynchronousSignalHandler handler;
  handler.Init();
  Subprocess subprocess;
  subprocess.Init(&handler);

  SideloadDaemonState sideload_daemon_state(
      brillo::FileStream::FromFileDescriptor(status_fd, true, nullptr));

  // During the sideload we don't access the prefs persisted on disk but instead
  // use a temporary memory storage.
  MemoryPrefs prefs;

  std::unique_ptr<BootControlInterface> boot_control =
      boot_control::CreateBootControl();
  if (!boot_control) {
    LOG(ERROR) << "Error initializing the BootControlInterface.";
    return ErrorCode::kError;
  }

  std::unique_ptr<HardwareInterface> hardware = hardware::CreateHardware();
  if (!hardware) {
    LOG(ERROR) << "Error initializing the HardwareInterface.";
    return ErrorCode::kError;
  }

  UpdateAttempterAndroid update_attempter(&sideload_daemon_state,
                                          &prefs,
                                          boot_control.get(),
                                          hardware.get(),
                                          nullptr);
  update_attempter.Init();

  if (!update_attempter.ApplyPayload(
          payload, payload_offset, payload_size, headers, nullptr)) {
    LOG(ERROR) << "Error attempting the ApplyPayload.";
  }

  loop.Run();
  return sideload_daemon_state.error_code();
}

}  // namespace
}  // namespace chromeos_update_engine

int main(int argc, char** argv) {
  DEFINE_string(payload,
                "file:///data/payload.bin",
                "The URI to the update payload to use.");
  DEFINE_int64(
      offset, 0, "The offset in the payload where the CrAU update starts. ");
  DEFINE_int64(size,
               0,
               "The size of the CrAU part of the payload. If 0 is passed, it "
               "will be autodetected.");
  DEFINE_string(headers,
                "",
                "A list of key-value pairs, one element of the list per line.");
  DEFINE_int64(status_fd, -1, "A file descriptor to notify the update status.");

  chromeos_update_engine::Terminator::Init();
  chromeos_update_engine::SetupLogging(true /* stderr */, false /* file */);
  brillo::FlagHelper::Init(argc, argv, "Update Engine Sideload");

  LOG(INFO) << "Update Engine Sideloading starting";

  // xz-embedded requires to initialize its CRC-32 table once on startup.
  xz_crc32_init();

  vector<string> headers = base::SplitString(
      FLAGS_headers, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  return static_cast<int>(chromeos_update_engine::ApplyUpdatePayload(
      FLAGS_payload, FLAGS_offset, FLAGS_size, headers, FLAGS_status_fd));
}
