/**
 * @file SensorRegistry.cpp
 * @brief Implementation of SensorRegistry
 */

#include "SensorRegistry.h"
#include <cstdio>
#include <cstring>


SensorRegistry::SensorRegistry() {
  _hardware.reserve(8); // Pre-allocate for typical use
}

// =============================================================================
// Registration
// =============================================================================

bool SensorRegistry::registerHardware(const char *key, const char *name) {
  if (!key || strlen(key) == 0)
    return false;
  if (hasHardware(key))
    return false; // Already exists

  HardwareDef hw;
  strncpy(hw.key, key, sizeof(hw.key) - 1);
  hw.key[sizeof(hw.key) - 1] = '\0';

  strncpy(hw.name, name ? name : key, sizeof(hw.name) - 1);
  hw.name[sizeof(hw.name) - 1] = '\0';

  hw.enabled = true;
  hw.intervalMs = 60000;  // Default 60s
  hw.lastPublishTime = 0; // Initialize to 0 to allow immediate first publish

  _hardware.push_back(hw);
  return true;
}

bool SensorRegistry::addSensor(const char *hardwareKey,
                               const char *sensorType) {
  if (!hardwareKey || !sensorType)
    return false;

  HardwareDef *hw = getHardware(hardwareKey);
  if (!hw)
    return false;

  // Check if sensor already exists
  if (hasSensor(hardwareKey, sensorType))
    return false;

  SensorDef sensor;
  strncpy(sensor.type, sensorType, sizeof(sensor.type) - 1);
  sensor.type[sizeof(sensor.type) - 1] = '\0';

  strcpy(sensor.status, "missing");
  sensor.lastValue = 0.0f;
  sensor.hasValue = false;
  sensor.lastUpdate = 0;

  hw->sensors.push_back(sensor);
  return true;
}

// =============================================================================
// Queries
// =============================================================================

bool SensorRegistry::hasHardware(const char *key) const {
  return findHardwareIndex(key) >= 0;
}

bool SensorRegistry::hasSensor(const char *hardwareKey,
                               const char *sensorType) const {
  const HardwareDef *hw = getHardware(hardwareKey);
  if (!hw)
    return false;
  return findSensorIndex(*hw, sensorType) >= 0;
}

HardwareDef *SensorRegistry::getHardware(const char *key) {
  int idx = findHardwareIndex(key);
  if (idx < 0)
    return nullptr;
  return &_hardware[idx];
}

const HardwareDef *SensorRegistry::getHardware(const char *key) const {
  int idx = findHardwareIndex(key);
  if (idx < 0)
    return nullptr;
  return &_hardware[idx];
}

SensorDef *SensorRegistry::getSensor(const char *hardwareKey,
                                     const char *sensorType) {
  HardwareDef *hw = getHardware(hardwareKey);
  if (!hw)
    return nullptr;

  int idx = findSensorIndex(*hw, sensorType);
  if (idx < 0)
    return nullptr;
  return &hw->sensors[idx];
}

// =============================================================================
// Composite Keys
// =============================================================================

void SensorRegistry::buildCompositeKey(const char *hardwareKey,
                                       const char *sensorType, char *buffer,
                                       size_t bufferSize) {
  snprintf(buffer, bufferSize, "%s:%s", hardwareKey, sensorType);
}

bool SensorRegistry::parseCompositeKey(const char *compositeKey,
                                       char *hardwareKey, char *sensorType,
                                       size_t bufferSize) {
  if (!compositeKey)
    return false;

  const char *colon = strchr(compositeKey, ':');
  if (!colon)
    return false;

  size_t hwLen = colon - compositeKey;
  if (hwLen >= bufferSize)
    hwLen = bufferSize - 1;

  strncpy(hardwareKey, compositeKey, hwLen);
  hardwareKey[hwLen] = '\0';

  strncpy(sensorType, colon + 1, bufferSize - 1);
  sensorType[bufferSize - 1] = '\0';

  return strlen(hardwareKey) > 0 && strlen(sensorType) > 0;
}

// =============================================================================
// State Management
// =============================================================================

void SensorRegistry::updateSensorValue(const char *hardwareKey,
                                       const char *sensorType, float value) {
  SensorDef *sensor = getSensor(hardwareKey, sensorType);
  if (!sensor)
    return;

  sensor->lastValue = value;
  sensor->hasValue = true;
  sensor->lastUpdate = millis();

  // Update status based on enabled state
  HardwareDef *hw = getHardware(hardwareKey);
  if (hw && !hw->enabled) {
    strcpy(sensor->status, "disabled");
  } else if (!isnan(value)) {
    strcpy(sensor->status, "ok");
  } else {
    strcpy(sensor->status, "missing");
  }
}

void SensorRegistry::setHardwareEnabled(const char *hardwareKey, bool enabled) {
  HardwareDef *hw = getHardware(hardwareKey);
  if (!hw)
    return;

  hw->enabled = enabled;

  // Update all sensor statuses
  for (auto &sensor : hw->sensors) {
    if (!enabled) {
      strcpy(sensor.status, "disabled");
    } else if (sensor.hasValue) {
      strcpy(sensor.status, "ok");
    } else {
      strcpy(sensor.status, "missing");
    }
  }
}

bool SensorRegistry::isHardwareEnabled(const char *hardwareKey) const {
  const HardwareDef *hw = getHardware(hardwareKey);
  return hw ? hw->enabled : false;
}

void SensorRegistry::setHardwareInterval(const char *hardwareKey,
                                         int intervalMs) {
  HardwareDef *hw = getHardware(hardwareKey);
  if (hw) {
    hw->intervalMs = intervalMs;
  }
}

