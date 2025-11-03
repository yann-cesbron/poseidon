#ifndef WEBSERVERHANDLER_H
#define WEBSERVERHANDLER_H

#include "pins.h"
#include <WiFi.h>
#include <SD.h>
#include <SPI.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);
SPIClass sdSPI(HSPI);  // HSPI pour SD card

const char *ssid = "poseidon";
const char *password = "Poseidon";
bool shouldReboot = false;
extern void rebootDevice();

// Fonction pour déterminer le type de contenu
String getContentType(String filename) {
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".apk")) return "application/vnd.android.package-archive";
  return "text/plain";
}

// Fonction pour servir les fichiers depuis la SD
bool serveFileFromSD(String path, AsyncWebServerRequest *request) {
  if (SD.exists(path)) {
    String contentType = getContentType(path);
    request->send(SD, path, contentType);
    Serial.printf("✓ Fichier servi: %s (%s)\n", path.c_str(), contentType.c_str());
    return true;
  } else {
    Serial.printf("✗ Fichier non trouvé: %s\n", path.c_str());
    return false;
  }
}

// Fonction pour lister le contenu d'un dossier (debug)
void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("📁 Listing du dossier: %s\n", dirname);
  File root = fs.open(dirname);
  if (!root || !root.isDirectory()) {
    Serial.println("❌ Échec d'ouverture du dossier");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.printf("   📂 %s/\n", file.name());
      if (levels) listDir(fs, file.path(), levels - 1);
    } else {
      Serial.printf("   📄 %s  (%d bytes)\n", file.name(), file.size());
    }
    file = root.openNextFile();
  }
}


void initWebServer() {
  Serial.println("Initialisation serveur web Async...");

  // Initialisation SD Card
  Serial.println("Init SD...");
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, sdSPI)) {
    Serial.println("❌ SD Card absente");
    return;
  }

  Serial.println("✅ SD Card OK");
  listDir(SD, "/", 2);
  delay(1000);

  // Démarrer le point d'accès
  WiFi.softAP(ssid, password);
  delay(500);
  Serial.println("✅ softAP OK");
  Serial.print("AP WiFi: ");
  Serial.println(ssid);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  // Page principale
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    serveFileFromSD("/web/index.html", request);
  });

  // Fichiers statiques (HTML, CSS, JS, ICO)
  server.on("/web/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    serveFileFromSD("/web/style.css", request);
  });
  server.on("/web/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
    serveFileFromSD("/web/favicon.ico", request);
  });

  // Téléchargement APK
  server.on("/apk", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (SD.exists("/poseidon.apk")) {
      AsyncWebServerResponse *response = request->beginResponse(SD, "/poseidon.apk", "application/vnd.android.package-archive");
      response->addHeader("Content-Disposition", "attachment; filename=\"poseidon.apk\"");
      // Rebooter quand le client se déconnecte (téléchargement terminé)
      request->onDisconnect([]() {
        Serial.println("📦 Téléchargement terminé - Reboot dans 2 secondes...");
        delay(2000);
        rebootDevice() ;
      });
      request->send(response);
      Serial.println("📦 Téléchargement APK envoyé");
    } else {
      request->send(404, "text/plain", "Fichier poseidon.apk non trouvé");
    }
  });

  //  reboot
  server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("📦 Reboot immediat...");
        rebootDevice() ;
  });

  // Page de statut système
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<title>Statut Serveur Poseidon</title>";
    html += "<style>body{font-family:Arial,sans-serif;margin:20px;background:#f0f8ff}</style></head>";
    html += "<body><div style='background:white;padding:20px;border-radius:10px;max-width:500px;margin:auto;text-align:center'>";
    html += "<h1>🌍 Serveur Poseidon</h1>";
    html += "<p><strong>SSID:</strong> " + String(ssid) + "</p>";
    html += "<p><strong>IP:</strong> " + WiFi.softAPIP().toString() + "</p>";
    html += "<p><strong>Clients connectés:</strong> " + String(WiFi.softAPgetStationNum()) + "</p>";

    // Informations système
    html += "<div style='margin:20px 0;padding:15px;background:#f8f9fa;border-radius:8px;text-align:left'>";
    html += "<h3>📊 Statut Système</h3>";
    html += "<p><strong>SD Card:</strong> ✅ OK</p>";
    html += "<p><strong>Web Server:</strong> ✅ Async</p>";
    html += "<p><strong>Free Heap:</strong> " + String(esp_get_free_heap_size()) + " bytes</p>";
    html += "</div>";

    html += "<p><a href='/'>🏠 Page principale</a> | ";
    html += "<a href='/file'>📱 Télécharger APK</a></p>";
    html += "</div></body></html>";
    request->send(200, "text/html", html);
  });

  // API pour les données en temps réel (optionnel)
  // server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request) {
  //   String json = "{";
  //   json += "\"gps_updated\":" + String(currentData.gps_updated ? "true" : "false") + ",";
  //   json += "\"sensors_updated\":" + String(currentData.sensors_updated ? "true" : "false") + ",";
  //   json += "\"latitude\":" + String(currentData.latitude, 6) + ",";
  //   json += "\"longitude\":" + String(currentData.longitude, 6) + ",";
  //   json += "\"altitude\":" + String(currentData.altitude, 1) + ",";
  //   json += "\"satellites\":" + String(currentData.satellites) + ",";
  //   json += "\"temperature\":" + String(currentData.temperature, 1) + ",";
  //   json += "\"ph\":" + String(currentData.ph, 1) + ",";
  //   json += "\"turbidity\":" + String(currentData.turbidity, 1) + ",";
  //   json += "\"resistivity\":" + String(currentData.resistivity, 0) + ",";
  //   json += "\"battery\":" + String(currentData.battery) + ",";
  //   json += "\"rssi\":" + String(currentData.rssi);
  //   json += "}";

  //   request->send(200, "application/json", json);
  // });

  // Gestionnaire pour les URLs non trouvées
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "404 - Fichier non trouvé");
  });

  // Démarrer le serveur
  server.begin();
  Serial.println("✅ Serveur Web démarré");
}

#endif