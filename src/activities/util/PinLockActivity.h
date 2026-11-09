#pragma once

#include <FreeInkUIGfxRenderer.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Full-screen 4-digit PIN prompt shown on boot / wake when a PIN is configured.
//
// Touch devices (X4 Pro): a numeric keypad. Tap the digits; the PIN is checked
// automatically after the fourth one. "Del" removes the last digit.
//
// Devices without touch: Up/Down (or Left/Right) change the current digit,
// Confirm accepts it and moves on, Back returns to the previous digit.
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

  // Touch mode: digits[0..position-1] are the digits entered so far.
  // Button mode: digits[position] is the digit currently being edited.
  std::array<uint8_t, 4> digits{};
  uint8_t position = 0;
  bool wrongPin = false;
  bool unlocked = false;

  void handleKeyboardValue(int16_t value);
  void enterDigit(uint8_t digit);
  void deleteDigit();
  void adjustDigit(int delta);
  void confirmDigit();
  void submit();
  void unlock();
};
