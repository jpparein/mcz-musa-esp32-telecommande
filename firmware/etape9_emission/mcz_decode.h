// ============================================================
// Decodage des trames de la telecommande MCZ (ecoute seule)
// Ce fichier ne contient AUCUNE emission : il lit seulement des durees
// d'impulsions deja enregistrees et en tire l'etat du poele.
//
// Rappel du protocole (voir protocole_mcz.md) :
//  - unite de base 422 us ; un bit = 2 unites (codage "biphase mark")
//  - une trame = 7 blocs de 12 bits, repetee 5 fois
//  - chaque bloc : XOR cumule des 12 bits = 9 bits de donnees + 1 bit de
//    parite (impair) + "11"
//  - blocs 1, 2, 3 : adresse de la telecommande (constants)
//  - blocs 4, 5 : etat (flamme, ventilateurs, mode)
//  - blocs 6, 7 : controle (non utilises ici pour decoder)
// ============================================================
#pragma once
#include <stdint.h>
#include <string.h>

// Adresse de la telecommande (blocs 1, 2, 3 en bits bruts)
static const char MCZ_M1[] = "000100000000";
static const char MCZ_M2[] = "100010000100";
static const char MCZ_M3[] = "011111001010";

const int MCZ_UNITE_US   = 422;    // duree d'une unite
const int MCZ_MAX_BORDS  = 3000;   // nombre max de changements traites
const int MCZ_MAX_FLUX   = 1800;   // longueur max du flux d'unites
const int MCZ_MAX_TRAMES = 8;      // nombre max de repetitions traitees

struct TrameMCZ {
  bool valide;          // trame complete et verifiee
  char mots[7][13];     // blocs bruts (12 bits + fin de chaine)
  int  nbMots;
  int  chiffreMode;     // 0 = MAN, 2 = arret, 4 = ECO, 6 = AUTO
  int  flamme;          // 0 a 5
  int  fan1, fan2, fan3;// 0 a 5, 6 = AUTO
  bool marche;
  char mode[5];         // "MAN", "AUTO", "ECO" ou "" (arret)
  bool complet;         // les 7 blocs sont verifies (necessaire pour fabriquer une trame)
  int  x[18];           // 18 bits de donnees des blocs 4 et 5 (domaine cumule)
  int  y[18];           // 18 bits de donnees des blocs 6 et 7 (controle)
};

// ---- Outils internes ----
static void mcz_cumul(const char* brut, int* x) {   // XOR cumule d'un bloc brut
  int a = 0;
  for (int i = 0; i < 12; i++) { a ^= (brut[i] == '1'); x[i] = a; }
}

// Meme chose a partir d'un texte de 12 caracteres '0'/'1' (utile pour les tests)
static void mcz_cumulBrut(const char* brut, int* x) { mcz_cumul(brut, x); }

// Decode un mot de "unites" (0/1) en bits bruts (biphase mark).
// Retourne le nombre de bits obtenus.
static int mcz_biphase(const char* w, int len, char* sortie) {
  int n = 0;
  for (int i = 0; i + 1 < len && n < 16; i += 2) sortie[n++] = (w[i] != w[i + 1]) ? '1' : '0';
  sortie[n] = 0;
  return n;
}

// Coupe un flux a chaque "111" et garde les mots de 22 unites ou plus.
static int mcz_mots(const char* s, int L, int* deb, int* lon, int max) {
  int nb = 0, debut = 0, i = 0;
  while (i + 2 < L) {
    if (s[i] == '1' && s[i + 1] == '1' && s[i + 2] == '1') {
      if (i - debut >= 22 && nb < max) { deb[nb] = debut; lon[nb] = i - debut; nb++; }
      debut = i + 3; i += 3;
    } else i++;
  }
  if (L - debut >= 22 && nb < max) { deb[nb] = debut; lon[nb] = L - debut; nb++; }
  return nb;
}

