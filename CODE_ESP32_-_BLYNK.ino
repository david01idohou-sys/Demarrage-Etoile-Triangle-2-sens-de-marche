/*
 * ================================================================
 *  DÉMARRAGE ÉTOILE-TRIANGLE — ESP32 (contrôle distant Blynk)
 *  Communication UART avec Arduino Uno
 * ================================================================
 *  BROCHES ESP32 :
 *   GPIO16 = RX2 ← Arduino A1 (via diviseur tension 5V→3.3V)
 *   GPIO17 = TX2 → Arduino A0
 *
 *  BLYNK - Broches virtuelles :
 *   V0 → Bouton S1 (Marche 1)      V1 → Bouton S2 (Marche 2)
 *   V2 → Bouton S0 (Arrêt)         V3 → LED HV (Veille)
 *   V4 → LED H1                    V5 → LED H2
 *   V6 → LED HY (Étoile)           V7 → LED HD (Triangle)
 *   V8 → Label état texte
 *
 *  LIBRAIRIE : Blynk by Volodymyr Shymanskyy ≥ 1.3.2
 *  Gestionnaire de bibliothèques Arduino IDE → chercher "Blynk"
 * ================================================================
 */

// ── REMPLIR avec vos identifiants Blynk ──────────────────────
#define BLYNK_TEMPLATE_ID    "TMPLxxxxxxxx"       // ← Template ID
#define BLYNK_TEMPLATE_NAME  "EtoileTriangle"
#define BLYNK_AUTH_TOKEN     "xxxxxxxxxxxxxxxxxxxx" // ← Auth Token

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// ── WiFi — REMPLIR ────────────────────────────────────────────
char ssid[] = "VOTRE_SSID";
char pass[] = "VOTRE_MOT_DE_PASSE";

// ── UART vers Arduino ─────────────────────────────────────────
#define ARD_RX  16
#define ARD_TX  17

// ── États (miroir de l'Arduino) ───────────────────────────────
#define VEILLE      0
#define ETOILE_1    1
#define TRIANGLE_1  2
#define ETOILE_2    3
#define TRIANGLE_2  4

int etatCourant = VEILLE;
BlynkTimer timer;

// ── Mise à jour interface Blynk ──────────────────────────────
void majBlynk(int etat) {
  etatCourant = etat;

  // Réinitialiser tous les voyants virtuels
  for (int v = 3; v <= 7; v++) Blynk.virtualWrite(v, 0);

  switch (etat) {
    case VEILLE:
      Blynk.virtualWrite(V3, 255);
      Blynk.virtualWrite(V8, "VEILLE");
      break;
    case ETOILE_1:
      Blynk.virtualWrite(V4, 255);      // H1
      Blynk.virtualWrite(V6, 255);      // HY
      Blynk.virtualWrite(V8, "MARCHE 1 - ETOILE");
      break;
    case TRIANGLE_1:
      Blynk.virtualWrite(V4, 255);      // H1
      Blynk.virtualWrite(V7, 255);      // HD
      Blynk.virtualWrite(V8, "MARCHE 1 - TRIANGLE");
      break;
    case ETOILE_2:
      Blynk.virtualWrite(V5, 255);      // H2
      Blynk.virtualWrite(V6, 255);      // HY
      Blynk.virtualWrite(V8, "MARCHE 2 - ETOILE");
      break;
    case TRIANGLE_2:
      Blynk.virtualWrite(V5, 255);      // H2
      Blynk.virtualWrite(V7, 255);      // HD
      Blynk.virtualWrite(V8, "MARCHE 2 - TRIANGLE");
      break;
  }
}

// ── Réception commandes depuis Blynk ─────────────────────────

BLYNK_WRITE(V0) {                       // Bouton S1 — Marche 1
  if (param.asInt() == 1 && etatCourant == VEILLE) {
    Serial2.println("CMD:S1");
    Serial.println(F("[Blynk] CMD:S1"));
  }
}

BLYNK_WRITE(V1) {                       // Bouton S2 — Marche 2
  if (param.asInt() == 1 && etatCourant == VEILLE) {
    Serial2.println("CMD:S2");
    Serial.println(F("[Blynk] CMD:S2"));
  }
}

BLYNK_WRITE(V2) {                       // Bouton S0 — Arrêt
  if (param.asInt() == 1 && etatCourant != VEILLE) {
    Serial2.println("CMD:S0");
    Serial.println(F("[Blynk] CMD:S0"));
  }
}

// Synchronisation à la reconnexion
BLYNK_CONNECTED() {
  majBlynk(etatCourant);               // Restaurer état affiché
}

// ── Lecture état depuis Arduino (toutes les 200 ms) ──────────
void lireArduino() {
  while (Serial2.available()) {
    String msg = Serial2.readStringUntil('\n');
    msg.trim();
    if (msg.startsWith("STATE:")) {
      int nouvelEtat = msg.substring(6).toInt();
      if (nouvelEtat != etatCourant) {
        majBlynk(nouvelEtat);
        Serial.print(F("[Arduino] Etat ")); Serial.println(nouvelEtat);
      }
    }
  }
}

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, ARD_RX, ARD_TX);

  Serial.println(F("Connexion WiFi + Blynk..."));
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  timer.setInterval(200L, lireArduino); // Polling Arduino

  majBlynk(VEILLE);
  Serial.println(F("Systeme pret."));
}

void loop() {
  Blynk.run();
  timer.run();
}
