#include "Recuperacion_Trayectoria.h"

Recuperacion_Trayectoria::Recuperacion_Trayectoria() {
  estado = REC_TRAY_INACTIVO;
  anguloInicial = 0.0;
  anguloPrevio = 0.0;
  anguloGirado = 0.0;
  tiempoInicioEspera = 0;
  exito = false;
}

void Recuperacion_Trayectoria::iniciar(float headingActual) {
  estado = REC_TRAY_GIRANDO_180;
  anguloInicial = headingActual;
  anguloPrevio = headingActual;
  anguloGirado = 0.0;
  exito = false;
}

bool Recuperacion_Trayectoria::actualizar(float headingActual, unsigned long msActual, bool& girarIzq) {
  girarIzq = false;

  if (estado == REC_TRAY_GIRANDO_180) {
  
    float diff = headingActual - anguloPrevio;
    while (diff >  180.0) diff -= 360.0;
    while (diff < -180.0) diff += 360.0;

    anguloGirado += fabs(diff);
    anguloPrevio = headingActual;

    if (anguloGirado >= 175.0) {
      estado = REC_TRAY_ESPERANDO_IA;
      tiempoInicioEspera = msActual;
    } else {
      girarIzq = true; // Gira buscando el residuo
    }
    return true;
  }
  else if (estado == REC_TRAY_ESPERANDO_IA) {

    if (msActual - tiempoInicioEspera > 5000) {
      estado = REC_TRAY_INACTIVO;
      exito = false;
      return false;
    }
    return true;
  }
  return false;
}

void Recuperacion_Trayectoria::confirmar_exito() {
  if (estado == REC_TRAY_ESPERANDO_IA) {
    estado = REC_TRAY_INACTIVO;
    exito = true;   
  }
}

void Recuperacion_Trayectoria::reset() {
  estado = REC_TRAY_INACTIVO;
  exito = false;
}