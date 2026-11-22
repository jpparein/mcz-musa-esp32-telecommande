# Journal du projet

Chronologie du projet, du 16 au 19 novembre 2026 pour l'essentiel (mise à jour au fil des étapes). Rien de secret ici.

## 1. Le but

Piloter un poêle à pellets MCZ (Musa Comfort Air 2.0) depuis un téléphone, un PC ou une tablette :
- **Mode A, « avec télécommande »** : l'application et la télécommande d'origine cohabitent.
- **Mode B, « remplacement »** : l'ESP32 fait le thermostat (sonde de température, programmes) au cas où la télécommande tomberait en panne ou serait absente.

## 2. En résumé (les grandes étapes)

| # | Étape | En clair |
|---|---|---|
| 1 | Écouter | on trouve la fréquence (434,5 MHz) en appuyant sur la télécommande |
| 2 | Enregistrer | on mesure la durée de chaque impulsion du signal |
| 3 | Décoder | on découvre comment les bits sont codés et regroupés en 7 blocs |
| 4 | Comprendre | un seul réglage modifié à la fois : on repère quels bits changent (flamme, ventilateurs, mode) |
| 5 | Contrôle | on découvre que les 2 derniers blocs se calculent avec des XOR : on peut fabriquer n'importe quelle trame |
| 6 | Vérifier | on compare nos trames fabriquées aux vraies : toutes identiques |
| 7 | Émettre | premier ordre réel : le poêle obéit |
| 8 | Application | page web, file de commandes, minuteurs, programmes |
| 9 | Cohabiter | l'application et la télécommande ne s'écrasent plus |
| 10 | Remplacer | thermostat dans l'ESP32 : AUTO, puis NO AIR, puis ECO (validés le 19 novembre) |
| 11 | Secours | page et Wi-Fi propres à l'ESP32 si le serveur tombe |
| 12 | Fiabiliser | coupures réseau corrigées (signal, fils fixés, relance automatique) |

Le détail technique de chaque étape est dans [PROTOCOLE.md](PROTOCOLE.md) et [PIEGES.md](PIEGES.md).

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
