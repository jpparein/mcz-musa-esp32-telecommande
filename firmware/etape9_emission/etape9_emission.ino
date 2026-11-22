// ============================================================
// ETAPE 9 : ecoute de la telecommande + EMISSION vers le poele
//
// - Ecoute la telecommande MCZ et envoie l'etat au Raspberry Pi (comme l'etape 8).
// - Peut EMETTRE un ordre au poele (marche/arret, flamme, ventilateurs).
//
// SECURITE : l'emission est BLOQUEE tant que EMISSION_AUTORISEE vaut false
// dans l'onglet secrets.h (c'est le reglage par defaut).
// Meme autorisee, une seule commande est envoyee a la fois, et jamais plus
// d'une toutes les 3 secondes.
//
// Carte : ESP32 Dev Module   |   Moniteur serie : 115200 bauds
// ============================================================
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <esp_system.h>
#include <stdarg.h>
#include "secrets.h"        // wifi + adresse du Pi + jeton + interrupteur d'emission
#include "mcz_decode.h"     // lecture des trames de la telecommande
#include "mcz_emission.h"   // fabrication des trames a emettre
#include "mcz_autotest.h"   // trames reelles pour verifier le generateur

// ---------- Broches (cablage deja fait) ----------
const int PIN_SCK  = 18;
const int PIN_MISO = 19;
const int PIN_MOSI = 23;
const int PIN_CSN  = 5;
const int PIN_GDO0 = 4;   // donnees brutes : sortie en reception, ENTREE du CC1101 en emission

// ---------- Reglages radio ----------
const float FREQ_MHZ   = 434.5;
const int   SEUIL_RSSI = -70;   // etait -45 (telecommande collee) ; -70 = ecoute a distance
const byte  MODULATION = 0x30;   // OOK (tout ou rien)

// ---------- Commandes et registres du CC1101 ----------
const byte CMD_RESET = 0x30;
const byte CMD_TX    = 0x35;
const byte CMD_RX    = 0x34;
const byte CMD_IDLE  = 0x36;

const byte REG_IOCFG0   = 0x02;
const byte REG_PKTCTRL0 = 0x08;
const byte REG_FREQ2    = 0x0D;
const byte REG_FREQ1    = 0x0E;
const byte REG_FREQ0    = 0x0F;
const byte REG_MDMCFG4  = 0x10;
const byte REG_MDMCFG3  = 0x11;
const byte REG_MDMCFG2  = 0x12;
const byte REG_MCSM0    = 0x18;
const byte REG_FREND0   = 0x22;
const byte REG_PATABLE  = 0x3E;
const byte REG_RSSI     = 0x34 | 0xC0;

// ---------- Memoire des impulsions recues ----------
const int MAX_BORDS = 3000;
volatile uint16_t durees[MAX_BORDS];
volatile int nbBords = 0;
volatile bool enregistre = false;
volatile bool premierFront = true;
volatile uint32_t dernierTemps = 0;

// ---------- Derniere trame reelle (base pour fabriquer les ordres) ----------
TrameMCZ derniere;
volatile bool deriereValide = false;
int dernierModeAllume = 0;          // chiffre de mode a reprendre pour "allumer" (0 = MAN)
unsigned long derniereEmission = 0;

// ---------- Echanges entre la radio (coeur 1) et le reseau (coeur 0) ----------
struct EtatPartage {
  bool nouveau;
  bool marche;
  char mode[5];
  int  flamme, fan1, fan2, fan3;
};
EtatPartage partage = {false, false, "", 0, 0, 0, 0};

struct CommandeAttente {              // une commande venue de la page web
  bool presente;
  int  id;
  char type[10];
  char valeur[6];
};
CommandeAttente cmdAttente = {false, 0, "", ""};

struct Resultat {                     // compte rendu a renvoyer au Pi
  bool present;
  int  id;
  char statut[10];                    // envoyee / echec / ignoree
};
Resultat resultat = {false, 0, ""};

struct EtatPi {                       // dernier etat connu du poele, donne par le Pi
  bool valide;
  bool marche;
  char mode[5];
  int  flamme, fan1, fan2, fan3;
};
EtatPi etatPi = {false, true, "MAN", 3, 1, 1, 1};

portMUX_TYPE verrou = portMUX_INITIALIZER_UNLOCKED;
volatile bool espEnLigne = false;

// ---------- Reseau regle depuis la page de secours (gardes en memoire de l'ESP32) ----------
Preferences prefsRes;
String wifiNom, wifiMdp, piAdresse;       // lus au demarrage ; valeurs de secrets.h si rien d'enregistre
volatile bool apActif = false;            // mode secours : Wi-Fi "Poele-MCZ" actif
volatile bool dejaConnecte = false;       // le Wi-Fi de la maison a ete trouve depuis le demarrage
void demarrerAP();
bool baseDepuisSauvegarde();

// ---------- Thermostat du mode REMPLACEMENT (voir thermostat()) ----------
Preferences prefs;                       // memoire flash : le thermostat survit a une coupure du Pi ou du Wi-Fi
volatile float thConsigne = 20.0f;       // consigne (donnee par le Pi, gardee en flash)
volatile bool  thNuit = false;            // mode nuit (NO AIR) demande par l'app : puissance bloquee a 1
volatile bool  thRemplacement = false;   // mode de fonctionnement "remplacement" (donne par le Pi, garde en flash)
volatile float tempCourante = NAN;       // temperature filtree de la piece
volatile unsigned long tempQuand = 0;    // moment de la derniere mesure valide
volatile unsigned long teleVue = 0;      // derniere trame de la VRAIE telecommande entendue
char thRaison[16] = "inactif";           // pourquoi le thermostat est (in)actif, a montrer sur la page
volatile int  thPuissance = 0;
volatile bool thActif = false;

// ---------- Journal des evenements : garde en memoire, envoye au Pi (lisible dans Reglages) ----------
#define JMAX 40
struct JLigne { unsigned long ms; char m[100]; };
JLigne jBuf[JMAX];
int jTete = 0, jN = 0;
void jrn(const char* fmt, ...) {
  char tmp[100];
  va_list ap; va_start(ap, fmt); vsnprintf(tmp, sizeof(tmp), fmt, ap); va_end(ap);
  for (char* q = tmp; *q; q++) { if (*q == '"') *q = '\''; else if (*q == '\\') *q = '/'; else if ((unsigned char)*q < 32) *q = ' '; }
  portENTER_CRITICAL(&verrou);
  jBuf[jTete].ms = millis();
  strcpy(jBuf[jTete].m, tmp);
  jTete = (jTete + 1) % JMAX;
  if (jN < JMAX) jN++;
  portEXIT_CRITICAL(&verrou);
}
const char* causeDemarrage() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "mise sous tension";
    case ESP_RST_SW: return "redemarrage logiciel (mise a jour ou reset)";
    case ESP_RST_PANIC: return "PLANTAGE du programme";
    case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT: case ESP_RST_WDT: return "PLANTAGE (chien de garde)";
    case ESP_RST_BROWNOUT: return "baisse de tension";
    case ESP_RST_EXT: return "bouton reset";
    default: return "autre";
  }
}

// ---------- Interruption : a chaque changement de GDO0 (reception) ----------
void IRAM_ATTR surFront() {
  if (!enregistre) return;
  uint32_t t = micros();
  if (premierFront) { premierFront = false; dernierTemps = t; return; }
  uint32_t d = t - dernierTemps;
  dernierTemps = t;
  if (d > 65535) d = 65535;
  if (nbBords < MAX_BORDS) durees[nbBords++] = (uint16_t)d;
}

