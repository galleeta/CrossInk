#pragma once

#include <FreeInkUIGfxRenderer.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Full-screen PIN prompt shown on boot / wake when a PIN is configured.
//
// Touch devices (X4 Pro): a numeric keypad with Clear / 0 / OK on the last row.
// Tap the digits, then OK. Clear erases everything typed so far. The field
// shows one "*" per digit, or nothing at all if "show_dots" is false in
// pin.json.
//
// Devices without touch: Up/Down (or Left/Right) cycle through 0-9 and OK,
// Confirm adds the shown digit (or submits on OK), Back removes the last digit.
//
// On success the callback launches the normal start-up screen (home or last
// book). This screen never calls finish(): popping the last activity would go
// Home and skip the lock.
class PinLockActivity final : public Activity {
 public:
  using UnlockCallback = std::function<void()>;

  PinLockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, UnlockCallback onUnlock);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

  // Keep every global shortcut / gesture (Home key, chords, frontlight panel,
  // screenshot...) from acting behind the lock screen.
  bool blocksGlobalInput() const override { return true; }
  bool allowGlobalHomeGesture() const override { return false; }
  bool allowGlobalHomeSwipeGesture() const override { return false; }
  bool allowFrontlightPanelGesture() const override { return false; }

 private:
  UnlockCallback onUnlock;
  ButtonNavigator buttonNavigator;
  freeink::ui::InteractionBuffer<48> keyboardInteractions;
  freeink::ui::TouchHoldRouter keyboardTouchRouter;
  std::atomic<bool> keyboardInteractionsReady{false};

  std::string entry;       // digits typed so far, as characters '0'-'9'
  uint8_t candidate = 0;   // button mode only: 0-9 = digit, 10 = OK
  bool showDots = true;    // touch mode: draw one mark per typed digit
  bool wrongPin = false;
  bool unlocked = false;

  void handleKeyboardValue(int16_t value);
  void enterDigit(uint8_t digit);
  void clearEntry();
  void adjustCandidate(int delta);
  void confirmCandidate();
  void submit();
  void unlock();
};
