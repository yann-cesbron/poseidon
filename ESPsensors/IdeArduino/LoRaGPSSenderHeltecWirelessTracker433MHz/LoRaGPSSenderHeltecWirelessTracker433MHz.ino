//-- 26/10/2024 YC
// version 0.9
#include "Arduino.h"
#include <OneWire.h>
#include <DallasTemperature.h>

// Please provide a correct license! For more information:
// http://www.heltec.cn/search/
//-- License key
// ESP32ChipID=3C1A3B43CA48  Heltec 1.2
// uint32_t license[4] = { 0x1509E5FF, 0x936FFF81, 0xD99CDDAC, 0x6648BDD0 };

// ESP32ChipID=90793B43CA48  Heltec 1.1
uint32_t license[4] = { 0x3A4EAB51, 0xC9738BA8, 0xD40D6062, 0x29233553 };

//-- LORA
#include "LoRaWan_APP.h"
//-- GPS
#include "HT_TinyGPS++.h"
TinyGPSPlus GPS;

//-- Pour le format JSON
#include <ArduinoJson.h>

// UART2 connecté à l'ESP2
#define SERIAL2_TX_PIN 45
#define SERIAL2_RX_PIN 46
HardwareSerial Esp2_serial(2);  // vers esp32 arriere
String receivedFromEsp2 = "";   // buffer pour send vers ESP2

//====================================  Afficheur
#include <Adafruit_GFX.h>     // Core graphics library
#include <Adafruit_ST7735.h>  // Hardware-specific library for ST7735
#include <Adafruit_ST7789.h>  // Hardware-specific library for ST7789
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
// Broches pour Wireless Tracker ESP32-S3 Version 1.1
#define TFT_CS 38
#define TFT_DC 40
#define TFT_RST 39
#define TFT_BACKLIGHT 3  // Display backlight pin
#define TFT_LED_K_Pin 21

#define TFT_MOSI 42  // Data out
#define TFT_SCLK 41  // Clock out
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// couleur exprimée en format 16 bits BGR (souvent appelé RGB565)
const uint16_t Display_Color_Black = 0x0000;
const uint16_t Display_Color_Blue = 0xF800;
const uint16_t Display_Color_Red = 0x001F;
const uint16_t Display_Color_Green = 0x07E0;
const uint16_t Display_Color_Cyan = 0x07FF;
const uint16_t Display_Color_Magenta = 0xF81F;
const uint16_t Display_Color_Yellow = 0xFFE0;
const uint16_t Display_Color_Orange = 0x02BF;
const uint16_t Display_Color_White = 0xFFFF;

// The colors we actually want to use
uint16_t Display_Text_Color = Display_Color_Black;
uint16_t Display_Backround_Color = Display_Color_Blue;

//====================================  Capteurs ========================================================
//-- Entreés analogiques
// const uint16_t TEST = 4;  utilisée par oneWire
const uint16_t PH_Po = 5;
const uint16_t RES_AO = 6;
const uint16_t TURB_Ao = 7;
float kADC = 0.87;
float attenuationADC = pow(10, 11.0 / 20.0) * kADC;

#define LED 18
#define MIN_NB_SAT 3

//======================== Capteur Température:  DS18B20 ======================

// Data wire is plugged into port 0 on the Arduino
#define ONE_WIRE_BUS 4
#define TEMPERATURE_PRECISION 9  // Lower resolution
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature Tsensor(&oneWire);

//========================================== GPS ========================================================
#define TX 34
#define RX 33
#define GPS_OK 36
#define VGNSS_CTRL 3

//========================================== one wire ========================================================
#define ONE_WIRE_BUS 4

//========================================== LORA ========================================================
// Paramètres radio - DOIVENT ÊTRE IDENTIQUES aux deux devices
#define RF_FREQUENCY 433775000   // Hz
#define TX_OUTPUT_POWER 14       // dBm
#define LORA_BANDWIDTH 0         // 0: 125 kHz
#define LORA_SPREADING_FACTOR 7  // SF7
#define LORA_CODINGRATE 1        // 4/5
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false

#define BUFFER_SIZE 200  // Augmenté pour accommoder le JSON

// Buffer pour l'emission' LoRa
char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];

