#include "FallaRecoleccion.h"

FallaRecoleccion::FallaRecoleccion() {
  estado = FALLA_INACTIVA;
  intentos = 0;
  exito = false;
  angulo_previo = 0.0;
  angulo_girado = 0.0;
  tiempo_inicio_espera = 0;
}

void FallaRecoleccion::iniciar(float heading_actual) {
  estado = FALLA_GIRANDO_180;
  angulo_previo = heading_actual;
  angulo_girado = 0.0;
  exito = false;
}

void FallaRecoleccion::confirmar_fallo() {
  if (estado == FALLA_ESPERANDO_IA) {
    intentos++;
    exito = false;
    if (intentos < 3) {
      estado = FALLA_REINTENTANDO;
      angulo_girado = 0.0;
    } else {
      estado = FALLA_INACTIVA; // Pasa a Recuperacion_Recoleccion
    }
  }
}

void FallaRecoleccion::confirmar_exito() {
  if (estado == FALLA_ESPERANDO_IA) {
    exito = true;
    estado = FALLA_INACTIVA; // Regresa a ruta
  }
}

bool FallaRecoleccion::actualizar(float heading_actual, unsigned long ms_actual, bool& girar_izq, bool& girar_der) {
  girar_izq = false; girar_der = false;

  if (estado == FALLA_GIRANDO_180) {
    float diff = heading_actual - angulo_previo;
    while (diff > 180.0) diff -= 360.0;
    while (diff < -180.0) diff += 360.0;

    angulo_girado += fabs(diff);
    angulo_previo = heading_actual;

    if (angulo_girado >= 175.0) {
      estado = FALLA_ESPERANDO_IA;
      tiempo_inicio_espera = ms_actual;
    } else {
      girar_izq = true;
    }
    return true;
  }
  else if (estado == FALLA_ESPERANDO_IA) {

    if (ms_actual - tiempo_inicio_espera > 5000) {
      confirmar_fallo();
      if (estado == FALLA_REINTENTANDO) {
         angulo_previo = heading_actual; // Empezar a medir el giro de vuelta
      }
    }
    return (estado != FALLA_INACTIVA);
  }
  else if (estado == FALLA_REINTENTANDO) {
    float diff = heading_actual - angulo_previo;
    while (diff > 180.0) diff -= 360.0;
    while (diff < -180.0) diff += 360.0;

    angulo_girado += fabs(diff);
    angulo_previo = heading_actual;

    if (angulo_girado >= 175.0) {
      estado = FALLA_INACTIVA;
      return false; // Terminó, listo para recolectar de nuevo
    } else {
      girar_der = true;
    }
    return true;
  }
  return false;
}