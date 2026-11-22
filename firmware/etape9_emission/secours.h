// =====================================================================
// MODE SECOURS : l'ESP32 pilote le poele seul, sans le Pi ni la box.
//  - une page web sur l'ESP32 (http://adresse-de-l-esp32/) : marche/arret, mode, consigne,
//    flamme, ventilos, lune ;
//  - un Wi-Fi propre "Poele-MCZ" (mot de passe AP_MDP, dans secrets.h) : appui de 3 s sur BOOT,
//    ou tout seul si la box reste introuvable 30 s au demarrage ;
//  - LED bleue (GPIO2) : eteinte = normal ; un eclat toutes les 3 s = Pi injoignable ;
//    clignote vite = recherche du Wi-Fi ; clignote lentement = mode secours actif ;
//  - reglages Wi-Fi et adresse du Pi gardes dans la memoire de l'ESP32 (plus dans le code).
// Les reglages ne se changent que par le Wi-Fi de secours (jamais depuis le reseau de la maison).
// Les ordres exigent l'en-tete X-Secours : un site web quelconque ne peut pas les envoyer.
// =====================================================================
#pragma once
#include <esp_http_server.h>      // serveur web d'ESP-IDF : plusieurs connexions a la fois, les connexions muettes ne bloquent rien
#include <lwip/sockets.h>
#include "secours_page.h"

#ifndef AP_MDP
#error "AP_MDP manque dans secrets.h (mot de passe du Wi-Fi de secours, 8 caracteres ou plus)"
#endif
#define AP_SSID "Poele-MCZ"

const int PIN_BOUTON = 0;     // bouton BOOT de la carte
const int PIN_LED    = 2;     // LED bleue de la carte (si elle existe)

// Petite enveloppe autour du serveur d'ESP-IDF : les fonctions de page ci-dessous s'ecrivent comme avec WebServer
// (serveur.arg(), serveur.hasHeader(), serveur.send(), ...).
class MiniServeur {
 public:
  httpd_req_t* req = nullptr;
  String corps;                                  // corps de la requete POST (formulaire)
  const char* enteteK[3]; const char* enteteV[3]; int nEntete = 0;

  void debut(httpd_req_t* r) {
    req = r; corps = ""; nEntete = 0;
    if (r->method == HTTP_POST && r->content_len > 0 && r->content_len <= 400) {
      char buf[401]; int recu = 0;
      while (recu < (int)r->content_len) {
        int n = httpd_req_recv(r, buf + recu, r->content_len - recu);
        if (n <= 0) break;
        recu += n;
      }
      buf[recu] = 0; corps = buf;
    }
  }
  bool hasHeader(const char* nom) { return httpd_req_get_hdr_value_len(req, nom) > 0; }
  static String decoder(const String& in) {
    String o; o.reserve(in.length());
    for (unsigned i = 0; i < in.length(); i++) {
      char c = in[i];
      if (c == '+') o += ' ';
      else if (c == '%' && i + 2 < in.length() + 0 && isxdigit((unsigned char)in[i + 1]) && isxdigit((unsigned char)in[i + 2])) {
        char h[3] = { in[i + 1], in[i + 2], 0 }; o += (char)strtol(h, nullptr, 16); i += 2;
      } else o += c;
    }
    return o;
  }
  String arg(const char* nom) {
    String cle = String(nom) + "=";
    int p = 0, L = corps.length();
    while (p <= L) {
      int e = corps.indexOf('&', p); if (e < 0) e = L;
      if (corps.startsWith(cle, p)) return decoder(corps.substring(p + cle.length(), e));
      p = e + 1;
    }
    return "";
  }
  IPAddress remoteIP() {
    struct sockaddr_storage a; socklen_t l = sizeof(a);
    int fd = httpd_req_to_sockfd(req);
    if (fd < 0 || getpeername(fd, (struct sockaddr*)&a, &l) != 0) return IPAddress(0, 0, 0, 0);
    if (a.ss_family == AF_INET) { uint32_t v = ((struct sockaddr_in*)&a)->sin_addr.s_addr; return IPAddress(v & 255, (v >> 8) & 255, (v >> 16) & 255, (v >> 24) & 255); }
    if (a.ss_family == AF_INET6) { const uint8_t* b = ((struct sockaddr_in6*)&a)->sin6_addr.s6_addr; if (b[10] == 0xff && b[11] == 0xff) return IPAddress(b[12], b[13], b[14], b[15]); }
    return IPAddress(0, 0, 0, 0);
  }
  void sendHeader(const char* k, const char* v) { if (nEntete < 3) { enteteK[nEntete] = k; enteteV[nEntete] = v; nEntete++; } }
  void envoyer(int code, const char* type, const char* data, size_t n) {
    httpd_resp_set_status(req, code == 200 ? "200 OK" : (code == 403 ? "403 Forbidden" : "404 Not Found"));
    httpd_resp_set_type(req, type);
    for (int i = 0; i < nEntete; i++) httpd_resp_set_hdr(req, enteteK[i], enteteV[i]);
    httpd_resp_send(req, data, n);
  }
  void send(int code, const char* type, const String& s) { envoyer(code, type, s.c_str(), s.length()); }
  void send_P(int code, const char* type, const char* p) { envoyer(code, type, p, strlen(p)); }
};
MiniServeur serveur;
static unsigned long apDepuis = 0, apDerniereStation = 0, redemarrerA = 0;
static volatile bool secFait = false;
static char secStatut[12] = "";
static int  nuitFlammeAvant = 3;

