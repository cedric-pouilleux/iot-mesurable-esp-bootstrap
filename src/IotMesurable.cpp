/**
 * @file IotMesurable.cpp
 * @brief Main library implementation
 */

#include "IotMesurable.h"
#include "core/ConfigManager.h"
#include "core/MqttClient.h"
#include "core/SensorRegistry.h"

#include <cstdio>
#include <cstring>

#ifndef NATIVE_BUILD
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#ifdef ESP32
#include <WiFi.h>
#include <esp_system.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#endif

// =============================================================================
// Constructor / Destructor
// =============================================================================

IotMesurable::IotMesurable(const char *moduleId)
    : _port(1883), _lastStatusPublish(0), _lastSystemPublish(0),
      _lastConfigPublish(0), _lastAnnouncePublish(0),
      _announcePublished(false) {

  strncpy(_moduleId, moduleId, sizeof(_moduleId) - 1);
  _moduleId[sizeof(_moduleId) - 1] = '\0';

  _moduleType[0] = '\0';
  memset(_broker, 0, sizeof(_broker));

  // Extract unique chip ID
#ifndef NATIVE_BUILD
#ifdef ESP32
  uint64_t chipid = ESP.getEfuseMac();
  snprintf(_chipId, sizeof(_chipId), "%016llX", chipid);
#elif defined(ESP8266)
  uint32_t chipid = ESP.getChipId();
  snprintf(_chipId, sizeof(_chipId), "%08X", chipid);
#endif
#else
  snprintf(_chipId, sizeof(_chipId), "NATIVE_TEST_ID");
#endif

  _registry = new SensorRegistry();
  _mqtt = new MqttClient();
  _config = new ConfigManager();

  _mqtt->setClientId(_chipId);
}

IotMesurable::~IotMesurable() {
  delete _registry;
  delete _mqtt;
  delete _config;
}

// =============================================================================
// Initialization
// =============================================================================

bool IotMesurable::begin() {
  _config->loadConfig();

  // Use WiFiManager with module ID as AP name
  if (!_config->beginWiFiManager(_moduleId)) {
    return false;
  }

#ifdef ESP32
  // Critical for AsyncTCP stability
  WiFi.setSleep(false);
#endif

  // Get broker from config
  if (strlen(_config->getBroker()) > 0) {
    setBroker(_config->getBroker(), _config->getPort());
  }

  // Setup MQTT callbacks and connect
  _mqtt->onConnect([this](bool connected) {
    if (connected) {
      setupSubscriptions();
      publishAnnounce();
    }
    if (_onConnect) {
      _onConnect(connected);
    }
  });

  _mqtt->onMessage([this](const char *topic, const char *payload) {
    handleMqttMessage(topic, payload);
  });

#ifndef NATIVE_BUILD
  // Start OTA
  ArduinoOTA.setHostname(_moduleId);
  ArduinoOTA.begin();
#endif

  // Allow WiFi stack to stabilize
  delay(1000);

  return _mqtt->connect();
}

bool IotMesurable::begin(const char *ssid, const char *password) {
  _config->loadConfig();

  if (!_config->beginWiFi(ssid, password)) {
    return false;
  }

#ifdef ESP32
  WiFi.setSleep(false);
#endif

  // Get broker from config or use default
  if (strlen(_broker) == 0 && strlen(_config->getBroker()) > 0) {
    setBroker(_config->getBroker(), _config->getPort());
  }

  _mqtt->onConnect([this](bool connected) {
    if (connected) {
      setupSubscriptions();
      publishAnnounce();
    }
    if (_onConnect) {
      _onConnect(connected);
    }
  });

  _mqtt->onMessage([this](const char *topic, const char *payload) {
    handleMqttMessage(topic, payload);
  });

#ifndef NATIVE_BUILD
  // Start OTA
  ArduinoOTA.setHostname(_moduleId);
  ArduinoOTA.begin();
#endif

  return _mqtt->connect();
}

bool IotMesurable::begin(const char *ssid, const char *password,
                         const char *broker, uint16_t port) {
  setBroker(broker, port);
  return begin(ssid, password);
}

