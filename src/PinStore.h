#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <mutex>
#include <string>

/**
 * Stores the optional privacy PIN in /.crosspoint/pin.json on the SD card.
 *
 *   {"pin": "123456"}                      -> PIN lock enabled
 *   {"pin": "123456", "show_dots": false}  -> nothing is drawn while typing
 *   file missing, empty or invalid         -> PIN lock disabled
 *
 * The PIN is 4 to 16 digits, written as a string. It is kept in plain text on
 * purpose: this is a casual privacy barrier, not security. Anyone with the SD
 * card can read or delete the file.
 */
class PinStore : public PersistableStore<PinStore> {
 private:
  std::string pin;  // MIN_PIN_LENGTH..MAX_PIN_LENGTH digits, or empty when disabled
  bool dots = true;
  mutable std::once_flag loadOnce;

  PinStore() = default;
  void ensureLoaded() const;

  friend class PersistableStore<PinStore>;

 public:
  static constexpr size_t MIN_PIN_LENGTH = 4;
  static constexpr size_t MAX_PIN_LENGTH = 16;

  static const char* getFilePath() { return "/.crosspoint/pin.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  bool isEnabled() const;
  // Whether the PIN screen draws one mark per typed digit (default true).
  bool showDots() const;
  bool matches(const std::string& attempt) const;

  // Not used yet (the PIN is created by editing the file), but ready for a
  // future "set PIN" screen.
  bool setPin(const std::string& newPin);
  void clearPin();
};

#define PIN_STORE PinStore::getInstance()
