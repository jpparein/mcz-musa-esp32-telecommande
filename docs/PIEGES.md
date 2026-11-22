# Pièges rencontrés et leçons

## Radio

- **Mauvaise fréquence** : 434,5 MHz, pas 433,92. Trouvée au RSSI en appuyant sur la télécommande.
- **2-FSK = du bruit** : il faut l'OOK.
- **Seuil de déclenchement** : −45 dBm n'entendait la télécommande qu'à 20 cm ; −70 dBm convient à distance raisonnable.
- **Impulsions parasites** (< 150 µs) : elles inversent la polarité apparente. Absorber l'impulsion dans la suivante et essayer les deux polarités.
- **Fin de trame tronquée** : le dernier bit du bloc 7 est souvent coupé. On le force à 0 si le bloc a 11 bits, et on écarte les trames dont la fin est abîmée pour fabriquer des trames (23 « incohérences » au départ venaient d'une seule capture tronquée).
- **Fil mal placé / qui bouge** : un fil Dupont sur la mauvaise broche, ou qui bouge, a donné un bruit radio constant (captures illisibles). Après un rebranchement, **redémarrer** l'ESP32. Fixer tous les fils (ruban, soudure) et placer l'ESP32 définitivement avant de juger la fiabilité.
- **Cadence d'émission** : ne pas émettre deux fois à moins de 3 s d'écart ; le serveur espace les commandes de 4 s.

## Protocole

- **La télécommande envoie tout son état à chaque trame.** Si l'application règle la flamme à 3 et que la télécommande croit être à 1, un appui sur un ventilateur de la télécommande remet la flamme à 1. Solution : fusionner (comparer à la trame précédente, appliquer ce qui a changé, renvoyer l'ensemble).
- **Pas de bip du poêle** sur les ordres de l'application : le bit `x9` (drapeau « action de l'utilisateur ») vaut 1 quand on appuie sur une touche, 0 pour les envois automatiques du thermostat. Le forcer à 1 fait revenir le bip.
- **NO AIR** : le poêle ne coupe pas ses ventilateurs tout seul. La télécommande envoie une trame silencieuse 5 minutes après avec les 3 ventilateurs à 0.
- **ECO** : la trame d'arrêt est flamme 0 ; le rallumage vient d'une trame flamme 3. Une des trames « arrêt » enregistrées était en réalité une erreur de réception : confirmer sur une seconde trame (la télécommande la renvoie).
- **AUTO** : la consigne et la température ne sont pas dans le signal ; la puissance émise est un maximum, le poêle module seul. Le niveau des ventilateurs limite la flamme maximale (Comfort Air : somme des niveaux ÷ 2).
- **Le poêle n'émet rien** : impossible de connaître son état réel. L'application montre ce qui a été entendu ou envoyé. Elle ne peut pas savoir que quelqu'un a appuyé sur le panneau de secours du poêle.
- **Après un redémarrage de l'ESP32** il n'a plus de trame de départ : le serveur lui redonne le dernier état connu pour reconstruire une trame de base.
- **Ne jamais deviner le sens d'une icône** : la « lune » de la télécommande est NO AIR (pas SLEEP, qui fixe une heure d'extinction). Relire la notice.

## Réseau et serveur

- **Coupures « Pi injoignable »** : les causes étaient un signal Wi-Fi faible (−70 dBm et moins) et des fils lâches. Après correction : 0 échec sur 1085 appels à −30 dBm. Une relance automatique (1,5 s puis 3 s) reste en filet de sécurité.
- **Un seul client à la fois** avec le serveur `WebServer` d'Arduino : le remplacer par `esp_http_server`.
- **La base SQLite** ne doit pas être dans le dossier web (sinon elle est téléchargeable).
- **Le 3V3 de l'ESP32** est déjà pris par le CC1101 : alimenter le DHT22 par une broche GPIO.
- **Le DHT22 peut se figer** (4 h une fois) : prévoir la coupure/remise de son alimentation.
- **Port série occupé** pendant un téléversement : un programme de journalisation oublié tenait le port.
- **Cache du navigateur** : ajouter `?v=2` à l'adresse pour forcer le rechargement pendant les essais.

## Interface

- **Curseurs tactiles** : ils ne répondaient pas aux appuis rapides sur téléphone : de gros boutons de niveau les remplacent.
- **`cqw`** : sur un élément qui est lui-même le conteneur, les `cqw` se calculent sur la largeur de l'écran, pas du conteneur. Mettre le boîtier dans un cadre qui est le conteneur.
- **Anciens navigateurs** (Chrome 64) : `AbortController`, `cqw`, `min()`, `clamp()` et `gap` flex absents ; voir [MODE_SECOURS.md](MODE_SECOURS.md).
- **Certificat HTTPS** : un vieux téléphone peut refuser un certificat récent (`ERR_CERT_AUTHORITY_INVALID`) : utiliser le HTTP local.
- **Une adresse tapée avec un tiret au lieu d'un point** donne « site inaccessible » : vérifier avant d'accuser le réseau.