bool SensorRegistry::canPublish(const char *hardwareKey) const {
  const HardwareDef *hw = getHardware(hardwareKey);
  if (!hw)
    return false;

  unsigned long now = millis();
  unsigned long elapsed = now - hw->lastPublishTime;

  // Handle millis() rollover (occurs every ~49 days)
  if (now < hw->lastPublishTime) {
    elapsed = (0xFFFFFFFF - hw->lastPublishTime) + now;
  }

  return elapsed >= (unsigned long)hw->intervalMs;
}

void SensorRegistry::updatePublishTime(const char *hardwareKey) {
  HardwareDef *hw = getHardware(hardwareKey);
  if (hw) {
    hw->lastPublishTime = millis();
  }
}

unsigned long
SensorRegistry::getLastPublishTime(const char *hardwareKey) const {
  const HardwareDef *hw = getHardware(hardwareKey);
  return hw ? hw->lastPublishTime : 0;
}

// =============================================================================
// Status Building
// =============================================================================

size_t SensorRegistry::buildStatusJson(char *buffer, size_t bufferSize) const {
  size_t written = 0;
  written += snprintf(buffer + written, bufferSize - written, "{");

  bool firstSensor = true;

  for (const auto &hw : _hardware) {
    for (const auto &sensor : hw.sensors) {
      if (!firstSensor) {
        written += snprintf(buffer + written, bufferSize - written, ",");
      }
      firstSensor = false;

      char compositeKey[64];
      buildCompositeKey(hw.key, sensor.type, compositeKey,
                        sizeof(compositeKey));

      // Determine effective status
      const char *effectiveStatus = hw.enabled ? sensor.status : "disabled";

      if (sensor.hasValue && !isnan(sensor.lastValue)) {
        written += snprintf(buffer + written, bufferSize - written,
                            "\"%s\":{\"status\":\"%s\",\"value\":%.2f}",
                            compositeKey, effectiveStatus, sensor.lastValue);
      } else {
        written += snprintf(buffer + written, bufferSize - written,
                            "\"%s\":{\"status\":\"%s\",\"value\":null}",
                            compositeKey, effectiveStatus);
      }
    }
  }

  written += snprintf(buffer + written, bufferSize - written, "}");
  return written;
}

size_t SensorRegistry::buildConfigJson(char *buffer, size_t bufferSize) const {
  size_t written = 0;
  written += snprintf(buffer + written, bufferSize - written, "{");

  bool firstSensor = true;

  for (const auto &hw : _hardware) {
    // Convert intervalMs to seconds (backend expects seconds)
    int intervalSeconds = hw.intervalMs / 1000;
    if (intervalSeconds <= 0)
      intervalSeconds = 60; // Default 60s

    for (const auto &sensor : hw.sensors) {
      if (!firstSensor) {
        written += snprintf(buffer + written, bufferSize - written, ",");
      }
      firstSensor = false;

      char compositeKey[64];
      buildCompositeKey(hw.key, sensor.type, compositeKey,
                        sizeof(compositeKey));

      written +=
          snprintf(buffer + written, bufferSize - written,
                   "\"%s\":{\"interval\":%d,\"enabled\":%s}", compositeKey,
                   intervalSeconds, hw.enabled ? "true" : "false");
    }
  }

  written += snprintf(buffer + written, bufferSize - written, "}");
  return written;
}

// =============================================================================
// Announce Building
// =============================================================================

size_t SensorRegistry::buildAnnounceHardwareJson(char *buffer,
                                                 size_t bufferSize) const {
  size_t pos = 0;

  // Safety check
  if (!buffer || bufferSize < 3)
    return 0;

  buffer[pos++] = '[';

  bool firstHw = true;
  for (const auto &hw : _hardware) {
    if (!firstHw) {
      buffer[pos++] = ',';
    }
    firstHw = false;

    // {"key":"scd41","name":"SCD41","sensors":["co2","temperature","humidity"]}
    int written = snprintf(buffer + pos, bufferSize - pos,
                           "{\"key\":\"%s\",\"name\":\"%s\",\"sensors\":[",
                           hw.key, hw.name);
    if (written < 0 || (size_t)written >= bufferSize - pos)
      break;
    pos += written;

    bool firstSensor = true;
    for (const auto &sensor : hw.sensors) {
      if (!firstSensor) {
        buffer[pos++] = ',';
      }
      firstSensor = false;

      written = snprintf(buffer + pos, bufferSize - pos, "\"%s\"", sensor.type);
      if (written < 0 || (size_t)written >= bufferSize - pos)
        break;
      pos += written;
    }

    // Close sensors array and hardware object
    written = snprintf(buffer + pos, bufferSize - pos, "]}");
    if (written < 0 || (size_t)written >= bufferSize - pos)
      break;
    pos += written;
  }

  buffer[pos++] = ']';
  buffer[pos] = '\0';

  return pos;
}

// =============================================================================
// Private Helpers
// =============================================================================

int SensorRegistry::findHardwareIndex(const char *key) const {
  for (size_t i = 0; i < _hardware.size(); i++) {
    if (strcmp(_hardware[i].key, key) == 0) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

int SensorRegistry::findSensorIndex(const HardwareDef &hw,
                                    const char *sensorType) const {
  for (size_t i = 0; i < hw.sensors.size(); i++) {
    if (strcmp(hw.sensors[i].type, sensorType) == 0) {
      return static_cast<int>(i);
    }
  }
  return -1;
}