// ---------- Communication SPI avec le CC1101 ----------
void attendrePret() {
  unsigned long t = millis();
  while (digitalRead(PIN_MISO) == HIGH && millis() - t < 100) {}
}
void commande(byte c) {
  digitalWrite(PIN_CSN, LOW); attendrePret();
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  SPI.transfer(c);
  SPI.endTransaction();
  digitalWrite(PIN_CSN, HIGH);
}
void ecrireRegistre(byte adresse, byte valeur) {
  digitalWrite(PIN_CSN, LOW); attendrePret();
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  SPI.transfer(adresse);
  SPI.transfer(valeur);
  SPI.endTransaction();
  digitalWrite(PIN_CSN, HIGH);
}
void ecrirePATABLE(const byte* v, int n) {      // table de puissance d'emission
  digitalWrite(PIN_CSN, LOW); attendrePret();
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  SPI.transfer(REG_PATABLE | 0x40);              // acces en rafale
  for (int i = 0; i < n; i++) SPI.transfer(v[i]);
  SPI.endTransaction();
  digitalWrite(PIN_CSN, HIGH);
}
byte lireStatut(byte adresseAvecBits) {
  digitalWrite(PIN_CSN, LOW); attendrePret();
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  SPI.transfer(adresseAvecBits);
  byte v = SPI.transfer(0);
  SPI.endTransaction();
  digitalWrite(PIN_CSN, HIGH);
  return v;
}
int lireRSSI() {
  byte brut = lireStatut(REG_RSSI);
  if (brut >= 128) return ((int)brut - 256) / 2 - 74;
  return (int)brut / 2 - 74;
}
void resetCC1101() {
  digitalWrite(PIN_CSN, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_CSN, LOW);  delayMicroseconds(10);
  digitalWrite(PIN_CSN, HIGH); delayMicroseconds(60);
  commande(CMD_RESET);
  delay(5);
}
void regleFrequence(float mhz) {
  uint32_t mot = (uint32_t)(mhz * 65536.0 / 26.0 + 0.5);
  ecrireRegistre(REG_FREQ2, (mot >> 16) & 0xFF);
  ecrireRegistre(REG_FREQ1, (mot >> 8) & 0xFF);
  ecrireRegistre(REG_FREQ0, mot & 0xFF);
}

// ---------- Reglages de reception ----------
void configReception() {
  commande(CMD_IDLE);
  ecrireRegistre(REG_MDMCFG4, 0x58);
  ecrireRegistre(REG_MDMCFG3, 0x22);
  ecrireRegistre(REG_MDMCFG2, MODULATION);
  ecrireRegistre(REG_PKTCTRL0, 0x32);       // mode serie asynchrone
  ecrireRegistre(REG_IOCFG0, 0x0D);         // GDO0 = donnees recues
  ecrireRegistre(REG_FREND0, 0x10);
  ecrireRegistre(REG_MCSM0, 0x14);
  pinMode(PIN_GDO0, INPUT);
  commande(CMD_RX);
  delay(10);
}

// =====================================================================
// EMISSION RADIO : cette fonction est le SEUL endroit qui emet.
// Elle n'est appelee que si EMISSION_AUTORISEE vaut true.
// =====================================================================
void emettreDurees(const uint16_t* d, int n) {
  detachInterrupt(PIN_GDO0);
  enregistre = false;
  commande(CMD_IDLE);

  ecrireRegistre(REG_MDMCFG4, 0xF6);        // debit d'environ 2,4 kbauds pour l'emission
  ecrireRegistre(REG_MDMCFG3, 0x83);
  ecrireRegistre(REG_MDMCFG2, MODULATION);  // OOK
  ecrireRegistre(REG_PKTCTRL0, 0x32);       // mode serie asynchrone
  ecrireRegistre(REG_IOCFG0, 0x2E);         // GDO0 devient une entree pour les donnees a emettre
  ecrireRegistre(REG_FREND0, 0x11);         // OOK : on utilise la case 1 de la table de puissance
  const byte puissance[8] = {0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};   // case 0 = eteint, case 1 = emission
  ecrirePATABLE(puissance, 8);

  pinMode(PIN_GDO0, OUTPUT);
  digitalWrite(PIN_GDO0, LOW);
  commande(CMD_TX);
  delay(3);                                  // calibration et demarrage de l'emetteur

  uint32_t debut = micros();
  uint32_t cumul = 0;
  int niveau = HIGH;
  for (int i = 0; i < n; i++) {
    digitalWrite(PIN_GDO0, niveau);
    cumul += d[i];
    while ((uint32_t)(micros() - debut) < cumul) {}
    niveau = !niveau;
  }
  digitalWrite(PIN_GDO0, LOW);

  commande(CMD_IDLE);
  configReception();
  premierFront = true;
  attachInterrupt(digitalPinToInterrupt(PIN_GDO0), surFront, CHANGE);
}

// Fabrique la trame de base a partir de l'etat connu du Pi, quand la telecommande
// n'est pas entendue : on part d'une vraie trame et on la met champ par champ a l'etat voulu.
bool baseDepuisPi() {
  EtatPi e;
  portENTER_CRITICAL(&verrou);
  e = etatPi;
  portEXIT_CRITICAL(&verrou);
  if (!e.valide) return false;
  const VecteurTest& v = VECTEURS[0];          // vraie trame servant de modele
  int xb[12];
  for (int m = 3; m < 7; m++) {
    mcz_cumulBrut(v.a[m], xb);
    for (int i = 0; i < 9; i++) {
      if (m == 3) derniere.x[i] = xb[i]; else if (m == 4) derniere.x[9 + i] = xb[i];
      else if (m == 5) derniere.y[i] = xb[i]; else derniere.y[9 + i] = xb[i];
    }
  }
  int mode = !e.marche ? 2 : (strcmp(e.mode, "AUTO") == 0 ? 6 : (strcmp(e.mode, "ECO") == 0 ? 4 : 0));
  struct { ChampMCZ champ; int valeur; } liste[5] = {
    {CH_MODE, mode}, {CH_FLAMME, e.flamme}, {CH_FAN1, e.fan1}, {CH_FAN2, e.fan2}, {CH_FAN3, e.fan3}
  };
  for (int k = 0; k < 5; k++) {
    int x2[18], y2[18];
    if (mcz_changer(derniere.x, derniere.y, liste[k].champ, liste[k].valeur, x2, y2))
      for (int i = 0; i < 18; i++) { derniere.x[i] = x2[i]; derniere.y[i] = y2[i]; }
  }
  derniere.complet = true;
  if (mode != 2) dernierModeAllume = mode;
  deriereValide = true;
  Serial.println("  Trame de base reconstruite a partir de l'etat connu du Pi.");
  return true;
}

// ---------- Execution d'une commande venue de la page ----------
// Retourne "envoyee", "echec" ou "ignoree".
// =====================================================================
// NO AIR (lune). Mesure sur la vraie telecommande (19/11) : 5 min apres l'ordre, elle envoie
// elle-meme une trame AUTO, flamme 1, ventilos 1/2/3 a 0, drapeau 0 (silencieuse). C'est ce qui
// coupe les ventilos. A la sortie elle renvoie les ventilos d'avant, la flamme, drapeau 1.
// =====================================================================
#define DELAI_NOAIR_MS 300000UL  // 5 min : mesure sur la vraie telecommande
static bool ventCoupes = false;          // nos trames ont coupe les ventilos (NO AIR)
static int  ventSauve[3] = {1, 0, 1};    // ventilos d'avant le NO AIR

