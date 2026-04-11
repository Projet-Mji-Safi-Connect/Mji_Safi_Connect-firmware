#include <Arduino.h>
#include <LoRaWan_APP.h>
#include <esp_sleep.h>
#include <math.h>

#include "project_config.h"

#if __has_include("lorawan_secrets.h")
#include "lorawan_secrets.h"
#else
#error "Fichier manquant: include/lorawan_secrets.h. Copiez include/lorawan_secrets.h.example puis renseignez les clés OTAA."
#endif

// ======================================================================
// Ce firmware implémente le contrat du cahier des charges firmware.md :
// - GPIO 12: pilotage MOSFET (alimentation capteur JSN-SR04T)
// - GPIO 13/14: Trig/Echo via Level Shifter
// - Deep Sleep 4h pour autonomie maximale
// - Payload binaire de 2 octets :
//   Octet 0 = taux de remplissage (0..100)
//   Octet 1 = tension batterie en décivolts (ex: 41 => 4.1V)
//
// Flux global d'un cycle:
//   1) Boot / Réveil
//   2) Join LoRaWAN si nécessaire
//   3) Allumage capteur via MOSFET
//   4) Mesure distance + batterie
//   5) Encodage payload binaire (2 octets)
//   6) Uplink LoRaWAN
//   7) Deep Sleep 4 heures
// ======================================================================

// ----------------------------------------------------------------------
// Paramètres LoRaWAN (OTAA)
// ----------------------------------------------------------------------
uint8_t devEui[] = LORAWAN_DEVICE_EUI;
uint8_t appEui[] = LORAWAN_APPLICATION_EUI;
uint8_t appKey[] = LORAWAN_APPLICATION_KEY;

// Variables ABP non utilisées ici (obligatoires pour certaines versions de stack).
uint8_t nwkSKey[] = {0};
uint8_t appSKey[] = {0};
uint32_t devAddr = 0;

uint16_t userChannelsMask[6] = {0x00FF, 0, 0, 0, 0, 0};

// Classe A: plus économe, adaptée aux objets sur batterie.
DeviceClass_t loraWanClass = CLASS_A;
LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;
// Cette valeur est fournie à la stack Heltec. Le Deep Sleep explicite du code
// reste le mécanisme principal qui cadence le cycle réel.
uint32_t appTxDutyCycle = MEASUREMENT_PERIOD_SECONDS * 1000UL;
bool overTheAirActivation = true;
bool loraWanAdr = true;
// keepNet=true demande à conserver le contexte réseau quand possible.
bool keepNet = true;
// false = uplink non confirmé (moins coûteux en énergie qu'un confirmé).
bool isTxConfirmed = false;
uint8_t appPort = LORAWAN_APP_PORT;
uint8_t confirmedNbTrials = 4;

// Buffer uplink transmis à la passerelle LoRaWAN.
uint8_t appData[LORAWAN_APP_DATA_MAX_SIZE];
uint8_t appDataSize = 0;

// ----------------------------------------------------------------------
// Persistance RTC (survive aux réveils Deep Sleep)
// ----------------------------------------------------------------------
struct SessionRtc {
  // Marqueur pour savoir si la mémoire RTC a déjà été initialisée.
  uint32_t magic;
  // Nombre total de réveils (utile pour diagnostic terrain).
  uint32_t wakeCounter;
  // Nombre total d'uplinks envoyés.
  uint32_t uplinkCounter;
  // Dernier niveau valide calculé (fallback en cas de mesure capteur invalide).
  uint8_t lastFillPercent;
  // Dernière tension batterie valide en décivolts.
  uint8_t lastBatteryDeciVolt;
};

static constexpr uint32_t RTC_MAGIC = 0x4D4A4953;  // "MJIS"
RTC_DATA_ATTR SessionRtc rtcSession;

