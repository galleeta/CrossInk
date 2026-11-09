#include "PinStore.h"

#include <Logging.h>

#include <cstdio>

namespace {
bool isValidPin(const std::string& value) {
  if (value.size() != PinStore::PIN_LENGTH) return false;
  for (const char c : value) {
    if (c < '0' || c > '9') return false;
  }
  return true;
}
}  // namespace

void PinStore::ensureLoaded() const {
  std::call_once(loadOnce, [this] { const_cast<PinStore*>(this)->PersistableStore<PinStore>::loadFromFile(); });
}

void PinStore::toJson(JsonDocument& doc) const { doc["pin"] = pin; }

bool PinStore::fromJson(JsonVariantConst doc) {
  std::string candidate;
  JsonVariantConst value = doc["pin"];
  if (value.is<const char*>()) {
    candidate = value.as<const char*>();
  } else if (value.is<int>()) {
    // Tolerate {"pin": 1234} typed by hand (leading zeros need the string form).
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d", value.as<int>());
    candidate = buf;
  }

  if (isValidPin(candidate)) {
    pin = candidate;
  } else {
    pin.clear();
    if (!candidate.empty()) {
      LOG_ERR("PIN", "Ignoring invalid PIN in file (must be exactly %u digits)", static_cast<unsigned>(PIN_LENGTH));
    }
  }
  return true;
}

bool PinStore::isEnabled() const {
  ensureLoaded();
  return !pin.empty();
}

bool PinStore::matches(const std::string& attempt) const {
  ensureLoaded();
  return !pin.empty() && attempt == pin;
}

bool PinStore::setPin(const std::string& newPin) {
  ensureLoaded();
  if (!isValidPin(newPin)) return false;
  pin = newPin;
  return saveToFile();
}

void PinStore::clearPin() {
  ensureLoaded();
  pin.clear();
  saveToFile();
}
