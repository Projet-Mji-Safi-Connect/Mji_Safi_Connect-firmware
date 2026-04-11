# Analyse Technique Approfondie - Mji_Safi_Connect

Ce document traduit `firmware.md` en exigences firmware concrètes pour faciliter la maintenance par les prochains développeurs.

## 1. Exigences non négociables

- Autonomie cible: plus de 6 mois sur batterie 18650.
- Cycle de travail imposé: réveil périodique (4h), mesure, transmission, puis Deep Sleep.
- Robustesse électrique: aucun signal 5V direct vers GPIO ESP32.
- Contrat backend figé: payload uplink de 2 octets uniquement.

## 2. Traduction architecture firmware

- Bloc alimentation capteur:
  - GPIO12 pilote un MOSFET canal N (ON pendant acquisition, OFF sinon).
  - Effet attendu: suppression du courant de repos du JSN-SR04T.
- Bloc mesure niveau:
  - GPIO13 = Trig, GPIO14 = Echo via Level Shifter.
  - Mesure ultrason prise en rafale courte, filtrée par médiane.
- Bloc énergie:
  - Deep Sleep ESP32 sur timer 4h.
  - Réveil, exécution unique du cycle, puis retour sommeil.
- Bloc communication:
  - LoRaWAN OTAA Classe A.
  - Payload métier compact:
    - byte[0] = taux de remplissage (0..100)
    - byte[1] = tension batterie en décivolts

## 3. Points de calibration terrain

- `DISTANCE_EMPTY_CM` et `DISTANCE_FULL_CM` doivent être mesurées sur le bac réel.
- `BATTERY_ADC_PIN` peut varier selon la révision carte Heltec V3.
- `BATTERY_DIVIDER_RATIO` dépend du pont diviseur effectivement câblé.

## 4. Stratégie de résilience logicielle

- Conservation RTC des dernières valeurs valides pour éviter un payload vide si lecture capteur échoue ponctuellement.
- Timeout Echo pour éviter le blocage en cas de capteur déconnecté ou noyé.
- Filtrage médian pour rejeter des valeurs aberrantes (vibrations, rebonds acoustiques).

## 5. Risques identifiés et mitigation

- Risque: partir en Deep Sleep avant fin uplink.
  - Mitigation: se baser sur l’état de cycle de la stack LoRaWAN avant l’entrée sommeil.
- Risque: batterie mal estimée.
  - Mitigation: calibration ADC sur banc avec multimètre.
- Risque: dérive de pourcentage remplissage selon forme de bac.
  - Mitigation: affiner les constantes de distance par type de poubelle.

## 6. Fichiers de référence dans ce dépôt

- `src/main.cpp`: implémentation firmware complète.
- `include/project_config.h`: constantes de brochage, timing et calibration.
- `include/lorawan_secrets.h.example`: modèle des clés OTAA à copier en local.