static bool changerVentilos(const int* x, const int* y, int f1, int f2, int f3, int* x2, int* y2) {
  int xa[18], ya[18], xb[18], yb[18];
  if (!mcz_changer(x, y, CH_FAN1, f1, xa, ya)) return false;
  if (!mcz_changer(xa, ya, CH_FAN2, f2, xb, yb)) return false;
  return mcz_changer(xb, yb, CH_FAN3, f3, x2, y2);
}
static void emettreTrame(int* x2, int* y2) {
  char mots[7][13];
  mcz_mots_depuis(x2, y2, mots);
  static char flux[260];
  static uint16_t imp[900];
  int nu = mcz_unites(mots, flux);
  int ni = mcz_durees(flux, nu, 5, imp, 900);
  emettreDurees(imp, ni);
  derniereEmission = millis();
  for (int i = 0; i < 18; i++) { derniere.x[i] = x2[i]; derniere.y[i] = y2[i]; }
}
// Remet dans "derniere" les ventilos d'avant le NO AIR (sans emettre) : la prochaine trame les porte
static void ventilosRemisDansBase() {
  if (!ventCoupes) return;
  int x2[18], y2[18];
  if (changerVentilos(derniere.x, derniere.y, ventSauve[0], ventSauve[1], ventSauve[2], x2, y2)) {
    for (int i = 0; i < 18; i++) { derniere.x[i] = x2[i]; derniere.y[i] = y2[i]; }
  }
  ventCoupes = false;
  thNuit = false;      // l'app a deja ote le NO AIR cote Pi : on ne recoupe pas les ventilos en attendant la prochaine lecture
  jrn("NO AIR : ventilos remis (%d/%d/%d)", ventSauve[0], ventSauve[1], ventSauve[2]);
}
void nuitVentilos() {
  static bool nuitVue = false;
  static unsigned long debut = 0, sortie = 0;
  if (!EMISSION_AUTORISEE || !thRemplacement || !deriereValide || !derniere.complet) return;
  bool nuit = thNuit;
  if (nuit && !nuitVue) { nuitVue = true; debut = millis(); jrn("NO AIR : debut, coupure des ventilos dans 5 min"); }
  if (!nuit && nuitVue) { nuitVue = false; sortie = millis(); }
  bool libre = (derniereEmission == 0 || millis() - derniereEmission >= 3000);
  if (nuit && !ventCoupes && millis() - debut >= DELAI_NOAIR_MS && libre) {
    ventSauve[0] = mcz_valeur(derniere.x, CH_FAN1); ventSauve[1] = mcz_valeur(derniere.x, CH_FAN2); ventSauve[2] = mcz_valeur(derniere.x, CH_FAN3);
    int x2[18], y2[18];
    if (changerVentilos(derniere.x, derniere.y, 0, 0, 0, x2, y2)) {
      mcz_drapeau(x2, y2, 0);
      jrn("NO AIR : emission ventilos 0/0/0 (avant %d/%d/%d)", ventSauve[0], ventSauve[1], ventSauve[2]);
      emettreTrame(x2, y2);
      ventCoupes = true;
    }
  }
  // sortie du NO AIR sans ordre de l'app : on remet nous-memes les ventilos (drapeau 1)
  if (!nuit && ventCoupes && sortie != 0 && millis() - sortie >= 8000UL && libre) {
    int x2[18], y2[18];
    if (changerVentilos(derniere.x, derniere.y, ventSauve[0], ventSauve[1], ventSauve[2], x2, y2)) {
      mcz_drapeau(x2, y2, 1);
      jrn("NO AIR : fin, emission ventilos %d/%d/%d", ventSauve[0], ventSauve[1], ventSauve[2]);
      emettreTrame(x2, y2);
      ventCoupes = false;
    }
  }
}

const char* executerCommande(const CommandeAttente& c) {
  if (!EMISSION_AUTORISEE) return "ignoree";                 // interrupteur ferme
  if (!deriereValide || !derniere.complet) {
    if (!baseDepuisPi() && !baseDepuisSauvegarde()) {
      Serial.println("  Commande refusee : etat du poele inconnu (ni trame entendue, ni etat du Pi, ni memoire).");
      return "echec";
    }
  }
  if (millis() - derniereEmission < 3000 && derniereEmission != 0) {
    Serial.println("  Commande refusee : trop rapprochee de la precedente.");
    return "echec";
  }

  // Quel champ, quelle valeur ?
  ChampMCZ champ; int valeur;
  int v = atoi(c.valeur);
  if (strcmp(c.type, "marche") == 0) {
    champ = CH_MODE;
    valeur = (strcmp(c.valeur, "off") == 0) ? 2 : dernierModeAllume;
  } else if (strcmp(c.type, "mode") == 0) {
    int chiffre = strcmp(c.valeur, "AUTO") == 0 ? 6 : (strcmp(c.valeur, "ECO") == 0 ? 4 : 0);
    if (mcz_valeur(derniere.x, CH_MODE) == 2) {          // poele eteint : on retient le mode pour l'allumage
      dernierModeAllume = chiffre;
      Serial.print("  Mode retenu pour le prochain allumage : "); Serial.println(c.valeur);
      return "envoyee";
    }
    champ = CH_MODE; valeur = chiffre;
  } else if (strcmp(c.type, "flamme") == 0) { champ = CH_FLAMME; valeur = v; }
  else if (strcmp(c.type, "vent1") == 0)    { champ = CH_FAN1;   valeur = v; }
  else if (strcmp(c.type, "vent2") == 0)    { champ = CH_FAN2;   valeur = v; }
  else if (strcmp(c.type, "vent3") == 0)    { champ = CH_FAN3;   valeur = v; }
  else return "echec";

  ventilosRemisDansBase();      // sortie du NO AIR : la trame de l'app porte aussi les ventilos d'avant
  int x2[18], y2[18];
  if (!mcz_changer(derniere.x, derniere.y, champ, valeur, x2, y2)) {
    Serial.println("  Commande refusee : valeur inconnue.");
    return "echec";
  }
  mcz_drapeau(x2, y2, 1);       // action d'utilisateur : le poele doit confirmer par un bip
  char mots[7][13];
  mcz_mots_depuis(x2, y2, mots);
  static char flux[260];
  static uint16_t imp[900];
  int nu = mcz_unites(mots, flux);
  int ni = mcz_durees(flux, nu, 5, imp, 900);

  Serial.print("  EMISSION : "); Serial.print(c.type); Serial.print(" = "); Serial.println(c.valeur);
  emettreDurees(imp, ni);
  derniereEmission = millis();

  // Notre trame devient la nouvelle base (jusqu'a la prochaine trame de la telecommande)
  for (int i = 0; i < 18; i++) { derniere.x[i] = x2[i]; derniere.y[i] = y2[i]; }
  if (champ == CH_MODE && valeur != 2) dernierModeAllume = valeur;
  return "envoyee";
}

