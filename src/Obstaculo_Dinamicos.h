#ifndef OBSTACULO_DINAMICOS_H
#define OBSTACULO_DINAMICOS_H

#include <Arduino.h>

class Obstaculo_Dinamicos {
public:
  Obstaculo_Dinamicos();
  
  void detectar();      // Marca que se detectó una persona/animal
  bool fue_detectado(); // Devuelve true si hay un objetivo activo
  void reset();         // Limpia la detección

private:
  bool detectado;
};

#endif