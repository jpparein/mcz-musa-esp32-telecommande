# Journal du projet

Chronologie du projet, du 16 au 19 novembre 2026 pour l'essentiel (mise à jour au fil des étapes). Rien de secret ici.

## 1. Le but

Piloter un poêle à pellets MCZ (Musa Comfort Air 2.0) depuis un téléphone, un PC ou une tablette :
- **Mode A, « avec télécommande »** : l'application et la télécommande d'origine cohabitent.
- **Mode B, « remplacement »** : l'ESP32 fait le thermostat (sonde de température, programmes) au cas où la télécommande tomberait en panne ou serait absente.

## 2. Chronologie

1. **Écoute radio.** Le CC1101 en réception OOK repère la fréquence réelle (434,5 MHz) en mesurant le RSSI pendant qu'on appuie sur la télécommande.
2. **Capture des impulsions.** Unité de base 422 µs, 189 unités par trame, 5 répétitions séparées de 4,6 ms.
3. **Décodage.** Biphase mark, 7 blocs de 12 bits, XOR cumulé, parité impaire. Les 3 premiers blocs sont l'adresse.
4. **Champs.** En faisant varier un seul réglage à la fois : flamme, 3 ventilateurs, mode (MAN, AUTO, ECO, arrêt).
5. **Contrôle linéaire.** Les blocs 6 et 7 sont une fonction XOR-linéaire des blocs 4 et 5. Vérifié sur 47 trames réelles.
6. **Générateur hors-ligne**, validé sur 156 changements de réglage, porté en C++ dans l'ESP32 avec un autotest de 12 trames à chaque démarrage.
7. **ESP32 sur le Wi-Fi** : chaque trame entendue est envoyée au serveur.
8. **Premier envoi réel** : le poêle a obéi depuis 7 m à travers des murs.
9. **Page web et file de commandes** (SQLite côté serveur, l'ESP32 vient chercher les ordres).
10. **Cohabitation avec la télécommande** : fusion des états, drapeau « action utilisateur » pour le bip.
11. **Mise à jour OTA** de l'ESP32.
12. **Minuteur et programmation hebdomadaire.**
13. **Capteur DHT22**, carte température, page « Apprentissage » pour comprendre la régulation AUTO de la télécommande.
14. **Mode remplacement** : thermostat AUTO dans l'ESP32 (clone de la télécommande), consigne et mode gardés en flash.
15. **NO AIR validé (19 novembre)** : la télécommande envoie une trame AUTO flamme 1, puis 5 minutes plus tard une trame silencieuse avec les ventilateurs à 0.
16. **ECO mesuré puis validé en réel (19 novembre)** : arrêt automatique 20 minutes après la consigne, veille, rallumage à consigne − 2 °C ; testé sans télécommande, de bout en bout.
17. **Page de secours** dans l'ESP32 (Wi-Fi propre, portail captif, compatible vieux Android).
18. **Fiabilité réseau** : diagnostic des coupures (signal faible, fils lâches), relance automatique, passage à `esp_http_server`, ESP32 fixé et fils immobilisés ; 0 échec sur 1085 appels à −30 dBm.

## 3. Résultats des essais réels

| Test | Résultat |
|---|---|
| Écoute de la télécommande vers la page | OK |
| Ventilateurs, flamme, marche/arrêt depuis l'application | OK |
| Bip du poêle sur les ordres de l'application | OK après correction du drapeau |
| Cohabitation avec la télécommande | OK après la fusion des états |
| Thermostat AUTO de l'ESP32 | OK (suivi de la consigne) |
| NO AIR de l'ESP32 | OK (ventilateurs coupés à 5 min) |
| ECO de l'ESP32 | OK (arrêt à 20 min, rallumage à consigne − 2 °C) |
| Mode TIMER de la télécommande | non décodé |

## 4. Idées pour la suite

- Écran tactile dans la pièce pour une télécommande autonome.
- Affichage de la veille ECO dans l'application.
- Décodage du mode TIMER.
