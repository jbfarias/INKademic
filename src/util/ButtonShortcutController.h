#pragma once

#include <cstdint>

#include "QuickLockState.h"
#include "QuickLockTrigger.h"

// Owns the Power + side-button chord latch.  Once a chord fires, both buttons
// must be released before another shortcut can fire from the same hold.
class ButtonShortcutController {
 public:
  enum class ChordAction : uint8_t {
    Screenshot = 0,
    QuickLock = 1,
    Disabled = 4,
    Sleep = 5,
    PageTurn = 6,
    ToggleBookmark = 7,
    ReadingStats = 8,
    MarkFinished = 9,
    ForceRefresh = 10,
    ToggleFont = 11,
    ToggleGuideDots = 12,
    ToggleBionicReading = 13,
    CyclePageTurn = 14,
    SyncProgress = 15,
    FileTransfer = 16,
    CalibreWireless = 17,
    JoinNetwork = 18,
    CreateHotspot = 19,
    ToggleDarkMode = 20,
    Footnotes = 21,
    FileBrowser = 22,
    CreateClipping = 23,
    LookupWord = 24,
    ToggleHomeButton = 25,
    QuickActions = 26,
    ToggleFrontlight = 27,
    ToggleTouchscreen = 28,
  };

  enum class Event : uint8_t { None, QuickLockChanged, Screenshot, PageTurn, ConfiguredAction };

  struct Result {
    Event event = Event::None;
    bool consumeInput = false;
    ChordAction action = ChordAction::Disabled;
  };

  Result update(uint32_t nowMs, bool powerPressed, bool chordButtonPressed, bool shortPowerRelease,
                bool quickLockOnShortPower, ChordAction action) {
    if (chordActive_) {
      if (!powerPressed && !chordButtonPressed) chordActive_ = false;
      return {Event::None, true};
    }

    if (powerPressed && chordButtonPressed && action != ChordAction::Disabled) {
      chordActive_ = true;
      if (quickLockState_.isLocked() &&
          (action != ChordAction::QuickLock || quickLockTrigger_ != QuickLockTrigger::PowerUp)) {
        return {Event::None, true};
      }
      switch (action) {
        case ChordAction::Screenshot:
          return {Event::Screenshot, true};
        case ChordAction::QuickLock:
          toggleQuickLock(nowMs, QuickLockTrigger::PowerUp);
          return {Event::QuickLockChanged, true};
        case ChordAction::PageTurn:
          return {Event::PageTurn, true};
        case ChordAction::Disabled:
          break;
        default:
          return {Event::ConfiguredAction, true, action};
      }
    }

    if (shortPowerRelease) {
      if ((quickLockState_.isLocked() && quickLockTrigger_ == QuickLockTrigger::ShortPower) ||
          (!quickLockState_.isLocked() && quickLockOnShortPower)) {
        toggleQuickLock(nowMs, QuickLockTrigger::ShortPower);
        return {Event::QuickLockChanged, true};
      }
    }
    return {Event::None, quickLockState_.isLocked()};
  }

  bool isQuickLocked() const { return quickLockState_.isLocked(); }
  QuickLockTrigger quickLockTrigger() const { return quickLockTrigger_; }
  bool isChordActive() const { return chordActive_; }
  void toggleQuickLock(uint32_t nowMs, QuickLockTrigger trigger, bool requireRelease = false) {
    const bool locked = quickLockState_.toggle(nowMs);
    quickLockTrigger_ = locked ? trigger : QuickLockTrigger::None;
    unlockTriggerReleased_ = !locked || !requireRelease;
  }
  bool tryUnlockLongPower(uint32_t nowMs, bool longPowerPressed) {
    if (!quickLockState_.isLocked() || quickLockTrigger_ != QuickLockTrigger::LongPower) return false;
    if (!unlockTriggerReleased_) {
      unlockTriggerReleased_ = !longPowerPressed;
      return false;
    }
    if (!longPowerPressed) return false;
    toggleQuickLock(nowMs, QuickLockTrigger::LongPower, true);
    return true;
  }
  void restoreQuickLock(uint32_t nowMs, QuickLockTrigger trigger) {
    if (!quickLockState_.isLocked()) {
      // Older state files did not retain the activating shortcut. Keep their
      // established Power-release escape route for this one migration wake.
      toggleQuickLock(nowMs, trigger == QuickLockTrigger::None ? QuickLockTrigger::ShortPower : trigger,
                      trigger == QuickLockTrigger::LongPower);
    }
  }
  bool shouldQuickLockSleep(uint32_t nowMs, uint32_t timeoutMs) const {
    return quickLockState_.shouldSleep(nowMs, timeoutMs);
  }

 private:
  QuickLockState quickLockState_;
  QuickLockTrigger quickLockTrigger_ = QuickLockTrigger::None;
  bool chordActive_ = false;
  bool unlockTriggerReleased_ = true;
};
