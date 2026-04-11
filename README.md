# Mji_Safi_Connect-formware

Firmware embarqué pour nœuds capteurs Heltec WiFi LoRa 32 V3 (ESP32 + SX1262), conforme au cahier des charges matériel décrit dans `firmware.md`.

## Voici Ce qu'on a implémenté :

- Le Pilotage du MOSFET sur `GPIO 12` pour couper/rétablir l'alimentation du JSN-SR04T.
- L'Acquisition distance via `GPIO 13` (Trig) et `GPIO 14` (Echo), avec filtrage médian.
- L'Encodage payload binaire exact de `2 octets`:
  - Octet 0: taux de remplissage `0..100`
  - Octet 1: tension batterie en décivolts (exemple `41` => `4.1V`)
- Le Transmission LoRaWAN OTAA (classe A, région EU868 par défaut).
- Le Deep Sleep de `4 heures` entre deux cycles.
- La Persistance RTC minimale (compteurs + dernières valeurs utiles).

## Démarrage rapide

1. Copier `include/lorawan_secrets.h.example` en `include/lorawan_secrets.h`.
2. Renseigner vos clés OTAA (`DeviceEUI`, `AppEUI`, `AppKey`).
3. Vérifier les constantes matérielles dans `include/project_config.h`:
   - `BATTERY_ADC_PIN`
   - `BATTERY_DIVIDER_RATIO`
   - `DISTANCE_EMPTY_CM` / `DISTANCE_FULL_CM`
4. Compiler/téléverser avec PlatformIO sur l'environnement `heltec_wifi_lora_32_v3`.

## Fichiers principaux

- `src/main.cpp`: logique complète firmware (mesure, payload, LoRaWAN, sommeil).
- `include/project_config.h`: paramètres de brochage, timing et calibration.
- `include/lorawan_secrets.h.example`: modèle de secrets LoRaWAN.
