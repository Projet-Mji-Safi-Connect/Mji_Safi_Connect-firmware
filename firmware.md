# Spécifications Matérielles - Projet Smart Waste Goma

Ce document sert de guide de conception et d'assemblage (cahier des charges) pour la partie matérielle du projet. L'objectif est de produire **4 prototypes de nœuds capteurs** et **1 passerelle de réception**, en respectant les contraintes d'autonomie et de robustesse définies.

---

## 1. 🎯 Objectif de Conception

Le matériel doit répondre à deux objectifs critiques :

1.  **Autonomie Énergétique Extrême :** Le nœud capteur doit pouvoir fonctionner pendant **plus de 6 mois sur une seule batterie 18650**. La consommation journalière doit être optimisée pour être **inférieure à 1 Wh/jour**.
2.  **Robustesse (IP67) :** Le nœud doit être totalement étanche à la pluie, à l'humidité et à la poussière, car il sera déployé en extérieur (sur les poubelles).

---

## 2. 📋 Liste des Composants (Nomenclature)

### 2.1 Pour la Passerelle (x1 Unité)

| Composant | Spécification Recommandée | Rôle |
| :--- | :--- | :--- |
| **Passerelle LoRaWAN** | 8-Canaux, basée sur **SX1308** ou **SX1302** | Réception multi-fréquences conforme à la norme (ex: Dragino LG308, Heltec HT-M01). |
| **Antenne 868MHz** | Antenne externe à gain (3dBi ou 5dBi) | Assurer une couverture maximale pour la réception des 4 nœuds. |
| **Câble Ethernet** | Cat 5e ou 6 | Connexion fiable de la passerelle au routeur Internet. |
| **Alimentation** | 5V ou 12V (selon la passerelle) | Alimentation principale de la passerelle. |

### 2.2 Pour les Nœuds Capteurs (x4 Unités)

*La liste ci-dessous est pour **1 nœud**. Vous devez la multiplier par 4.*

| Composant | Spécification Recommandée | Rôle |
| :--- | :--- | :--- |
| **Cœur & Radio** | **Heltec WiFi LoRa 32 V3** | Le cerveau : ESP32 (pour Deep Sleep), LoRa (SX1262), et chargeur LiPo intégré. |
| **Capteur de Distance** | **JSN-SR04T 2.0 (Waterproof)** | Les yeux : Mesure étanche du niveau de remplissage. |
| **Batterie** | **Li-Ion 18650, 3.7V** | L'énergie : Haute capacité (ex: 3400mAh) pour une autonomie maximale. |
| **Support Batterie** | Support 18650 pour PCB | Fixation sécurisée de la batterie sur le circuit d'assemblage. |
| **Gestion Puissance**| **MOSFET Canal-N (Logic Level)** | **L'OPTIMISATION CLÉ :** Agit comme interrupteur pour couper le 5V du JSN. (Réf: **BS170** ou **2N7000**). |
| **Gestion Tension** | **Level Shifter Bidirectionnel** | **LA SÉCURITÉ CLÉ :** Convertit les signaux 5V (JSN) en 3.3V (ESP32) et vice-versa. |
| **Boîtier** | **Boîte de Jonction IP67** | La coque : Protection étanche (ex: 100x100x70mm). |
| **Presse-Étoupe** | **M12** | Le joint : Assure l'étanchéité du câble capteur à la sortie du boîtier. |
| **Circuit d'Assemblage**| **Perfboard (Plaque de prototypage)** | Le "PCB maison" : Support pour souder les composants de manière propre. |
| **Connectique** | Peignes Mâles & Femelles | Pour rendre la carte Heltec amovible de la Perfboard. |

---

## 3. 💡 Philosophie de Conception (L'Approche Ingénieur)

C'est ici que nous résolvons les contraintes.

### A. L'Optimisation Énergétique (MOSFET)

* **Le Problème :** Le capteur JSN-SR04T, même en veille (non déclenché), consomme du courant (fuite de courant/quiescent current). Sur une batterie, cela viderait l'accumulateur en quelques semaines.
* **La Solution (Matérielle) :** Nous n'allons pas seulement mettre l'ESP32 en "Deep Sleep". Nous allons utiliser un **MOSFET comme interrupteur** contrôlé par l'ESP32.
* **Logique :** L'ESP32 coupe totalement l'alimentation 5V du capteur JSN, dort pendant 4 heures (Consommation quasi nulle), puis se réveille, réalimente le capteur via le MOSFET, prend la mesure, coupe l'alimentation, et se rendort.

### B. La Compatibilité des Tensions (Level Shifter)