static RadioEvents_t RadioEvents;
void OnTxDone(void);
void OnTxTimeout(void);
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);

// Variables pour le mode asynchrone
int16_t packetCounter = 0;
int16_t lastRssi = 0;
int8_t lastSnr = 0;
bool isTransmitting = false;
unsigned long lastTxTime = 0;
#define TX_INTERVAL 5000  // Envoi alternativement les trames toutes les 5 secondes
bool flagTr = false;      // sert à envoyer altenativement les 2 trames

int batteryValue = 0;  // pour stocker la valeur "bat" recue de l'ESP2
char driveMode = 'M';  // pour stocker le mode 'M', 'A'

// Données capteurs partagées
float temperature = 0.0;
float turbidite = 0.0;
float resistivite = 0.0;
float ph = 0.0;

SemaphoreHandle_t dataMutex;

// --- Fonction d’arrondi à 1 décimale ---
float arrondir1(float value) {
  return roundf(value * 10.0f) / 10.0f;
}

// --- Tâche lecture capteurs ---
void taskLectureCapteurs(void *pvParameters) {
  float _temperature = 0.0;
  float _turbidite = 0.0;
  float _resistivite = 0.0;
  float _ph = 0.0;

  for (;;) {
    // Serial.println("-- début lecture capteurs");
    _temperature = arrondir1(lectureTemp());
    _turbidite = arrondir1(lectureTurbidite());
    _resistivite = arrondir1(LectureResistivite());
    _ph = arrondir1(LecturePH());
    xSemaphoreTake(dataMutex, portMAX_DELAY);  // protege les variables partagées
    temperature = _temperature;
    turbidite = _turbidite;
    resistivite = _resistivite;
    ph = _ph;
    xSemaphoreGive(dataMutex);
    // Serial.println("-- fin mise à jour capteurs");
    vTaskDelay(pdMS_TO_TICKS(5000));  // toutes les 5s
  }
}

void sendGPSData(void) {
  StaticJsonDocument<256> doc;
  // Données GPS uniquement
  // if (GPS.location.isValid()) {
  doc["lat"] = GPS.location.lat();
  doc["lng"] = GPS.location.lng();
  doc["alt"] = GPS.altitude.meters();
  doc["sat"] = GPS.satellites.value();
  doc["hdop"] = GPS.hdop.value() / 100.0;
  // }
  // Heure
  // if (GPS.time.isValid()) {
  doc["hour"] = GPS.time.hour();
  doc["minute"] = GPS.time.minute();
  doc["second"] = GPS.time.second();
  // }
  // Date
  // if (GPS.date.isValid()) {
  doc["year"] = GPS.date.year();
  doc["month"] = GPS.date.month();
  doc["day"] = GPS.date.day();
  // }

  // Metadata
  // doc["id"] = "HTIT-Tracker";
  // doc["type"] = "gps";
  doc["ts"] = millis();

  String jsonString;
  serializeJson(doc, jsonString);

  String message = "GPS:" + jsonString;

  if (message.length() >= BUFFER_SIZE) {
    Serial.println("ERREUR: Trame GPS trop longue!");
    return;
  }

  message.toCharArray(txpacket, BUFFER_SIZE);
  digitalWrite(LED, HIGH);

  isTransmitting = true;
  Radio.Send((uint8_t *)txpacket, strlen(txpacket));

  flagTr = true;
  lastTxTime = millis();

  Serial.print("=== Envoi trame GPS ===");
  Serial.print("Taille: ");
  Serial.print(strlen(txpacket));
  Serial.println(" octets");
  Serial.println(jsonString);
  forwardToEsp2Serial(txpacket);  //  Envoi vers ESP2
}

