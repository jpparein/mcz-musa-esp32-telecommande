# Le serveur : application web + file de commandes (Apache + PHP + SQLite)

Le serveur est **facultatif** pour piloter le poêle (l'ESP32 a sa page de secours, voir [MODE_SECOURS.md](MODE_SECOURS.md)), mais il apporte : l'application sur tous les appareils, les minuteurs, la programmation hebdomadaire, l'historique, le journal de l'ESP32.

Choix de sécurité : **le serveur ne commande jamais le poêle directement.** Il ajoute des demandes dans une file ; c'est l'ESP32 qui vient les chercher (une fois par seconde), les envoie, puis rend compte. Une demande de plus de 2 minutes n'est pas exécutée.

```
navigateur  --api.php-->  SQLite (file de commandes, état)  <--esp.php (jeton)--  ESP32  --radio-->  poêle
```

## Installation

1. Serveur : Apache 2 + PHP 8 (extensions `pdo_sqlite`, `mbstring`). Testé sur un Raspberry Pi (Debian).
2. Copiez le contenu de `web/` dans un dossier du site (ex. `/var/www/html/poele-mcz/`).
3. Créez le **dossier de données, hors du dossier web** (sinon la base serait téléchargeable) et donnez les droits au serveur web :
   ```
   sudo mkdir -p /home/UTILISATEUR/poele-mcz-data
   sudo chown www-data:www-data /home/UTILISATEUR/poele-mcz-data
   sudo chmod 750 /home/UTILISATEUR/poele-mcz-data
   ```
4. Créez le jeton partagé (une longue chaîne aléatoire) :
   ```
   openssl rand -hex 24 | sudo tee /home/UTILISATEUR/poele-mcz-data/token.txt
   ```
   Le **même** jeton va dans `secrets.h` (`PI_JETON`). Il sert aussi de mot de passe pour la mise à jour OTA.
5. Dans les fichiers PHP, adaptez la variable `$dossier` (`/home/rpi/poele-mcz-data` dans le code de l'auteur) au chemin choisi.
6. Les tables SQLite sont créées automatiquement au premier appel.
7. Ouvrez `http://ADRESSE-DU-SERVEUR/poele-mcz/` : la pastille doit indiquer « ESP32 connecté » une fois l'ESP32 allumé.

L'application est installable sur téléphone (manifest + service worker, `manifest.webmanifest`, `sw.js`). Sur iPhone : Safari → Partager → « Sur l'écran d'accueil ».

> La page n'a pas de mot de passe : toute personne du réseau local peut commander le poêle. Ne l'exposez pas sur Internet sans authentification.

## Fichiers

| Fichier | Rôle |
|---|---|
| `index.html` | page d'accueil (état, température, commandes) |
| `telecommande.html` | télécommande à l'écran, avec écran LCD, touches, panneau TIMER |
| `reglages.html`, `reglages_api.php` | mode de fonctionnement, journal de l'ESP32, apparence |
| `aide.html` | aide en mots simples (MAN, AUTO, ECO…) pour l'entourage |
| `apprentissage.html`, `apprentissage*.php` | outil d'enregistrement utilisé pour comprendre l'AUTO |
| `api.php` | API publique (navigateur) |
| `esp.php` | API réservée à l'ESP32, protégée par le jeton |
| `planning.php` | minuteurs et programmes (exécutés quand l'ESP32 interroge la file) |
| `mode.js`, `theme.js`, `sw.js`, `manifest.webmanifest`, `icons/` | divers |

## API de l'ESP32 : `esp.php?action=…`

Toutes en **POST**, en-tête `X-Token: <jeton>`, corps JSON. Réponse JSON.

| Action | Corps | Effet |
|---|---|---|
| `ping` | — | « je suis en vie » |
| `trame` | `{mode, marche, flamme, vent1, vent2, vent3}` | une trame de la télécommande a été décodée |
| `mesure` | `{t, h}` | température (0–45 °C) et humidité du DHT22 |
| `etat` | — | l'ESP32 reçoit l'état voulu (dernier état connu, consigne, mode de fonctionnement, `nuit`) |
| `journal` | `{l:[{m, a}]}` | lignes de journal de l'ESP32 (500 dernières gardées) |
| `thermo` | `{actif, puissance, raison}` | compte rendu du thermostat |
| `commande` | — | donne **la plus ancienne commande en attente** (`{id,type,valeur}`) ou `{}` ; exécute aussi minuteurs et programmes |
| `fait` | `{id, statut}` | `envoyee`, `echec` ou `ignoree` |

Types de commande : `marche` (`on`/`off`), `mode` (`MAN`/`AUTO`/`ECO`), `flamme`, `vent1`, `vent2`, `vent3`.

Règles de la file : une commande restée « en cours » plus de 30 s passe en échec ; deux envois sont espacés d'au moins 4 s (l'ESP32 en refuse deux à moins de 3 s) ; une demande de plus de 2 minutes est ignorée.

## API de l'application : `api.php?action=…`

Requêtes POST avec l'en-tête `X-Requested-With: poele` (protection simple contre les requêtes envoyées par un autre site) :

`etat`, `commande`, `minuteur`, `minuteur_annuler`, `programme_enregistrer`, `programme_actif`, `programme_supprimer`, `annuler`.

## Tables SQLite

| Table | Contenu |
|---|---|
| `etat` | paires clé / valeur : `poele`, `mode`, `flamme`, `vent1-3`, `temperature`, `esp_vu`, `consigne_ecran`, `mode_fonct`, `nuit`, `thermo_*`… |
| `commandes` | file : `id, date_demande, type, valeur, statut (attente/envoi/envoyee/echec/ignoree), date_statut` |
| `minuteurs` | `echeance, action, statut` |
| `programmes` | `jours, heure, action, flamme, vent1-3, actif, derniere, mode, consigne` |
| `apprentissage` | enregistrement des trames et températures (pour étudier la régulation) |
| `journal` | journal de l'ESP32 |

Faites une **sauvegarde** de `poele.db` avant de modifier la base.

## À adapter

Les pages `index.html` et `telecommande.html` contiennent un lien « Ouvrir le mode secours » vers `http://192.168.1.50/` : remplacez-le par l'adresse fixe de votre ESP32 (réservez-la dans la box).
