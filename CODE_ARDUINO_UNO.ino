/*
 * ================================================================
 *  DÉMARRAGE ÉTOILE-TRIANGLE — Arduino Uno (contrôle local)
 *  Communication UART avec ESP32 pour supervision distante Blynk
 * ================================================================
 *  BROCHES :
 *   S0 (Arrêt)   → D2   S1 (Marche1) → D3   S2 (Marche2) → D4
 *   R_KM1 → D5   R_KM2 → D6   R_KMY → D7    R_KMD → D8
 *   R_HV  → D9   R_H1  → D10  R_H2  → D11
 *   R_HY  → D12  R_HD  → D13
 *   SoftwareSerial : RX=A0 ← ESP32 TX  |  TX=A1 → ESP32 RX
 *
 *  ATTENTION : Diviseur de tension obligatoire sur A1 → ESP32 RX
 *              (5V Arduino → 3.3V ESP32) : R1=1kΩ, R2=2kΩ
 * ================================================================
 */

#include <SoftwareSerial.h>

// ── Communication série avec l'ESP32 ──────────────────────────
SoftwareSerial espSerial(A0, A1);   // RX, TX

// ── Boutons poussoirs (INPUT_PULLUP → actif à LOW) ────────────
#define BTN_S0  2   // Arrêt
#define BTN_S1  3   // Marche 1
#define BTN_S2  4   // Marche 2

// ── Relais (module relais standard = actif à LOW) ─────────────
#define R_KM1  5    // Contacteur sens 1
#define R_KM2  6    // Contacteur sens 2
#define R_KMY  7    // Contacteur Étoile
#define R_KMD  8    // Contacteur Triangle
#define R_HV   9    // Voyant Veille
#define R_H1   10   // Voyant Marche 1
#define R_H2   11   // Voyant Marche 2
#define R_HY   12   // Voyant Étoile
#define R_HD   13   // Voyant Triangle

#define RELAY_ON   LOW
#define RELAY_OFF  HIGH

// ── États machine ─────────────────────────────────────────────
#define VEILLE      0
#define ETOILE_1    1
#define TRIANGLE_1  2
#define ETOILE_2    3
#define TRIANGLE_2  4

// ── Variables globales ────────────────────────────────────────
int etat = VEILLE;
unsigned long tEtoile  = 0;
bool timerActif        = false;
const unsigned long DELAI_ETOILE = 5000UL;  // 5 secondes

// Débounce boutons
unsigned long tDS0=0, tDS1=0, tDS2=0;
bool prevS0=HIGH, prevS1=HIGH, prevS2=HIGH;
const unsigned long DEBOUNCE = 50UL;

// ── Utilitaires ───────────────────────────────────────────────
void tousRelaisOff() {
  digitalWrite(R_KM1, RELAY_OFF); digitalWrite(R_KM2, RELAY_OFF);
  digitalWrite(R_KMY, RELAY_OFF); digitalWrite(R_KMD, RELAY_OFF);
  digitalWrite(R_HV,  RELAY_OFF); digitalWrite(R_H1,  RELAY_OFF);
  digitalWrite(R_H2,  RELAY_OFF); digitalWrite(R_HY,  RELAY_OFF);
  digitalWrite(R_HD,  RELAY_OFF);
}

// Retourne true sur front descendant avec débounce
bool appuye(int pin, bool &prev, unsigned long &t) {
  bool cur = digitalRead(pin);
  bool ok  = false;
  if (cur != prev) t = millis();
  if ((millis() - t) > DEBOUNCE && cur == LOW && prev == HIGH) ok = true;
  prev = cur;
  return ok;
}

// ── Transitions d'état ────────────────────────────────────────
void allerVeille() {
  tousRelaisOff();
  digitalWrite(R_HV, RELAY_ON);       // Voyant veille allumé
  etat = VEILLE;
  timerActif = false;
  espSerial.println("STATE:0");
  Serial.println(F(">> VEILLE"));
}

