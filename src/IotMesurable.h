/**
 * @file IotMesurable.h
 * @brief Main header for iot-mesurable library
 *
 * ESP32 library for easy sensor integration with IoT Grow Brain ecosystem.
 * Register your own sensors and publish data with minimal code.
 *
 * @example
 * #include <IotMesurable.h>
 *
 * IotMesurable brain("my-module");
 *
 * void setup() {
 *     brain.begin();
 *     brain.registerHardware("dht22", "DHT22");
 *     brain.addSensor("dht22", "temperature");
 * }
 *
 * void loop() {
 *     brain.publish("dht22", "temperature", 23.5);
 *     brain.loop();
 * }
 *
 * @author Cédric Pouilleux
 * @license MIT
 */

#ifndef IOT_MESURABLE_H
#define IOT_MESURABLE_H

#include <Arduino.h>
#include <functional>

// Forward declarations
class SensorRegistry;
class MqttClient;
class ConfigManager;

/**
 * @brief Callback types
 */
using ConfigCallback =
    std::function<void(const char *hardware, int intervalMs)>;
using EnableCallback = std::function<void(const char *hardware, bool enabled)>;
using ConnectCallback = std::function<void(bool connected)>;
using ResetCallback = std::function<void(const char *hardware)>;

/**
 * @brief Main library class
 *
 * Provides a simple API to register hardware sensors and publish
 * telemetry data to the IoT Grow Brain ecosystem via MQTT.
 */
class IotMesurable {
public:
  /**
   * @brief Construct with module ID
   * @param moduleId Unique identifier for this device (e.g., "growbox-01")
   */
  explicit IotMesurable(const char *moduleId);

  /**
   * @brief Destructor
   */
  ~IotMesurable();

  // =========================================================================
  // Initialization
  // =========================================================================

  /**
   * @brief Initialize with WiFiManager (captive portal if not configured)
   * @return true if connected successfully
   */
  bool begin();

  /**
   * @brief Initialize with direct WiFi credentials
   * @param ssid WiFi network name
   * @param password WiFi password
   * @return true if connected successfully
   */
  bool begin(const char *ssid, const char *password);

  /**
   * @brief Initialize with WiFi and custom MQTT broker
   * @param ssid WiFi network name
   * @param password WiFi password
   * @param broker MQTT broker hostname or IP
   * @param port MQTT port (default 1883)
   * @return true if connected successfully
   */
  bool begin(const char *ssid, const char *password, const char *broker,
             uint16_t port = 1883);

  // =========================================================================
  // Configuration
  // =========================================================================

  /**
   * @brief Set MQTT broker address
   * @param host Broker hostname or IP
   * @param port Broker port (default 1883)
   */
  void setBroker(const char *host, uint16_t port = 1883);

  /**
   * @brief Set module type (e.g., "air-quality", "climate")
   * @param type Module type string
   */
  void setModuleType(const char *type);

  /**
   * @brief Set MQTT credentials
   * @param username MQTT username
   * @param password MQTT password
   */
  void setCredentials(const char *username, const char *password);

  // =========================================================================

  // =========================================================================
  // Sensor Registration
  // =========================================================================

  /**
   * @brief Register a hardware component
   * @param key Unique hardware key (e.g., "dht22", "sps30")
   * @param name Human-readable name (e.g., "DHT22 Sensor")
   */
  void registerHardware(const char *key, const char *name);

  /**
   * @brief Add a sensor type to a registered hardware
   * @param hardwareKey The hardware key to attach this sensor to
   * @param sensorType Type of measurement (e.g., "temperature", "humidity")
   */
  void addSensor(const char *hardwareKey, const char *sensorType);

  // =========================================================================
  // Publishing
  // =========================================================================

  /**
   * @brief Publish a float sensor value
   * @param hardwareKey Hardware key
   * @param sensorType Sensor type
   * @param value Float value to publish
   */
  void publish(const char *hardwareKey, const char *sensorType, float value);

  /**
   * @brief Publish an integer sensor value
   * @param hardwareKey Hardware key
   * @param sensorType Sensor type
   * @param value Integer value to publish
   */
  void publish(const char *hardwareKey, const char *sensorType, int value);

  /**
   * @brief Publish a log message
   * @param level Log level (e.g., "error", "info", "warn")
   * @param msg Log message
   */
  void log(const char *level, const char *msg);

  /**
   * @brief Publish sensor status immediately
   *
   * Useful to call after hardware registration to immediately notify
   * the backend of sensor availability, rather than waiting for the
   * next automatic status publish (which occurs every 5 seconds).
   */
  void publishStatusNow();

  /**
   * @brief Publish announce message (auto-discovery)
   *
   * Publishes a retained message to mesurable/{chipId}/announce with
   * the module type, firmware version, and registered hardware/sensors.
   * Called automatically on MQTT connect and every 5 minutes.
   */
  void publishAnnounce();

  // =========================================================================
  // Main Loop
  // =========================================================================

  /**
   * @brief Must be called in Arduino loop()
   *
   * Handles MQTT connection, message reception, and status publishing.
   */
  void loop();

  // =========================================================================
  // Callbacks
  // =========================================================================

  /**
   * @brief Set callback for configuration changes
   * @param callback Function called when interval changes
   */
  void onConfigChange(ConfigCallback callback);

  /**
   * @brief Set callback for enable/disable changes
   * @param callback Function called when hardware is enabled/disabled
   */
  void onEnableChange(EnableCallback callback);

  /**
   * @brief Set callback for hardware reset requests
   * @param callback Function called when a reset is requested for a hardware
   */
  void onResetChange(ResetCallback callback);

  /**
   * @brief Set callback for connection state changes
   * @param callback Function called on connect/disconnect
   */
  void onConnect(ConnectCallback callback);

  // =========================================================================
  // State
  // =========================================================================

  /**
   * @brief Check MQTT connection status
   * @return true if connected to broker
   */
  bool isConnected() const;

  /**
   * @brief Check if hardware is enabled
   * @param hardwareKey Hardware key to check
   * @return true if enabled (not paused)
   */
  bool isHardwareEnabled(const char *hardwareKey) const;

  /**
   * @brief Get the module ID
   * @return Module ID string
   */
  const char *getModuleId() const;

  /**
   * @brief Get the chip ID (unique hardware identifier)
   * @return Chip ID string (hexadecimal)
   */
  const char *getChipId() const;

private:
  char _moduleId[64];
  char _moduleType[64];
  char _chipId[17]; // Hex string: 16 chars + null terminator
  char _broker[128];
  uint16_t _port;

  SensorRegistry *_registry;
  MqttClient *_mqtt;
  ConfigManager *_config;

  ConfigCallback _onConfigChange;
  EnableCallback _onEnableChange;
  ResetCallback _onResetChange;
  ConnectCallback _onConnect;

  unsigned long _lastStatusPublish;
  unsigned long _lastSystemPublish;
  unsigned long _lastConfigPublish;
  unsigned long _lastAnnouncePublish;
  bool _announcePublished;
  static const unsigned long STATUS_INTERVAL = 5000;
  static const unsigned long SYSTEM_INTERVAL =
      30000; // Publish system info every 30s
  static const unsigned long CONFIG_INTERVAL =
      60000; // Publish sensors config every 60s
  static const unsigned long ANNOUNCE_INTERVAL =
      300000; // Re-publish announce every 5 minutes

  void publishStatus();
  void publishConfig();
  void publishSystemInfo();
  void publishHardwareInfo();
  void handleMqttMessage(const char *topic, const char *payload);
  void setupSubscriptions();
};

#endif // IOT_MESURABLE_H
