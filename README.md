# Railuino

Bibliothèque Arduino / PlatformIO pour piloter un réseau Märklin par son bus CAN (Gleisbox, MS2, CS2) depuis un **ESP32** ou un **Arduino Uno / Mega avec un MCP2515**.
Réécriture de la version 0.9 de la bibliothèque [Railuino](https://code.google.com/archive/p/railuino/) de Joerg Pleumann.

*English below.*

Vidéo : https://youtu.be/x5fVPsChlIA

## Ce que fait la bibliothèque

- Marche / arrêt de la voie, arrêt d'urgence d'une locomotive
- Locomotives MM2, MFX et DCC : vitesse, sens de marche, fonctions
- Aiguillages et accessoires
- Lecture et écriture des CV des décodeurs
- Rétrosignalisation S88

## Cartes prises en charge

| Carte | Contrôleur CAN | Pilote Railuino | Bibliothèque CAN |
| --- | --- | --- | --- |
| ESP32 | TWAI intégré + transceiver (SN65HVD230…) | `Esp32TwaiDriver` (par défaut) | [ACAN_ESP32](https://github.com/pierremolinaro/acan-esp32) ≥ 3.0.3 |
| ESP32 | MCP2515 sur le bus SPI | `Mcp2515Driver` | [ACAN2515](https://github.com/pierremolinaro/acan2515) ≥ 2.1.3 |
| Arduino Uno, Mega | MCP2515 sur le bus SPI | `Mcp2515Driver` (par défaut) | [ACAN2515](https://github.com/pierremolinaro/acan2515) ≥ 2.1.3 |

Sans pilote indiqué, `TrackController` prend le pilote par défaut de la carte. Pour en choisir un autre :

```cpp
#include <ACAN2515.h>
#include <Railuino.h>

Mcp2515Driver can(5, 4, 8000000UL);   // CS, INT, quartz
TrackController ctrl(can, 0xDF24);
```

Sur AVR, le quartz et les broches du MCP2515 par défaut se changent aussi sans modifier la bibliothèque, par exemple dans `platformio.ini` :

```ini
build_flags = -D RAILUINO_MCP2515_QUARTZ=8000000UL -D RAILUINO_MCP2515_CS=10 -D RAILUINO_MCP2515_INT=2
```

Pour un autre contrôleur CAN, dérivez la classe `CanDriver` (`src/CanDriver.h`).

## Installation

**PlatformIO** : dans le `platformio.ini` de votre projet

```ini
lib_deps = https://github.com/BOBILLEChristophe/Railuino.git
```

**IDE Arduino** : *Croquis → Inclure une bibliothèque → Ajouter la bibliothèque .ZIP*, avec le ZIP du dépôt. Installez aussi ACAN_ESP32 (ESP32) ou ACAN2515 (Uno, Mega) depuis le gestionnaire de bibliothèques.

## Premier programme

```cpp
#include <Railuino.h>

const uint8_t CAN_RX = 22; // ESP32 uniquement, ignoré sur Uno/Mega
const uint8_t CAN_TX = 23;
const uint16_t LOCO = ADDR_MFX + 7;

TrackController ctrl(0xDF24); // hash ; 0 = généré automatiquement

void setup() {
  Serial.begin(115200);
  ctrl.begin(CAN_RX, CAN_TX);
  ctrl.setPower(true);
  ctrl.setLocoFunction(LOCO, 0, 1); // feux
  ctrl.setLocoSpeed(LOCO, 300);     // 0 à 1000
}

void loop() {}
```

Le constructeur prend toujours le hash en premier : `TrackController(hash, debug = false, timeout_ms = 1000, loopback = false)`. Pour changer seulement le délai d'attente : `ctrl.setTimeout(500)`.

## Exemples

Dans `examples/01.Controller` :

| Exemple | Rôle | Cartes |
| --- | --- | --- |
| Power | Marche / arrêt toutes les 5 s | toutes |
| Speed | Fait rouler une loco puis l'arrête | toutes |
| Direction | Change le sens de marche | toutes |
| Headlight | Allume et éteint les feux | toutes |
| Function | Fonctions 0 à 12 une par une | toutes |
| Turnout | Manœuvre un aiguillage | toutes |
| CV | Lit les CV d'un décodeur | toutes |
| TestAddressLoco | Cherche les adresses MFX occupées | toutes |
| SerialCommand | Commandes texte depuis le moniteur série | toutes |
| TcpCommand | Mêmes commandes en TCP (port 15731) | ESP32 |
| WebControl | Interface web (pages dans `data/`) | ESP32 |

Dans `examples/02.Drivers` : `Mcp2515OnEsp32`, un ESP32 avec un module MCP2515 au lieu de son contrôleur intégré.

TcpCommand et WebControl lisent les identifiants WiFi dans un fichier `secrets.h`, à créer dans le dossier de l'exemple à partir de `secrets.h.example`. Ce fichier est ignoré par Git.

## Développer la bibliothèque

Le dépôt s'ouvre directement dans VS Code avec PlatformIO. `platformio.ini` compile **un** exemple (WebControl par défaut) avec la bibliothèque de `src/` ; changez `src_dir` pour en essayer un autre.

- `pio test -e native` : tests unitaires sur PC, sans carte, avec un bus CAN simulé (`test/`).
- `sh extras/build_examples.sh` : compile tous les exemples pour toutes leurs cartes.

GitHub Actions lance ces deux vérifications à chaque envoi et à chaque pull request.

Historique des changements : [CHANGELOG.md](CHANGELOG.md).

## Licence

Bibliothèque (`src/`) : GNU LGPL 2.1 ou ultérieure, voir [LICENSE](LICENSE). Exemples : Creative Commons Zero.
Copyright (C) 2012 Joerg Pleumann, (C) 2024 Christophe Bobille, (C) 2026 Uwe Monreal.

---

## English

Arduino / PlatformIO library to control a Märklin layout through its CAN bus (Gleisbox, MS2, CS2) from an **ESP32** or an **Arduino Uno / Mega with an MCP2515**. Rewrite of Joerg Pleumann's Railuino 0.9.

- Install with PlatformIO: `lib_deps = https://github.com/BOBILLEChristophe/Railuino.git`, or add the repository ZIP in the Arduino IDE (plus ACAN_ESP32 or ACAN2515).
- Include `<Railuino.h>` only. The constructor always takes the hash first: `TrackController(hash, debug = false, timeout_ms = 1000, loopback = false)`.
- CAN drivers: `Esp32TwaiDriver` (ESP32 default) and `Mcp2515Driver` (AVR default, also on ESP32). Pass one to `TrackController(can, hash)` to choose; derive `CanDriver` for another controller. ESP32 pins go to `begin(rx, tx)`; default MCP2515 settings: `RAILUINO_MCP2515_QUARTZ`, `_CS`, `_INT`.
- Examples are in `examples/01.Controller` (see the table above). TcpCommand and WebControl need an ESP32 and a `secrets.h` file created from `secrets.h.example`.
- Development: `platformio.ini` builds one example against `src/`; `pio test -e native` runs the unit tests; `sh extras/build_examples.sh` builds every example. GitHub Actions runs both on every push.

License: LGPL 2.1 or later for the library, CC0 for the examples.
