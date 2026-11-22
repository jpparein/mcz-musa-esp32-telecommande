# Reproduire le projet pas à pas

Objectif : piloter un poêle à pellets MCZ avec un ESP32 + un module radio CC1101, en gardant (ou en remplaçant) la télécommande d'origine.

Temps prévu : une journée pour le matériel et la capture, quelques jours pour valider chaque fonction en réel.

> **Sécurité.** Un poêle est un appareil de chauffage. Faites vos premiers essais d'émission **en étant présent**, avec une seule fonction à la fois (marche/arrêt d'abord). Les ventilateurs et la flamme se contrôlent à l'oreille et à la vue. Ne laissez jamais un automatisme allumer le poêle sans surveillance tant qu'il n'est pas éprouvé. Ce projet n'a aucun lien avec le fabricant et peut ne pas être compatible avec votre modèle.

## 1. Matériel

| Quantité | Élément | Notes |
|---|---|---|
| 1 | **ESP32 DevKit** (ESP32-WROOM-32, 4 Mo) | photo : `photos/materiel/esp32-devkit.jpg` |
| 1 | **Module CC1101 433/434 MHz** (type E07-M1101D, avec antenne) | l'étage radio. Vérifiez que c'est bien un module 433 MHz et qu'il a une antenne ; alimentation **3,3 V uniquement** |
| 6-8 | fils Dupont femelle-femelle | à fixer ensuite (ruban, soudure) : des fils qui bougent ont causé des bruits radio et des coupures |
| 1 | capteur **DHT22** (module 3 broches) | uniquement pour le mode « remplacement » (thermostat) |
| 1 | alimentation USB 5 V | |
| option | Raspberry Pi (ou tout serveur Apache + PHP + SQLite) | pour l'application web, les minuteurs, l'historique |
| — | la télécommande d'origine (**MAX LCD**) | indispensable pour capturer l'adresse |

Photos : [`photos/materiel/`](photos/materiel/).

## 2. Câblage ESP32 ↔ CC1101

Le CC1101 se parle en SPI. La broche **GDO0** est la « donnée brute » : une **sortie** du CC1101 en réception, une **entrée** en émission (mode asynchrone).

| CC1101 | ESP32 | Rôle |
|---|---|---|
| VCC | 3V3 | alimentation (3,3 V, **jamais 5 V**) |
| GND | GND | masse |
| SCK | GPIO 18 | horloge SPI |
| SI (MOSI) | GPIO 23 | SPI |
| SO (MISO) | GPIO 19 | SPI |
| CSN | GPIO 5 | sélection |
| GDO0 | GPIO 4 | données brutes (OOK asynchrone) |
| GDO2 | libre | non utilisé |

**Il suffit de 7 fils** (VCC, GND, SCK, MOSI, MISO, CSN, GDO0). GDO2 n'est pas nécessaire.