// ----------------------------------------------------------------------
// Utilitaires
// ----------------------------------------------------------------------
static int clampInt(const int value, const int minValue, const int maxValue) {
  // Empêche toute sortie hors plage, indispensable avant de caster en uint8_t.
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

// Tri simple pour extraire une médiane sur un petit tableau.
static void sortFloatArray(float *values, const uint8_t count) {
  // Tri quadratique volontaire: tableau très petit, code lisible et robuste.
  for (uint8_t i = 0; i < count; ++i) {
    for (uint8_t j = i + 1; j < count; ++j) {
      if (values[j] < values[i]) {
        const float tmp = values[i];
        values[i] = values[j];
        values[j] = tmp;
      }
    }
  }
}

// ----------------------------------------------------------------------
// Gestion puissance capteur (MOSFET)
// ----------------------------------------------------------------------
static void setSensorPower(const bool enabled) {
  // HIGH sur la Gate => fermeture du chemin GND capteur via MOSFET => capteur actif.
  // LOW => capteur réellement hors tension (réduction consommation de fuite).
  digitalWrite(PIN_MOSFET_GATE, enabled ? HIGH : LOW);
  if (enabled) {
    // Temps de stabilisation électronique avant la première mesure.
    delay(SENSOR_BOOT_DELAY_MS);
  }
}

// ----------------------------------------------------------------------
// Mesure distance JSN-SR04T
// ----------------------------------------------------------------------
static float readDistanceCmSingleShot() {
  // Impulsion de déclenchement standard du capteur ultrasons.
  digitalWrite(PIN_JSN_TRIG, LOW);
  delayMicroseconds(4);
  digitalWrite(PIN_JSN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_JSN_TRIG, LOW);

  const unsigned long echoDurationUs = pulseIn(PIN_JSN_ECHO, HIGH, ECHO_TIMEOUT_US);
  if (echoDurationUs == 0) {
    // Aucun écho reçu dans la fenêtre de temps: mesure invalide.
    return -1.0F;
  }

  // Vitesse du son ~343 m/s => 0.0343 cm/us, aller-retour /2.
  return (static_cast<float>(echoDurationUs) * 0.0343F) / 2.0F;
}

static float readDistanceCmMedian() {
  // Le JSN-SR04T peut produire des mesures aberrantes ponctuelles.
  // On capture N mesures puis on prend la médiane (plus robuste qu'une moyenne).
  float samples[DISTANCE_SAMPLE_COUNT];
  uint8_t validCount = 0;

  for (uint8_t i = 0; i < DISTANCE_SAMPLE_COUNT; ++i) {
    const float cm = readDistanceCmSingleShot();
    if (cm > 0.0F) {
      samples[validCount++] = cm;
    }
    delay(60);
  }

  if (validCount == 0) {
    return -1.0F;
  }

  sortFloatArray(samples, validCount);
  return samples[validCount / 2];
}

// Convertit une distance en pourcentage de remplissage.
static uint8_t computeFillPercent(const float distanceCm) {
  // Si mesure invalide ou calibration incohérente, on conserve la dernière valeur fiable.
  if (distanceCm <= 0.0F || DISTANCE_EMPTY_CM <= DISTANCE_FULL_CM) {
    return rtcSession.lastFillPercent;
  }

  const float ratio = (DISTANCE_EMPTY_CM - distanceCm) / (DISTANCE_EMPTY_CM - DISTANCE_FULL_CM);
  const int percent = static_cast<int>(roundf(ratio * 100.0F));
  return static_cast<uint8_t>(clampInt(percent, 0, 100));
}

// ----------------------------------------------------------------------
// Mesure batterie
// ----------------------------------------------------------------------
static uint16_t readBatteryMilliVolts() {
  // Lecture multi-échantillons pour lisser le bruit ADC.
  uint32_t accumulator = 0;
  for (uint8_t i = 0; i < BATTERY_SAMPLE_COUNT; ++i) {
    accumulator += static_cast<uint32_t>(analogReadMilliVolts(BATTERY_ADC_PIN));
    delay(5);
  }

  const float avgMv = static_cast<float>(accumulator) / static_cast<float>(BATTERY_SAMPLE_COUNT);
  // Reprojection vers la tension batterie réelle selon le pont diviseur.
  const float batteryMv = avgMv * BATTERY_DIVIDER_RATIO;
  return static_cast<uint16_t>(batteryMv);
}

static uint8_t milliVoltsToDeciVolts(const uint16_t milliVolts) {
  // Arrondi au décivolt le plus proche: +50 mV avant division par 100.
  const int deciVolts = static_cast<int>((milliVolts + 50U) / 100U);
  return static_cast<uint8_t>(clampInt(deciVolts, 0, 255));
}

// ----------------------------------------------------------------------
// Acquisition globale + construction payload métier
// ----------------------------------------------------------------------
static void buildApplicationPayload() {
  // 1) Capteur ON le plus tard possible (économie d'énergie).
  setSensorPower(true);
  const float distanceCm = readDistanceCmMedian();
  // 2) Capteur OFF immédiatement après acquisition.
  setSensorPower(false);

  // 3) Mesure batterie et conversion métier.
  const uint16_t batteryMv = readBatteryMilliVolts();
  const uint8_t fillPercent = computeFillPercent(distanceCm);
  const uint8_t batteryDeciV = milliVoltsToDeciVolts(batteryMv);

  // 4) Sauvegarde RTC: permet de garder une valeur exploitable au cycle suivant
  //    même en cas de mesure ponctuellement invalide.
  rtcSession.lastFillPercent = fillPercent;
  rtcSession.lastBatteryDeciVolt = batteryDeciV;

  // Contrat backend: payload binaire EXACT de 2 octets.
  appData[0] = fillPercent;
  appData[1] = batteryDeciV;
  appDataSize = 2;

  Serial.printf(
      "[MESURE] distance=%.2f cm | remplissage=%u%% | batterie=%u mV (%u dV)\n",
      distanceCm,
      fillPercent,
      batteryMv,
      batteryDeciV);
}

// Fonction attendue par la stack Heltec pour préparer l'uplink.
void prepareTxFrame(uint8_t port) {
  // Le port est défini globalement; on l'ignore ici pour rester explicite.
  (void)port;
  buildApplicationPayload();
}

// ----------------------------------------------------------------------
// Gestion Deep Sleep
// ----------------------------------------------------------------------
static void enterDeepSleepForNextCycle() {
  Serial.printf(
      "[SLEEP] cycle termine. prochaine mesure dans %lu secondes.\n",
      static_cast<unsigned long>(MEASUREMENT_PERIOD_SECONDS));
  Serial.flush();

  esp_sleep_enable_timer_wakeup(
      static_cast<uint64_t>(MEASUREMENT_PERIOD_SECONDS) * 1000000ULL);
  // À partir d'ici, le CPU s'arrête et redémarre sur setup() au prochain réveil.
  esp_deep_sleep_start();
}

// ----------------------------------------------------------------------
// Cycle principal
// ----------------------------------------------------------------------
void setup() {
  // Console série uniquement pour diagnostic (peut être désactivée en production stricte).
  Serial.begin(115200);
  delay(200);

  // Initialisation I/O matérielles.
  pinMode(PIN_MOSFET_GATE, OUTPUT);
  pinMode(PIN_JSN_TRIG, OUTPUT);
  pinMode(PIN_JSN_ECHO, INPUT);
  // Garantie sécurité: capteur hors tension dès le boot.
  setSensorPower(false);

  // Résolution ADC standard ESP32.
  analogReadResolution(12);

  // Première exécution après flash/redémarrage complet: on initialise la session RTC.
  if (rtcSession.magic != RTC_MAGIC) {
    rtcSession.magic = RTC_MAGIC;
    rtcSession.wakeCounter = 0;
    rtcSession.uplinkCounter = 0;
    rtcSession.lastFillPercent = 0;
    rtcSession.lastBatteryDeciVolt = 0;
  }

  rtcSession.wakeCounter++;

  Serial.printf(
      "\n[BOOT] wake #%lu | uplinks=%lu | wakeup_cause=%d\n",
      static_cast<unsigned long>(rtcSession.wakeCounter),
      static_cast<unsigned long>(rtcSession.uplinkCounter),
      static_cast<int>(esp_sleep_get_wakeup_cause()));

  // Initialisation bas niveau Heltec (MCU + radio SX1262 + stack associée).
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  // Démarrage machine d'état LoRaWAN.
  deviceState = DEVICE_STATE_INIT;
}

void loop() {
  // Machine d'état LoRaWAN Heltec:
  // INIT  -> configuration stack
  // JOIN  -> attache réseau OTAA
  // SEND  -> préparation payload + transmission
  // CYCLE -> planification prochain cycle (ici Deep Sleep explicite)
  // SLEEP -> fallback sécurité vers Deep Sleep
  switch (deviceState) {
    case DEVICE_STATE_INIT:
      // Charge configuration région/classe et data rate initial.
      LoRaWAN.init(loraWanClass, loraWanRegion);
      LoRaWAN.setDefaultDR(LORAWAN_DEFAULT_DR);
      deviceState = DEVICE_STATE_JOIN;
      break;

    case DEVICE_STATE_JOIN:
      // Tentative de join OTAA (devEui/appEui/appKey).
      LoRaWAN.join();
      break;

    case DEVICE_STATE_SEND:
      // Préparation stricte du payload métier puis émission radio.
      prepareTxFrame(appPort);
      LoRaWAN.send();
      rtcSession.uplinkCounter++;
      deviceState = DEVICE_STATE_CYCLE;
      break;

    case DEVICE_STATE_CYCLE:
      // On force le Deep Sleep long plutôt que la veille légère.
      enterDeepSleepForNextCycle();
      break;

    case DEVICE_STATE_SLEEP:
      // Fallback sécurité: si la stack repasse ici, on dort tout de suite.
      enterDeepSleepForNextCycle();
      break;

    default:
      deviceState = DEVICE_STATE_INIT;
      break;
  }
}