// ---------- Reglages gardes en memoire (Wi-Fi, adresse du Pi) ----------
void chargerReseau() {
  prefsRes.begin("reseau", false);
  wifiNom    = prefsRes.getString("ssid", WIFI_NOM);
  wifiMdp    = prefsRes.getString("mdp",  WIFI_MOT_DE_PASSE);
  piAdresse  = prefsRes.getString("pi",   PI_ADRESSE);
}

// ---------- Derniere trame connue gardee en flash : sert de base si on demarre sans Pi ni telecommande ----------
static Preferences prefsEtat;
static int  sauveX[18], sauveY[18];
static bool sauveValide = false;
static unsigned long derniereSauv = 0;

void chargerEtatSauve() {
  prefsEtat.begin("etat", false);
  uint8_t b[36];
  if (prefsEtat.getBytes("xy", b, 36) == 36) {
    for (int i = 0; i < 18; i++) { sauveX[i] = b[i] & 1; sauveY[i] = b[18 + i] & 1; }
    sauveValide = true;
    int m = prefsEtat.getInt("modeOn", 0);
    if (m == 0 || m == 4 || m == 6) dernierModeAllume = m;
  }
}
bool baseDepuisSauvegarde() {
  if (!sauveValide) return false;
  for (int i = 0; i < 18; i++) { derniere.x[i] = sauveX[i]; derniere.y[i] = sauveY[i]; }
  derniere.complet = true; deriereValide = true;
  Serial.println("  Trame de base reprise de la memoire de l'ESP32 (ni Pi ni telecommande).");
  jrn("base reprise de la memoire de l'ESP32");
  return true;
}
// Appelee depuis loop() : au plus une ecriture toutes les 10 minutes (usure de la memoire)
void sauvegardeEtat() {
  if (!deriereValide || !derniere.complet) return;
  if (millis() - derniereSauv < 600000UL && derniereSauv != 0) return;
  bool diff = !sauveValide;
  for (int i = 0; i < 18 && !diff; i++) if (sauveX[i] != (derniere.x[i] & 1) || sauveY[i] != (derniere.y[i] & 1)) diff = true;
  if (!diff) return;
  uint8_t b[36];
  for (int i = 0; i < 18; i++) { b[i] = derniere.x[i] & 1; b[18 + i] = derniere.y[i] & 1; sauveX[i] = b[i]; sauveY[i] = b[18 + i]; }
  prefsEtat.putBytes("xy", b, 36);
  prefsEtat.putInt("modeOn", dernierModeAllume);
  sauveValide = true; derniereSauv = millis();
}

