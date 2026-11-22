// Copier ce fichier en "secrets.h" (meme dossier que etape9_emission.ino) et remplir les valeurs.
// NE JAMAIS publier secrets.h : il est dans .gitignore.
#pragma once
#define WIFI_NOM          "nom-de-votre-wifi"        // Wi-Fi 2,4 GHz de la maison (l'ESP32 ne fait pas le 5 GHz)
#define WIFI_MOT_DE_PASSE "mot-de-passe-wifi"
#define PI_ADRESSE        "192.168.1.20"             // adresse fixe du Raspberry Pi
#define PI_JETON          "jeton-long-aleatoire"     // le meme que dans token.txt cote Pi ; sert aussi de mot de passe OTA
#define EMISSION_AUTORISEE true                      // false = l'ESP32 ecoute seulement, n'emet rien
#define AP_MDP            "mot-de-passe-secours"     // mot de passe du Wi-Fi de secours "Poele-MCZ" (8 caracteres ou plus)
