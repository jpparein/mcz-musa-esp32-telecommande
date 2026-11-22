# Mode secours : l'ESP32 pilote le poêle seul

Si le serveur, la box ou le Wi-Fi de la maison tombent, l'ESP32 reste capable de piloter le poêle : il a sa propre page web et son propre Wi-Fi.

## Page de secours

`http://ADRESSE-DE-L-ESP32/` : marche/arrêt, mode (MAN / AUTO / ECO), consigne, flamme, ventilateurs, mode nuit (NO AIR). Elle a la même présentation que la télécommande de l'application, en une seule page autonome (`secours_page.h`).

## Wi-Fi propre « Poele-MCZ »

- Mot de passe : `AP_MDP` de `secrets.h` (8 caractères ou plus).
- Démarrage : **appui de 3 s sur le bouton BOOT**, ou tout seul si le Wi-Fi de la maison reste introuvable 30 s au démarrage.
- Adresse de la page : `http://192.168.4.1/` (adresse par défaut d'un point d'accès ESP32).
- Le point d'accès est sur le **canal 1**, 4 clients au maximum.
- **Portail captif** : en se connectant au Wi-Fi, le téléphone ouvre la page tout seul. Cela repose sur l'option DHCP 114 (« captive portal ») activée par `WiFi.AP.enableDhcpCaptivePortal()` (noyau esp32 ≥ 3.3, ESP-IDF ≥ 5.4.2), donc **sans serveur DNS** sur l'ESP32. Les appareils anciens (Android avant ~11) ne comprennent pas cette option : ouvrez simplement l'adresse à la main.

## Témoin lumineux (LED bleue, GPIO 2)

| LED | Signification |
|---|---|
| éteinte | tout va bien |
| un éclat toutes les 3 s | serveur injoignable |
| clignote vite | recherche du Wi-Fi |
| clignote lentement | mode secours actif |

## Sécurité

- Les réglages Wi-Fi et l'adresse du serveur se changent **uniquement** par le Wi-Fi de secours (jamais depuis le réseau de la maison) et sont gardés dans la mémoire de l'ESP32.
- Les ordres exigent l'en-tête `X-Secours` : une page web quelconque ouverte dans le navigateur ne peut pas les envoyer.
- Chaque ordre passe par la même fabrique de trames que les ordres de l'application.

## Compatibilité des vieux navigateurs

La page de secours est écrite pour fonctionner jusqu'à **Chrome 64** (vieil Android) : pas de `AbortController`, pas de `cqw`, `min()`, `clamp()`, ni de `gap` en flex. Les valeurs de repli sont écrites avant les valeurs modernes (déclarations en double), `grid-gap` en plus de `gap`, l'unité de mise à l'échelle est calculée en JavaScript quand `cqw` n'existe pas, et une classe `vieux` ajoute des marges quand le `gap` des boîtes flex n'est pas géré.

## Serveur web : choix technique

Le serveur de l'ESP32 est celui d'ESP-IDF (`esp_http_server`) : plusieurs connexions en même temps (5), purge automatique de la plus ancienne connexion inactive, délais de réception 4 s et d'envoi 5 s, exécuté sur le cœur 0 avec une pile de 8 Ko. Le serveur `WebServer` d'Arduino, lui, ne gère qu'une connexion à la fois : un navigateur qui ouvre une connexion sans rien envoyer bloque tout le reste.