// ---------- Wi-Fi de secours ----------
void demarrerAP() {
  if (apActif) return;
  apActif = true;
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_AP_STA);                 // STA gardee seulement pour chercher les reseaux a la demande
  WiFi.setSleep(false);
  WiFi.softAP(AP_SSID, AP_MDP, 1, 0, 4);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 2)
  // Portail captif (DHCP option 114) : le telephone ouvre tout seul la page http://192.168.4.1/ (Android 11+, iOS 14+)
  if (!WiFi.AP.enableDhcpCaptivePortal()) jrn("portail captif : echec");
#endif
  apDepuis = apDerniereStation = millis();
  jrn("MODE SECOURS : Wi-Fi %s actif", AP_SSID);
  Serial.println("MODE SECOURS : Wi-Fi " AP_SSID " actif, page sur http://192.168.4.1/");
}

// ---------- Page web ----------
static bool enteteOk() { return serveur.hasHeader("X-Secours"); }
static bool depuisSecours() {            // la requete vient bien de notre Wi-Fi de secours (192.168.4.x)
  IPAddress ip = serveur.remoteIP();
  return apActif && ip[0] == 192 && ip[1] == 168 && ip[2] == 4;
}
static void refuser() { serveur.send(403, "application/json", "{\"ok\":false,\"m\":\"Interdit\"}"); }
static void repondre(bool ok, const char* m = "") {
  String s = String("{\"ok\":") + (ok ? "true" : "false") + ",\"m\":\"" + m + "\"}";
  serveur.send(200, "application/json", s);
}
static String nettoyer(const String& in) {
  String o; o.reserve(in.length() + 4);
  for (unsigned i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '"' || c == '\\') o += '\''; else if ((unsigned char)c < 32) o += ' '; else o += c;
  }
  return o;
}
static const char* nomMode(int m) { return m == 6 ? "AUTO" : (m == 4 ? "ECO" : "MAN"); }

static void hPage() {
  serveur.sendHeader("Cache-Control", "no-store");
  serveur.send_P(200, "text/html; charset=utf-8", PAGE_SECOURS);
}

static void hEtat() {
  if (!enteteOk()) { refuser(); return; }
  int x[18], y[18]; bool base = false;
  portENTER_CRITICAL(&verrou);
  if (deriereValide && derniere.complet) { for (int i = 0; i < 18; i++) { x[i] = derniere.x[i]; y[i] = derniere.y[i]; } base = true; }
  portEXIT_CRITICAL(&verrou);
  if (!base && sauveValide) { for (int i = 0; i < 18; i++) { x[i] = sauveX[i]; y[i] = sauveY[i]; } base = true; }
  int mode = 2, fl = 0, f1 = 0, f2 = 0, f3 = 0;
  if (base) { mode = mcz_valeur(x, CH_MODE); fl = mcz_valeur(x, CH_FLAMME); f1 = mcz_valeur(x, CH_FAN1); f2 = mcz_valeur(x, CH_FAN2); f3 = mcz_valeur(x, CH_FAN3); }
  bool marche = base && mode != 2;
  const char* nm = nomMode(marche ? mode : dernierModeAllume);
  float t = tempCourante;
  char ligne[560];
  char tt[12], hh[12];
  if (isnan(t)) strcpy(tt, "null"); else snprintf(tt, sizeof(tt), "%.1f", t);
  if (isnan(t) || mesH <= 0) strcpy(hh, "null"); else snprintf(hh, sizeof(hh), "%.0f", (float)mesH);
  char raison[16]; bool tha; int thp;
  portENTER_CRITICAL(&verrou); strcpy(raison, thRaison); tha = thActif; thp = thPuissance; portEXIT_CRITICAL(&verrou);
  snprintf(ligne, sizeof(ligne),
    "{\"base\":%s,\"marche\":%s,\"mode\":\"%s\",\"flamme\":%d,\"vent1\":%d,\"vent2\":%d,\"vent3\":%d,"
    "\"consigne\":%.1f,\"temp\":%s,\"hum\":%s,\"nuit\":%s,\"remplacement\":%s,\"thActif\":%s,\"thPuissance\":%d,\"thRaison\":\"%s\","
    "\"pi\":%s,\"wifi\":%s,\"rssi\":%d,\"ap\":%s,\"piAdr\":\"%s\",\"ssid\":\"",
    base ? "true" : "false", marche ? "true" : "false", nm, fl, f1, f2, f3,
    (float)thConsigne, tt, hh, thNuit ? "true" : "false", thRemplacement ? "true" : "false", tha ? "true" : "false", thp, raison,
    espEnLigne ? "true" : "false", WiFi.status() == WL_CONNECTED ? "true" : "false",
    WiFi.status() == WL_CONNECTED ? (int)WiFi.RSSI() : 0, apActif ? "true" : "false", nettoyer(piAdresse).c_str());
  String s = String(ligne) + nettoyer(WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String("")) + "\"}";
  serveur.sendHeader("Cache-Control", "no-store");
  serveur.send(200, "application/json", s);
}

