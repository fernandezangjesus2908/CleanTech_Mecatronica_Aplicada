#ifndef RECUPERACION_RECOLECCION_H
#define RECUPERACION_RECOLECCION_H

#include <Arduino.h>

class RecuperacionRecoleccion {
public:
  RecuperacionRecoleccion();
  
  void iniciar(float x, float y);
  bool actualizar(unsigned long ms_actual);
  
  bool hay_marcador() { return marker_active; }
  float get_x() { return marker_x; }
  float get_y() { return marker_y; }
  void reset();

private:
  bool marker_active;
  float marker_x;
  float marker_y;
  unsigned long tiempo_inicio;
};

#endif