#pragma once

#include "activities/Activity.h"
#include "activities/ScreenTransitionRefresh.h"

class UsbDriveActivity final : public Activity {
 public:
  UsbDriveActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("UsbDrive", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }
  bool requiresExclusiveStorageLoop() const override { return true; }

 private:
  enum class State { Unsupported, WaitingForHost, Connected, Accessed, Ejected, Disconnected, IoError };

  void restartToHome();
  void renderMessage(const char* message, const char* detail = nullptr) const;

  State state = State::Unsupported;
  bool preparing = true;
  bool restartRequested = false;
  ScreenTransitionRefresh screenTransitionRefresh;
};
