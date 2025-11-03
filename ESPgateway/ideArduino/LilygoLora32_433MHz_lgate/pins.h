#ifndef PINS_H
#define PINS_H


// led
#define EMBEDDED_LED 25

// buton at start, enable wifi for web server & disable BT
#define WIFI_MODE_BUTTON 0

// Batterie analog
#define BATTERY_PIN 35

// =======================================  Carte SD (HSPI) ==============================
#define SD_CS    13
#define SD_MISO  2
#define SD_MOSI  15
#define SD_SCK   14

// =======================================  LoRa (VSPI) ==================================
#define LORA_SS   18
#define LORA_RST  23
#define LORA_DIO0 26
#define LORA_SCK  5
#define LORA_MISO 19
#define LORA_MOSI 27

// =======================================  Display OLED =================================
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_RST -1
#endif