void sendSensorData(void) {
  isTransmitting = true;
  StaticJsonDocument<256> doc;
  // Serial.println("-- debut lecture capteurs");
  xSemaphoreTake(dataMutex, portMAX_DELAY);  // protege les variables partagées
  // Données des capteurs lues par la tache
  doc["temp"] = temperature;
  doc["turb"] = turbidite;
  doc["res"] = resistivite;
  doc["ph"] = ph;
  xSemaphoreGive(dataMutex);
  // Serial.println("-- fin lecture capteurs");
  // Métadonnées
  // doc["id"] = "HTIT-Tracker";
  // doc["type"] = "sensors";
  // doc["ts"] = millis();
  doc["bat"] = batteryValue;
  char modeString[2] = { driveMode, '\0' };
  doc["mode"] = modeString;

  String jsonString;
  serializeJson(doc, jsonString);
  String message = "SEN:" + jsonString;
  if (message.length() >= BUFFER_SIZE) {
    Serial.println("ERREUR: Trame capteurs trop longue!");
    return;
  }
  message.toCharArray(txpacket, BUFFER_SIZE);
  digitalWrite(LED, HIGH);
  Radio.Send((uint8_t *)txpacket, strlen(txpacket));
  flagTr = false;
  lastTxTime = millis();
  Serial.print("=== Envoi trame capteurs ===");
  Serial.print("  Taille: ");
  Serial.print(strlen(txpacket));
  Serial.println(" octets");  //forwardToEsp2Serial
  Serial.println(jsonString);
  // forwardToEsp2Serial(txpacket);  // Envoi vers ESP2
}

// Fonction pour envoyer les données reçues via LoRa vers Esp2_serial
void forwardToEsp2Serial(const char *data) {
  // Esp2_serial.println("test envoi dans forwardToEsp2Serial");  // a virer
  if (data != NULL && strlen(data) > 0) {
    Serial.print("->Envoi vers Esp2_serial: ");
    Serial.println(data);

    // Envoyer les données vers Esp2_serial
    Esp2_serial.print(data);
    Esp2_serial.print("\r\n");  // Terminaison CR+LF

    // Vérification que l'envoi est complet
    Esp2_serial.flush();  // Attend que l'envoi soit terminé
    Serial.println("     Données envoyées vers Esp2_serial avec succès");
    // Serial.println();
  } else {
    Serial.println("❌ Données vides, aucun envoi vers Esp2_serial");
  }
}

//-- Capteur de température
float lectureTemp(void) {         // attention la librairie oneWire est modifiée, predre celle livrée
  Tsensor.requestTemperatures();  // Send the command to get temperatures
  float tempC = Tsensor.getTempCByIndex(0);
  return tempC;
}

// //-- Mesure % batterie
// float lectureBat(void) {  //  --- recuperé dans trame transmise de l'ESP arriere
// }

//-- Capteur de turbidité
float lectureTurbidite(void) {
  float temp = analogRead(TURB_Ao) * 100.0 / 4096;
  return temp;
}

//-- Capteur de résistivité
float LectureResistivite(void) {
  uint8_t nb = 32;
  float temp = 0;
  for (uint8_t k = 0; k < nb; k++) temp += conversionAnalog(RES_AO, attenuationADC);
  temp /= nb;
  return temp;
}

//-- Capteur de PH
float LecturePH(void) {
  uint8_t nb = 4;
  float PH = 0;
  for (uint8_t k = 0; k < nb; k++) {
    PH += conversionAnalog(PH_Po, attenuationADC);
    delay(1000);
  }
  PH /= nb;
  return PH;
}

float conversionAnalog(uint16_t broche, float corADC) {
  return (analogRead(broche) * corADC * 1.1 / 4096);
}
void clearTftScrean() {
  tft.fillRect(0, 0, 187, 318, Display_Backround_Color);  //largeur, hauteur
  // Dessiner un rectangle autour de l'écran
  tft.drawRect(0, 0, 160, 80, Display_Color_Yellow);
}