// =============================================================================
// Configuration
// =============================================================================

void IotMesurable::setBroker(const char *host, uint16_t port) {
  strncpy(_broker, host, sizeof(_broker) - 1);
  _broker[sizeof(_broker) - 1] = '\0';
  _port = port;

  _mqtt->setBroker(host, port);
  _config->setBroker(host, port);
}

void IotMesurable::setModuleType(const char *type) {
  if (!type)
    return;
  strncpy(_moduleType, type, sizeof(_moduleType) - 1);
  _moduleType[sizeof(_moduleType) - 1] = '\0';
}

void IotMesurable::setCredentials(const char *username, const char *password) {
  _mqtt->setCredentials(username, password);
}

// =============================================================================
// Sensor Registration
// =============================================================================

void IotMesurable::registerHardware(const char *key, const char *name) {
  _registry->registerHardware(key, name);

  // Load persisted enabled state
  bool enabled = _config->loadHardwareEnabled(key, true);
  _registry->setHardwareEnabled(key, enabled);

  // Load persisted interval
  int interval = _config->loadInterval(key, 60000);
  _registry->setHardwareInterval(key, interval);
}

void IotMesurable::addSensor(const char *hardwareKey, const char *sensorType) {
  _registry->addSensor(hardwareKey, sensorType);
}

// =============================================================================
// Publishing
// =============================================================================

void IotMesurable::publish(const char *hardwareKey, const char *sensorType,
                           float value) {
  // Skip if hardware is disabled
  if (!_registry->isHardwareEnabled(hardwareKey)) {
    return;
  }

  // Get current time and last publish time
  unsigned long now = millis();
  unsigned long lastPublish = _registry->getLastPublishTime(hardwareKey);

  // Check throttling: allow if interval passed OR within 10ms of last publish
  // (same read cycle) This ensures all measurements from the same hardware can
  // be published together even if they span multiple milliseconds during
  // sequential publish() calls
  const unsigned long SAME_CYCLE_WINDOW_MS = 10;
  bool intervalElapsed = _registry->canPublish(hardwareKey);
  bool sameCycle =
      (lastPublish == 0) || ((now - lastPublish) < SAME_CYCLE_WINDOW_MS);
  bool shouldPublish = intervalElapsed || sameCycle;

  if (!shouldPublish) {
    return;
  }

  // Update registry
  _registry->updateSensorValue(hardwareKey, sensorType, value);

  // Build topic: mesurable/{chipId}/data/{hardware}/{sensor}
  char topic[128];
  snprintf(topic, sizeof(topic), "mesurable/%s/data/%s/%s", _chipId,
           hardwareKey, sensorType);

  // Build payload
  char payload[32];
  snprintf(payload, sizeof(payload), "%.2f", value);

  _mqtt->publish(topic, payload, false, 1); // QoS 1 for sensor data

  // Update timestamp only if not already updated this millisecond
  if (now != lastPublish) {
    _registry->updatePublishTime(hardwareKey);
  }
}

void IotMesurable::publish(const char *hardwareKey, const char *sensorType,
                           int value) {
  publish(hardwareKey, sensorType, static_cast<float>(value));
}

void IotMesurable::log(const char *level, const char *msg) {
  if (!isConnected())
    return;

  // Build JSON: {"level":"error","msg":"...","time":12345}
  char buffer[256];
  snprintf(buffer, sizeof(buffer),
           "{\"level\":\"%s\",\"msg\":\"%s\",\"time\":%lu}", level, msg,
           millis());

  // Publish to mesurable/{chipId}/log
  char topic[128];
  snprintf(topic, sizeof(topic), "mesurable/%s/log", _chipId);

  _mqtt->publish(topic, buffer, false);
}

void IotMesurable::publishStatusNow() { publishStatus(); }

