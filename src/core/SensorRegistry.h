/**
 * @file SensorRegistry.h
 * @brief Sensor and hardware registration management
 */

#ifndef SENSOR_REGISTRY_H
#define SENSOR_REGISTRY_H

#include <Arduino.h>
#include <map>
#include <vector>


/**
 * @brief Sensor definition
 */
struct SensorDef {
  char type[32];   // e.g., "temperature", "humidity"
  char status[16]; // "ok", "missing", "disabled"
  float lastValue;
  bool hasValue;
  unsigned long lastUpdate;
};

/**
 * @brief Hardware definition with its sensors
 */
struct HardwareDef {
  char key[32];  // e.g., "dht22"
  char name[64]; // e.g., "DHT22 Temperature/Humidity Sensor"
  bool enabled;
  int intervalMs;
  unsigned long
      lastPublishTime; // Last time data was published for this hardware
  std::vector<SensorDef> sensors;
};

/**
 * @brief Registry for managing hardware and sensors
 */
class SensorRegistry {
public:
  SensorRegistry();

  // =========================================================================
  // Registration
  // =========================================================================

  /**
   * @brief Register a new hardware
   * @param key Unique hardware key
   * @param name Human-readable name
   * @return true if registered successfully
   */
  bool registerHardware(const char *key, const char *name);

  /**
   * @brief Add a sensor to a hardware
   * @param hardwareKey Hardware to add sensor to
   * @param sensorType Type of sensor measurement
   * @return true if added successfully
   */
  bool addSensor(const char *hardwareKey, const char *sensorType);

  // =========================================================================
  // Queries
  // =========================================================================

  /**
   * @brief Check if hardware exists
   */
  bool hasHardware(const char *key) const;

  /**
   * @brief Check if sensor exists on hardware
   */
  bool hasSensor(const char *hardwareKey, const char *sensorType) const;

  /**
   * @brief Get hardware by key
   * @return Pointer to hardware or nullptr
   */
  HardwareDef *getHardware(const char *key);
  const HardwareDef *getHardware(const char *key) const;

  /**
   * @brief Get sensor by hardware and type
   */
  SensorDef *getSensor(const char *hardwareKey, const char *sensorType);

  // =========================================================================
  // Composite Keys
  // =========================================================================

  /**
   * @brief Build composite key (hardware:sensor)
   * @param hardwareKey Hardware key
   * @param sensorType Sensor type
   * @param buffer Output buffer
   * @param bufferSize Buffer size
   */
  static void buildCompositeKey(const char *hardwareKey, const char *sensorType,
                                char *buffer, size_t bufferSize);

  /**
   * @brief Parse composite key
   * @param compositeKey Key to parse (e.g., "dht22:temperature")
   * @param hardwareKey Output hardware key
   * @param sensorType Output sensor type
   * @param bufferSize Size of output buffers
   * @return true if parsed successfully
   */
  static bool parseCompositeKey(const char *compositeKey, char *hardwareKey,
                                char *sensorType, size_t bufferSize);

  // =========================================================================
  // State Management
  // =========================================================================

  /**
   * @brief Update sensor value
   */
  void updateSensorValue(const char *hardwareKey, const char *sensorType,
                         float value);

  /**
   * @brief Set hardware enabled state
   */
  void setHardwareEnabled(const char *hardwareKey, bool enabled);

  /**
   * @brief Check if hardware is enabled
   */
  bool isHardwareEnabled(const char *hardwareKey) const;

  /**
   * @brief Set hardware interval
   */
  void setHardwareInterval(const char *hardwareKey, int intervalMs);

  /**
   * @brief Check if enough time has passed since last publish for this hardware
   * @param hardwareKey Hardware to check
   * @return true if publish is allowed based on interval
   */
  bool canPublish(const char *hardwareKey) const;

  /**
   * @brief Update the last publish time for a hardware
   * @param hardwareKey Hardware to update
   */
  void updatePublishTime(const char *hardwareKey);

  /**
   * @brief Get the last publish time for a hardware
   * @return Last publish time in milliseconds, or 0 if never published
   */
  unsigned long getLastPublishTime(const char *hardwareKey) const;

  // =========================================================================
  // Status Building
  // =========================================================================

  /**
   * @brief Build status JSON for all sensors
   * @param buffer Output buffer
   * @param bufferSize Buffer size
   * @return Number of bytes written
   */
  size_t buildStatusJson(char *buffer, size_t bufferSize) const;

  /**
   * @brief Build config JSON for all sensors (intervals)
   * @param buffer Output buffer
   * @param bufferSize Buffer size
   * @return Number of bytes written
   */
  size_t buildConfigJson(char *buffer, size_t bufferSize) const;

  /**
   * @brief Build announce JSON hardware array
   *
   * Generates the "hardware" array for the announce message:
   * [{"key":"scd41","name":"SCD41","sensors":["co2","temperature","humidity"]},
   * ...]
   *
   * @param buffer Output buffer
   * @param bufferSize Buffer size
   * @return Number of bytes written
   */
  size_t buildAnnounceHardwareJson(char *buffer, size_t bufferSize) const;

  /**
   * @brief Get all hardware definitions
   */
  const std::vector<HardwareDef> &getAllHardware() const { return _hardware; }

private:
  std::vector<HardwareDef> _hardware;

  int findHardwareIndex(const char *key) const;
  int findSensorIndex(const HardwareDef &hw, const char *sensorType) const;
};

#endif // SENSOR_REGISTRY_H
