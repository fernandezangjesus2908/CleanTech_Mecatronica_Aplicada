#include "RecuperacionRecoleccion.h"

RecuperacionRecoleccion::RecuperacionRecoleccion() {
  marker_active = false;
  marker_x = 0.0;
  marker_y = 0.0;
  tiempo_inicio = 0;
}

void RecuperacionRecoleccion::iniciar(float x, float y) {
  marker_active = true;
  marker_x = x;
  marker_y = y;
  tiempo_inicio = millis();
}

bool RecuperacionRecoleccion::actualizar(unsigned long ms_actual) {
  if (marker_active) {
    // Pausa de 1 segundo para registrar la coordenada y avisar a la app
    if (ms_actual - tiempo_inicio > 1000) {
      return false; // Terminó, listo para volver a ruta
    }
    return true; // Sigue en pausa
  }
  return false;
}

void RecuperacionRecoleccion::reset() {
  marker_active = false;
}