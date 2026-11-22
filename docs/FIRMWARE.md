# Le firmware de l'ESP32

Dossier : `firmware/etape9_emission/` (un croquis Arduino, noyau `esp32` 3.3.x, carte « ESP32 Dev Module »).

## Configuration

1. Copiez `secrets.example.h` en `secrets.h` (même dossier). **Ne publiez jamais `secrets.h`** (il est dans `.gitignore`).
2. Renseignez : `WIFI_NOM`, `WIFI_MOT_DE_PASSE` (Wi-Fi 2,4 GHz), `PI_ADRESSE` (adresse fixe du serveur), `PI_JETON` (celui de `token.txt` ; sert aussi de mot de passe OTA), `EMISSION_AUTORISEE`, `AP_MDP` (Wi-Fi de secours, 8 caractères ou plus).
3. Mettez **votre adresse de télécommande** dans `mcz_decode.h` (`MCZ_M1`, `MCZ_M2`, `MCZ_M3`) après l'avoir capturée ([REPRODUIRE.md](REPRODUIRE.md)).
4. Compilez et téléversez (USB la première fois) :
   ```
   arduino-cli compile --fqbn esp32:esp32:esp32 firmware/etape9_emission
   arduino-cli upload  --fqbn esp32:esp32:esp32 -p COM5 firmware/etape9_emission
   ```
5. Ensuite, les mises à jour se font par Wi-Fi (OTA, `espota.py` fourni avec le noyau esp32, mot de passe = le jeton).

La mémoire flash est utilisée à environ 90 % avec le schéma de partitions par défaut (2 × 1,25 Mo, plus 1,4 Mo de SPIFFS inutilisés). Si vous ajoutez des choses, changez de schéma de partitions (plus de place pour l'application, moins de SPIFFS), ce qui demande un téléversement par USB.

## Fichiers

| Fichier | Rôle |
|---|---|
| `etape9_emission.ino` | programme principal : radio, thermostat, réseau, OTA, capteur |
| `mcz_decode.h` | impulsions → trames (biphase mark, blocs, parité, champs) |
| `mcz_tables.h` | tables de contrôle (voir [PROTOCOLE.md](PROTOCOLE.md)) |
| `mcz_emission.h` | fabrication d'une trame (changer un champ, recalculer le contrôle, refaire les impulsions) |
| `mcz_autotest.h` | 12 couples de trames réelles pour l'autotest au démarrage |
| `secours.h`, `secours_page.h` | Wi-Fi et page de secours ([MODE_SECOURS.md](MODE_SECOURS.md)) |
| `secrets.example.h` | modèle de `secrets.h` |

## Architecture

- **Cœur 1** : boucle principale. Écoute radio par interruption sur GDO0 (durées des fronts), déclenchement sur RSSI (seuil −70 dBm), décodage, thermostat, émission.
- **Cœur 0** : tâche réseau (appels au serveur, avec relance en cas d'échec), serveur web de secours (`esp_http_server`, plusieurs connexions simultanées).
- **Tâche dédiée** : mise à jour OTA.

## Écoute et émission

- Écoute : `configReception()`, déclenchement quand `lireRSSI() > SEUIL_RSSI`, enregistrement jusqu'à 3000 changements de niveau, décodage dans `mcz_decode.h` (les deux polarités sont essayées).
- Émission : `emettreDurees()` met le CC1101 en émission asynchrone OOK, GDO0 en sortie de l'ESP32, rejoue les durées, puis revient en écoute. Elle n'est appelée que si `EMISSION_AUTORISEE` est vrai.
- Fabrication d'une trame (`mcz_emission.h`) : partir des bits `x`/`y` de la dernière trame connue, `mcz_changer()` (nouvelle valeur d'un champ + différence de contrôle), `mcz_drapeau()` (drapeau « action de l'utilisateur »), `mcz_mots_depuis()` (blocs bruts), `mcz_unites()` (flux d'unités), `mcz_durees()` (durées en µs avec 5 répétitions).
- **Fusion télécommande / application** : l'ESP32 garde l'état voulu ; quand la vraie télécommande émet, il compare avec sa trame précédente, applique à l'état voulu ce qui a changé (et seulement cela), puis renvoie l'ensemble.

## Thermostat (mode remplacement)

Il copie ce que fait la télécommande :

- **AUTO** : si consigne > partie entière de la température mesurée, puissance +1 par minute jusqu'à 5 ; sinon −1 par minute jusqu'à 1 ; renvoi de la trame toutes les 20 minutes ; drapeau à 0 (pas de bip). Sonde muette : garde la dernière puissance. Vraie télécommande entendue : silence 30 minutes.
- **ECO** : comme AUTO ; à la consigne tenue 20 minutes, trame flamme 0 (veille) ; rallumage (flamme 3) à consigne − 2 °C et au moins 5 minutes après l'arrêt.
- **NO AIR** : trame AUTO flamme 1 (bip), puis 5 minutes plus tard trame silencieuse avec les 3 ventilateurs à 0 ; à la sortie, retour aux réglages précédents.
- La consigne et le mode de fonctionnement viennent du serveur (toutes les 15 s) et sont gardés dans la flash de l'ESP32, qui continue donc sans serveur ni Wi-Fi.

## Capteur DHT22

Lu sans bibliothèque (40 bits, somme de contrôle). Filtres : valeurs hors 0–45 °C rejetées, saut de plus de 3 °C rejeté, médiane des 5 dernières mesures, envoi toutes les 30 s. Le capteur est alimenté par une broche : après 12 essais sans réponse (environ 1 minute) le firmware coupe puis remet son alimentation (le capteur s'était figé une fois pendant 4 h).

## Journal et diagnostic

L'ESP32 envoie son journal au serveur (carte « Journal » dans Réglages). Lignes utiles : redémarrages, Wi-Fi perdu/retrouvé avec le signal en dBm, « Pi injoignable » avec le code d'erreur et la durée, commandes envoyées, thermostat, bilans de diagnostic du réseau (lignes `CH …`).

## Ce qu'il ne faut pas faire

- Ne jamais mettre des identifiants dans le code : ils vont dans `secrets.h`.
- Pour les clients réseau lents ou muets, utiliser `esp_http_server` (comme ici) plutôt que la bibliothèque `WebServer` d'Arduino : cette dernière est mono-connexion et une connexion muette la bloque.