DHT22 (mode remplacement) : `-` sur GND, `OUT` sur **GPIO 25**, `+` sur **GPIO 26**. Astuce : le 3V3 de l'ESP32 est déjà pris par le CC1101 ; le capteur consomme moins de 2 mA, on l'alimente donc par la broche GPIO 26 mise à 3,3 V (cela permet aussi de le couper puis le rallumer s'il se fige).

Photos du câblage : [`photos/cablage/`](photos/cablage/) (`tableau-des-fils.png`, `esp32-cablage-annote.jpg`, `cc1101-cablage-annote.jpg`).

### Où placer l'ESP32 ?

L'ESP32 doit être à la fois à portée du Wi-Fi **et** de la radio du poêle (il émet à portée raisonnable : un essai réussi à 7 m à travers des murs) et, pour écouter la vraie télécommande, assez près d'elle (seuil de déclenchement −70 dBm). Fixez les fils : un fil qui bouge fait du bruit radio.

## 3. Logiciels

- **Arduino CLI** (ou Arduino IDE) avec le noyau **esp32 version 3.3.x** (carte « ESP32 Dev Module »).
- Pour compiler en ligne de commande :
  ```
  arduino-cli compile --fqbn esp32:esp32:esp32 firmware/etape9_emission
  ```
- Python 3 (analyse des captures, génération des tables).
- Pour la page web : un serveur Apache + PHP 8 + extension SQLite (un Raspberry Pi convient).

## 4. Étape A — Trouver la fréquence

1. Téléversez le programme d'écoute (la partie réception de `etape9_emission.ino`, ou un petit programme qui lit `RSSI` du CC1101).
2. Parcourez les fréquences 433,0 à 435,0 MHz par pas de 50 kHz en appuyant sur une touche de la télécommande : le RSSI monte à la bonne fréquence. Ici : **434,5 MHz** (et non 433,92). Confirmé par la notice de la télécommande.
3. Modulation : **OOK** (registre `MDMCFG2 = 0x30`). En 2-FSK on ne voit que du bruit.

Réglages réception utilisés (voir `regleFrequence` et `configReception` dans le firmware) : `MDMCFG4 = 0x58`, `MDMCFG3 = 0x22`, `MDMCFG2 = 0x30`, `PKTCTRL0 = 0x32` (mode série asynchrone), `IOCFG0 = 0x0D` (GDO0 = données reçues), `FREND0 = 0x10`, `MCSM0 = 0x14`. Fréquence : `FREQ = f / (26 MHz / 2^16)`.

## 5. Étape B — Capturer les impulsions

Le firmware déclenche l'enregistrement quand le RSSI dépasse un seuil (−70 dBm), puis note la durée de chaque état haut/bas de GDO0 (interruption sur front). Une capture = environ 630 changements pour une émission de la télécommande.

Vous devez retrouver : unité 422 µs, synchro de 3 unités hautes, 5 répétitions, silence de 4,6 ms. Exemple de capture brute : [`captures/journal_captures_brutes.txt`](captures/journal_captures_brutes.txt).

## 6. Étape C — Décoder

Appliquez la méthode de [PROTOCOLE.md](PROTOCOLE.md) : biphase mark → 7 blocs de 12 bits → XOR cumulé → 9 bits de données + parité impaire + `11`. Si la parité et le `11` sont vérifiés sur chaque bloc, votre décodage est bon.

Notez les blocs 1 à 3 : c'est **l'adresse de votre télécommande**. Mettez-la dans `mcz_decode.h` (`MCZ_M1`, `MCZ_M2`, `MCZ_M3`).

## 7. Étape D — Retrouver les champs (si votre poêle diffère)

Si les tables de `mcz_tables.h` ne reproduisent pas vos trames, refaites la mesure :

1. Changez **un seul réglage à la fois** sur la télécommande (flamme 1→2→3…, un ventilateur, le mode…) et capturez chaque trame.
2. Notez les valeurs affichées à l'écran à côté de chaque capture.
3. Pour chaque paire de trames qui ne diffèrent que d'un réglage, calculez la différence XOR des bits de `x` et de `y`.
4. Vérifiez que `y` est XOR-linéaire en `x` (`analyse/lin.py`).
5. Générez vos tables avec `analyse/gen_tables.py` (elles remplacent `mcz_tables.h`).

Trames à capturer au minimum : flamme 1 à 5 ; chaque ventilateur de 0 à 5 et AUTO ; modes MAN, AUTO, ECO, arrêt ; une trame avec le drapeau à 0 (envoi automatique d'AUTO).

## 8. Étape E — Valider hors-ligne

Le firmware rejoue à chaque démarrage un **autotest** (`mcz_autotest.h`) : 12 couples « trame réelle A → champ changé → trame réelle B ». Il doit afficher 12/12. Ajoutez vos propres couples pour valider vos tables avant d'émettre quoi que ce soit.

## 9. Étape F — Émettre (premier essai)

1. Dans `secrets.h`, laissez `EMISSION_AUTORISEE` à `false` tant que l'autotest n'est pas parfait.
2. Passez-le à `true`. Le premier essai : **marche/arrêt**, poêle sous vos yeux, ESP32 à quelques mètres.
3. Puis une flamme, puis un ventilateur (à l'oreille).

Émission : `MDMCFG4 = 0xF6`, `MDMCFG3 = 0x83` (≈ 2,4 kbauds), `MDMCFG2 = 0x30`, `PKTCTRL0 = 0x32`, `IOCFG0 = 0x2E` (GDO0 devient une **entrée** pour les données à émettre), `FREND0 = 0x11`, table de puissance `PATABLE = {0x00, 0xC0, …}` : en OOK la case 0 est « éteint » et la case 1 « allumé » (0xC0 ≈ puissance maximale). Les durées sont rejouées en `digitalWrite` sur GDO0 avec des délais en microsecondes. Le firmware refuse deux émissions à moins de 3 s d'écart.

Une trame émise = 5 répétitions de 189 unités avec 4586 µs de silence entre elles, avec l'**adresse de la vraie télécommande** et le drapeau `x9 = 1` (le poêle bipe en signe de réception).

## 10. Étape G — Le serveur, la page web

Voir [SERVEUR.md](SERVEUR.md). Sans serveur, le firmware sait aussi servir une petite page de secours ([MODE_SECOURS.md](MODE_SECOURS.md)).

## 11. Étape H — Cohabiter avec la télécommande, ou la remplacer

- **Avec télécommande** : l'ESP32 écoute la télécommande ; pour envoyer un ordre il part de sa **dernière trame** et ne change **qu'un champ**. Piège : la télécommande renvoie tout son état à chaque trame ; si l'application ne tient pas compte de ce que vous faites sur la télécommande, un simple appui sur un ventilateur de la télécommande rétablit la flamme à l'ancienne valeur. Le firmware compare chaque trame de la télécommande à la précédente, repère ce qui a changé, l'applique à l'état de l'application, puis renvoie l'ensemble une seconde plus tard.
- **Remplacement** : l'ESP32 fait le thermostat avec le DHT22 (règles AUTO/ECO/NO AIR de [PROTOCOLE.md](PROTOCOLE.md)). Si une vraie télécommande est entendue, il se tait 30 minutes.

## 12. Liste de contrôle de validation

| Test | Attendu |
|---|---|
| Autotest au démarrage | 12/12 |
| Marche / arrêt depuis la page | le poêle obéit, bip de confirmation |
| Flamme 1 à 5 en MAN | la puissance change |
| Ventilateurs 0 à 5, AUTO | le bruit change |
| Changement sur la télécommande | repris par l'application sans écraser le reste |
| Mode NO AIR | ventilateurs coupés après 5 min |
| Mode ECO | arrêt auto à la consigne, rallumage à consigne − 2 |

Pièges rencontrés : voir [PIEGES.md](PIEGES.md).