void majDisplay(bool flag) {
  tft.setTextColor(Display_Color_Yellow);
  if (flag == false) {
    // Nb sat                                // maj nbsat & precision
    tft.fillRect(35, 64, 25, 13, Display_Backround_Color);  // colonne ,ligne, largeur, hauteur
    tft.setCursor(35, 75);
    if (GPS.satellites.value() < 3) {
      tft.setTextColor(Display_Color_Orange);  // si - de 10 % affiche en rouge
    } else {
      tft.setTextColor(Display_Color_Green);
    }  // colonne ligne
    tft.print(GPS.satellites.value());
    // //batterie
    // tft.fillRect(105, 64, 54, 13, Display_Backround_Color);  // colonne ,ligne, largeur, hauteur
    // tft.setCursor(105, 75);                                  // colonne ligne
    // // tft.print(GPS.hdop.value() / 100.0, 1);
    // if (batteryValue < 10) {
    //   tft.setTextColor(Display_Color_Orange);  // si - de 10 % affiche en rouge
    // } else {
    //   tft.setTextColor(Display_Color_Yellow);
    // }
    // tft.print(batteryValue, 0);
    // tft.setCursor(125, 75);
    // tft.print("%");
  } else {  // maj sensors
    int ligne = 15;
    int interligne = 15;
    int col = 70;
    // sensors
    tft.fillRect(col - 1, 2, 80, interligne * 4, Display_Backround_Color);  // colonne ,ligne, largeur, hauteur
    tft.setTextColor(Display_Color_Yellow);
    xSemaphoreTake(dataMutex, portMAX_DELAY);      // protege les variables partagées
    tft.setCursor(col, (ligne + interligne * 0));  // colonne ligne
    tft.println(temperature);
    tft.setCursor(col, ligne + (interligne * 1));  // colonne ligne
    tft.println(turbidite);
    tft.setCursor(col, ligne + (interligne * 2));  // colonne ligne
    tft.println(ph);
    tft.setCursor(col, ligne + (interligne * 3));  // colonne ligne
    tft.println(resistivite);
    xSemaphoreGive(dataMutex);  // libere le mutex
    //batterie
    tft.fillRect(105, 64, 54, 13, Display_Backround_Color);  // colonne ,ligne, largeur, hauteur
    tft.setCursor(105, 75);                                  // colonne ligne
    // tft.print(GPS.hdop.value() / 100.0, 1);
    if (batteryValue < 10) {
      tft.setTextColor(Display_Color_Orange);  // si - de 10 % affiche en rouge
    } else {
      tft.setTextColor(Display_Color_Yellow);
    }
    tft.print(batteryValue, 0);
    tft.setCursor(125, 75);
    tft.print("%");
  }
  // float temperature = 0.0;
  // float turbidite = 0.0;
  // float resistivite = 0.0;
  // float ph = 0.0;
}