void IotMesurable::publishAnnounce() {
#ifndef NATIVE_BUILD
  if (!isConnected())
    return;

  // Build hardware array from registry
  char hardwareJson[1024];
  _registry->buildAnnounceHardwareJson(hardwareJson, sizeof(hardwareJson));

  // Build full announce JSON
  char buffer[1536];
  snprintf(buffer, sizeof(buffer),
           "{\"type\":\"%s\",\"moduleId\":\"%s\",\"firmware\":\"1.0.0\","
           "\"hardware\":%s}",
           _moduleType, _moduleId, hardwareJson);

  // Publish to mesurable/{chipId}/announce (retained)
  char topic[128];
  snprintf(topic, sizeof(topic), "mesurable/%s/announce", _chipId);

  _mqtt->publish(topic, buffer, true);
  _announcePublished = true;
  _lastAnnouncePublish = millis();
#endif
}

// =============================================================================
// Main Loop
// =============================================================================

void IotMesurable::loop() {
  _mqtt->loop();

#ifndef NATIVE_BUILD
  ArduinoOTA.handle();
#endif

  // Publish status periodically
  unsigned long now = millis();
  if (now - _lastStatusPublish >= STATUS_INTERVAL) {
    _lastStatusPublish = now;
    publishStatus();
  }

  // Publish system info less frequently
  if (now - _lastSystemPublish >= SYSTEM_INTERVAL) {
    _lastSystemPublish = now;
    publishSystemInfo();
    publishHardwareInfo();
  }

  // Publish sensors config periodically (for storage projections)
  if (now - _lastConfigPublish >= CONFIG_INTERVAL) {
    _lastConfigPublish = now;
    publishConfig();
  }

  // Re-publish announce periodically (heartbeat)
  if (now - _lastAnnouncePublish >= ANNOUNCE_INTERVAL) {
    publishAnnounce();
  }
}

// =============================================================================
// Callbacks
// =============================================================================

void IotMesurable::onConfigChange(ConfigCallback callback) {
  _onConfigChange = callback;
}

void IotMesurable::onEnableChange(EnableCallback callback) {
  _onEnableChange = callback;
}

void IotMesurable::onResetChange(ResetCallback callback) {
  _onResetChange = callback;
}

void IotMesurable::onConnect(ConnectCallback callback) {
  _onConnect = callback;
}

// =============================================================================
// State
// =============================================================================

bool IotMesurable::isConnected() const { return _mqtt->isConnected(); }

bool IotMesurable::isHardwareEnabled(const char *hardwareKey) const {
  return _registry->isHardwareEnabled(hardwareKey);
}

const char *IotMesurable::getModuleId() const { return _moduleId; }

const char *IotMesurable::getChipId() const { return _chipId; }

// =============================================================================
// Private Methods
// =============================================================================

void IotMesurable::publishStatus() {
  if (!isConnected())
    return;

  // Build sensors status JSON
  char sensorsBuffer[1024];
  _registry->buildStatusJson(sensorsBuffer, sizeof(sensorsBuffer));

  // Build complete status with metadata
  char fullBuffer[1536];
  snprintf(fullBuffer, sizeof(fullBuffer),
           "{\"moduleId\":\"%s\",\"moduleType\":\"%s\",\"sensors\":%s}",
           _moduleId, _moduleType, sensorsBuffer);

  // Publish to mesurable/{chipId}/status
  char topic[128];
  snprintf(topic, sizeof(topic), "mesurable/%s/status", _chipId);

  _mqtt->publish(topic, fullBuffer, true);
}

void IotMesurable::publishConfig() {
  if (!isConnected())
    return;

  // Build sensors config JSON
  char configBuffer[1024];
  _registry->buildConfigJson(configBuffer, sizeof(configBuffer));

  // Publish to mesurable/{chipId}/config
  char topic[128];
  snprintf(topic, sizeof(topic), "mesurable/%s/config", _chipId);

  _mqtt->publish(topic, configBuffer, true);
}