// =====================================================================
// THERMOSTAT (mode remplacement, poele en AUTO) : clone de la telecommande.
// Regle mesuree : toutes les minutes, si la consigne est AU-DESSUS de la temperature
// entiere (virgule ignoree) la puissance monte d'un cran (max 5), sinon elle baisse
// d'un cran (min 1). La trame est renvoyee au moins toutes les 20 min.
// Capteur muet : on garde la derniere puissance (comme une telecommande sans pile,
// le poele a ses propres securites). Si on entend la vraie telecommande, on se tait 30 min.
// =====================================================================
void thermostat() {
  static unsigned long prochain = 0, dernierEnvoi = 0, dernierEssaiBase = 0;
  static bool etaitActif = false, aEnvoyer = false;
  static int puissance = 3;
  // ECO : comme AUTO, plus (mesure le 19/11) arret apres 20 min a la consigne (trame flamme 0),
  // puis rallumage (flamme 3) quand la temperature est a consigne-2 et 5 min au moins apres l'arret
  static bool ecoVeille = false, ecoPrec = false;
  static unsigned long ecoTenue = 0, ecoArretT = 0;
  const char* raison = "actif";
  bool actif = false;

  if (!EMISSION_AUTORISEE) raison = "bloque";
  else if (!thRemplacement) raison = "inactif";
  else if (teleVue != 0 && millis() - teleVue < 1800000UL) raison = "tele";
  else {
    if ((!deriereValide || !derniere.complet) && millis() - dernierEssaiBase > 5000) {
      dernierEssaiBase = millis();
      baseDepuisPi();
    }
    if (!deriereValide || !derniere.complet) raison = "attente";
    else {
      int md = mcz_valeur(derniere.x, CH_MODE);
      if (md == 2) raison = "arret";
      else if (md != 6 && md != 4) raison = "pas_auto";     // AUTO (6) et ECO (4)
      else actif = true;
    }
  }

  if (!actif) {
    etaitActif = false; aEnvoyer = false;
  } else {
    if (!etaitActif) {
      etaitActif = true;
      puissance = mcz_valeur(derniere.x, CH_FLAMME);
      ecoVeille = (mcz_valeur(derniere.x, CH_MODE) == 4 && puissance == 0);   // poele deja en veille ECO
      ecoArretT = millis(); ecoTenue = 0;
      if (puissance < 1 && !ecoVeille) puissance = 1;
      if (puissance > 5) puissance = 5;
      prochain = millis() + 5000;
      dernierEnvoi = millis();
      Serial.print("[thermo] ACTIF, puissance de depart "); Serial.println(puissance);
    }
    static bool nuitPrec = false;
    bool nuit = thNuit;
    if (nuit != nuitPrec) {
      nuitPrec = nuit;
      prochain = millis() + 60000UL;
      if (nuit) { puissance = 1; }
      else {                                   // sortie du mode nuit : on repart de la puissance remise par l'app
        puissance = mcz_valeur(derniere.x, CH_FLAMME);
        if (puissance < 1) puissance = 1;
        if (puissance > 5) puissance = 5;
      }
    }
    bool capteurOk = !isnan(tempCourante) && tempQuand != 0 && millis() - tempQuand < 600000UL;
    bool eco = (mcz_valeur(derniere.x, CH_MODE) == 4);
    if (eco != ecoPrec) {
      ecoPrec = eco; ecoTenue = 0;
      if (!eco && ecoVeille) { ecoVeille = false; puissance = 1; aEnvoyer = true; prochain = millis() + 60000UL; }   // ECO -> AUTO pendant la veille
      jrn("thermostat : mode %s", eco ? "ECO" : "AUTO");
    }
    if (eco && !nuit && capteurOk) {
      int t = (int)floorf(tempCourante);
      if (!ecoVeille) {
        if (!(thConsigne > (float)t)) { if (ecoTenue == 0) ecoTenue = millis(); } else ecoTenue = 0;
        if (ecoTenue != 0 && millis() - ecoTenue >= 1200000UL) {          // 20 min a la consigne : arret
          ecoVeille = true; puissance = 0; aEnvoyer = true; ecoArretT = millis();
          jrn("ECO : arret (20 min a la consigne, temp %.1f, consigne %.1f)", (float)tempCourante, (float)thConsigne);
        }
      } else if (millis() - ecoArretT >= 300000UL && (float)t <= thConsigne - 2.0f) {   // consigne -2 : rallumage
        ecoVeille = false; ecoTenue = 0; puissance = 3; aEnvoyer = true; prochain = millis() + 60000UL;
        jrn("ECO : rallumage (temp %.1f, consigne %.1f)", (float)tempCourante, (float)thConsigne);
      }
    }
    if (nuit) { raison = "nuit"; puissance = 1; }
    else if (ecoVeille) raison = "eco_veille";
    else if (!capteurOk) raison = "capteur";
    else if ((long)(millis() - prochain) >= 0) {
      prochain = millis() + 60000UL;
      int t = (int)floorf(tempCourante);                 // la virgule est ignoree
      int cible = puissance + ((thConsigne > (float)t) ? 1 : -1);
      if (cible < 1) cible = 1;
      if (cible > 5) cible = 5;
      Serial.print("[thermo] temp "); Serial.print(tempCourante, 1); Serial.print(" (retenue "); Serial.print(t);
      Serial.print(")  consigne "); Serial.print(thConsigne, 1); Serial.print("  puissance "); Serial.print(puissance);
      Serial.print(" -> "); Serial.println(cible);
      if (cible != puissance) { puissance = cible; aEnvoyer = true; }
    }
    if (!ecoVeille && millis() - dernierEnvoi >= 1200000UL) aEnvoyer = true;       // renvoi toutes les 20 min (pas pendant la veille ECO)
    if (aEnvoyer && (derniereEmission == 0 || millis() - derniereEmission >= 3000)) {
      int x2[18], y2[18];
      if (mcz_changer(derniere.x, derniere.y, CH_FLAMME, puissance, x2, y2)) {
        mcz_drapeau(x2, y2, 0);                                      // envoi automatique : le poele ne bipe pas
        char mots[7][13];
        mcz_mots_depuis(x2, y2, mots);
        static char flux[260];
        static uint16_t imp[900];
        int nu = mcz_unites(mots, flux);
        int ni = mcz_durees(flux, nu, 5, imp, 900);
        Serial.print("[thermo] EMISSION puissance "); Serial.println(puissance);
        jrn("thermostat : emission puissance %d (temp %.1f, consigne %.1f)", puissance, (float)tempCourante, (float)thConsigne);
        emettreDurees(imp, ni);
        derniereEmission = millis();
        for (int i = 0; i < 18; i++) { derniere.x[i] = x2[i]; derniere.y[i] = y2[i]; }
      }
      dernierEnvoi = millis();
      aEnvoyer = false;
    }
  }
  static char raisonPrec[16] = "";
  if (strcmp(raisonPrec, raison) != 0) { jrn("thermostat : %s", raison); strncpy(raisonPrec, raison, 15); raisonPrec[15] = 0; }
  portENTER_CRITICAL(&verrou);
  thActif = actif; thPuissance = actif ? puissance : 0;
  strncpy(thRaison, raison, 15); thRaison[15] = 0;
  portEXIT_CRITICAL(&verrou);
}