//======================================= SETUP =====================================================
void setup() {
  delay(1000);
  Serial.begin(115200);

  //-- Display
  pinMode(TFT_LED_K_Pin, OUTPUT);
  digitalWrite(TFT_LED_K_Pin, HIGH);  // led
  // pinMode(TFT_BACKLIGHT, OUTPUT);
  // digitalWrite(TFT_BACKLIGHT, HIGH);  // Backlight on

  tft.init(188, 318);  // Init ST7789 ?? dimensions
  tft.setRotation(1);
  // tft.setAddrWindow(0, 0, 80, 160);
  tft.enableDisplay(true);

  // ---------------- SPLASH
  tft.setTextColor(Display_Color_Green);
  clearTftScrean();
  tft.setFont(&FreeSans12pt7b);
  tft.setTextSize(1);
  tft.setCursor(20, 25);  // colonne ligne
  tft.println("POSEIDON");
  tft.setFont(&FreeMono9pt7b);
  tft.setCursor(20, 40);  // colonne ligne
  tft.println("Drone marin ");
  tft.setTextSize(1);
  tft.setCursor(20, 55);  // colonne ligne
  tft.println("Version 0.9");
  tft.setCursor(25, 70);  // colonne ligne
  tft.println("10/2025 YC");

  // -- temperature ds18b20
  Tsensor.begin();
  // Vérification du nombre de capteurs
  int deviceCount = Tsensor.getDeviceCount();
  Serial.print("Nb Capteurs DS18B20 détectés: ");
  Serial.println(deviceCount);

  // Initialisation de l'UART2
  Esp2_serial.begin(19200, SERIAL_8N1, SERIAL2_RX_PIN, SERIAL2_TX_PIN);
  Esp2_serial.println("debut setup");  // A VIRER
  //-- LED blanche
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  //-- GPS
  pinMode(GPS_OK, INPUT);
  pinMode(VGNSS_CTRL, OUTPUT);
  digitalWrite(VGNSS_CTRL, HIGH);
  Serial1.begin(115200, SERIAL_8N1, RX, TX);
  delay(100);
  Serial.print("Attente du GPS");
  while (!digitalRead(GPS_OK)) {
    Serial.print(".");
    delay(100);
  }
  Serial.println(" ");
  Serial.println("GPS verouille");

  //-- LORA - Configuration unifiée
  Mcu.setlicense(license, HELTEC_BOARD);
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  Serial.println("====================================");
  Serial.println("📡 Tracker LoRa - Mode Asynchrone");
  Serial.println("====================================");

  // Configuration des événements Radio
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxDone = OnRxDone;
  Radio.Init(&RadioEvents);

  Radio.SetChannel(RF_FREQUENCY);

  // Configuration de l'émission
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

  // Configuration de la réception
  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                    0, true, 0, 0, LORA_IQ_INVERSION_ON, true);

  Serial.println("✅ Radio initialisée - Mode asynchrone");
  Serial.printf("📡 Fréquence: %.3f MHz\n", RF_FREQUENCY / 1e6);
  Serial.printf("🔊 Puissance TX: %d dBm\n", TX_OUTPUT_POWER);
  Serial.printf("🌊 Spreading Factor: SF%d\n", LORA_SPREADING_FACTOR);

  //--- Configuration de l'ADC
  pinMode(RES_AO, INPUT);
  pinMode(PH_Po, INPUT);
  analogSetAttenuation(ADC_11db);

  // creation tache lecture capteurs
  dataMutex = xSemaphoreCreateMutex();
  // Créer la tâche capteurs
  xTaskCreatePinnedToCore(
    taskLectureCapteurs,  // fonction
    "LectureCapteurs",    // nom
    4096,                 // taille stack
    NULL,                 // paramètre
    1,                    // priorité
    NULL,                 // handle
    1                     // core 1 pour ESP32
  );

  // Démarrer la réception LoRa en continu
  Radio.Rx(0);
  lastTxTime = millis();
  Serial.println("🚀 Envoi automatique donnees GNSS ou Capteurs toutes les 5s");

  // -----------------    affiche le texte fixe
  unsigned long startTime = millis();
  while (millis() - startTime < 2000) {
  }

  int ligne = 15;
  int interligne = 15;
  tft.setFont(&FreeMono9pt7b);
  // tft.enableDisplay(false);
  clearTftScrean();
  // tft.enableDisplay(true);
  tft.setTextColor(Display_Color_Yellow);
  tft.setTextSize(1);
  tft.setCursor(5, (ligne + interligne * 0));  // colonne ligne
  tft.println(" Temp:");
  tft.setCursor(5, ligne + (interligne * 1));  // colonne ligne
  tft.println(" Turb:");
  tft.setCursor(5, ligne + (interligne * 2));  // colonne ligne
  tft.println(" Acid:");
  tft.setCursor(5, ligne + (interligne * 3));  // colonne ligne
  tft.println(" Res :");
  tft.setCursor(5, ligne + (interligne * 4));  // colonne ligne
  tft.println("St:   Bt:");

  Esp2_serial.println("fin setup");  // A VIRER
}