void IotMesurable::publishSystemInfo() {
#ifndef NATIVE_BUILD
  if (!isConnected())
    return;

  // Get system info
  char ip[16] = "0.0.0.0";
  char mac[18] = "00:00:00:00:00:00";
  unsigned long uptimeSeconds = millis() / 1000;

  // Memory info
  uint32_t heapFree = 0;
  uint32_t heapTotal = 0;
  uint32_t heapMinFree = 0;

  // Flash info
  uint32_t flashTotal = 0;
  uint32_t flashSketchSize = 0;
  uint32_t flashFreeSketch = 0;

#ifdef ESP32
  heapFree = ESP.getFreeHeap() / 1024;
  heapTotal = ESP.getHeapSize() / 1024;
  heapMinFree = ESP.getMinFreeHeap() / 1024;

  flashTotal = ESP.getFlashChipSize() / 1024;
  flashSketchSize = ESP.getSketchSize() / 1024;
  flashFreeSketch = ESP.getFreeSketchSpace() / 1024;
#elif defined(ESP8266)
  heapFree = ESP.getFreeHeap() / 1024;
  // ESP8266 doesn't have getHeapSize() easily available like ESP32 without
  // internal SDK calls We'll leave heapTotal as 0 or estimate if needed, but
  // for simplicity 0
  heapMinFree =
      ESP.getMaxFreeBlockSize() / 1024; // Approximation for fragmentation

  flashTotal = ESP.getFlashChipRealSize() / 1024;
  flashSketchSize = ESP.getSketchSize() / 1024;
  flashFreeSketch = ESP.getFreeSketchSpace() / 1024;
#endif

  // Get WiFi info
  if (WiFi.status() == WL_CONNECTED) {
    IPAddress localIP = WiFi.localIP();
    snprintf(ip, sizeof(ip), "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2],
             localIP[3]);

    uint8_t macAddr[6];
    WiFi.macAddress(macAddr);
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", macAddr[0],
             macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
  }

  // Get RSSI
  int rssi = WiFi.RSSI();

  // Build JSON
  char buffer[640];
  snprintf(buffer, sizeof(buffer),
           "{\"ip\":\"%s\",\"mac\":\"%s\",\"chipId\":\"%s\",\"moduleType\":\"%"
           "s\",\"uptimeStart\":%lu,"
           "\"memory\":{\"heapTotalKb\":%lu,\"heapFreeKb\":%lu,"
           "\"heapMinFreeKb\":%lu},"
           "\"flash\":{\"totalKb\":%lu,\"usedKb\":%lu,\"freeKb\":%lu},"
           "\"rssi\":%d}",
           ip, mac, _chipId, _moduleType, uptimeSeconds,
           (unsigned long)heapTotal, (unsigned long)heapFree,
           (unsigned long)heapMinFree, (unsigned long)flashTotal,
           (unsigned long)flashSketchSize, (unsigned long)flashFreeSketch,
           rssi);

  // Publish to mesurable/{chipId}/system
  char topic[128];
  snprintf(topic, sizeof(topic), "mesurable/%s/system", _chipId);
  _mqtt->publish(topic, buffer, true);
#endif
}

void IotMesurable::publishHardwareInfo() {
#ifndef NATIVE_BUILD
  if (!isConnected())
    return;

  const char *chipModel = "Unknown";
  int cpuFreq = 0;
  int flashKb = 0;
  int cores = 1;
  int rev = 0;

#ifdef ESP32
  // Get chip info
  esp_chip_info_t chipInfo;
  esp_chip_info(&chipInfo);

  if (chipInfo.model == CHIP_ESP32)
    chipModel = "ESP32";
  else if (chipInfo.model == CHIP_ESP32S2)
    chipModel = "ESP32-S2";
  else if (chipInfo.model == CHIP_ESP32S3)
    chipModel = "ESP32-S3";
  else if (chipInfo.model == CHIP_ESP32C3)
    chipModel = "ESP32-C3";

  cpuFreq = ESP.getCpuFreqMHz();
  flashKb = ESP.getFlashChipSize() / 1024;
  cores = chipInfo.cores;
  rev = chipInfo.revision;
#elif defined(ESP8266)
  chipModel = "ESP8266";
  cpuFreq = ESP.getCpuFreqMHz();
  flashKb = ESP.getFlashChipRealSize() / 1024;
  cores = 1;
  // ESP8266 revision?
  rev = 0;
#endif

  // Build JSON
  char buffer[256];
  snprintf(buffer, sizeof(buffer),
           "{\"chip\":{\"model\":\"%s\",\"rev\":%d,\"cpuFreqMhz\":%d,"
           "\"flashKb\":%d,\"cores\":%d}}",
           chipModel, rev, cpuFreq, flashKb, cores);

  // Publish to mesurable/{chipId}/hardware
  char topic[128];
  snprintf(topic, sizeof(topic), "mesurable/%s/hardware", _chipId);
  _mqtt->publish(topic, buffer, true);
#endif
}

