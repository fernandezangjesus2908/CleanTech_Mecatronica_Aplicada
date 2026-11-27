#ifndef OBSTACULO_ESTATICOS_H
#define OBSTACULO_ESTATICOS_H

#include <Arduino.h>

class Obstaculo_Estaticos {
public:
  Obstaculo_Estaticos();
  
  void detectar();      // Marca que se detectó un obstáculo
  bool fue_detectado(); // Devuelve true si hay un obstáculo activo
  void reset();         // Limpia la detección

private:
  bool detectado;
};

#endif