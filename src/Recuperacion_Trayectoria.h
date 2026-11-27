#ifndef RECUPERACION_TRAYECTORIA_H
#define RECUPERACION_TRAYECTORIA_H

#include <Arduino.h>

enum EstadoRecTrayectoria {
  REC_TRAY_INACTIVO,
  REC_TRAY_GIRANDO_180,
  REC_TRAY_ESPERANDO_IA
};

class Recuperacion_Trayectoria {
public:
  Recuperacion_Trayectoria();
  void iniciar(float headingActual);
  bool actualizar(float headingActual, unsigned long msActual, bool& girarIzq);
  void confirmar_exito();
  void reset();

  // NUEVO: para distinguir éxito (IA vio el residuo) de timeout (no lo vio)
  bool fue_exitoso() const { return exito; }

private:
  EstadoRecTrayectoria estado;
  float anguloInicial;   // histórico (no usado en la versión incremental, se conserva por claridad)
  float anguloPrevio;    // NUEVO: referencia para acumulación incremental
  float anguloGirado;
  unsigned long tiempoInicioEspera;
  bool exito;            // NUEVO
};

#endif