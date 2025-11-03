//-- Modele ESP32: Lilygo T-display
//
//    ELECTROMAKERS
//
//  15/10/2025  YC V0.9 release
//----------------------------------
#include "Arduino.h"
#include <SD.h>
#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <BluetoothSerial.h>

// Fichiers headers
#include "pins.h"
#include "WebServerHandler.h"
#include "DisplayHandler.h"
#include "LoRaHandler.h"

// Variables globales
SensorData currentData;
BluetoothSerial SerialBT;

const char *bluetoothName = "Poseidon";
SPIClass spiLoRa(VSPI);  // Créer une instance SPI

#define TXPACKET_BUFFER_SIZE 100  // à ajuster

char txpacket[TXPACKET_BUFFER_SIZE];
extern void sendPacket(char[]);

bool modeWifi = false;  // false = Bluetooth , true = WiFi


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== Poseidon Receiver ===");
  // led
  pinMode(EMBEDDED_LED, OUTPUT);
  // bouton SW_BUTTON
  pinMode(WIFI_MODE_BUTTON, INPUT_PULLUP);

  // batterie
  analogReadResolution(12);  // Résolution 12 bits (0-4095)

  // Écran
  initDisplay();

  // Détermine le mode basé sur l'état du bouton WIFI_MODE_BUTTON
  if (digitalRead(WIFI_MODE_BUTTON) == LOW) {
    // Wifi pour le webServer
    initWebServer();
    displayStartupScreenWifiOnly();
    while (1) {  // on bloque tout le reste
      delay(2000);
      if (digitalRead(WIFI_MODE_BUTTON) == LOW) {
        Serial.println("📦 Reboot dans 2 secondes...");
        rebootDevice();
      }
    };
  } else {
    // Bluetooth
    if (!SerialBT.begin(bluetoothName)) {
      Serial.println("Bluetooth échoué");
    } else {
      Serial.println("Bluetooth OK");
      displayStartupScreen();
    }
  }

  // LoRa - Configuration alignée avec Heltec
  Serial.println("Init LoRa...");
  spiLoRa.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setSPI(spiLoRa);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(RF_FREQUENCY)) {
    Serial.println("LoRa échoué!");
  } else {
    // Configuration LoRa identique au Heltec
    LoRa.setFrequency(RF_FREQUENCY);
    LoRa.setSignalBandwidth(LORA_BANDWIDTH);
    LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
    LoRa.setCodingRate4(LORA_CODINGRATE);
    LoRa.setPreambleLength(LORA_PREAMBLE_LENGTH);
    LoRa.setTxPower(TX_OUTPUT_POWER);

    Serial.println("✅ LoRa OK - Paramètres unifiés");
    Serial.printf("📡 Fréquence: %.3f MHz\n", RF_FREQUENCY / 1e6);
    Serial.printf("🔊 Puissance TX: %d dBm\n", TX_OUTPUT_POWER);
    Serial.printf("🌊 Spreading Factor: SF%d\n", LORA_SPREADING_FACTOR);
    Serial.printf("🔧 Coding Rate: 4/%d\n", LORA_CODINGRATE + 4);
  }


  // Initialiser les données
  memset(&currentData, 0, sizeof(currentData));
  currentData.device_id = "Attente";
  currentData.temperature = -127.0;
  currentData.gps_updated = false;
  currentData.sensors_updated = false;
  currentData.last_update = 0;

  Serial.println("Système prêt");
  displayCurrentData();
}

void loop() {

  // Traiter LoRa
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    int rssi = LoRa.packetRssi();
    String message = "";

    while (LoRa.available()) {
      message += (char)LoRa.read();
    }
    processReceivedMessage(message, rssi);
  }

  // Vérifier données anciennes
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 30000) {
    lastCheck = millis();
    if (currentData.last_update > 0 && (millis() - currentData.last_update) > 120000) {
      currentData.gps_updated = false;
      currentData.sensors_updated = false;
      displayCurrentData();
    }
  }

  // serial Bluetooth envoi au Heltec
  if (SerialBT.hasClient()) {
    if (SerialBT.available()) {
      memset(txpacket, 0, TXPACKET_BUFFER_SIZE);  // init du buffer
      // Lecture des données Bluetooth
      int bytesRead = SerialBT.readBytesUntil('\n', txpacket, TXPACKET_BUFFER_SIZE - 1);
      txpacket[bytesRead] = '\0';  // Null-terminate la chaîne
      Serial.printf("📱 Données Bluetooth reçues: %s\n", txpacket);
      // Appel de la fonction avec les données reçues
      sendPacket(txpacket);
      // Vide tout ce qui reste dans le buffer
      while (SerialBT.available()) {
        SerialBT.read();
      }
    }
  }
  if (digitalRead(WIFI_MODE_BUTTON) == LOW) {
    Serial.println("Redémarrage en cours...");
    rebootDevice();
  }
  // delay(10);
}