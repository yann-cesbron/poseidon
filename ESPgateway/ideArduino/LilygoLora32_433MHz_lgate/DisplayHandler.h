#ifndef DISPLAYHANDLER_H
#define DISPLAYHANDLER_H

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BluetoothSerial.h>
#include "pins.h"

// Configuration de l'écran OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
extern BluetoothSerial SerialBT;
bool flagAffHeure = false;

// Structure des données
struct SensorData {
  // Données GPS
  float latitude;
  float longitude;
  float altitude;
  int satellites;
  float hdop;
  int hour;
  int minute;
  int second;
  int year;
  int month;
  int day;
  long gps_timestamp;

  // Données capteurs
  float temperature;
  float turbidity;
  float resistivity;
  float ph;
  int battery;
  long sensors_timestamp;

  String device_id;
  int rssi;
  bool gps_updated;
  bool sensors_updated;
  unsigned long last_update;
};

extern SensorData currentData;
// extern BluetoothSerial SerialBT;

// Fonctions d'affichage
void initDisplay();
void displayStartupScreen();
void displayCurrentData();
void displayError(String errorMsg);

// Initialisation de l'écran
void initDisplay() {
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Échec allocation SSD1306"));
    // Ne pas bloquer, continuer sans écran
    return;
  }
  Serial.println("Écran OLED initialisé");
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(9, 0);
  display.println("POSEIDON");
  display.setTextSize(1);
  display.setCursor(9, 25);
  display.println("LilyGO T3 V1.6.1");
}

// Écran de démarrage
void displayStartupScreen() {
  display.setTextSize(1);
  display.setCursor(10, 50);
  display.println("Bluetooth ok");
  display.setCursor(6, 40);
  // display.println("GPS+SEN Receiver");
  display.println("Passerelle GPS+SEN");
  display.display();
  delay(5000);
}

void displayStartupScreenWifiOnly() {
  display.setTextSize(1);
  display.setCursor(9, 45);
  display.println("  Wifi OK");
  display.setCursor(15, 55);
  display.println("Mode web server");
  display.display();
  // delay(2000);
}

// Affichage des données courantes
void displayCurrentData() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  int line = 0;
  // Ligne 0: ID et heure  ou batt%
  display.setCursor(0, line);
  display.print("Poseidon");
  display.setCursor(55, line);
  if (flagAffHeure) {
    String timeStr = "---:--:--";
    if (currentData.gps_updated) {
      timeStr = String(currentData.hour) + ":" + (currentData.minute < 10 ? "0" : "") + String(currentData.minute) + ":" + (currentData.second < 10 ? "0" : "") + String(currentData.second);
    }
    display.print(timeStr);
    flagAffHeure = false;
  } else {
    // tension batterie
    int adcValue = analogRead(BATTERY_PIN);
    float batteryVoltage = (adcValue * 3.3 / 4095) * 2;  // Conversion avec facteur 2
    float voltage_percent = ((constrain(batteryVoltage, 3.3, 4.2) - 3.3) / (4.2 - 3.3)) * 100;
    // Serial.print("bat: ");
    // Serial.print(batteryVoltage, 1);
    // Serial.print("v ");
    // Serial.print(voltage_percent, 1);
    // Serial.println("%");

    display.print("B:");
    display.print(voltage_percent, 1);
    display.print("%");
    flagAffHeure = true;
  }

  line += 10;
  // Ligne 1: Latitude (si GPS disponibles) et RSSI
  if (currentData.gps_updated && currentData.latitude != -1.0 && currentData.longitude != -1.0) {
    display.setCursor(0, line);
    display.print("Lat:");
    display.print(currentData.latitude, 6);
    display.print("  ");
    display.print(currentData.rssi);
    display.print("dBm");
    line += 10;

    // Ligne 2: Longitude (si GPS disponibles) et Nb satellites
    display.setCursor(0, line);
    display.print("Lon:");
    display.print(currentData.longitude, 6);
    display.print("  Sat:");
    display.print(currentData.satellites);
    line += 10;

    // Ligne 3: Altitude et niveau batterie
    display.setCursor(0, line);
    display.print("Alt:");
    display.print(currentData.altitude, 1);
    display.print("m  Bat:");
    display.print(currentData.battery);
    display.print("%");
    line += 10;
  } else {
    display.setCursor(0, line);
    display.print("Attente GPS.");
    line += 10;
    display.setCursor(0, line);
    display.print("                ");
    line += 10;
    display.setCursor(0, line);
    display.print("                ");
    line += 10;
  }

  // Ligne 4: Température et pH (si disponibles)
  if (currentData.sensors_updated) {
    display.setCursor(0, line);
    display.print("Temp:");
    display.print(currentData.temperature, 1);
    display.print("C  pH:");
    display.print(currentData.ph, 1);
    line += 10;

    // Ligne 5: Turbidité et résistivité (si disponibles)
    display.setCursor(0, line);
    display.print("Turb:");
    display.print(currentData.turbidity, 1);
    display.print("%  Res:");
    display.print(currentData.resistivity, 0);
    display.print(" ");
  } else {
    display.setCursor(0, line);
    display.print("Attente Capteurs.");
    line += 10;
    display.setCursor(0, line);
    display.print("                    ");
  }

  // Indicateurs de mise à jour (en haut à droite)
  display.setCursor(110, 0);
  display.print(currentData.gps_updated ? "G" : "_");
  display.setCursor(118, 0);
  display.print(currentData.sensors_updated ? "S" : "_");

  // Indicateur de fraîcheur des données
  if (currentData.last_update > 0) {
    unsigned long dataAge = (millis() - currentData.last_update) / 1000;
    if (dataAge > 60) {
      display.setCursor(20, 58);
      display.print("OLD datas");
    }
  }

  // Indicateur de connexion Bluetooth
  display.setCursor(118, 58);
  if (SerialBT.hasClient()) {
    display.print("#");
  } else {
    display.print(" ");
  }

  // // Indicateur serveur web
  // display.setCursor(0, 58);
  // display.print("WEB");

  display.display();
}

// Affichage d'erreur
void displayError(String errorMsg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("ERREUR:");
  display.setCursor(0, 12);
  display.println(errorMsg);
  display.setCursor(0, 30);
  display.println("Verifier:");
  display.setCursor(0, 40);
  display.println("- Emetteur");
  display.setCursor(0, 50);
  display.println("- Parametres LoRa");

  display.display();
}

void rebootDevice() {
  Serial.println("*** reboot");
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 25);
  //display.print("*REBOOT*");
  display.print("RESTART!");
  display.display();
  delay(2000);
  ESP.restart();
}

#endif