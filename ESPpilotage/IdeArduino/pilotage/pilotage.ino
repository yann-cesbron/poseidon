#include <ArduinoJson.h>
#include <pins.h>

// --- Définitions des Broches (Pins) (Inchangé) ---
// const int PIN_COMMUTATEUR = 15;
// const int PIN_LED_RX = 2;
// const int PIN_RX2 = 16;
// const int PIN_TX2 = 17;

// --- Constantes de Communication et Minuteries (Inchangé) ---
const long INTERVALLE_POTENTIOMETRE = 30000;  // 30s
const long INTERVALLE_PILOTE = 1000;          // 1s
const long DELAI_LED_RX = 100;
const long BAUD_RATE_COMM = 19200;

// --- Variables d'État  ---
unsigned long derniereLecturePot = 0;
unsigned long dernierAjustGouvernail = 0;
int etatCommutateurPrecedent = HIGH;
String modeOperation = "M";

// --- Variables Globales pour le Stockage des Coordonnées ---
double gps_lat = 0.0;
double gps_lng = 0.0;
double wp_lat = 0.0;
double wp_lng = 0.0;
double distance_m = 0.0;
double cap_deg = 0.0;

// batterie
int niveauBatterie = 0;
// Tension de Référence de l'ADC de l'ESP32 (Vref)
const float VREF = 3.3;
// Résolution de l'ADC (12 bits)
const int ADC_MAX_VALUE = 4095;
// Facteur de conversion V / ADC_Value (VREF / 4095)
const float V_PER_UNIT = VREF / ADC_MAX_VALUE;

// --- BORNES DE CALIBRATION DU POURCENTAGE (en Volts mesurés sur l'ADC) ---
// Tension ADC pour 18.0V batterie (0%) -> 2.75V
const float V_ADC_MIN = 2.75;
// Tension ADC pour 19.0V batterie (100%) -> 2.90V
const float V_ADC_MAX = 2.90;
// Plage de tension ADC utilisable
const float V_ADC_RANGE = V_ADC_MAX - V_ADC_MIN;


// --- Déclaration des Fonctions  ---
void envoyerJson(int bat, String mode);
void lireEtTraiterSerial2();

// --- SETUP et LOOP ---
void setup() {
  Serial.begin(115200);
  Serial.println("Démarrage du système ESP32 (v5.0 - WP/GPS Affichage Corrigé)...");
  Serial2.begin(BAUD_RATE_COMM, SERIAL_8N1, PIN_RX2, PIN_TX2);
  Serial.print("Communication série (RX/TX) démarrée sur GPIO 16/17 à ");
  Serial.print(BAUD_RATE_COMM);
  Serial.println(" bauds.");

  pinMode(PIN_MODE, INPUT_PULLUP);
  pinMode(PIN_LED_RX, OUTPUT);
  digitalWrite(PIN_LED_RX, LOW);

  analogSetAttenuation(ADC_11db); // pour 0-3.3V
  // 1ere lecture batterie
  int adcValue = analogRead(PIN_BATTERIE);
  niveauBatterie = calculateBatteryPercent(adcValue);

  etatCommutateurPrecedent = digitalRead(PIN_MODE);
  modeOperation = (etatCommutateurPrecedent == LOW) ? "A" : "M";
  //  init bousole
  initBousole();
  //   init du servomoteur
  initialiserServos();
}