void IotMesurable::handleMqttMessage(const char *topic, const char *payload) {
#ifndef NATIVE_BUILD
  // Parse topic to extract message type
  // Expected formats:
  //   mesurable/{chipId}/config  - configuration update
  //   mesurable/{chipId}/enable  - enable/disable hardware
  //   mesurable/{chipId}/reset   - reset hardware

  char expectedConfigTopic[128];
  char expectedEnableTopic[128];
  char expectedResetTopic[128];
  snprintf(expectedConfigTopic, sizeof(expectedConfigTopic),
           "mesurable/%s/config", _chipId);
  snprintf(expectedEnableTopic, sizeof(expectedEnableTopic),
           "mesurable/%s/enable", _chipId);
  snprintf(expectedResetTopic, sizeof(expectedResetTopic), "mesurable/%s/reset",
           _chipId);

  if (strcmp(topic, expectedConfigTopic) == 0) {
    Serial.printf("[MQTT] Config received on topic: %s\n", topic);
    Serial.printf("[MQTT] Payload: %s\n", payload);

    // Parse config message
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      Serial.printf("[MQTT] JSON parse error: %s\n", error.c_str());
      return;
    }

    // Look for interval updates per hardware
    JsonObject sensors = doc["sensors"];
    if (sensors) {
      Serial.println("[MQTT] Processing sensor configs...");
      for (const auto &hw : _registry->getAllHardware()) {
        if (sensors.containsKey(hw.key)) {
          JsonObject hwConfig = sensors[hw.key];
          if (hwConfig.containsKey("interval")) {
            int interval = hwConfig["interval"];
            Serial.printf("[MQTT] Setting %s interval to %d seconds\n", hw.key,
                          interval);
            _registry->setHardwareInterval(hw.key, interval * 1000);
            _config->saveInterval(hw.key, interval * 1000);

            if (_onConfigChange) {
              _onConfigChange(hw.key, interval * 1000);
            }
          }
        }
      }
    }
  } else if (strcmp(topic, expectedEnableTopic) == 0) {
    // Parse enable message: { "hardware": "dht22", "enabled": true }
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
      return;

    const char *hardware = doc["hardware"];
    bool enabled = doc["enabled"];

    if (hardware) {
      _registry->setHardwareEnabled(hardware, enabled);
      _config->saveHardwareEnabled(hardware, enabled);

      if (_onEnableChange) {
        _onEnableChange(hardware, enabled);
      }

      // Publish new config immediately to update server state
      publishConfig();
    }
  } else if (strcmp(topic, expectedResetTopic) == 0) {
    // Parse reset message: { "sensor": "dht22" }
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
      return;

    const char *sensor = doc["sensor"];

    if (sensor && _onResetChange) {
      _onResetChange(sensor);
    }
  }
#endif
}

void IotMesurable::setupSubscriptions() {
  char topic[128];

  // Subscribe to config topic
  snprintf(topic, sizeof(topic), "mesurable/%s/config", _chipId);
  _mqtt->subscribe(topic);

  // Subscribe to enable topic
  snprintf(topic, sizeof(topic), "mesurable/%s/enable", _chipId);
  _mqtt->subscribe(topic);

  // Subscribe to reset topic
  snprintf(topic, sizeof(topic), "mesurable/%s/reset", _chipId);
  _mqtt->subscribe(topic);
}