* **Le Problème :** Le capteur JSN-SR04T fonctionne en **5V** (VCC, Trig, Echo). La carte Heltec (ESP32) fonctionne en **3.3V**.
* **Le Risque :** Connecter la broche "Echo" (5V) du capteur directement à une broche de l'ESP32 (3.3V) **détruira** le microcontrôleur.
* **La Solution (Matérielle) :** Un **Level Shifter** (convertisseur de niveau) doit être placé entre les broches Trig/Echo du capteur et les broches GPIO de l'ESP32 pour traduire les signaux de 3.3V à 5V (et vice-versa) en toute sécurité.

---

## 4. 🔧 Guide d'Assemblage (Le "Prompt" de Réalisation)

Suivre ces étapes pour assembler **un (1) nœud capteur** de manière professionnelle sur Perfboard.

### Étape 1 : Préparation de la Perfboard et du Cœur
1.  Souder les **peignes mâles** sur la carte Heltec V3.
2.  Souder les **peignes femelles** correspondants sur la Perfboard. Cela crée un "socket" pour que la carte Heltec soit amovible.
3.  Identifier les sorties de la carte Heltec nécessaires : `3.3V`, `5V`, `GND`, et les 3 `GPIO` (ex: `GPIO 12` pour le MOSFET, `GPIO 13`/`14` pour le Level Shifter).

### Étape 2 : Le Circuit de Puissance (Le MOSFET)
*C'est le circuit qui contrôle le 5V du capteur JSN.*
1.  Prendre le MOSFET (ex: BS170).
2.  Souder la broche **Source** du MOSFET au `GND` principal (venant du Heltec).
3.  Souder la broche **Gate** du MOSFET à un `GPIO` du Heltec (ex: `GPIO 12`).
4.  La broche **Drain** du MOSFET devient le **nouveau GND** pour le capteur JSN.
5.  Souder le `VCC` du capteur JSN directement au `5V` du Heltec.
    *(Logique : En mettant `GPIO 12` à HIGH, le MOSFET connecte le Drain à la Source, fermant le circuit GND du capteur et l'allumant).*

### Étape 3 : Le Circuit Logique (Le Level Shifter)
*C'est le circuit qui traduit les signaux Trig/Echo.*
1.  Prendre le Level Shifter.
2.  Alimenter le côté **HV (Haute Tension)** avec le `5V` et le `GND` du Heltec.
3.  Alimenter le côté **LV (Basse Tension)** avec le `3.3V` et le `GND` du Heltec.
4.  Connecter la broche `Trig` (du JSN) au côté **HV** (ex: `HV1`).
5.  Connecter la broche `Echo` (du JSN) au côté **HV** (ex: `HV2`).
6.  Connecter le `GPIO 13` (du Heltec) au côté **LV** (ex: `LV1`).
7.  Connecter le `GPIO 14` (du Heltec) au côté **LV** (ex: `LV2`).

### Étape 4 : L'Alimentation Principale
1.  Souder le **support de batterie 18650** sur la Perfboard.
2.  Connecter les sorties `+` et `-` du support de batterie directement aux entrées `BAT+` et `GND` de la carte Heltec V3.

### Étape 5 : L'Assemblage du Boîtier (Étanchéité)
1.  Percer deux trous propres dans le boîtier IP67 :
    * Un trou (ex: 20mm) pour le **capteur JSN-SR04T** (la sonde).
    * Un trou (ex: 12mm) pour le **presse-étoupe M12**.
2.  (Si le câble JSN est trop court) Couper le câble du JSN. Le faire passer par le presse-étoupe (de l'intérieur vers l'extérieur). Ressouder le capteur.
3.  Fixer la sonde JSN sur le couvercle (utiliser de la colle chaude ou du silicone pour l'étanchéité).
4.  Visser le presse-étoupe.
5.  Fixer la Perfboard assemblée à l'intérieur du boîtier (avec des vis ou de l'adhésif VHB).
6.  Ajouter un **sachet de silice (Silica Gel)** avant de visser le couvercle final.

---

## 5. 🔗 Contrat d'Interface (Le Lien avec le Logiciel)

Ce matériel est conçu pour exécuter un firmware qui respecte le contrat suivant :

* **Firmware :** Le code doit utiliser les `GPIO` définis (12, 13, 14) pour contrôler le MOSFET et le Level Shifter. Il doit utiliser le **Deep Sleep** et la **sauvegarde de session RTC** (définis dans le prompt logiciel Phase 3).
* **Payload de Données :** Pour être compatible avec le backend, le firmware doit envoyer un payload **binaire de 2 octets** :
    * `Octet 0`: Remplissage (0-100)
    * `Octet 1`: Tension Batterie (ex: 41 pour 4.1V)