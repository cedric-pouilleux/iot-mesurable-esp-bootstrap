/**
 * @file MqttClient.h
 * @brief MQTT client wrapper for iot-mesurable
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>
#include <functional>

#ifndef NATIVE_BUILD
#include <AsyncMqttClient.h>
#endif

using MqttMessageCallback =
    std::function<void(const char *topic, const char *payload)>;
using MqttConnectCallback = std::function<void(bool connected)>;

/**
 * @brief MQTT client wrapper with auto-reconnect
 */
class MqttClient {
public:
  MqttClient();
  ~MqttClient();

  /**
   * @brief Configure broker
   */
  void setBroker(const char *host, uint16_t port);

  /**
   * @brief Set MQTT client ID
   */
  void setClientId(const char *clientId);

  /**
   * @brief Set MQTT credentials
   */
  void setCredentials(const char *username, const char *password);

  /**
   * @brief Connect to broker
   * @return true if connection initiated
   */
  bool connect();

  /**
   * @brief Disconnect from broker
   */
  void disconnect();

  /**
   * @brief Check connection status
   */
  bool isConnected() const;

  /**
   * @brief Subscribe to a topic
   */
  void subscribe(const char *topic);

  /**
   * @brief Publish a message
   * @param topic MQTT topic
   * @param payload Message payload
   * @param retain Whether to retain message
   * @param qos QoS level (0=fire-and-forget, 1=at-least-once)
   */
  void publish(const char *topic, const char *payload, bool retain = false,
               uint8_t qos = 0);

  /**
   * @brief Set message callback
   */
  void onMessage(MqttMessageCallback callback);

  /**
   * @brief Set connection callback
   */
  void onConnect(MqttConnectCallback callback);

  /**
   * @brief Handle reconnection logic
   */
  void loop();

private:
#ifndef NATIVE_BUILD
  AsyncMqttClient _client;
#endif
  char _host[128];
  char _clientId[64];
  char _username[64];
  char _password[64];
  uint16_t _port;
  bool _connected;
  unsigned long _lastReconnectAttempt;

  MqttMessageCallback _onMessage;
  MqttConnectCallback _onConnect;

  static const unsigned long RECONNECT_INTERVAL = 5000;

  void setupCallbacks();
};

#endif // MQTT_CLIENT_H
