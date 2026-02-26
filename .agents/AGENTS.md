# IoT Mesurable - ESP Bootstrap Library

Librairie PlatformIO (C++) pour l'intégration plug-and-play des modules ESP32 avec l'écosystème IoT Mesurable.

## Rôle dans l'écosystème

C'est la **dépendance fondamentale** de tous les modules ESP32. Elle gère :
- WiFiManager (portail captif pour config WiFi)
- Connexion/reconnexion MQTT automatique
- Enregistrement et publication des capteurs
- Enable/disable à distance depuis le dashboard
- Synchronisation des configurations (intervalles)
- Persistance des settings (EEPROM/Preferences)

## Architecture MQTT

### Topics publiés
- `{moduleId}/{hardwareId}/{measurement}` → Valeurs capteurs
- `{moduleId}/sensors/status` → JSON statut (retained)

### Topics souscrits
- `{moduleId}/sensors/config` → Config intervalles
- `{moduleId}/sensors/enable` → Enable/disable hardware

## API clé

```cpp
IotMesurable brain("module-id");
brain.begin();                                    // WiFiManager
brain.begin("ssid", "pass");                     // Direct
brain.begin("ssid", "pass", "broker", 1883);     // Full
brain.registerHardware("dht22", "DHT22");
brain.addSensor("dht22", "temperature");
brain.publish("dht22", "temperature", 23.5);
brain.loop();
```

## Bonnes pratiques

- Toujours appeler `brain.loop()` dans le `loop()` Arduino
- Utiliser `brain.isConnected()` avant de publier (optionnel, la lib gère)
- Le `moduleId` doit correspondre à l'identifiant enregistré côté serveur
- Le `hardwareId` dans `registerHardware()` doit matcher le nom utilisé dans les topics MQTT

## Tests

```bash
pio test -e native
```

## Publication PlatformIO Registry

```bash
pio account login
pio package pack
pio package publish
```
