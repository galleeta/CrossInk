#include "PinLockActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <string>
#include <utility>

#include "MappedInputManager.h"
#include "PinStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int kBoxWidth = 64;
constexpr int kBoxHeight = 84;
constexpr int kBoxGap = 16;
}  // namespace

PinLockActivity::PinLockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, UnlockCallback onUnlock)
    : Activity("PinLock", renderer, mappedInput), onUnlock(std::move(onUnlock)) {}

void PinLockActivity::onEnter() {
  Activity::onEnter();
  digits.fill(0);
  position = 0;
  wrongPin = false;
  unlocked = false;
  requestUpdate();
}

void PinLockActivity::adjustDigit(const int delta) {
  wrongPin = false;
  digits[position] = static_cast<uint8_t>((static_cast<int>(digits[position]) + delta + 10) % 10);
}

void PinLockActivity::confirmDigit() {
  wrongPin = false;
  if (position + 1 < static_cast<int>(PinStore::PIN_LENGTH)) {
    position++;
    digits[position] = 0;
    requestUpdate();
    return;
  }

  std::string attempt;
  for (const uint8_t d : digits) attempt.push_back(static_cast<char>('0' + d));

  if (PIN_STORE.matches(attempt)) {
    unlock();
    return;
  }

  wrongPin = true;
  position = 0;
  digits.fill(0);
  requestUpdate();
}

void PinLockActivity::unlock() {
  if (unlocked) return;
  unlocked = true;
  // Move the callback out first: it queues the next activity, which will
  // destroy this one, and the captured state must outlive that.
  UnlockCallback callback = std::move(onUnlock);
  if (callback) callback();
}

void PinLockActivity::loop() {
  if (unlocked) return;

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (position > 0) {
      position--;
      wrongPin = false;
      requestUpdate();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    confirmDigit();
    return;
  }

  buttonNavigator.onNextRelease([this] {
    adjustDigit(+1);
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this] {
    adjustDigit(-1);
    requestUpdate();
  });
  buttonNavigator.onNextContinuous([this] {
    adjustDigit(+1);
    requestUpdate();
  });
  buttonNavigator.onPreviousContinuous([this] {
    adjustDigit(-1);
    requestUpdate();
  });
}

void PinLockActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const int count = static_cast<int>(PinStore::PIN_LENGTH);
  const int totalWidth = count * kBoxWidth + (count - 1) * kBoxGap;
  const int startX = (renderer.getScreenWidth() - totalWidth) / 2;
  const int boxY = renderer.getScreenHeight() / 2 - kBoxHeight / 2;
  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);

  const char* title = "Enter PIN";
  const int titleWidth = renderer.getTextWidth(UI_12_FONT_ID, title, EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID, (renderer.getScreenWidth() - titleWidth) / 2, boxY - lineHeight * 3, title, true,
                    EpdFontFamily::BOLD);

  for (int i = 0; i < count; i++) {
    const int x = startX + i * (kBoxWidth + kBoxGap);
    const bool current = i == position;

    renderer.fillRectDither(x, boxY, kBoxWidth, kBoxHeight, current ? Color::LightGray : Color::White);
    renderer.drawRect(x, boxY, kBoxWidth, kBoxHeight, true);
    if (current) renderer.drawRect(x + 1, boxY + 1, kBoxWidth - 2, kBoxHeight - 2, true);

    // Confirmed digits are masked; only the digit being edited is visible.
    char text[2] = {0, 0};
    if (i < position) {
      text[0] = '*';
    } else if (current) {
      text[0] = static_cast<char>('0' + digits[i]);
    } else {
      continue;
    }
    const int textWidth = renderer.getTextWidth(UI_12_FONT_ID, text, EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, x + (kBoxWidth - textWidth) / 2, boxY + (kBoxHeight - lineHeight) / 2, text, true,
                      EpdFontFamily::BOLD);
  }

  if (wrongPin) {
    const char* message = "Wrong PIN, try again";
    const int messageWidth = renderer.getTextWidth(UI_12_FONT_ID, message, EpdFontFamily::REGULAR);
    renderer.drawText(UI_12_FONT_ID, (renderer.getScreenWidth() - messageWidth) / 2, boxY + kBoxHeight + lineHeight,
                      message, true, EpdFontFamily::REGULAR);
  }

  const auto labels =
      mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
