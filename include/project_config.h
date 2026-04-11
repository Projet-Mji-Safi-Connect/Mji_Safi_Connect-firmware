#pragma once

#include <stdint.h>

// ============================
// Brochage matériel (imposé)
// ============================
// GPIO qui pilote la Gate du MOSFET (interrupteur GND du JSN-SR04T).
static constexpr uint8_t PIN_MOSFET_GATE = 12;

// GPIO côté ESP32 vers Trig/Echo du capteur via Level Shifter.
static constexpr uint8_t PIN_JSN_TRIG = 13;
static constexpr uint8_t PIN_JSN_ECHO = 14;

// ============================
// Cycle énergie / transmission
// ============================
// Ici notre cahier des charges nous impose une mesure + envoi toutes les 4 heures.
static constexpr uint32_t MEASUREMENT_PERIOD_SECONDS = 4UL * 60UL * 60UL;

// ============================
// Acquisition capteur ultrasons
// ============================
// Temps de stabilisation après alimentation du JSN via MOSFET.
static constexpr uint32_t SENSOR_BOOT_DELAY_MS = 250;

// Timeout pour pulseIn afin d'éviter de rester bloqué si la mesure échoue.
static constexpr uint32_t ECHO_TIMEOUT_US = 35000;

// Nombre de mesures brutes; on prendra la médiane pour rejeter les valeurs aberrantes.
static constexpr uint8_t DISTANCE_SAMPLE_COUNT = 3;

// Géométrie de la poubelle (à ajuster pendant la calibration terrain).
// distance_empty_cm: distance sonde->déchets lorsque la poubelle est vide.
// distance_full_cm : distance sonde->déchets lorsque la poubelle est considérée pleine.
static constexpr float DISTANCE_EMPTY_CM = 55.0F;
static constexpr float DISTANCE_FULL_CM = 10.0F;

// ============================
// Mesure batterie
// ============================
// Chose IMPORTANT que vous devez reternir ici est :
// Sur Heltec V3, la broche réelle de mesure batterie dépend du routage carte/révision.
// Si la valeur est incohérente, corriger BATTERY_ADC_PIN selon votre PCB.
static constexpr uint8_t BATTERY_ADC_PIN = 1;

// Coefficient du pont diviseur matériel pour remonter à la tension batterie réelle.
// 2.0 signifie que la tension mesurée au pin ADC est la moitié de la batterie.
static constexpr float BATTERY_DIVIDER_RATIO = 2.0F;

// Nombre d'échantillons ADC pour lisser la lecture.
static constexpr uint8_t BATTERY_SAMPLE_COUNT = 8;

// ============================
// LoRaWAN
// ============================
// Port applicatif utilisé pour le payload binaire métier.
static constexpr uint8_t LORAWAN_APP_PORT = 2;

// Data rate par défaut (EU868: DR0..DR5 selon couverture).
static constexpr uint8_t LORAWAN_DEFAULT_DR = 3;

