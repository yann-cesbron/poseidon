// variable cap_actuel à mettre a jour
#include <Wire.h>

const int sdaPin = 21;  // (blanc)  sur module bousole 'rubys - LB compas' QMC5883L
const int sclPin = 22;  // (janne)
#define COMPASS_ADDR 0x2D

// Registres QMC5883L
#define QMC5883_REG_X_LSB 0x00
#define QMC5883_REG_X_MSB 0x01
#define QMC5883_REG_Y_LSB 0x02
#define QMC5883_REG_Y_MSB 0x03
#define QMC5883_REG_Z_LSB 0x04
#define QMC5883_REG_Z_MSB 0x05
#define QMC5883_REG_STATUS 0x06
#define QMC5883_REG_TEMP_LSB 0x07
#define QMC5883_REG_TEMP_MSB 0x08
#define QMC5883_REG_CONTROL1 0x09
#define QMC5883_REG_CONTROL2 0x0A
#define QMC5883_REG_PERIOD 0x0B
#define QMC5883_REG_CHIP_ID 0x0D


void setupQMC5883L() {
  Serial.println("\n3. CONFIGURATION QMC5883L...");

  // Configuration pour mode continu, 200Hz, ±8 Gauss, OSR=256
  Wire.beginTransmission(COMPASS_ADDR);
  Wire.write(QMC5883_REG_CONTROL1);
  Wire.write(0b01111001);  // Continuous, 200Hz, ±8G, OSR=256
  Wire.endTransmission();

  // Pas de reset soft
  Wire.beginTransmission(COMPASS_ADDR);
  Wire.write(QMC5883_REG_CONTROL2);
  Wire.write(0x00);  // No soft reset
  Wire.endTransmission();

  // Période de mesure (00 = recommandé)
  Wire.beginTransmission(COMPASS_ADDR);
  Wire.write(QMC5883_REG_PERIOD);
  Wire.write(0x01);  // Période standard
  Wire.endTransmission();

  Serial.println("   Configuration appliquée:");
  Serial.println("   - Mode: Continu");
  Serial.println("   - Fréquence: 200Hz");
  Serial.println("   - Gamme: ±8 Gauss");
  Serial.println("   - OSR: 256");
  Serial.println("   - Prêt pour lecture!");
}

void readCompassData() {
  // Vérifier si les données sont prêtes
  Wire.beginTransmission(COMPASS_ADDR);
  Wire.write(QMC5883_REG_STATUS);
  Wire.endTransmission();

  Wire.requestFrom((uint8_t)COMPASS_ADDR, (uint8_t)1);
  if (!Wire.available() || !(Wire.read() & 0x01)) {
    Serial.println("⏳ Données non prêtes...");
    return;
  }
}

void identifyQMC5883L() {
  Serial.println("\n1. IDENTIFICATION QMC5883L...");

  // Lecture du registre Chip ID
  Wire.beginTransmission(COMPASS_ADDR);
  Wire.write(QMC5883_REG_CHIP_ID);
  Wire.endTransmission();

  Wire.requestFrom((uint8_t)COMPASS_ADDR, (uint8_t)1);
  if (Wire.available()) {
    byte chipId = Wire.read();
    Serial.print("   Chip ID: 0x");
    if (chipId < 16) Serial.print("0");
    Serial.print(chipId, HEX);

    if (chipId == 0xFF) {
      Serial.println(" → QMC5883L CONFIRMÉ !");
    } else {
      Serial.println(" → Chip ID anormal");
    }
  }

  // Lecture de tous les registres de contrôle
  Serial.println("\n2. REGISTRES DE CONTRÔLE:");
  readRegister(QMC5883_REG_CONTROL1, "Control 1");
  readRegister(QMC5883_REG_CONTROL2, "Control 2");
  readRegister(QMC5883_REG_PERIOD, "Period");
  readRegister(QMC5883_REG_STATUS, "Status");
}

void readRegister(byte reg, const char* name) {
  Wire.beginTransmission(COMPASS_ADDR);
  Wire.write(reg);
  Wire.endTransmission();

  Wire.requestFrom((uint8_t)COMPASS_ADDR, (uint8_t)1);
  if (Wire.available()) {
    byte value = Wire.read();
    Serial.print("   ");
    Serial.print(name);
    Serial.print(" (0x");
    if (reg < 16) Serial.print("0");
    Serial.print(reg, HEX);
    Serial.print("): 0x");
    if (value < 16) Serial.print("0");
    Serial.print(value, HEX);
    Serial.print(" - B");
    for (int i = 7; i >= 0; i--) {
      Serial.print(bitRead(value, i));
      if (i == 4) Serial.print(" ");
    }
    Serial.println();

    // Interprétation
    interpretRegister(reg, value);
  }
}

