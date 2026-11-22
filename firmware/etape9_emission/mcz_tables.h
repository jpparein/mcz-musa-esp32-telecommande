// ============================================================
// Tables de controle des trames MCZ (fichier genere automatiquement)
// Pour chaque champ et chaque valeur : l'effet sur les 18 bits de controle
// (blocs 6 et 7). Calcule a partir de 47 trames reelles de la telecommande.
// ============================================================
#pragma once
// fan1 : reference = 2
static const char* const DY_FAN1[7] = {
  "000011010110011000",  // 0
  "000010110001010100",  // 1
  "000000000000000000",  // 2
  "000001100111001100",  // 3
  "000101100010101000",  // 4
  "000100000101100100",  // 5
  "000110110100110000",  // 6
};
// flame : reference = 3
static const char* const DY_FLAMME[8] = {
  "010001010011011010",  // 0 (arret ECO : deduit par linearite, verifie sur trame reelle)
  "100010010101010010",  // 1
  "110011000110001000",  // 2
  "000000000000000000",  // 3
  "010001100000111100",  // 4
  "100010100110110100",  // 5
  nullptr,  // 6
  nullptr,  // 7
};
// mode : reference = 0
static const char* const DY_MODE[8] = {
  "000000000000000000",  // 0
  nullptr,  // 1
  "001100110001100010",  // 2
  nullptr,  // 3
  "011001100011000100",  // 4
  nullptr,  // 5
  "010101010010100110",  // 6
  nullptr,  // 7
};
// fan2 : reference = 2
static const char* const DY_FAN2[7] = {
  "000100100001100010",  // 0
  "100100110001110010",  // 1
  "000000000000000000",  // 2
  "100000010000010000",  // 3
  "001101100010100110",  // 4
  "101101110010110110",  // 5
  "001001000011000100",  // 6
};
// fan3 : reference = 2
static const char* const DY_FAN3[7] = {
  "001000000010000100",  // 0
  "001100000011000110",  // 1
  "000000000000000000",  // 2
  "000100000001000010",  // 3
  "011000000110001100",  // 4
  "011100000111001110",  // 5
  "010000000100001000",  // 6
};
// Retourner le drapeau du bloc 5 (bit 9), que la telecommande bascule en AUTO/ECO
static const char DY_DRAPEAU[] = "100100010100010000";
