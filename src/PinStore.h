#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <mutex>
#include <string>

/**
 * Stores the optional 4-digit privacy PIN in /.crosspoint/pin.json on the SD card.
 *
 *   {"pin": "1234"}   -> PIN lock enabled
 *   file missing, empty or invalid -> PIN lock disabled
 *
 * The PIN is kept in plain text on purpose: this is a casual privacy barrier,
 * not security. Anyone with the SD card can read or delete the file.
 */
class PinStore : public PersistableStore<PinStore> {
 private:
  std::string pin;  // exactly PIN_LENGTH digits, or empty when disabled
  mutable std::once_flag loadOnce;

  PinStore() = default;
  void ensureLoaded() const;

  friend class PersistableStore<PinStore>;

 public:
  static constexpr size_t PIN_LENGTH = 4;

  static const char* getFilePath() { return "/.crosspoint/pin.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  bool isEnabled() const;
  bool matches(const std::string& attempt) const;

  // Not used by the first version (the PIN is created by editing the file),
  // but ready for a future "set PIN" screen.
  bool setPin(const std::string& newPin);
  void clearPin();
};

#define PIN_STORE PinStore::getInstance()