void interpretRegister(byte reg, byte value) {
  switch (reg) {
    case QMC5883_REG_CONTROL1:
      Serial.print("        Mode: ");
      switch (value & 0b11) {
        case 0: Serial.println("Standby"); break;
        case 1: Serial.println("Continuous"); break;
      }
      Serial.print("        ODR: ");
      switch ((value >> 2) & 0b11) {
        case 0: Serial.println("10Hz"); break;
        case 1: Serial.println("50Hz"); break;
        case 2: Serial.println("100Hz"); break;
        case 3: Serial.println("200Hz"); break;
      }
      Serial.print("        Range: ");
      switch ((value >> 4) & 0b11) {
        case 0: Serial.println("±2 Gauss"); break;
        case 1: Serial.println("±8 Gauss"); break;
      }
      Serial.print("        OSR: ");
      switch ((value >> 6) & 0b11) {
        case 0: Serial.println("512"); break;
        case 1: Serial.println("256"); break;
        case 2: Serial.println("128"); break;
        case 3: Serial.println("64"); break;
      }
      break;

    case QMC5883_REG_CONTROL2:
      Serial.print("        Soft Reset: ");
      Serial.println((value & 0x80) ? "Yes" : "No");
      Serial.print("        ROL_PNT: ");
      Serial.println((value & 0x40) ? "Set" : "Clear");
      Serial.print("        INT_ENB: ");
      Serial.println((value & 0x01) ? "Enabled" : "Disabled");
      break;

    case QMC5883_REG_STATUS:
      Serial.print("        DRDY: ");
      Serial.println((value & 0x01) ? "Data Ready" : "Not Ready");
      Serial.print("        OVL: ");
      Serial.println((value & 0x02) ? "Overflow" : "Normal");
      Serial.print("        DOR: ");
      Serial.println((value & 0x04) ? "Data Skipped" : "OK");
      break;
  }
}

double getAngleBousole() {
  Wire.beginTransmission(COMPASS_ADDR);
  Wire.write(0x00);  // Premier registre données
  Wire.endTransmission();

  Wire.requestFrom(COMPASS_ADDR, 6);
  if (Wire.available() >= 6) {
    int16_t x = Wire.read() | (Wire.read() << 8);
    int16_t y = Wire.read() | (Wire.read() << 8);
    int16_t z = Wire.read() | (Wire.read() << 8);

    Serial.println("\n📊 DONNÉES COMPAS:");
    Serial.println("-------------------");
    Serial.print("X: ");
    Serial.print(x);
    Serial.print(" LSB");
    Serial.print("  |  Y: ");
    Serial.print(y);
    Serial.print(" LSB");
    Serial.print("  |  Z: ");
    Serial.print(z);
    Serial.println(" LSB");

    // Conversion en Gauss (±8 Gauss = 4096 LSB/Gauss)
    float gaussX = x / 4096.0;
    float gaussY = y / 4096.0;
    float gaussZ = z / 4096.0;

    Serial.print("X: ");
    Serial.print(gaussX, 4);
    Serial.print(" G");
    Serial.print("  |  Y: ");
    Serial.print(gaussY, 4);
    Serial.print(" G");
    Serial.print("  |  Z: ");
    Serial.print(gaussZ, 4);
    Serial.println(" G");

    // Calcul orientation
    float heading = atan2(y, x) * 180.0 / PI;
    if (heading < 0) heading += 360;

    Serial.print("🎯 Orientation: ");
    Serial.print(heading, 1);
    Serial.print("° - ");

    // Direction cardinale
    if (heading < 22.5 || heading >= 337.5) Serial.println("NORD");
    else if (heading < 67.5) Serial.println("NORD-EST");
    else if (heading < 112.5) Serial.println("EST");
    else if (heading < 157.5) Serial.println("SUD-EST");
    else if (heading < 202.5) Serial.println("SUD");
    else if (heading < 247.5) Serial.println("SUD-OUEST");
    else if (heading < 292.5) Serial.println("OUEST");
    else Serial.println("NORD-OUEST");

    // float angle = atan2(y, x) * 180.0 / PI;
    // if (angle < 0) angle += 360;
    Serial.print("angle Bousole: ");
    Serial.println(heading, 0);
    return heading;
  }
  Serial.println("erreur angle Bousole");
  return 0.0;
}

void initBousole() {
  if (Wire.begin(sdaPin, sclPin)) {
    // Wire.setClock(10000);  // 50kHz - plus lent pour stabilité
    //                        // Wire.setClock(10000);   // 10kHz
    //                        // Wire.setClock(100000);  // 100kHz
    // Wire.setTimeOut(100);  // Timeout plus court
    // Wire.setTimeOut(1000);  // Timeout en ms pour ESP32
    // Attendre que le composant soit prêt
    Serial.println("  init I2c OK");
  } else {
    Serial.println("  erreur init I2c");
  }
  delay(1000);

  // Identification du composant
  identifyQMC5883L();
  // Configuration et test
  setupQMC5883L();
  delay(500);
  Serial.println("Compass Roborock Ruby - Orientation");
}