void allerEtoile1() {
  // Interlock KM1/KM2 et KMY/KMD garantis par tousRelaisOff()
  tousRelaisOff();
  digitalWrite(R_KM1, RELAY_ON);      // KM1 ON
  digitalWrite(R_KMY, RELAY_ON);      // KMY (étoile) ON
  digitalWrite(R_H1,  RELAY_ON);      // Voyant H1
  digitalWrite(R_HY,  RELAY_ON);      // Voyant HY
  etat = ETOILE_1;
  tEtoile    = millis();
  timerActif = true;
  espSerial.println("STATE:1");
  Serial.println(F(">> ETOILE_1"));
}

void allerTriangle1() {
  // Sécurité : couper KMY avant d'activer KMD
  digitalWrite(R_KMY, RELAY_OFF);
  digitalWrite(R_HY,  RELAY_OFF);
  delay(100);                          // 100 ms délai sécurité
  digitalWrite(R_KMD, RELAY_ON);      // KMD (triangle) ON
  digitalWrite(R_HD,  RELAY_ON);      // Voyant HD
  etat = TRIANGLE_1;
  timerActif = false;
  espSerial.println("STATE:2");
  Serial.println(F(">> TRIANGLE_1"));
}

void allerEtoile2() {
  tousRelaisOff();
  digitalWrite(R_KM2, RELAY_ON);      // KM2 ON
  digitalWrite(R_KMY, RELAY_ON);      // KMY (étoile) ON
  digitalWrite(R_H2,  RELAY_ON);      // Voyant H2
  digitalWrite(R_HY,  RELAY_ON);      // Voyant HY
  etat = ETOILE_2;
  tEtoile    = millis();
  timerActif = true;
  espSerial.println("STATE:3");
  Serial.println(F(">> ETOILE_2"));
}

void allerTriangle2() {
  digitalWrite(R_KMY, RELAY_OFF);
  digitalWrite(R_HY,  RELAY_OFF);
  delay(100);
  digitalWrite(R_KMD, RELAY_ON);
  digitalWrite(R_HD,  RELAY_ON);
  etat = TRIANGLE_2;
  timerActif = false;
  espSerial.println("STATE:4");
  Serial.println(F(">> TRIANGLE_2"));
}

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);

  // Boutons
  pinMode(BTN_S0, INPUT_PULLUP);
  pinMode(BTN_S1, INPUT_PULLUP);
  pinMode(BTN_S2, INPUT_PULLUP);

  // Relais — tous éteints au démarrage
  const int relais[] = {R_KM1,R_KM2,R_KMY,R_KMD,R_HV,R_H1,R_H2,R_HY,R_HD};
  for (int i = 0; i < 9; i++) {
    pinMode(relais[i], OUTPUT);
    digitalWrite(relais[i], RELAY_OFF);
  }

  delay(200);
  allerVeille();                       // État initial : veille
}

void loop() {
  // ── Boutons locaux ──────────────────────────────────────────
  if (appuye(BTN_S1, prevS1, tDS1) && etat == VEILLE) allerEtoile1();
  if (appuye(BTN_S2, prevS2, tDS2) && etat == VEILLE) allerEtoile2();
  if (appuye(BTN_S0, prevS0, tDS0) && etat != VEILLE) allerVeille();

  // ── Timer étoile → triangle (5 s) ─────────────────────────
  if (timerActif && (millis() - tEtoile) >= DELAI_ETOILE) {
    if      (etat == ETOILE_1) allerTriangle1();
    else if (etat == ETOILE_2) allerTriangle2();
  }

  // ── Commandes distantes depuis ESP32 / Blynk ──────────────
  if (espSerial.available()) {
    String cmd = espSerial.readStringUntil('\n');
    cmd.trim();
    Serial.print(F("[ESP32] ")); Serial.println(cmd);

    if      (cmd == "CMD:S1" && etat == VEILLE) allerEtoile1();
    else if (cmd == "CMD:S2" && etat == VEILLE) allerEtoile2();
    else if (cmd == "CMD:S0" && etat != VEILLE) allerVeille();
  }
}
