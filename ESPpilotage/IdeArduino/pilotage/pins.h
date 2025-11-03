#ifndef PINS_H
#define PINS_H

// Composant,Broche ESP32 DevKit V1
// Bouton,GPIO 15,Entrée,Câbler le bouton entre GPIO 15 et GND. Le code utilise INPUT_PULLUP.
// RX de la Communication,GPIO 16 (RX2),Entrée Série,Câbler au TX de l'appareil externe.
// TX de la Communication,GPIO 17 (TX2),Sortie Série,Câbler au RX de l'appareil externe.
// i2c  SCL 22 , SDA 21
// led
#define PIN_LED_RX 2

// servo gouvernail
#define PIN_SERVO_GOUVERNAIL 13
#define PIN_SERVO_MOTEUR 12

// buton
#define PIN_MODE 15  // pour le mode A ou M

#define PIN_BATTERIE 4

//  RX/TX  sur serial2
#define PIN_RX2 16  // à connecter à pin 45 du heltec
#define PIN_TX2 17  // à connecter à pin 46
#endif
