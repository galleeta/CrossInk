#include "PinLockActivity.h"

#include <FreeInkUIGfxRenderer.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <string>
#include <utility>

#include "MappedInputManager.h"
#include "PinStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
namespace fui = freeink::ui;

constexpr int kFieldHeight = 64;
constexpr int kFieldMaxWidth = 380;
constexpr int kKeyboardRows = 4;
constexpr fui::ActionId kKeyboardAction = 1;
// Value the router would emit for a long press; ignored (long press is off).
constexpr int16_t kKeyboardIgnore = -1;
constexpr int16_t kKeyboardClear = -2;
constexpr int16_t kKeyboardOk = -3;
constexpr uint8_t kCandidateOk = 10;  // button mode: the "OK" entry after 0-9

// Like the keypad of the frontlight time picker, but taller: this screen has
// room to spare, and bigger keys mean fewer missed or mistaken taps. It keeps
// enough space above for the title and the PIN field.
fui::Rect keypadRect(const GfxRenderer& renderer) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int gap = metrics.keyboardKeySpacing;
  const int bottom = renderer.getScreenHeight() - metrics.buttonHintsHeight - metrics.verticalSpacing +
                     metrics.keyboardVerticalOffset;
  const int themeHeight = kKeyboardRows * metrics.keyboardKeyHeight + (kKeyboardRows - 1) * gap;
  const int reserved = kFieldHeight + renderer.getLineHeight(UI_12_FONT_ID) * 8;
  int height = renderer.getScreenHeight() * 50 / 100;
  height = std::min(height, bottom - reserved);
  height = std::max(height, themeHeight);
  const int width = renderer.getScreenWidth() * metrics.keyboardWidthPercent / 100;
  const int x = (renderer.getScreenWidth() - width) / 2;
  const int y = bottom - height;
  return {static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(width), static_cast<int16_t>(height)};
}
}  // namespace

PinLockActivity::PinLockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, UnlockCallback onUnlock)
    : Activity("PinLock", renderer, mappedInput), onUnlock(std::move(onUnlock)) {}

void PinLockActivity::onEnter() {
  Activity::onEnter();
  entry.clear();
  candidate = 0;
  showDots = PIN_STORE.showDots();
  wrongPin = false;
  unlocked = false;
  keyboardTouchRouter.overrideValue = kKeyboardIgnore;
  keyboardTouchRouter.reset();
  keyboardInteractionsReady.store(false, std::memory_order_release);
  requestUpdate();
}

// ---- input handling -------------------------------------------------------

void PinLockActivity::handleKeyboardValue(const int16_t value) {
  if (value >= 0 && value <= 9) {
    enterDigit(static_cast<uint8_t>(value));
  } else if (value == kKeyboardClear) {
    clearEntry();
  } else if (value == kKeyboardOk) {
    submit();
  }
}

void PinLockActivity::enterDigit(const uint8_t digit) {
  if (entry.size() >= PinStore::MAX_PIN_LENGTH) return;
  const bool hadMessage = wrongPin;
  wrongPin = false;
  entry.push_back(static_cast<char>('0' + digit));
  // With hidden input nothing on screen changes per tap, so skip the refresh.
  if (showDots || hadMessage) requestUpdate();
}

void PinLockActivity::clearEntry() {
  const bool needsRedraw = wrongPin || (showDots && !entry.empty());
  entry.clear();
  wrongPin = false;
  if (needsRedraw) requestUpdate();
}

void PinLockActivity::adjustCandidate(const int delta) {
  wrongPin = false;
  candidate = static_cast<uint8_t>((static_cast<int>(candidate) + delta + 11) % 11);
}

void PinLockActivity::confirmCandidate() {
  wrongPin = false;
  if (candidate == kCandidateOk) {
    submit();
    return;
  }
  if (entry.size() < PinStore::MAX_PIN_LENGTH) entry.push_back(static_cast<char>('0' + candidate));
  candidate = 0;
  requestUpdate();
}