void loop() {
  unsigned long currentTime = millis();

  // Radio.IrqProcess();  // deplacé à la fin de la loop
  // ---- Gestion des événements Serial
  // lecture trames GPS en continu
  if (Serial1.available() > 0) {
    if (Serial1.peek() != '\n') {
      GPS.encode(Serial1.read());
    } else {
      Serial1.read();
    }
  }
  // Json recu de l'ESP2
  if (Esp2_serial.available() > 0) {
    while (Esp2_serial.available() > 0) {
      char c = Esp2_serial.read();
      if (c == '\n' || c == '\r') {
        // Fin de ligne détectée
        if (receivedFromEsp2.length() > 0) {
          Serial.print("  --> Recu de Esp2: ");
          Serial.println(receivedFromEsp2);

          // ---------- Lecture du JSON reçu ----------
          StaticJsonDocument<128> inDoc;
          DeserializationError error = deserializeJson(inDoc, receivedFromEsp2);
          if (error) {
            Serial.print("Erreur de parsing JSON: ");
            Serial.println(error.c_str());
            receivedFromEsp2 = "";
            return;
          } else {
            // Lecture des champs et envoi immediat
            batteryValue = inDoc["bat"];             // lecture de l'entier
            const char *modeString = inDoc["mode"];  // lecture de la chaîne
            if (modeString != nullptr && strlen(modeString) > 0) {
              driveMode = modeString[0];  // on prend le premier caractère
            }
            // // Affichage pour vérification
            // Serial.print("Batterie = ");
            // Serial.print(batteryValue);
            // Serial.print("% | Mode = ");
            // Serial.println(driveMode);
            if (isTransmitting == false) {
              sendSensorData();
            }
            receivedFromEsp2 = "";  // Reset pour le prochain message
          }
        }
      } else {
        // Ajouter le caractère au message
        receivedFromEsp2 += c;
      }
    }
  }

  // Émission périodique des données (mode asynchrone)
  if (isTransmitting == false) {
    if (currentTime - lastTxTime > TX_INTERVAL) {
      Serial.print("⏰ Timer - ");
      Serial.print(currentTime - lastTxTime);
      Serial.print("ms écoulés | Type: ");
      Serial.println(flagTr ? "SENSORS" : "GPS");

      if (flagTr == false) {  // si false envoi GPS sinon sensors
        // Envoyer trames GPS si on a une position GPS valide
        if (GPS.location.isValid() && GPS.location.lat() != 0 && GPS.location.lng() != 0) {
          Serial.println("*** debut envoi trames GPS");
          sendGPSData();
        } else {
          Serial.println("⏳ En attente de position GPS valide...");
          Serial.println();
          flagTr = true;
          lastTxTime = millis();
        }

      } else {
        // Envoyer trames SENSORS
        Serial.println("*** debut envoi trames SENSORS");
        sendSensorData();
      }
      majDisplay(flagTr);
    }
  }  // else {
  //    Serial.print("⏰ Attente fin transmission ");
  // }
  Radio.IrqProcess();
}

void OnTxDone(void) {
  digitalWrite(LED, LOW);
  Serial.println("✅ Trame LoRa envoyée à la passerelle");
  Serial.println();
  // Redémarrer la réception après l'émission
  Radio.Rx(0);
  delay(10);
  isTransmitting = false;
  // Serial.println("--------- fin de transmission LoRa");
}

void OnTxTimeout(void) {
  digitalWrite(LED, LOW);
  Radio.Sleep();
  Serial.println("❌ Timeout LoRa");
  // Redémarrer la réception après erreur
  Radio.Rx(0);
  isTransmitting = false;
}

bool isValidPacket(const char *packet) {
  // Vérifier que le paquet n'est pas vide
  if (strlen(packet) == 0) {
    return false;
  }

  // Vérifier le patern attendu (WP:)
  if (strstr(packet, "WP:") != packet) {
    return false;
  }

  // Vérifier la présence d'accolades pour JSON
  if (strchr(packet, '{') != NULL && strchr(packet, '}') != NULL) {
    return true;
  }

  return false;
}

// Fonction pour gérer la réception LoRa
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  if (isTransmitting) {
    Serial.println("⚠️  Réception ignorée (en cours d'émission)");
    //   Radio.Rx(0);
    return;
  }
  isTransmitting = true;
  Serial.println("\n📥=== Données LoRa reçues de la passerelle ===");
  // NETTOYER le buffer avant de copier
  memset(rxpacket, 0, BUFFER_SIZE);

  // Copier les données reçues dans le buffer
  memcpy(rxpacket, payload, size);
  rxpacket[size] = '\0';
  if (isValidPacket(rxpacket)) {
    Serial.print("RSSI: ");
    Serial.print(rssi);
    Serial.print(" dBm, SNR: ");
    Serial.print(snr);
    Serial.print(" dB");
    Serial.print("   Taille: ");
    Serial.print(size);
    Serial.println(" octets");
    Serial.print("Contenu: ");
    Serial.println(rxpacket);
    Serial.println();

    // passe plat Envoyer les données reçues vers Esp2_serial
    forwardToEsp2Serial(rxpacket);
  } else {
    Serial.println("❌ Trame incomplète ou corrompue - Ignorée");
  }
  // Redémarrer la réception
  // Radio.Rx(0);
  isTransmitting = false;
}