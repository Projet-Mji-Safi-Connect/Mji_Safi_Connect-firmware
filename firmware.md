# Spécifications Matérielles - Mji_Safi_Connect

Ce document sert de guide de conception et d'assemblage (cahier des charges) pour la partie matérielle du projet. L'objectif est de produire **4 prototypes de nœuds capteurs** et **1 passerelle de réception**, en respectant les contraintes d'autonomie et de robustesse définies.

---

## 1. Objectif de Conception

Le matériel doit répondre à deux objectifs critiques :

1. **Autonomie énergétique extrême :** le nœud capteur doit pouvoir fonctionner pendant **plus de 6 mois sur une seule batterie 18650**. La consommation journalière doit être optimisée pour être **inférieure à 1 Wh/jour**.
2. **Robustesse (IP67) :** le nœud doit être totalement étanche à la pluie, à l'humidité et à la poussière, car il sera déployé en extérieur (sur les poubelles).

---

## 2. Liste des Composants (Nomenclature)

### 2.1 Pour la Passerelle (x1 unité)

| Composant | Spécification recommandée | Rôle |
| :--- | :--- | :--- |
| **Passerelle LoRaWAN** | 8 canaux, basée sur **SX1308** ou **SX1302** | Réception multi-fréquences conforme à la norme (ex: Dragino LG308, Heltec HT-M01). |
| **Antenne 868MHz** | Antenne externe à gain (3dBi ou 5dBi) | Assurer une couverture maximale pour la réception des 4 nœuds. |
| **Câble Ethernet** | Cat 5e ou 6 | Connexion fiable de la passerelle au routeur Internet. |
| **Alimentation** | 5V ou 12V (selon la passerelle) | Alimentation principale de la passerelle. |

### 2.2 Pour les Nœuds Capteurs (x4 unités)

La liste ci-dessous est pour **1 nœud**. Vous devez la multiplier par 4.

| Composant | Spécification recommandée | Rôle |
| :--- | :--- | :--- |
| **Cœur & Radio** | **Heltec WiFi LoRa 32 V3** | Le cerveau : ESP32 (pour Deep Sleep), LoRa (SX1262), et chargeur LiPo intégré. |
| **Capteur de Distance** | **JSN-SR04T 2.0 (Waterproof)** | Les yeux : mesure étanche du niveau de remplissage. |
| **Batterie** | **Li-Ion 18650, 3.7V** | L'énergie : haute capacité (ex: 3400mAh) pour une autonomie maximale. |
| **Support Batterie** | Support 18650 pour PCB | Fixation sécurisée de la batterie sur le circuit d'assemblage. |
| **Gestion Puissance** | **MOSFET canal N (logic level)** | **L'optimisation clé :** agit comme interrupteur pour couper le 5V du JSN. (Réf: **BS170** ou **2N7000**). |
| **Gestion Tension** | **Level Shifter bidirectionnel** | **La sécurité clé :** convertit les signaux 5V (JSN) en 3.3V (ESP32) et vice-versa. |
| **Boîtier** | **Boîte de jonction IP67** | La coque : protection étanche (ex: 100x100x70mm). |
| **Presse-étoupe** | **M12** | Le joint : assure l'étanchéité du câble capteur à la sortie du boîtier. |
| **Circuit d'assemblage** | **Perfboard (plaque de prototypage)** | Le PCB maison : support pour souder les composants proprement. |
| **Connectique** | Peignes mâles & femelles | Permet de rendre la carte Heltec amovible de la Perfboard. |

---

## 3. Philosophie de Conception (Approche Ingénieur)

### A. Optimisation Énergétique (MOSFET)

- **Problème :** le capteur JSN-SR04T, même en veille (non déclenché), consomme du courant (courant de fuite / quiescent current).
- **Solution matérielle :** utiliser un **MOSFET comme interrupteur** piloté par l'ESP32.
- **Logique :** l'ESP32 coupe totalement l'alimentation du capteur JSN, dort pendant 4 heures (consommation quasi nulle), se réveille, réalimente le capteur via le MOSFET, prend la mesure, recoupe l'alimentation, puis se rendort.