// Met un ordre dans la file de l'ESP32 et attend le verdict (au plus 5 s)
static bool envoyerOrdre(const char* type, const char* valeur, String& m) {
  if (!EMISSION_AUTORISEE) { m = "Emission bloquee dans le code."; return false; }
  bool pris = false;
  portENTER_CRITICAL(&verrou);
  if (!cmdAttente.presente) {
    cmdAttente.id = -1;                                  // -1 : ordre du mode secours (rien a rendre au Pi)
    strncpy(cmdAttente.type, type, 9); cmdAttente.type[9] = 0;
    strncpy(cmdAttente.valeur, valeur, 5); cmdAttente.valeur[5] = 0;
    secFait = false;
    cmdAttente.presente = true;
    pris = true;
  }
  portEXIT_CRITICAL(&verrou);
  if (!pris) { m = "L'ESP32 est occupe, recommencez."; return false; }
  for (int i = 0; i < 50 && !secFait; i++) vTaskDelay(100 / portTICK_PERIOD_MS);
  if (!secFait) { m = "Pas de verdict, verifiez le poele."; return false; }
  if (strcmp(secStatut, "envoyee") == 0) return true;
  m = "Refuse (ordre trop proche du precedent, ou etat du poele inconnu).";
  return false;
}

static void hCmd() {
  if (!enteteOk()) { refuser(); return; }
  String t = serveur.arg("t"), v = serveur.arg("v");
  bool bon = false;
  if (t == "marche") bon = (v == "on" || v == "off");
  else if (t == "mode") bon = (v == "MAN" || v == "AUTO" || v == "ECO");
  else if (t == "flamme") bon = (v.length() == 1 && v[0] >= '1' && v[0] <= '5');
  else if (t == "vent1" || t == "vent2" || t == "vent3") bon = (v.length() == 1 && v[0] >= '0' && v[0] <= '6');
  if (!bon) { repondre(false, "Ordre inconnu."); return; }
  String m;
  bool ok = envoyerOrdre(t.c_str(), v.c_str(), m);
  jrn("secours : %s %s", t.c_str(), v.c_str());
  repondre(ok, m.c_str());
}

static void hConsigne() {
  if (!enteteOk()) { refuser(); return; }
  float c = serveur.arg("v").toFloat();
  if (!(c >= 5.0f && c <= 30.0f)) { repondre(false, "Consigne hors limites."); return; }
  c = roundf(c * 2.0f) / 2.0f;
  thConsigne = c;
  prefs.putFloat("cons", c);
  jrn("secours : consigne %.1f", c);
  repondre(true);
}