// Tente de lire une trame dans un flux d'unites. Retourne true si valide.
static bool mcz_lireFlux(const char* s, int L, TrameMCZ& t) {
  int deb[12], lon[12];
  int nb = mcz_mots(s, L, deb, lon, 12);
  if (nb < 6) return false;
  if (nb > 7) nb = 7;
  t.nbMots = 0;
  for (int k = 0; k < nb; k++) {
    char bits[20];
    int n = mcz_biphase(s + deb[k], lon[k], bits);
    if (k == 6 && n == 11) { bits[11] = '0'; bits[12] = 0; n = 12; }   // fin de trame coupee : dernier bit = 0
    if (n != 12) { if (k < 6) return false; else break; }
    memcpy(t.mots[k], bits, 13);
    t.nbMots++;
  }
  if (t.nbMots < 6) return false;

  // Adresse de la telecommande
  if (strcmp(t.mots[0], MCZ_M1) != 0 || strcmp(t.mots[1], MCZ_M2) != 0 || strcmp(t.mots[2], MCZ_M3) != 0) return false;

  // Parite et fin "11" des blocs 1 a 6 (le bloc 7 est souvent coupe en fin de trame)
  int x[7][12];
  for (int k = 0; k < 6; k++) {
    mcz_cumul(t.mots[k], x[k]);
    int somme = 0;
    for (int i = 0; i < 10; i++) somme += x[k][i];
    if ((somme & 1) != 1) return false;
    if (x[k][10] != 1 || x[k][11] != 1) return false;
  }

  // Bloc 7 (s'il est la) : meme verification
  t.complet = false;
  if (t.nbMots == 7) {
    mcz_cumul(t.mots[6], x[6]);
    int somme = 0;
    for (int i = 0; i < 10; i++) somme += x[6][i];
    if ((somme & 1) == 1 && x[6][10] == 1 && x[6][11] == 1) t.complet = true;
  }

  // Les 18 bits de donnees des blocs 4 et 5 (x) et des blocs 6 et 7 (y)
  int d[18];
  for (int i = 0; i < 9; i++) { d[i] = x[3][i]; d[9 + i] = x[4][i]; }
  for (int i = 0; i < 9; i++) { t.x[i] = x[3][i]; t.x[9 + i] = x[4][i]; t.y[i] = x[5][i]; t.y[9 + i] = t.complet ? x[6][i] : 0; }
  auto bits3 = [&](int a) { return d[a] * 4 + d[a + 1] * 2 + d[a + 2]; };

  t.fan1        = bits3(0) ^ 2;
  t.flamme      = bits3(3) ^ 5;
  t.chiffreMode = bits3(6);
  // Ventilateur 2 : 4 bits (d11..d14), le dernier bit vaut toujours 1
  int v2 = (d[11] << 3) | (d[12] << 2) | (d[13] << 1) | 1;
  t.fan2 = (v2 ^ 5) >> 1;
  // Ventilateur 3 : 4 bits (d14..d17), d14 est partage avec le ventilateur 2
  int v3 = ((d[14] ^ 1) << 3) | (d[15] << 2) | (d[16] << 1) | d[17];
  t.fan3 = (v3 ^ 2) >> 1;

  // Valeurs plausibles seulement
  if (t.fan1 > 6 || t.fan2 > 6 || t.fan3 > 6 || t.flamme > 5) return false;
  if (t.chiffreMode != 0 && t.chiffreMode != 2 && t.chiffreMode != 4 && t.chiffreMode != 6) return false;

  t.marche = (t.chiffreMode != 2);
  if (t.chiffreMode == 0) strcpy(t.mode, "MAN");
  else if (t.chiffreMode == 6) strcpy(t.mode, "AUTO");
  else if (t.chiffreMode == 4) strcpy(t.mode, "ECO");
  else t.mode[0] = 0;
  t.valide = true;
  return true;
}

// ---- Fonction principale ----
// durees : duree de chaque etat (haut/bas en alternance), en microsecondes.
// Retourne true si une trame complete et verifiee a ete trouvee.
static uint16_t mcz_propre[MCZ_MAX_BORDS];
static char     mcz_flux[MCZ_MAX_FLUX + 4];
static char     mcz_inverse[MCZ_MAX_FLUX + 4];

static bool mcz_decoder(const volatile uint16_t* durees, int n, TrameMCZ& t) {
  t.valide = false;
  if (n > MCZ_MAX_BORDS) n = MCZ_MAX_BORDS;

  // 1) On absorbe les impulsions parasites (moins de 150 us) avec la suivante
  int c = 0;
  for (int i = 0; i < n;) {
    uint16_t d = durees[i];
    if (d < 150 && c > 0 && i + 1 < n) { mcz_propre[c - 1] += d + durees[i + 1]; i += 2; continue; }
    mcz_propre[c++] = d; i++;
  }

  // 2) On transforme les durees en flux d'unites, coupe aux grands silences
  int debuts[MCZ_MAX_TRAMES], fins[MCZ_MAX_TRAMES], nbTrames = 0;
  int L = 0, debutTrame = 0;
  int niveau = 1;
  for (int i = 0; i < c; i++) {
    int u = (mcz_propre[i] + MCZ_UNITE_US / 2) / MCZ_UNITE_US;
    if (u > 10) {                                   // silence : fin d'une repetition
      if (L - debutTrame >= 180 && nbTrames < MCZ_MAX_TRAMES) { debuts[nbTrames] = debutTrame; fins[nbTrames] = L; nbTrames++; }
      debutTrame = L;
    } else {
      for (int k = 0; k < u && L < MCZ_MAX_FLUX; k++) mcz_flux[L++] = niveau ? '1' : '0';
    }
    niveau = !niveau;
  }
  if (L - debutTrame >= 180 && nbTrames < MCZ_MAX_TRAMES) { debuts[nbTrames] = debutTrame; fins[nbTrames] = L; nbTrames++; }

  // 3) On essaie chaque repetition, dans les deux polarites
  for (int f = 0; f < nbTrames; f++) {
    int len = fins[f] - debuts[f];
    const char* a = mcz_flux + debuts[f];
    for (int i = 0; i < len; i++) mcz_inverse[i] = (a[i] == '1') ? '0' : '1';
    if (mcz_lireFlux(a, len, t)) return true;
    if (mcz_lireFlux(mcz_inverse, len, t)) return true;
  }
  return false;
}