// ---------- Echanges avec le Raspberry Pi (tache sur le coeur 0) ----------
// Dernier echec d'appel au Pi (pour comprendre les coupures)
volatile int piDernierCode = 0; volatile unsigned long piDerniereDuree = 0; char piDerniereAction[12] = "";
// ---- CHRONO reseau (diagnostic des coupures) : mesure chaque appel au Pi, et ce qui se passe quand il echoue ----
unsigned long chPiMs = 0;                      // temps total passe dans appelPi (pour reperer un blocage hors appel)
unsigned long chAppels = 0, chEchecs = 0, chMax = 0, chBlocMax = 0, chLoopMax = 0;
int chRssiMin = 0;
void chronoEchec(const char* action, int code, unsigned long dur) {
  static unsigned long dernierLog = 0;
  if (millis() - dernierLog < 2000) return;    // pas plus d'une ligne toutes les 2 s
  dernierLog = millis();
  int rssi = (WiFi.status() == WL_CONNECTED) ? (int)WiFi.RSSI() : 0;
  // Test direct : la connexion TCP vers le Pi (port 80) fonctionne-t-elle MAINTENANT ?
  WiFiClient c; unsigned long t1 = millis();
  bool tcp = c.connect(piAdresse.c_str(), 80, 1500);
  unsigned long tc = millis() - t1; c.stop();
  jrn("CH echec %s %d en %lu ms, wifi %d dBm st%d, tcp %s %lu ms, mem %u", action, code, dur, rssi, (int)WiFi.status(), tcp ? "ok" : "KO", tc, (unsigned)ESP.getFreeHeap());
}
String appelPiUnique(const char* action, const String& json, bool& ok, int delaiMs) {
  unsigned long tDebut = millis();
  HTTPClient http;
  String url = String("http://") + piAdresse + "/poele-mcz/esp.php?action=" + action;
  http.setConnectTimeout(delaiMs);
  http.setTimeout(delaiMs);
  ok = false;
  if (!http.begin(url)) return "";
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Token", PI_JETON);
  unsigned long t0 = millis();
  int code = http.POST(json);
  String corps = (code == 200) ? http.getString() : "";
  http.end();
  ok = (code == 200);
  unsigned long dur = millis() - t0;
  chAppels++; if (dur > chMax) chMax = dur;
  { int r = (WiFi.status() == WL_CONNECTED) ? (int)WiFi.RSSI() : 0; if (r != 0 && (chRssiMin == 0 || r < chRssiMin)) chRssiMin = r; }
  if (!ok) { chEchecs++; piDernierCode = code; piDerniereDuree = dur; strncpy(piDerniereAction, action, 11); piDerniereAction[11] = 0; chronoEchec(action, code, dur); }
  chPiMs += millis() - tDebut;
  return corps;
}
// Appel au Pi avec nouvel essai rapide : un paquet perdu coute 1,5 s au lieu de 3 s, puis on retente une fois.
// Pas de nouvel essai pour "commande", "fait" et "trame" : une reponse perdue ne doit pas faire rejouer ou perdre un ordre.
String appelPi(const char* action, const String& json, bool& ok) {
  bool sansRetour = (strcmp(action, "commande") == 0 || strcmp(action, "fait") == 0 || strcmp(action, "trame") == 0);
  if (sansRetour) return appelPiUnique(action, json, ok, strcmp(action, "commande") == 0 ? 1500 : 3000);
  String r = appelPiUnique(action, json, ok, 1500);
  if (!ok) r = appelPiUnique(action, json, ok, 3000);
  return r;
}

// Extrait la valeur d'une cle JSON simple : "cle":"texte" ou "cle":123
bool jsonValeur(const String& s, const char* cle, char* sortie, int max) {
  String motif = String("\"") + cle + "\":";
  int p = s.indexOf(motif);
  if (p < 0) return false;
  p += motif.length();
  bool guillemets = (s.charAt(p) == '"');
  if (guillemets) p++;
  int i = 0;
  while (p < (int)s.length() && i < max - 1) {
    char c = s.charAt(p);
    if (guillemets ? (c == '"') : (c == ',' || c == '}')) break;
    sortie[i++] = c; p++;
  }
  sortie[i] = 0;
  return i > 0;
}

// ---------- Capteur de temperature DHT22 (alimente par D26, donnee sur D25) ----------
const int PIN_DHT_ALIM = 26;   // sortie a 3,3 V qui alimente le capteur (il consomme moins de 2 mA)
const int PIN_DHT      = 25;   // fil de donnees (OUT)
volatile float mesT = 0, mesH = 0;
volatile bool mesNouveau = false;

static bool attendreNiveau(int niveau, uint32_t maxUs) {
  uint32_t t0 = micros();
  while (digitalRead(PIN_DHT) != niveau) { if (micros() - t0 > maxUs) return false; }
  return true;
}

// Une lecture brute du DHT22 (40 bits). Renvoie false si le capteur ne repond pas ou si la somme de controle est fausse.
bool lireDHT22(float& t, float& h) {
  uint8_t d[5] = {0, 0, 0, 0, 0};
  pinMode(PIN_DHT, OUTPUT); digitalWrite(PIN_DHT, LOW); delay(2);      // demande de mesure
  noInterrupts();
  pinMode(PIN_DHT, INPUT_PULLUP);
  bool ok = attendreNiveau(LOW, 200) && attendreNiveau(HIGH, 150) && attendreNiveau(LOW, 150);
  for (int i = 0; ok && i < 40; i++) {
    if (!attendreNiveau(HIGH, 100)) { ok = false; break; }
    uint32_t t0 = micros();
    if (!attendreNiveau(LOW, 150)) { ok = false; break; }
    d[i / 8] <<= 1;
    if (micros() - t0 > 50) d[i / 8] |= 1;                             // impulsion longue = 1
  }
  interrupts();
  if (!ok) return false;
  if (((d[0] + d[1] + d[2] + d[3]) & 0xFF) != d[4]) return false;
  h = ((d[0] << 8) | d[1]) / 10.0f;
  int brut = ((d[2] & 0x7F) << 8) | d[3];
  t = brut / 10.0f;
  if (d[2] & 0x80) t = -t;
  return true;
}

// Filtre : valeurs impossibles rejetees, saut brutal rejete, mediane des 5 dernieres mesures
float histT[5], histH[5];
int nHist = 0, posHist = 0, rejets = 0;
float derniereT = NAN;
unsigned long dernierDHT = 0, dernierEnvoiMes = 0;

static float mediane(const float* v, int n) {
  float c[5];
  for (int i = 0; i < n; i++) c[i] = v[i];
  for (int i = 1; i < n; i++) { float x = c[i]; int j = i - 1; while (j >= 0 && c[j] > x) { c[j + 1] = c[j]; j--; } c[j + 1] = x; }
  return c[n / 2];
}

void suiviTemperature() {
  if (millis() < 3000 || millis() - dernierDHT < 5000) return;        // le capteur a besoin de 2 s apres la mise sous tension
  dernierDHT = millis();
  float t, h;
  static int echecs = 0;
  if (!lireDHT22(t, h)) {
    Serial.println("[temp] lecture impossible (capteur muet ou somme fausse)");
    if (echecs == 0) jrn("capteur : premiere lecture ratee");
    if (++echecs >= 12) {
      jrn("capteur : muet depuis 12 essais, coupure d'alimentation");                    // environ 1 minute de silence : on coupe puis on remet l'alimentation du capteur
      Serial.println("[temp] capteur redemarre (coupure d'alimentation)");
      digitalWrite(PIN_DHT_ALIM, LOW); delay(1500);
      digitalWrite(PIN_DHT_ALIM, HIGH); delay(2500);
      echecs = 0;
    }
    return;
  }
  if (echecs > 0) jrn("capteur : lecture retablie apres %d echecs", echecs);
  echecs = 0;
  if (t < 0 || t > 45 || h < 0 || h > 100) { rejets++; jrn("capteur : valeur rejetee (hors limites) %.1f", t); Serial.print("[temp] valeur rejetee (hors limites) : "); Serial.println(t); return; }
  if (!isnan(derniereT) && fabs(t - derniereT) > 3.0f && rejets < 4) {
    rejets++; Serial.print("[temp] saut brutal rejete : "); Serial.println(t); return;
  }
  rejets = 0; derniereT = t;
  histT[posHist] = t; histH[posHist] = h;
  posHist = (posHist + 1) % 5; if (nHist < 5) nHist++;
  if (nHist >= 3) { tempCourante = mediane(histT, nHist); tempQuand = millis(); }
  Serial.print("[temp] "); Serial.print(t, 1); Serial.print(" C, "); Serial.print(h, 1); Serial.println(" %");
  if (nHist >= 3 && millis() - dernierEnvoiMes > 30000) {
    dernierEnvoiMes = millis();
    portENTER_CRITICAL(&verrou);
    mesT = mediane(histT, nHist); mesH = mediane(histH, nHist); mesNouveau = true;
    portEXIT_CRITICAL(&verrou);
  }
}

// Mise a jour par le Wi-Fi (OTA) : protegee par le jeton, meme reseau local seulement
void tacheOTA(void*) {
  bool demarre = false;
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      if (!demarre) {
        ArduinoOTA.setHostname("poele-esp32");
        ArduinoOTA.setPassword(PI_JETON);
        ArduinoOTA.begin();
        demarre = true;
        Serial.println("OTA prete (mise a jour par Wi-Fi)");
      }
      ArduinoOTA.handle();
    }
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}