static void hNuit() {
  if (!enteteOk()) { refuser(); return; }
  bool on = (serveur.arg("v") == "1");
  String m; bool ok;
  if (on) {
    int fl = 3; portENTER_CRITICAL(&verrou); if (deriereValide && derniere.complet) fl = mcz_valeur(derniere.x, CH_FLAMME); portEXIT_CRITICAL(&verrou);
    if (fl >= 1 && fl <= 5) nuitFlammeAvant = fl;
    thNuit = true;
    ok = envoyerOrdre("flamme", "1", m);
    if (!ok) thNuit = false;
  } else {
    thNuit = false;
    char f[2] = { (char)('0' + ((nuitFlammeAvant >= 1 && nuitFlammeAvant <= 5) ? nuitFlammeAvant : 3)), 0 };
    ok = envoyerOrdre("flamme", f, m);
  }
  jrn("secours : lune %s", on ? "activee" : "coupee");
  repondre(ok, m.c_str());
}

// ---- Reglages du reseau (seulement depuis le Wi-Fi de secours) ----
static void hReseaux() {
  if (!enteteOk()) { refuser(); return; }
  if (!depuisSecours()) { repondre(false, "Reglage possible seulement depuis le Wi-Fi de secours."); return; }
  int n = WiFi.scanNetworks(false, false);
  String noms[12]; int force[12]; bool ouvert[12]; int k = 0;
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i); if (s.length() == 0) continue;
    int r = WiFi.RSSI(i); int j;
    for (j = 0; j < k; j++) if (noms[j] == s) break;
    if (j < k) { if (r > force[j]) force[j] = r; continue; }
    if (k < 12) { noms[k] = s; force[k] = r; ouvert[k] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN); k++; }
  }
  WiFi.scanDelete();
  for (int a = 0; a < k; a++) for (int b = a + 1; b < k; b++) if (force[b] > force[a]) {
    String ts = noms[a]; noms[a] = noms[b]; noms[b] = ts; int tf = force[a]; force[a] = force[b]; force[b] = tf; bool to = ouvert[a]; ouvert[a] = ouvert[b]; ouvert[b] = to; }
  String js = "{\"r\":[";
  for (int i = 0; i < k; i++) {
    int q = 2 * (force[i] + 100); if (q < 0) q = 0; if (q > 100) q = 100;
    if (i) js += ",";
    js += String("{\"s\":\"") + nettoyer(noms[i]) + "\",\"q\":" + q + ",\"o\":" + (ouvert[i] ? "false" : "true") + "}";
  }
  js += "]}";
  serveur.send(200, "application/json", js);
}
static void hWifi() {
  if (!enteteOk()) { refuser(); return; }
  if (!depuisSecours()) { repondre(false, "Reglage possible seulement depuis le Wi-Fi de secours."); return; }
  String s = serveur.arg("s"), m = serveur.arg("m");
  if (s.length() < 1 || s.length() > 32 || m.length() > 63 || (m.length() > 0 && m.length() < 8)) { repondre(false, "Nom ou mot de passe invalide (8 caracteres minimum)."); return; }
  prefsRes.putString("ssid", s); prefsRes.putString("mdp", m);
  jrn("reglage Wi-Fi enregistre : %s", nettoyer(s).c_str());
  repondre(true);
  redemarrerA = millis() + 1500;
}
static void hPi() {
  if (!enteteOk()) { refuser(); return; }
  if (!depuisSecours()) { repondre(false, "Reglage possible seulement depuis le Wi-Fi de secours."); return; }
  String a = serveur.arg("a"); a.trim();
  bool bon = a.length() >= 3 && a.length() <= 40;
  for (unsigned i = 0; i < a.length() && bon; i++) { char c = a[i]; if (!(isalnum((unsigned char)c) || c == '.' || c == '-' || c == ':')) bon = false; }
  if (!bon) { repondre(false, "Adresse invalide."); return; }
  prefsRes.putString("pi", a);
  jrn("adresse du Pi enregistree : %s", a.c_str());
  repondre(true);
  redemarrerA = millis() + 1500;
}