void loop() {
  unsigned long tempsActuel = millis();

  // 1. Gestion du Commutateur (Changement d'état)
  int etatCommutateurActuel = digitalRead(PIN_MODE);
  if (etatCommutateurActuel != etatCommutateurPrecedent) {
    delay(50);
    etatCommutateurActuel = digitalRead(PIN_MODE);

    if (etatCommutateurActuel != etatCommutateurPrecedent) {
      modeOperation = (etatCommutateurActuel == HIGH) ? "A" : "M";
      Serial.print("MODE: Changement d'état. Nouveau mode: ");
      Serial.println(modeOperation);
      envoyerJson(niveauBatterie, modeOperation);
      etatCommutateurPrecedent = etatCommutateurActuel;
    }
  }

  // 2. Lecture niveau batterie (Toutes les 60 secondes)
  if (tempsActuel - derniereLecturePot >= INTERVALLE_POTENTIOMETRE) {
    derniereLecturePot = tempsActuel;
    int adcValue = analogRead(PIN_BATTERIE);
    niveauBatterie = calculateBatteryPercent(adcValue);
    float vBatt = ((float)adcValue * V_PER_UNIT) * 6.5517;  // Conversion pour affichage
    Serial.print("V_Batterie: ");
    Serial.print(vBatt, 2);
    Serial.print(" V -> Charge: ");
    Serial.print(niveauBatterie);
    Serial.println(" %");

    envoyerJson(niveauBatterie, modeOperation);
  }

  // 3. Traitement des données reçues sur Serial2 (RX)
  lireEtTraiterSerial2();

  if (millis() - dernierAjustGouvernail >= INTERVALLE_PILOTE) {
    dernierAjustGouvernail = millis();
    if (gps_lat != 0.0 && wp_lat != 0.0 && cap_deg != 0.0 && modeOperation == "A") {
      Serial.println("------- ajustement gouvernail");
      ajusterGouvernail(cap_deg);
    }
  }
}

// -----------------------------------------------------------------------------
// --- Fonctions d'Envoi (TX) (Inchangé) ---
// -----------------------------------------------------------------------------

/**
 * Génère et envoie le message JSON sur Serial2. Format: {"bat":XX,"mode":"Y"}
 */
void envoyerJson(int bat, String mode) {
  StaticJsonDocument<64> doc;
  doc["bat"] = bat;
  doc["mode"] = mode;

  if (serializeJson(doc, Serial2) == 0) {
    Serial.println("Erreur de sérialisation JSON TX.");
  } else {
    Serial2.println();
    
    Serial.print("TX JSON: ");
    serializeJson(doc, Serial);
    Serial.println();
  }
}

// -----------------------------------------------------------------------------
// --- Fonctions de Réception et Traitement (RX)  ---
// -----------------------------------------------------------------------------

/**
 * Lit, affiche, traite les messages JSON reçus et déclenche les calculs.
 */
