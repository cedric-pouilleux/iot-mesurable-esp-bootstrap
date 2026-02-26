/**
 * @file test_sensor_registry.cpp
 * @brief Unit tests for SensorRegistry
 */

#include "../src/core/SensorRegistry.h"
#include <unity.h>


void setUp(void) {
  // Runs before each test
}

void tearDown(void) {
  // Runs after each test
}

// =============================================================================
// Registration Tests
// =============================================================================

void test_register_hardware_success() {
  SensorRegistry reg;
  TEST_ASSERT_TRUE(reg.registerHardware("dht22", "DHT22 Sensor"));
  TEST_ASSERT_TRUE(reg.hasHardware("dht22"));
}

void test_register_hardware_duplicate_fails() {
  SensorRegistry reg;
  TEST_ASSERT_TRUE(reg.registerHardware("dht22", "DHT22"));
  TEST_ASSERT_FALSE(reg.registerHardware("dht22", "DHT22 Again"));
}

void test_register_hardware_empty_key_fails() {
  SensorRegistry reg;
  TEST_ASSERT_FALSE(reg.registerHardware("", "Empty Key"));
  TEST_ASSERT_FALSE(reg.registerHardware(nullptr, "Null Key"));
}

void test_add_sensor_success() {
  SensorRegistry reg;
  reg.registerHardware("dht22", "DHT22");
  TEST_ASSERT_TRUE(reg.addSensor("dht22", "temperature"));
  TEST_ASSERT_TRUE(reg.hasSensor("dht22", "temperature"));
}

void test_add_sensor_to_missing_hardware_fails() {
  SensorRegistry reg;
  TEST_ASSERT_FALSE(reg.addSensor("missing", "temperature"));
}

void test_add_sensor_duplicate_fails() {
  SensorRegistry reg;
  reg.registerHardware("dht22", "DHT22");
  TEST_ASSERT_TRUE(reg.addSensor("dht22", "temperature"));
  TEST_ASSERT_FALSE(reg.addSensor("dht22", "temperature"));
}

// =============================================================================
// Composite Key Tests
// =============================================================================

