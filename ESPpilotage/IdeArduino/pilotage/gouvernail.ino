// ==========================================================
// FICHIER: Gouvernail.ino
// Contient le contrôle du servomoteur et la logique de navigation.
// Ce fichier DOIT être inclus dans le même dossier que le fichier principal.
// ==========================================================

#include <ESP32Servo.h>
#include <math.h>  // Pour fmod, sin, cos, atan2 dans Navigation.ino

// --- Constantes et Variables du Servomoteur de direction et moteur---
//const int PIN_SERVO = 13;                // Broche GPIO pour le servomoteur (Gouvernail)
const int SERVO_ANGLE_MIN = 30;          // Angle minimum (ex: gouvernail tout à bâbord)
const int SERVO_ANGLE_MAX = 150;         // Angle maximum (ex: gouvernail tout à tribord)
const int SERVO_ANGLE_CENTRE = 90;       // Angle neutre (gouvernail droit)
const double MAX_CORRECTION_CAP = 45.0;  // Max erreur de cap prise en compte pour la correction (en degrés)

// const int PIN_SERVO_MOTEUR  = 0;
const int SERVO_MOTEUR_OFF = 0;
const int SERVO_MOTEUR_ON = 180;

// Instance du Servomoteur
Servo monGouvernail;
Servo monMoteur;

// --- Variable Globale de Simulation/Stockage du Cap Actuel ---
// cette variable est mise à jour par la lecture du cap bousole
double cap_actuel = 0.0;


/**
 * Initialise le servomoteur pour le gouvernail.
 * Doit être appelé dans setup().
 */
void initialiserServos() {
  //  servo
  monGouvernail.attach(PIN_SERVO_GOUVERNAIL);
  monGouvernail.write(SERVO_ANGLE_CENTRE);  // Centrer le gouvernail au démarrage
  Serial.print("Servomoteur initialisé sur GPIO ");
  Serial.print(PIN_SERVO_GOUVERNAIL);
  Serial.print(" à ");
  Serial.print(SERVO_ANGLE_CENTRE);
  Serial.println(" degrés.");
  // moteur
  monMoteur.attach(PIN_SERVO_MOTEUR);
  monMoteur.write(SERVO_MOTEUR_OFF);  // arreter le moteur au demarrage
  // pinMode(PIN_MOTOR, OUTPUT);
  // digitalWrite(PIN_MOTOR, LOW);
}


/**
 * Calcule l'erreur de cap et définit l'angle du servomoteur pour corriger la direction.
 * @param capVersWP Direction calculée (bearing) de GPS vers WP (0-360°).
 */
void ajusterGouvernail(double capVersWP) {
  // 0. lecture angle bousole
  cap_actuel = getAngleBousole();

  // 1. Calcul de l'Erreur de Cap
  // Erreur = Cap Cible - Cap Actuel
  double erreurCap = capVersWP - cap_actuel;

  // Normalisation de l'erreur entre -180° (tourner à gauche) et 180° (tourner à droite)
  if (erreurCap > 180.0) {
    erreurCap -= 360.0;
  } else if (erreurCap < -180.0) {
    erreurCap += 360.0;
  }

  // 2. Limitation de la Correction
  // Nous limitons l'erreur maximale pour le mappage
  double correction = erreurCap;
  correction = constrain(correction, -MAX_CORRECTION_CAP, MAX_CORRECTION_CAP);

  // 3. Mappage de l'Erreur au Servomoteur
  // On mappe la correction (ex: -45° à +45°) à l'angle du servo (ex: 30° à 150°).
  // Note: Nous utilisons la fonction map avec des long, donc nous multiplions les doubles par 100.

  int angleServo = (int)map((long)(correction * 100),
                            (long)(-MAX_CORRECTION_CAP * 100), (long)(MAX_CORRECTION_CAP * 100),
                            SERVO_ANGLE_MIN, SERVO_ANGLE_MAX);

  // S'assurer que l'angle reste dans les limites de sécurité
  angleServo = constrain(angleServo, SERVO_ANGLE_MIN, SERVO_ANGLE_MAX);

  // 4. Commande du Servomoteur de direction
  monGouvernail.write(angleServo);

  // 5. Affichage (Debug)
  Serial.println("--- CONTRÔLE GOUVERNAIL ---");
  Serial.print("Cap Actuel bousole: ");
  Serial.print(cap_actuel, 1);
  Serial.println("°");
  Serial.print("Cap Vers WP (Cible): ");
  Serial.print(capVersWP, 1);
  Serial.println("°");
  Serial.print("Correction: ");
  Serial.print(erreurCap, 1);
  Serial.println("°");
  Serial.print("Angle Servo (Mappé): ");
  Serial.print(angleServo);
  Serial.println("°");
  Serial.println("--------------------------");
}

// cotrole moteur
void set_motor(bool state) {
  // on/off
  if (state == true) {
    Serial.println("moteur on");
    monMoteur.write(SERVO_MOTEUR_ON);
  } else {
    monMoteur.write(SERVO_MOTEUR_OFF);
    Serial.println("moteur off");
  }
  // digitalWrite(PIN_MOTOR, state);
}
