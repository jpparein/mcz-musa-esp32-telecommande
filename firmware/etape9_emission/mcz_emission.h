// ============================================================
// Fabrication des trames MCZ a emettre (PARTIE CALCUL SEULEMENT)
// Ce fichier ne touche PAS a la radio : il prepare une trame, c'est tout.
// L'emission elle-meme est dans le programme principal, et elle est
// bloquee tant que EMISSION_AUTORISEE n'est pas mis a true dans secrets.h.
//
// Principe : on part de la derniere trame reelle (celle de la telecommande),
// on change UN champ (flamme, ventilateur, marche/arret), on recalcule le
// controle (blocs 6 et 7) avec les tables, puis on refait les impulsions.
// ============================================================
#pragma once
#include "mcz_decode.h"
#include "mcz_tables.h"

enum ChampMCZ { CH_FAN1 = 0, CH_FLAMME = 1, CH_MODE = 2, CH_FAN2 = 3, CH_FAN3 = 4 };

// Valeur actuelle d'un champ, lue dans les 18 bits de donnees x
static int mcz_valeur(const int* x, ChampMCZ c) {
  auto b3 = [&](int a) { return x[a] * 4 + x[a + 1] * 2 + x[a + 2]; };
  switch (c) {
    case CH_FAN1:   return b3(0) ^ 2;
    case CH_FLAMME: return b3(3) ^ 5;
    case CH_MODE:   return b3(6);
    case CH_FAN2:   return (((x[11] << 3) | (x[12] << 2) | (x[13] << 1) | 1) ^ 5) >> 1;
    case CH_FAN3:   return ((((x[14] ^ 1) << 3) | (x[15] << 2) | (x[16] << 1) | x[17]) ^ 2) >> 1;
  }
  return -1;
}

// Change les bits d'un champ (XOR de l'ancienne et de la nouvelle valeur codee)
static void mcz_poser(int* d, int debut, int largeur, int ancien, int nouveau) {
  int diff = ancien ^ nouveau;
  for (int i = 0; i < largeur; i++) d[debut + i] ^= (diff >> (largeur - 1 - i)) & 1;
}

static const char* mcz_table(ChampMCZ c, int v) {
  switch (c) {
    case CH_FAN1:   return (v >= 0 && v < 7) ? DY_FAN1[v] : nullptr;
    case CH_FLAMME: return (v >= 0 && v < 8) ? DY_FLAMME[v] : nullptr;
    case CH_MODE:   return (v >= 0 && v < 8) ? DY_MODE[v] : nullptr;
    case CH_FAN2:   return (v >= 0 && v < 7) ? DY_FAN2[v] : nullptr;
    case CH_FAN3:   return (v >= 0 && v < 7) ? DY_FAN3[v] : nullptr;
  }
  return nullptr;
}

// Change un champ : calcule les nouveaux bits x2 (blocs 4+5) et y2 (blocs 6+7).
// Retourne false si la valeur demandee n'est pas connue (rien n'est alors fabrique).
static bool mcz_changer(const int* x, const int* y, ChampMCZ c, int nouveau, int* x2, int* y2) {
  int ancien = mcz_valeur(x, c);
  const char* tAncien = mcz_table(c, ancien);
  const char* tNouveau = mcz_table(c, nouveau);
  if (!tAncien || !tNouveau) return false;
  for (int i = 0; i < 18; i++) { x2[i] = x[i]; y2[i] = y[i]; }

  switch (c) {
    case CH_FAN1:   mcz_poser(x2, 0, 3, ancien ^ 2, nouveau ^ 2); break;
    case CH_FLAMME: mcz_poser(x2, 3, 3, ancien ^ 5, nouveau ^ 5); break;
    case CH_MODE:   mcz_poser(x2, 6, 3, ancien, nouveau); break;
    case CH_FAN2:   mcz_poser(x2, 11, 4, (2 * ancien) ^ 5, (2 * nouveau) ^ 5); break;
    case CH_FAN3:   mcz_poser(x2, 14, 4, (2 * ancien) ^ 2, (2 * nouveau) ^ 2); break;
  }
  for (int i = 0; i < 18; i++) y2[i] ^= (tAncien[i] != tNouveau[i]) ? 1 : 0;
  return true;
}

// Drapeau "action de l'utilisateur" (bit 9 des donnees). Dans les vraies trames il vaut 1 quand
// on appuie sur une touche de la telecommande, et 0 pour les envois automatiques du thermostat.
// On le met a 1 pour les ordres venus de l'app (le poele doit les traiter comme une action
// d'utilisateur, et confirmer par un bip). Le changement correspondant des bits de controle
// est verifie sur les vraies trames.
static void mcz_drapeau(int* x, int* y, int valeur) {
  if (x[9] == valeur) return;
  x[9] ^= 1;
  for (int i = 0; i < 18; i++) if (DY_DRAPEAU[i] == '1') y[i] ^= 1;
}

// 9 bits de donnees (domaine cumule) -> bloc brut de 12 bits
static void mcz_bloc(const int* d9, char* sortie) {
  int p[12];
  int somme = 0;
  for (int i = 0; i < 9; i++) { p[i] = d9[i]; somme ^= d9[i]; }
  p[9] = 1 ^ somme;     // parite : le total des 10 premiers bits doit etre impair
  p[10] = 1; p[11] = 1;
  for (int i = 0; i < 12; i++) sortie[i] = '0' + (p[i] ^ (i ? p[i - 1] : 0));
  sortie[12] = 0;
}

// Fabrique les 7 blocs bruts d'une trame (adresse + etat + controle)
static void mcz_mots_depuis(const int* x2, const int* y2, char mots[7][13]) {
  memcpy(mots[0], MCZ_M1, 13);
  memcpy(mots[1], MCZ_M2, 13);
  memcpy(mots[2], MCZ_M3, 13);
  mcz_bloc(x2, mots[3]);
  mcz_bloc(x2 + 9, mots[4]);
  mcz_bloc(y2, mots[5]);
  mcz_bloc(y2 + 9, mots[6]);
}

// Blocs -> flux d'unites (0/1) : synchro haute de 3 unites, puis les bits en "biphase mark".
// Retourne le nombre d'unites (189 pour une trame complete).
static int mcz_unites(char mots[7][13], char* flux) {
  int n = 0;
  for (int k = 0; k < 7; k++) {
    flux[n++] = '1'; flux[n++] = '1'; flux[n++] = '1';
    int niveau = 1;
    for (int i = 0; i < 12; i++) {
      niveau ^= 1; flux[n++] = '0' + niveau;
      if (mots[k][i] == '1') niveau ^= 1;
      flux[n++] = '0' + niveau;
    }
  }
  return n;
}

// Flux d'unites -> liste de durees (haut, bas, haut, ...) en microsecondes,
// repetee `repetitions` fois, avec le silence de 4,6 ms entre deux repetitions.
// La premiere duree est un niveau HAUT. Retourne le nombre de durees.
static int mcz_durees(const char* flux, int nUnites, int repetitions, uint16_t* sortie, int max) {
  const int UNITE = 422, SILENCE = 4586;
  int n = 0;
  for (int r = 0; r < repetitions; r++) {
    int i = 0;
    while (i < nUnites) {
      int j = i;
      while (j < nUnites && flux[j] == flux[i]) j++;
      if (n >= max) return n;
      sortie[n++] = (j - i) * UNITE;
      i = j;
    }
    // La derniere duree de la repetition est un niveau bas : on la prolonge en silence
    if (r < repetitions - 1) sortie[n - 1] = SILENCE;
    // (le flux se termine bas : la repetition suivante recommence par un niveau haut)
  }
  return n;
}