void lireEtTraiterSerial2() {
  static String inputString = "";
  static unsigned long tempsExtinctionLed = 0;

  if (digitalRead(PIN_LED_RX) == HIGH && millis() > tempsExtinctionLed) {
    digitalWrite(PIN_LED_RX, LOW);
  }

  while (Serial2.available()) {
    // ... (Code de lecture de la trame et gestion LED inchangé) ...
    char inChar = (char)Serial2.read();
    inputString += inChar;
    digitalWrite(PIN_LED_RX, HIGH);
    tempsExtinctionLed = millis() + DELAI_LED_RX;

    if (inChar == '\n' || inChar == '\r') {
      inputString.trim();

      if (inputString.length() > 0) {
        // Serial.print("RX Trame brute: ");
        // Serial.println(inputString);

        String prefixe;
        String jsonPayload;
        int debutJson = inputString.indexOf(':');

        if (debutJson != -1) {
          prefixe = inputString.substring(0, debutJson);
          jsonPayload = inputString.substring(debutJson + 1);
          prefixe.toUpperCase();

          if (prefixe.equals("GPS") || prefixe.equals("WP")) {
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, jsonPayload);

            if (!error) {
              if (doc.containsKey("lat") && doc.containsKey("lng")) {
                double lat = doc["lat"];
                double lng = doc["lng"];

                Serial.println("--- DONNÉES REÇUES ---");

                if (prefixe.equals("GPS")) {
                  gps_lat = lat;  // Stockage de la dernière coordonnée GPS
                  gps_lng = lng;
                  Serial.print("GPS lat: ");
                  Serial.println(gps_lat, 6);
                  Serial.print("GPS lng: ");
                  Serial.println(gps_lng, 6);
                } else if (prefixe.equals("WP")) {
                  wp_lat = lat;  // Stockage de la dernière coordonnée WP
                  wp_lng = lng;
                  Serial.print("WP lat: ");
                  Serial.println(wp_lat, 6);
                  Serial.print("WP lon: ");
                  Serial.println(wp_lng, 6);  // Affichage en 'lon'
                }
                Serial.println("---------------------");
                if (modeOperation == "A") {
                  // --- NOUVEAU: Appel des Calculs ---  si mode 'auto'
                  // Les calculs sont déclenchés dès qu'une nouvelle coordonnée valide arrive
                  if (gps_lat != 0.0 && wp_lat != 0.0) {  // Vérifie si les deux points sont initialisés
                    distance_m = calculerDistance(gps_lat, gps_lng, wp_lat, wp_lng);
                    cap_deg = calculerCap(gps_lat, gps_lng, wp_lat, wp_lng);

                    Serial.println("=== CALCUL DE NAVIGATION ===");
                    Serial.print("Distance GPS -> WP: ");
                    if (distance_m > 10.0) {
                      set_motor(true);  // on
                    } else {
                      set_motor(false);  // off
                    }

                    if (distance_m < 1000.0) {
                      Serial.print(distance_m, 2);
                      Serial.println(" mètres");
                    } else {
                      Serial.print(distance_m / 1000.0, 3);
                      Serial.println(" km");
                    }

                    Serial.print("Direction (Cap): ");
                    Serial.print(cap_deg, 1);
                    Serial.println(" degrés (Nord=0)");
                    Serial.println("============================");
                    // ajustement gouvernail
                    ajusterGouvernail(cap_deg);
                  } else {
                    Serial.println("Attente de la deuxième coordonnée (GPS ou WP) pour le calcul.");
                  }
                } else {
                  Serial.println("mode manuel.. on ne fait rien");
                }

              } else {
                Serial.print("Trame ");
                Serial.print(prefixe);
                Serial.println(" reçue, mais 'lat' et/ou 'lng' manquent.");
              }
            } else {
              Serial.print("Erreur de désérialisation JSON (");
              Serial.print(prefixe);
              Serial.print("): ");
              Serial.println(error.c_str());
            }

          } else if (prefixe.equals("SEN")) {
            // Serial.println("Trame SEN: ignorée.");
          } else {
            // Serial.print("Préfixe non reconnu: ");
            // Serial.println(prefixe);
          }
        } else {
          // Serial.println("Trame reçue sans préfixe ou séparateur ':'.");
        }
      }
      inputString = "";  // Réinitialiser la chaîne pour le prochain message
    }
  }
}

/**
 * @brief Convertit la valeur brute de l'ADC en pourcentage de batterie utilisable (0 à 99%).
 * * @param rawADC La lecture brute du pin analogique (0 à 4095).
 * @return int Le pourcentage de batterie (0-99).
 */
int calculateBatteryPercent(int rawADC) {

  // 1. Convertir la lecture brute en tension réelle sur le pin ADC
  float vAdc = (float)rawADC * V_PER_UNIT;
  // 2. Calculer la position actuelle dans la plage utilisable
  // La tension doit être décalée par V_ADC_MIN
  float vOffset = vAdc - V_ADC_MIN;
  // 3. Calculer le pourcentage théorique (0.0 à 1.0)
  float percentFloat = vOffset / V_ADC_RANGE;
  // 4. Convertir en pourcentage (0 à 100)
  int percent = round(percentFloat * 100.0);
  // 5. Limiter le résultat dans la plage [0, 99]
  if (percent < 0) {
    return 0;
  }
  if (percent > 99) {
    return 99;  // Limité à 99% comme demandé
  }
  return percent;
}