void PinLockActivity::submit() {
  if (entry.empty()) return;

  if (PIN_STORE.matches(entry)) {
    unlock();
    return;
  }

  wrongPin = true;
  entry.clear();
  candidate = 0;
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
    if (!entry.empty()) {
      entry.pop_back();
      wrongPin = false;
      requestUpdate();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    confirmCandidate();
    return;
  }

  buttonNavigator.onNextRelease([this] {
    adjustCandidate(+1);
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this] {
    adjustCandidate(-1);
    requestUpdate();
  });
  buttonNavigator.onNextContinuous([this] {
    adjustCandidate(+1);
    requestUpdate();
  });
  buttonNavigator.onPreviousContinuous([this] {
    adjustCandidate(-1);
    requestUpdate();
  });
}

// ---- drawing --------------------------------------------------------------

void PinLockActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const bool touch = mappedInput.hasTouch();
  const int screenWidth = renderer.getScreenWidth();
  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);

  // With the keypad, centre the field in the space above it.
  const int areaHeight = touch ? keypadRect(renderer).y : renderer.getScreenHeight();
  const int fieldWidth = std::min(screenWidth - 40, kFieldMaxWidth);
  const int fieldX = (screenWidth - fieldWidth) / 2;
  const int fieldY = areaHeight / 2 - kFieldHeight / 2 + (touch ? lineHeight : 0);

  const char* title = "Enter PIN";
  const int titleWidth = renderer.getTextWidth(UI_12_FONT_ID, title, EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID, (screenWidth - titleWidth) / 2, fieldY - lineHeight * 3, title, true,
                    EpdFontFamily::BOLD);

  renderer.fillRectDither(fieldX, fieldY, fieldWidth, kFieldHeight, Color::White);
  renderer.drawRect(fieldX, fieldY, fieldWidth, kFieldHeight, true);

  // Touch mode: one mark per typed digit (or nothing if hidden).
  // Button mode: the marks plus the entry being chosen, e.g. "* * [5]".
  if (!touch || showDots) {
    auto build = [&](const char* separator) {
      std::string text;
      for (size_t i = 0; i < entry.size(); i++) {
        if (i > 0) text += separator;
        text += '*';
      }
      if (!touch) {
        if (!entry.empty()) text += separator;
        text += '[';
        text += (candidate == kCandidateOk) ? std::string("OK") : std::string(1, static_cast<char>('0' + candidate));
        text += ']';
      }
      return text;
    };

    std::string text = build(" ");
    if (renderer.getTextWidth(UI_12_FONT_ID, text.c_str(), EpdFontFamily::BOLD) > fieldWidth - 16) text = build("");
    if (!text.empty()) {
      const int textWidth = renderer.getTextWidth(UI_12_FONT_ID, text.c_str(), EpdFontFamily::BOLD);
      renderer.drawText(UI_12_FONT_ID, fieldX + (fieldWidth - textWidth) / 2, fieldY + (kFieldHeight - lineHeight) / 2,
                        text.c_str(), true, EpdFontFamily::BOLD);
    }
  }

  if (wrongPin) {
    const char* message = "Wrong PIN, try again";
    const int messageWidth = renderer.getTextWidth(UI_12_FONT_ID, message, EpdFontFamily::REGULAR);
    renderer.drawText(UI_12_FONT_ID, (screenWidth - messageWidth) / 2, fieldY + kFieldHeight + lineHeight, message,
                      true, EpdFontFamily::REGULAR);
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
    keys[9].label = "Clear";
    keys[9].value = kKeyboardClear;
    keys[10].label = kDigitLabels[0];
    keys[10].value = 0;
    keys[11].label = tr(STR_OK);
    keys[11].kind = fui::KeyKind::Ok;
    keys[11].value = kKeyboardOk;

    fui::KeyGridProps props;
    props.keys = keys;
    props.rows = kKeyboardRows;
    props.cols = 3;
    props.action = kKeyboardAction;
    // Touch only: a long press must not be turned into an action, which could
    // eat digits when tapping slowly or firmly.
    props.inputMask = static_cast<uint16_t>(fui::InputTouch);
    props.labelText.font = fui::GfxRendererTarget::FONT_BODY;
    const auto& metrics = UITheme::getInstance().getMetrics();
    props.gap = static_cast<int16_t>(metrics.keyboardKeySpacing);
    fui::keyGrid(frame, keypadRect(renderer), props);
    keyboardInteractions.publish();
    keyboardInteractionsReady.store(true, std::memory_order_release);
  } else {
    const auto labels = mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), tr(STR_SELECT),
                                              tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}