void test_build_composite_key() {
  char buffer[64];
  SensorRegistry::buildCompositeKey("dht22", "temperature", buffer,
                                    sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("dht22:temperature", buffer);
}

void test_parse_composite_key_success() {
  char hw[32], sensor[32];
  TEST_ASSERT_TRUE(
      SensorRegistry::parseCompositeKey("dht22:temperature", hw, sensor, 32));
  TEST_ASSERT_EQUAL_STRING("dht22", hw);
  TEST_ASSERT_EQUAL_STRING("temperature", sensor);
}

void test_parse_composite_key_no_colon_fails() {
  char hw[32], sensor[32];
  TEST_ASSERT_FALSE(SensorRegistry::parseCompositeKey("nodot", hw, sensor, 32));
}

void test_parse_composite_key_empty_parts_fails() {
  char hw[32], sensor[32];
  TEST_ASSERT_FALSE(
      SensorRegistry::parseCompositeKey(":sensor", hw, sensor, 32));
  TEST_ASSERT_FALSE(SensorRegistry::parseCompositeKey("hw:", hw, sensor, 32));
}

// =============================================================================
// State Tests
// =============================================================================

void test_update_sensor_value() {
  SensorRegistry reg;
  reg.registerHardware("dht22", "DHT22");
  reg.addSensor("dht22", "temperature");

  reg.updateSensorValue("dht22", "temperature", 23.5);

  SensorDef *sensor = reg.getSensor("dht22", "temperature");
  TEST_ASSERT_NOT_NULL(sensor);
  TEST_ASSERT_EQUAL_FLOAT(23.5, sensor->lastValue);
  TEST_ASSERT_TRUE(sensor->hasValue);
  TEST_ASSERT_EQUAL_STRING("ok", sensor->status);
}

void test_hardware_enabled_default() {
  SensorRegistry reg;
  reg.registerHardware("dht22", "DHT22");
  TEST_ASSERT_TRUE(reg.isHardwareEnabled("dht22"));
}

void test_set_hardware_disabled() {
  SensorRegistry reg;
  reg.registerHardware("dht22", "DHT22");
  reg.addSensor("dht22", "temperature");

  reg.setHardwareEnabled("dht22", false);

  TEST_ASSERT_FALSE(reg.isHardwareEnabled("dht22"));

  SensorDef *sensor = reg.getSensor("dht22", "temperature");
  TEST_ASSERT_EQUAL_STRING("disabled", sensor->status);
}

// =============================================================================
// JSON Status Tests
// =============================================================================

void test_build_status_json_empty() {
  SensorRegistry reg;
  char buffer[256];
  reg.buildStatusJson(buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("{}", buffer);
}

void test_build_status_json_with_sensor() {
  SensorRegistry reg;
  reg.registerHardware("dht22", "DHT22");
  reg.addSensor("dht22", "temperature");
  reg.updateSensorValue("dht22", "temperature", 25.0);

  char buffer[256];
  reg.buildStatusJson(buffer, sizeof(buffer));

  // Should contain the composite key and value
  TEST_ASSERT_NOT_NULL(strstr(buffer, "dht22:temperature"));
  TEST_ASSERT_NOT_NULL(strstr(buffer, "25.00"));
  TEST_ASSERT_NOT_NULL(strstr(buffer, "ok"));
}

void test_build_status_json_disabled_hardware() {
  SensorRegistry reg;
  reg.registerHardware("dht22", "DHT22");
  reg.addSensor("dht22", "temperature");
  reg.setHardwareEnabled("dht22", false);

  char buffer[256];
  reg.buildStatusJson(buffer, sizeof(buffer));

  TEST_ASSERT_NOT_NULL(strstr(buffer, "disabled"));
}

// =============================================================================
// Announce JSON Tests
// =============================================================================

void test_build_announce_json_empty() {
  SensorRegistry reg;
  char buffer[256];
  reg.buildAnnounceHardwareJson(buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("[]", buffer);
}

void test_build_announce_json_single_hardware() {
  SensorRegistry reg;
  reg.registerHardware("scd41", "SCD41");
  reg.addSensor("scd41", "co2");
  reg.addSensor("scd41", "temperature");
  reg.addSensor("scd41", "humidity");

  char buffer[512];
  reg.buildAnnounceHardwareJson(buffer, sizeof(buffer));

  // Should contain the hardware key and name
  TEST_ASSERT_NOT_NULL(strstr(buffer, "\"key\":\"scd41\""));
  TEST_ASSERT_NOT_NULL(strstr(buffer, "\"name\":\"SCD41\""));
  // Should contain sensors array
  TEST_ASSERT_NOT_NULL(strstr(buffer, "\"co2\""));
  TEST_ASSERT_NOT_NULL(strstr(buffer, "\"temperature\""));
  TEST_ASSERT_NOT_NULL(strstr(buffer, "\"humidity\""));
  // Should start and end correctly
  TEST_ASSERT_EQUAL('[', buffer[0]);
  TEST_ASSERT_EQUAL(']', buffer[strlen(buffer) - 1]);
}

void test_build_announce_json_multiple_hardware() {
  SensorRegistry reg;
  reg.registerHardware("scd41", "SCD41");
  reg.addSensor("scd41", "co2");
  reg.registerHardware("sgp40", "SGP40");
  reg.addSensor("sgp40", "voc");

  char buffer[512];
  reg.buildAnnounceHardwareJson(buffer, sizeof(buffer));

  // Should contain both hardware entries
  TEST_ASSERT_NOT_NULL(strstr(buffer, "\"key\":\"scd41\""));
  TEST_ASSERT_NOT_NULL(strstr(buffer, "\"key\":\"sgp40\""));
  TEST_ASSERT_NOT_NULL(strstr(buffer, "\"co2\""));
  TEST_ASSERT_NOT_NULL(strstr(buffer, "\"voc\""));
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char **argv) {
  UNITY_BEGIN();

  // Registration
  RUN_TEST(test_register_hardware_success);
  RUN_TEST(test_register_hardware_duplicate_fails);
  RUN_TEST(test_register_hardware_empty_key_fails);
  RUN_TEST(test_add_sensor_success);
  RUN_TEST(test_add_sensor_to_missing_hardware_fails);
  RUN_TEST(test_add_sensor_duplicate_fails);

  // Composite Keys
  RUN_TEST(test_build_composite_key);
  RUN_TEST(test_parse_composite_key_success);
  RUN_TEST(test_parse_composite_key_no_colon_fails);
  RUN_TEST(test_parse_composite_key_empty_parts_fails);

  // State
  RUN_TEST(test_update_sensor_value);
  RUN_TEST(test_hardware_enabled_default);
  RUN_TEST(test_set_hardware_disabled);

  // JSON Status
  RUN_TEST(test_build_status_json_empty);
  RUN_TEST(test_build_status_json_with_sensor);
  RUN_TEST(test_build_status_json_disabled_hardware);

  // Announce JSON
  RUN_TEST(test_build_announce_json_empty);
  RUN_TEST(test_build_announce_json_single_hardware);
  RUN_TEST(test_build_announce_json_multiple_hardware);

  return UNITY_END();
}