static esp_err_t enrober(httpd_req_t* r) {
  void (*f)() = (void (*)())r->user_ctx;
  serveur.debut(r);
  f();
  return ESP_OK;
}
void tacheWeb(void*) {
  httpd_handle_t h = nullptr;
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.server_port = 80;
  cfg.core_id = 0;
  cfg.stack_size = 8192;
  cfg.max_open_sockets = 5;          // garde de la place pour l'envoi OTA et les appels au Pi
  cfg.lru_purge_enable = true;       // si plein, on ferme la connexion la plus ancienne
  cfg.recv_wait_timeout = 4;         // une connexion muette est fermee au bout de 4 s
  cfg.send_wait_timeout = 5;
  cfg.max_uri_handlers = 10;
  if (httpd_start(&h, &cfg) == ESP_OK) {
    static httpd_uri_t u[8];
    memset(u, 0, sizeof(u));
    struct { const char* uri; httpd_method_t m; void (*f)(); } routes[8] = {
      { "/", HTTP_GET, hPage }, { "/api/etat", HTTP_GET, hEtat }, { "/api/cmd", HTTP_POST, hCmd },
      { "/api/consigne", HTTP_POST, hConsigne }, { "/api/nuit", HTTP_POST, hNuit },
      { "/api/reseaux", HTTP_POST, hReseaux }, { "/api/wifi", HTTP_POST, hWifi }, { "/api/pi", HTTP_POST, hPi } };
    for (int i = 0; i < 8; i++) {
      u[i].uri = routes[i].uri; u[i].method = routes[i].m; u[i].handler = enrober; u[i].user_ctx = (void*)routes[i].f;
      httpd_register_uri_handler(h, &u[i]);
    }
    Serial.println("Serveur web de secours demarre (ESP-IDF).");
  } else Serial.println("ERREUR : le serveur web de secours n'a pas demarre.");
  vTaskDelete(NULL);                 // le serveur tourne dans sa propre tache
}

// ---------- Bouton BOOT (appui de 3 s) et LED ----------
void tacheVoyant(void*) {
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BOUTON, INPUT_PULLUP);
  unsigned long appui = 0;
  bool declenche = (digitalRead(PIN_BOUTON) == LOW);     // un bouton deja enfonce au demarrage ne compte pas
  for (;;) {
    unsigned long t = millis();
    if (digitalRead(PIN_BOUTON) == LOW) {
      if (appui == 0) appui = t;
      else if (!declenche && t - appui >= 3000) {
        declenche = true;
        if (apActif) { jrn("BOOT : fin du mode secours, redemarrage"); redemarrerA = t + 300; }
        else demarrerAP();
      }
    } else { appui = 0; declenche = false; }

    bool led = false;
    if (apActif) led = ((t / 1000) % 2) == 0;                           // lent : mode secours
    else if (WiFi.status() != WL_CONNECTED) led = ((t / 150) % 2) == 0; // vite : recherche du Wi-Fi
    else if (!espEnLigne && t > 45000UL) led = (t % 3000UL) < 120;      // un eclat : Pi injoignable
    digitalWrite(PIN_LED, led ? HIGH : LOW);

    if (apActif) {
      if (WiFi.softAPgetStationNum() > 0) apDerniereStation = t;
      if (t - apDerniereStation > 3600000UL && redemarrerA == 0) { jrn("secours : 1 h sans telephone, redemarrage"); redemarrerA = t + 300; }
    }
    if (redemarrerA != 0 && (long)(t - redemarrerA) >= 0) { vTaskDelay(200 / portTICK_PERIOD_MS); ESP.restart(); }
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}
