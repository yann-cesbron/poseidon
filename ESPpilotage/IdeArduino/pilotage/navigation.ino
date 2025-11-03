// ==========================================================
// FICHIER: Navigation.ino
// Contient les fonctions de calcul de distance et de cap.
// ==========================================================

// Constante pour le rayon de la Terre en mètres (approximation WGS84)
// Utilisée pour la formule de Haversine.
const double RAYON_TERRE_M = 6371000.0;

/**
 * Convertit un angle en degrés en radians.
 */
double degToRad(double degrees) {
  return degrees * PI / 180.0;
}

/**
 * Convertit un angle en radians en degrés.
 */
double radToDeg(double radians) {
  return radians * 180.0 / PI;
}

/**
 * Calcule la distance entre deux points GPS en utilisant la formule de Haversine.
 * @param lat1 Latitude du point 1 (en degrés).
 * @param lon1 Longitude du point 1 (en degrés).
 * @param lat2 Latitude du point 2 (en degrés).
 * @param lon2 Longitude du point 2 (en degrés).
 * @return Distance en mètres.
 */
double calculerDistance(double lat1, double lon1, double lat2, double lon2) {
  // Conversion des degrés en radians
  lat1 = degToRad(lat1);
  lon1 = degToRad(lon1);
  lat2 = degToRad(lat2);
  lon2 = degToRad(lon2);

  // Différences
  double dLat = lat2 - lat1;
  double dLon = lon2 - lon1;

  // Formule de Haversine
  double a = sin(dLat / 2) * sin(dLat / 2) +
             cos(lat1) * cos(lat2) *
             sin(dLon / 2) * sin(dLon / 2);
  
  double c = 2 * atan2(sqrt(a), sqrt(1 - a));

  return RAYON_TERRE_M * c;
}


/**
 * Calcule la direction initiale (cap ou "bearing") entre deux points GPS.
 * @param lat1 Latitude du point de départ (en degrés).
 * @param lon1 Longitude du point de départ (en degrés).
 * @param lat2 Latitude du point d'arrivée (en degrés).
 * @param lon2 Longitude du point d'arrivée (en degrés).
 * @return Cap en degrés (0-360, Nord = 0).
 */
double calculerCap(double lat1, double lon1, double lat2, double lon2) {
  // Conversion des degrés en radians
  lat1 = degToRad(lat1);
  lon1 = degToRad(lon1);
  lat2 = degToRad(lat2);
  lon2 = degToRad(lon2);

  // Différences de longitude
  double dLon = lon2 - lon1;

  // Calcul du cap (bearing)
  double y = sin(dLon) * cos(lat2);
  double x = cos(lat1) * sin(lat2) -
             sin(lat1) * cos(lat2) * cos(dLon);
  
  double capRad = atan2(y, x);

  // Conversion en degrés et normalisation à 0-360
  double capDeg = radToDeg(capRad);

  return fmod((capDeg + 360.0), 360.0);
}