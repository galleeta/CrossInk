#include "PinLockActivity.h"

#include <FreeInkUIGfxRenderer.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <string>
#include <utility>

#include "MappedInputManager.h"
#include "PinStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
namespace fui = freeink::ui;

constexpr int kBoxWidth = 64;
constexpr int kBoxHeight = 84;
constexpr int kBoxGap = 16;
constexpr int kKeyboardRows = 4;
constexpr fui::ActionId kKeyboardAction = 1;
constexpr int16_t kKeyboardBackspace = -1;

// Same geometry as the keypad of the frontlight time picker.
fui::Rect keyboardRect(const GfxRenderer& renderer) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int gap = metrics.keyboardKeySpacing;
  const int height = kKeyboardRows * metrics.keyboardKeyHeight + (kKeyboardRows - 1) * gap;
  const int width = renderer.getScreenWidth() * metrics.keyboardWidthPercent / 100;
  const int x = (renderer.getScreenWidth() - width) / 2;
  const int y = renderer.getScreenHeight() - metrics.buttonHintsHeight - metrics.verticalSpacing - height +
                metrics.keyboardVerticalOffset;
  return {static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(width), static_cast<int16_t>(height)};
}
}  // namespace

PinLockActivity::PinLockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, UnlockCallback onUnlock)
    : Activity("PinLock", renderer, mappedInput), onUnlock(std::move(onUnlock)) {}

void PinLockActivity::onEnter() {
  Activity::onEnter();
  digits.fill(0);
  position = 0;
  wrongPin = false;
  unlocked = false;
  keyboardTouchRouter.overrideValue = kKeyboardBackspace;
  keyboardTouchRouter.reset();
  keyboardInteractionsReady.store(false, std::memory_order_release);
  requestUpdate();
}

// ---- input handling -------------------------------------------------------

void PinLockActivity::handleKeyboardValue(const int16_t value) {
  if (value >= 0 && value <= 9) {
    enterDigit(static_cast<uint8_t>(value));
  } else if (value == kKeyboardBackspace) {
    deleteDigit();
  }
}

void PinLockActivity::enterDigit(const uint8_t digit) {
  if (position >= PinStore::PIN_LENGTH) return;
  wrongPin = false;
  digits[position] = digit;
  position++;
  if (position >= PinStore::PIN_LENGTH) {
    submit();
  } else {
    requestUpdate();
  }
}

void PinLockActivity::deleteDigit() {
  if (position == 0) return;
  wrongPin = false;
  position--;
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
  position = static_cast<uint8_t>(PinStore::PIN_LENGTH);
  submit();
}

void PinLockActivity::submit() {
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

  if (mappedInput.hasTouch()) {
    // Touch keypad (same routing as the time picker keypad).
    if (keyboardInteractionsReady.load(std::memory_order_acquire)) {
      int tapX = 0;
      int tapY = 0;
      const bool tapped = mappedInput.wasScreenTapped(tapX, tapY);
      unsigned long heldMs = 0;
      int candidateX = 0;
      int candidateY = 0;
      const bool tapCandidate = mappedInput.isScreenTouchTapCandidate(candidateX, candidateY, heldMs);
      int heldX = 0;
      int heldY = 0;
      const bool inContact = mappedInput.isScreenTouchHeld(heldX, heldY);
      const fui::TouchHoldRouter::Result result = keyboardTouchRouter.update(
          keyboardInteractions, tapCandidate, static_cast<int16_t>(candidateX), static_cast<int16_t>(candidateY),
          tapped, static_cast<int16_t>(tapX), static_cast<int16_t>(tapY), inContact, millis());
      if (result.event) {
        handleKeyboardValue(result.event.value);
        return;
      }
      if (result.activeChanged) requestUpdate();
    }
    return;
  }

  // No touch hardware: physical buttons.
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

// ---- drawing --------------------------------------------------------------

void PinLockActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const bool touch = mappedInput.hasTouch();
  const int count = static_cast<int>(PinStore::PIN_LENGTH);
  const int totalWidth = count * kBoxWidth + (count - 1) * kBoxGap;
  const int startX = (renderer.getScreenWidth() - totalWidth) / 2;
  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);

  // With the keypad, centre the boxes in the space above it.
  const int areaHeight = touch ? keyboardRect(renderer).y : renderer.getScreenHeight();
  const int boxY = areaHeight / 2 - kBoxHeight / 2 + (touch ? lineHeight : 0);

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

    // Entered digits are masked. In button mode the digit being edited is shown.
    char text[2] = {0, 0};
    if (i < position) {
      text[0] = '*';
    } else if (current && !touch) {
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

  if (touch) {
    fui::GfxRendererTarget target(renderer);
    target.setFont(fui::GfxRendererTarget::FONT_SMALL, SMALL_FONT_ID);
    target.setFont(fui::GfxRendererTarget::FONT_BODY, UI_12_FONT_ID);
    const fui::DeviceContext device = target.deviceContext();
    const fui::InputSnapshot noInput{};
    keyboardInteractions.beginPublishCycle();
    fui::Frame<48> frame(target, device, noInput, keyboardInteractions);

    static constexpr const char* kDigitLabels[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
    fui::KeyGridKey keys[12];
    for (int i = 0; i < 9; ++i) {
      keys[i].label = kDigitLabels[i + 1];
      keys[i].value = static_cast<int16_t>(i + 1);
    }
    keys[9].label = "Del";
    keys[9].value = kKeyboardBackspace;
    keys[10].label = kDigitLabels[0];
    keys[10].value = 0;
    keys[11].kind = fui::KeyKind::Disabled;

    fui::KeyGridProps props;
    props.keys = keys;
    props.rows = kKeyboardRows;
    props.cols = 3;
    props.action = kKeyboardAction;
    props.inputMask = static_cast<uint16_t>(fui::InputTouch | fui::InputLongPress);
    props.labelText.font = fui::GfxRendererTarget::FONT_BODY;
    const auto& metrics = UITheme::getInstance().getMetrics();
    props.gap = static_cast<int16_t>(metrics.keyboardKeySpacing);
    fui::keyGrid(frame, keyboardRect(renderer), props);
    keyboardInteractions.publish();
    keyboardInteractionsReady.store(true, std::memory_order_release);
  } else {
    const auto labels = mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), tr(STR_SELECT),
                                              tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}