void tacheReseau(void*) {
  unsigned long dernierPing = 0, dernierEssaiWifi = 0, dernierSondage = 0;
  unsigned long chDebutIt = 0, chPiAvant = 0, chDernierBilan = millis();
  for (;;) {
    // CHRONO : cette tache est-elle restee bloquee (hors appels au Pi) ? et bilan toutes les 5 minutes
    { unsigned long maint = millis();
      if (chDebutIt) {
        unsigned long total = maint - chDebutIt, horsPi = total - (chPiMs - chPiAvant);
        if (horsPi > chBlocMax) chBlocMax = horsPi;
        if (horsPi > 1500 && !apActif) jrn("CH tache reseau bloquee %lu ms (hors appel au Pi)", horsPi);
      }
      chDebutIt = maint; chPiAvant = chPiMs;
      if (maint - chDernierBilan >= 300000UL) {
        chDernierBilan = maint;
        jrn("CH bilan 5 min : %lu appels, %lu echecs, max %lu ms, blocage %lu ms, loop %lu ms, wifi min %d", chAppels, chEchecs, chMax, chBlocMax, chLoopMax, chRssiMin);
        chAppels = chEchecs = chMax = chBlocMax = chLoopMax = 0; chRssiMin = 0;
      }
    }
    if (apActif) { espEnLigne = false; vTaskDelay(500 / portTICK_PERIOD_MS); continue; }   // mode secours : on ne cherche plus la box
    if (WiFi.status() == WL_CONNECTED) dejaConnecte = true;
    { static int etatW = -1; int nowW = (WiFi.status() == WL_CONNECTED) ? 1 : 0;
      if (nowW != etatW) { if (nowW) jrn("wifi connecte, signal %d dBm", (int)WiFi.RSSI()); else if (etatW != -1) jrn("wifi perdu"); etatW = nowW; } }
    if (WiFi.status() != WL_CONNECTED) {
      espEnLigne = false;
      if (!dejaConnecte && millis() > 30000UL) { demarrerAP(); continue; }   // box introuvable au demarrage : Wi-Fi de secours
      if (millis() - dernierEssaiWifi > 10000 || dernierEssaiWifi == 0) {
        dernierEssaiWifi = millis();
        WiFi.disconnect();
        WiFi.begin(wifiNom.c_str(), wifiMdp.c_str());
        Serial.println("Wifi : connexion en cours...");
      }
      vTaskDelay(500 / portTICK_PERIOD_MS);
      continue;
    }
    bool ok;

    // 1) Compte rendu d'une commande executee
    Resultat r; bool aRendre = false;
    portENTER_CRITICAL(&verrou);
    if (resultat.present) { r = resultat; resultat.present = false; aRendre = true; }
    portEXIT_CRITICAL(&verrou);
    if (aRendre && r.id >= 0) {
      appelPi("fait", String("{\"id\":") + r.id + ",\"statut\":\"" + r.statut + "\"}", ok);
    }

    // 2) Une trame decodee est-elle a envoyer ?
    EtatPartage copie; bool aEnvoyer = false;
    portENTER_CRITICAL(&verrou);
    if (partage.nouveau) { copie = partage; partage.nouveau = false; aEnvoyer = true; }
    portEXIT_CRITICAL(&verrou);
    if (aEnvoyer) {
      String json = String("{\"marche\":") + (copie.marche ? "true" : "false")
                  + ",\"mode\":\"" + copie.mode + "\""
                  + ",\"flamme\":" + copie.flamme
                  + ",\"vent1\":" + copie.fan1
                  + ",\"vent2\":" + copie.fan2
                  + ",\"vent3\":" + copie.fan3 + "}";
      appelPi("trame", json, ok);
      espEnLigne = ok;
      Serial.println(ok ? "  -> etat envoye au Pi" : "  -> ECHEC d'envoi au Pi");
      dernierPing = millis();
    } else if (millis() - dernierPing > 10000) {
      dernierPing = millis();
      appelPi("ping", "{}", ok);
      espEnLigne = ok;
    }

    // 2 bis) Tant qu'on n'a pas entendu la telecommande, on recupere l'etat connu du Pi
    static unsigned long dernierEtatPi = 0;
    if (millis() - dernierEtatPi > (deriereValide ? 15000UL : 5000UL)) {
      dernierEtatPi = millis();
      String rep = appelPi("etat", "{}", ok);
      char cs[12], mf[16];
      if (ok && jsonValeur(rep, "consigne", cs, 12) && jsonValeur(rep, "mode_fonct", mf, 16)) {
        float cons = atof(cs);
        if (cons < 5.0f) cons = 5.0f;
        if (cons > 30.0f) cons = 30.0f;
        bool rempl = (strcmp(mf, "remplacement") == 0);
        char nu[4];
        if (jsonValeur(rep, "nuit", nu, 4)) thNuit = (atoi(nu) != 0);
        if (cons != thConsigne || rempl != thRemplacement) {
          thConsigne = cons; thRemplacement = rempl;
          prefs.putFloat("cons", cons); prefs.putBool("rempl", rempl);
          jrn("reglage recu : consigne %.1f, %s", cons, rempl ? "remplacement" : "avec telecommande");
          Serial.print("[thermo] reglage recu : consigne "); Serial.print(cons, 1); Serial.println(rempl ? ", mode remplacement" : ", mode avec telecommande");
        }
      }
      char po[12], mo[8], fl[4], a[4], b[4], c[4];
      if (ok && jsonValeur(rep, "poele", po, 12) && jsonValeur(rep, "mode", mo, 8) && jsonValeur(rep, "flamme", fl, 4)
          && jsonValeur(rep, "vent1", a, 4) && jsonValeur(rep, "vent2", b, 4) && jsonValeur(rep, "vent3", c, 4)) {
        portENTER_CRITICAL(&verrou);
        etatPi.marche = (strcmp(po, "eteint") != 0);
        strncpy(etatPi.mode, mo, 4); etatPi.mode[4] = 0;
        etatPi.flamme = atoi(fl); etatPi.fan1 = atoi(a); etatPi.fan2 = atoi(b); etatPi.fan3 = atoi(c);
        etatPi.valide = true;
        portEXIT_CRITICAL(&verrou);
      }
    }

    // 2 ter) Une mesure de temperature est-elle a envoyer ?
    if (mesNouveau) {
      float t, h;
      portENTER_CRITICAL(&verrou); t = mesT; h = mesH; mesNouveau = false; portEXIT_CRITICAL(&verrou);
      appelPi("mesure", String("{\"t\":") + String(t, 1) + ",\"h\":" + String(h, 1) + "}", ok);
      Serial.println(ok ? "  -> temperature envoyee au Pi" : "  -> ECHEC d'envoi de la temperature");
    }

    // 2 quater) Compte rendu du thermostat (seulement en mode remplacement)
    static unsigned long dernierThermo = 0;
    static int puPrec = -1;
    int puMaintenant; portENTER_CRITICAL(&verrou); puMaintenant = thPuissance; portEXIT_CRITICAL(&verrou);
    // compte rendu toutes les 30 s, et tout de suite (en 2 s) quand la puissance change
    if (thRemplacement && (millis() - dernierThermo > 30000 || (puMaintenant != puPrec && millis() - dernierThermo > 2000))) {
      dernierThermo = millis();
      char r[16]; int pu; bool ac;
      portENTER_CRITICAL(&verrou); strcpy(r, thRaison); pu = thPuissance; ac = thActif; portEXIT_CRITICAL(&verrou);
      puPrec = pu;
      appelPi("thermo", String("{\"actif\":") + (ac ? "true" : "false") + ",\"puissance\":" + pu + ",\"raison\":\"" + r + "\"}", ok);
    }

    // 2 quinquies) Journal : envoi des evenements au Pi
    static unsigned long dernierJrn = 0;
    static int piEtat = -1;
    if ((int)espEnLigne != piEtat) { if (piEtat != -1) { if (espEnLigne) jrn("Pi de nouveau joignable"); else jrn("Pi injoignable : %s code %d (%lu ms), wifi %d dBm, memoire %u", piDerniereAction, (int)piDernierCode, (unsigned long)piDerniereDuree, WiFi.status() == WL_CONNECTED ? (int)WiFi.RSSI() : 0, (unsigned)ESP.getFreeHeap()); } piEtat = (int)espEnLigne; }
    if (jN > 0 && millis() - dernierJrn > 5000) {
      dernierJrn = millis();
      static JLigne copie[JMAX];
      int n, dep;
      portENTER_CRITICAL(&verrou);
      n = jN; dep = (jTete - n + JMAX) % JMAX;
      for (int i = 0; i < n; i++) copie[i] = jBuf[(dep + i) % JMAX];
      portEXIT_CRITICAL(&verrou);
      String js = "{\"l\":[";
      for (int i = 0; i < n; i++) {
        if (i) js += ",";
        js += String("{\"a\":") + String((millis() - copie[i].ms) / 1000) + ",\"m\":\"" + copie[i].m + "\"}";
      }
      js += "]}";
      appelPi("journal", js, ok);
      if (ok) { portENTER_CRITICAL(&verrou); jN -= n; if (jN < 0) jN = 0; portEXIT_CRITICAL(&verrou); }
    }

    // 3) Y a-t-il une commande a executer ? (seulement si l'emission est autorisee)
    if (EMISSION_AUTORISEE && !cmdAttente.presente && millis() - dernierSondage > 1000) {
      dernierSondage = millis();
      String rep = appelPi("commande", "{}", ok);
      char id[12], type[10], val[6];
      if (ok && jsonValeur(rep, "id", id, 12) && jsonValeur(rep, "type", type, 10) && jsonValeur(rep, "valeur", val, 6)) {
        portENTER_CRITICAL(&verrou);
        cmdAttente.id = atoi(id);
        strcpy(cmdAttente.type, type);
        strcpy(cmdAttente.valeur, val);
        cmdAttente.presente = true;
        portEXIT_CRITICAL(&verrou);
        Serial.print("Commande recue de la page : "); Serial.print(type); Serial.print(" = "); Serial.println(val);
      }
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

// ---------- Traitement d'une capture ----------
int nbOk = 0, nbEchec = 0;

// Renvoi de l'etat fusionne (app + changement fait sur la telecommande)
int fusionX[18], fusionY[18];
bool fusionAFaire = false;
unsigned long fusionQuand = 0;

void emettreFusion() {
  fusionAFaire = false;
  char mots[7][13];
  mcz_mots_depuis(fusionX, fusionY, mots);
  static char flux[260];
  static uint16_t imp[900];
  int nu = mcz_unites(mots, flux);
  int ni = mcz_durees(flux, nu, 5, imp, 900);
  Serial.println("  EMISSION : etat fusionne (app + telecommande)");
  jrn("emission fusion telecommande + application");
  emettreDurees(imp, ni);
  derniereEmission = millis();
}

void traiterCapture() {
  static TrameMCZ t;
  int n = nbBords;
  if (mcz_decoder(durees, n, t)) {
    nbOk++;
    teleVue = millis();
    { static unsigned long dj = 0; if (dj == 0 || millis() - dj > 300000UL) { dj = millis(); jrn("vraie telecommande entendue"); } }
    Serial.print("TRAME  mode="); Serial.print(t.mode[0] ? t.mode : "(arret)");
    Serial.print(t.marche ? "  allume" : "  eteint");
    Serial.print("  flamme="); Serial.print(t.flamme);
    Serial.print("  vent1="); Serial.print(t.fan1 == 6 ? "AUTO" : String(t.fan1));
    Serial.print("  vent2="); Serial.print(t.fan2 == 6 ? "AUTO" : String(t.fan2));
    Serial.print("  vent3="); Serial.print(t.fan3 == 6 ? "AUTO" : String(t.fan3));
    Serial.print(t.complet ? "  [complete]" : "  [incomplete]");
    Serial.print("  | M4="); Serial.print(t.mots[3]);
    Serial.print(" M5="); Serial.println(t.mots[4]);
    {   // Trame brute (7 blocs) au journal, seulement quand elle change : sert a capturer NO AIR, SLEEP, ECO
      static char prev[7][13]; static bool pv = false;
      bool diff = !pv;
      for (int m = 0; m < 7; m++) if (strcmp(prev[m], t.mots[m]) != 0) diff = true;
      if (diff) {
        jrn("RAW %s %s %s %s %s %s %s", t.mots[0], t.mots[1], t.mots[2], t.mots[3], t.mots[4], t.mots[5], t.mots[6]);
        for (int m = 0; m < 7; m++) { strncpy(prev[m], t.mots[m], 12); prev[m][12] = 0; }
        pv = true;
      }
    }

    // Etat a publier sur la page (par defaut : ce que la telecommande vient d'envoyer)
    bool pMarche = t.marche; char pMode[5]; strcpy(pMode, t.mode);
    int pFlamme = t.flamme, pF1 = t.fan1, pF2 = t.fan2, pF3 = t.fan3;

    if (t.complet) {
      // La telecommande envoie TOUT son etat. On repere ce qu'elle a vraiment change
      // (par rapport a sa trame precedente), on l'applique a l'etat de l'app,
      // et si ca differe de ce que le poele vient de recevoir, on renvoie l'etat fusionne.
      static int memTele[5];
      static bool memTeleValide = false;
      int now[5];
      for (int c = 0; c < 5; c++) now[c] = mcz_valeur(t.x, (ChampMCZ)c);

      bool fusion = false;
      if (EMISSION_AUTORISEE && memTeleValide && deriereValide && derniere.complet) {
        int vx[18], vy[18];
        for (int i = 0; i < 18; i++) { vx[i] = derniere.x[i]; vy[i] = derniere.y[i]; }
        for (int c = 0; c < 5; c++) {
          if (now[c] != memTele[c]) {
            int x2[18], y2[18];
            if (mcz_changer(vx, vy, (ChampMCZ)c, now[c], x2, y2))
              for (int i = 0; i < 18; i++) { vx[i] = x2[i]; vy[i] = y2[i]; }
          }
        }
        bool diff = false;
        for (int c = 0; c < 5; c++) if (mcz_valeur(vx, (ChampMCZ)c) != now[c]) diff = true;
        if (diff) {
          fusion = true;
          mcz_drapeau(vx, vy, t.x[9]);        // meme drapeau que la telecommande : un renvoi automatique reste silencieux
          for (int i = 0; i < 18; i++) { derniere.x[i] = vx[i]; derniere.y[i] = vy[i]; fusionX[i] = vx[i]; fusionY[i] = vy[i]; }
          fusionQuand = millis() + 800;      // on attend la fin des repetitions de la telecommande
          fusionAFaire = true;
          int md = mcz_valeur(vx, CH_MODE);
          pMarche = (md != 2);
          strcpy(pMode, md == 2 ? "" : (md == 6 ? "AUTO" : (md == 4 ? "ECO" : "MAN")));
          pFlamme = mcz_valeur(vx, CH_FLAMME);
          pF1 = mcz_valeur(vx, CH_FAN1); pF2 = mcz_valeur(vx, CH_FAN2); pF3 = mcz_valeur(vx, CH_FAN3);
          if (md != 2) dernierModeAllume = md;
          Serial.println("  Telecommande != etat de l'app : fusion programmee (changement garde, reste de l'app).");
        }
      }
      for (int c = 0; c < 5; c++) memTele[c] = now[c];
      memTeleValide = true;
      if (!fusion) {
        derniere = t; deriereValide = true;
        if (t.chiffreMode != 2) dernierModeAllume = t.chiffreMode;
      }
    }
    portENTER_CRITICAL(&verrou);
    partage.marche = pMarche;
    strcpy(partage.mode, pMode);
    partage.flamme = pFlamme;
    partage.fan1 = pF1; partage.fan2 = pF2; partage.fan3 = pF3;
    partage.nouveau = true;
    portEXIT_CRITICAL(&verrou);
  } else {
    nbEchec++;
    { static unsigned long dil = 0; if (dil == 0 || millis() - dil > 5000) { dil = millis(); jrn("signal illisible : %d changements", n); } }
    Serial.print("Capture illisible ("); Serial.print(n);
    Serial.println(" changements) : ignoree.");
  }
}

// ---------- Autotest au demarrage : AUCUNE emission, calcul seulement ----------
void autotest() {
  int justes = 0;
  for (int k = 0; k < NB_VECTEURS; k++) {
    const VecteurTest& v = VECTEURS[k];
    // bits de donnees (domaine cumule) de la trame A
    int x[18], y[18], xb[12];
    for (int m = 3; m < 7; m++) {
      mcz_cumulBrut(v.a[m], xb);
      for (int i = 0; i < 9; i++) {
        if (m == 3) x[i] = xb[i]; else if (m == 4) x[9 + i] = xb[i];
        else if (m == 5) y[i] = xb[i]; else y[9 + i] = xb[i];
      }
    }
    int x2[18], y2[18];
    char mots[7][13];
    bool ok = mcz_changer(x, y, (ChampMCZ)v.champ, v.valeur, x2, y2);
    if (ok) {
      mcz_mots_depuis(x2, y2, mots);
      for (int m = 0; m < 7; m++) if (strcmp(mots[m], v.b[m]) != 0) ok = false;
    }
    if (ok) justes++; else { Serial.print("  autotest : trame "); Serial.print(k); Serial.println(" differente !"); }
  }
  Serial.print("AUTOTEST generateur : "); Serial.print(justes); Serial.print("/"); Serial.println(NB_VECTEURS);

  // Aller-retour : trame -> impulsions -> decodeur de reception
  char mots[7][13];
  const VecteurTest& v = VECTEURS[0];
  for (int m = 0; m < 7; m++) strcpy(mots[m], v.b[m]);
  static char flux[260];
  static uint16_t imp[900];
  int nu = mcz_unites(mots, flux);
  int ni = mcz_durees(flux, nu, 5, imp, 900);
  static TrameMCZ t;
  bool lu = mcz_decoder(imp, ni, t) && t.complet;
  for (int m = 0; m < 7 && lu; m++) if (strcmp(t.mots[m], v.b[m]) != 0) lu = false;
  Serial.print("AUTOTEST aller-retour impulsions : "); Serial.println(lu ? "OK" : "ECHEC");
  Serial.print("  (unites "); Serial.print(nu); Serial.print(", durees "); Serial.print(ni); Serial.println(")");
}

#include "secours.h"      // mode secours : page web, Wi-Fi de secours, bouton BOOT, LED

void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(PIN_DHT_ALIM, OUTPUT); digitalWrite(PIN_DHT_ALIM, HIGH);   // alimente le capteur de temperature
  pinMode(PIN_DHT, INPUT_PULLUP);
  prefs.begin("thermo", false);
  thConsigne = prefs.getFloat("cons", 20.0f);
  thRemplacement = prefs.getBool("rempl", false);
  chargerReseau();
  chargerEtatSauve();
  Serial.println("Etape 9 : ecoute + emission + temperature + thermostat");
  jrn("DEMARRAGE, cause : %s", causeDemarrage());
  Serial.println(EMISSION_AUTORISEE ? "EMISSION : AUTORISEE" : "EMISSION : BLOQUEE (rien ne sera envoye au poele)");
  autotest();

  pinMode(PIN_CSN, OUTPUT);
  digitalWrite(PIN_CSN, HIGH);
  pinMode(PIN_GDO0, INPUT);
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CSN);

  resetCC1101();
  commande(CMD_IDLE);
  regleFrequence(FREQ_MHZ);
  configReception();
  attachInterrupt(digitalPinToInterrupt(PIN_GDO0), surFront, CHANGE);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);          // pas de mise en veille du Wi-Fi : evite les reponses retardees (coupures « Pi injoignable »)
  WiFi.setAutoReconnect(true);
  xTaskCreatePinnedToCore(tacheReseau, "reseau", 8192, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(tacheOTA, "ota", 6144, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(tacheWeb, "web", 10240, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(tacheVoyant, "voyant", 3072, NULL, 1, NULL, 0);
  Serial.println("En ecoute.");
}

unsigned long dernierBilan = 0;

void loop() {
  // CHRONO : la boucle principale est-elle restee bloquee ? (une emission radio dure normalement moins d'une seconde)
  { static unsigned long dernLoop = 0; unsigned long n = millis();
    if (dernLoop) { unsigned long g = n - dernLoop; if (g > chLoopMax) chLoopMax = g; if (g > 1500) jrn("CH loop() bloquee %lu ms", g); }
    dernLoop = n; }
  // 1) Une commande de la page est-elle a executer ?
  if (cmdAttente.presente) {
    CommandeAttente c;
    portENTER_CRITICAL(&verrou);
    c = cmdAttente;
    portEXIT_CRITICAL(&verrou);
    const char* st = executerCommande(c);
    portENTER_CRITICAL(&verrou);
    if (c.id >= 0) { resultat.present = true; resultat.id = c.id; strcpy(resultat.statut, st); }
    else { strncpy(secStatut, st, 11); secStatut[11] = 0; secFait = true; }     // ordre du mode secours
    cmdAttente.presente = false;
    portEXIT_CRITICAL(&verrou);
    Serial.print("  -> resultat : "); Serial.println(st);
    jrn("commande %s %s : %s", c.type, c.valeur, st);
  }

  // 1b) Renvoi de l'etat fusionne apres une action sur la telecommande
  if (fusionAFaire && (long)(millis() - fusionQuand) >= 0) emettreFusion();

  // 2) Ecoute de la telecommande
  if (lireRSSI() > SEUIL_RSSI) {
    nbBords = 0;
    premierFront = true;
    enregistre = true;
    unsigned long dernierFort = millis();
    int pic = -128;
    while (millis() - dernierFort < 20 && nbBords < MAX_BORDS) {
      int r = lireRSSI();
      if (r > pic) pic = r;
      if (r > SEUIL_RSSI) dernierFort = millis();
      delayMicroseconds(500);
    }
    enregistre = false;
    Serial.print("[signal] pic RSSI="); Serial.print(pic); Serial.print(" dBm, bords="); Serial.println(nbBords);
    traiterCapture();
    delay(100);
  } else {
    suiviTemperature();       // jamais pendant une capture de la telecommande
    thermostat();
    nuitVentilos();
  }

  if (millis() - dernierBilan > 60000) {
    dernierBilan = millis();
    Serial.print("[bilan] wifi="); Serial.print(WiFi.status() == WL_CONNECTED ? "ok" : "non");
    Serial.print("  Pi="); Serial.print(espEnLigne ? "ok" : "non");
    Serial.print("  trames lues="); Serial.print(nbOk);
    Serial.print("  illisibles="); Serial.println(nbEchec);
  }
  sauvegardeEtat();
  delay(1);
}
