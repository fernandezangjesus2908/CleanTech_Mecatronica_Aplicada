#ifndef CAMBIO_RUTA_DINAMICOS_H
#define CAMBIO_RUTA_DINAMICOS_H

#include <Arduino.h>

enum EstadoCambioDinamico {
  CAMBIO_DINAMICO_INACTIVO,
  CAMBIO_DINAMICO_ESCANEANDO
};

class Cambio_Ruta_Dinamicos {
public:
  Cambio_Ruta_Dinamicos();
  
  void iniciar_escaneo(float headingActual);
  bool actualizar_escaneo(unsigned long msActual, float headingActual, float& anguloObjetivo, bool& girarIzquierda);
  void objetivo_perdido();
  void reset();

private:
  EstadoCambioDinamico estado;
  unsigned long ultimoCambioMs;
  bool escaneandoIzquierda;
  float anguloBase;
  unsigned long inicioEscaneoMs; // NUEVO: Timeout de escaneo
};

#endif