### B. Compatibilité des Tensions (Level Shifter)

- **Problème :** le JSN-SR04T fonctionne en **5V** tandis que l'ESP32 de la Heltec fonctionne en **3.3V**.
- **Risque :** connecter `Echo` (5V) directement à un GPIO ESP32 (3.3V) peut **détruire** le microcontrôleur.
- **Solution matérielle :** placer un **Level Shifter** entre Trig/Echo du capteur et GPIO de l'ESP32 pour la translation 3.3V <-> 5V.

---

## 4. Guide d'Assemblage (Prompt de Réalisation)

Suivre ces étapes pour assembler **un (1) nœud capteur** de manière professionnelle sur Perfboard.

### Étape 1 : Préparation de la Perfboard et du Cœur

1. Souder les **peignes mâles** sur la carte Heltec V3.
2. Souder les **peignes femelles** correspondants sur la Perfboard pour créer un socket amovible.
3. Identifier les sorties nécessaires : `3.3V`, `5V`, `GND`, et les 3 GPIO (ex: `GPIO 12` MOSFET, `GPIO 13`/`14` Level Shifter).

### Étape 2 : Circuit de Puissance (MOSFET)

C'est le circuit qui contrôle l'alimentation du capteur JSN.

1. Prendre le MOSFET (ex: BS170).
2. Souder la broche **Source** au `GND` principal (Heltec).
3. Souder la broche **Gate** à un GPIO Heltec (ex: `GPIO 12`).
4. La broche **Drain** devient le **nouveau GND** pour le capteur JSN.
5. Souder le `VCC` du JSN directement au `5V` du Heltec.

Logique : lorsque `GPIO 12` est à HIGH, le MOSFET connecte Drain à Source et ferme le retour GND du capteur.

### Étape 3 : Circuit Logique (Level Shifter)

C'est le circuit qui traduit les signaux Trig/Echo.

1. Alimenter le côté **HV** avec `5V` et `GND` du Heltec.
2. Alimenter le côté **LV** avec `3.3V` et `GND` du Heltec.
3. Connecter `Trig` (JSN) au côté **HV** (ex: `HV1`).
4. Connecter `Echo` (JSN) au côté **HV** (ex: `HV2`).
5. Connecter `GPIO 13` (Heltec) au côté **LV** (ex: `LV1`).
6. Connecter `GPIO 14` (Heltec) au côté **LV** (ex: `LV2`).

### Étape 4 : Alimentation Principale

1. Souder le **support batterie 18650** sur la Perfboard.
2. Connecter les sorties `+` et `-` du support aux entrées `BAT+` et `GND` de la Heltec V3.

### Étape 5 : Assemblage Boîtier (Étanchéité)

1. Percer deux trous propres dans le boîtier IP67 :
   - un trou (ex: 20mm) pour la sonde **JSN-SR04T**
   - un trou (ex: 12mm) pour le **presse-étoupe M12**
2. Si le câble JSN est trop court, le faire passer par le presse-étoupe puis ressouder proprement.
3. Fixer la sonde JSN sur le couvercle (colle chaude ou silicone d'étanchéité).
4. Visser le presse-étoupe.
5. Fixer la Perfboard à l'intérieur (vis ou adhésif VHB).
6. Ajouter un sachet de **silice (Silica Gel)** avant fermeture finale.

---

## 5. Contrat d'Interface (Lien avec le Logiciel)

Ce matériel est conçu pour exécuter un firmware qui respecte le contrat suivant :

- **Firmware :** le code doit utiliser les GPIO définis (`12`, `13`, `14`) pour piloter le MOSFET et le Level Shifter. Il doit utiliser le **Deep Sleep** et la **sauvegarde de session RTC**.
- **Payload de données :** pour la compatibilité backend, le firmware doit envoyer un payload **binaire de 2 octets** :
  - `Octet 0` : Remplissage (`0` à `100`)
  - `Octet 1` : Tension Batterie (ex: `41` pour `4.1V`)

