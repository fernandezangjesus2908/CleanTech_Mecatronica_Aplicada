#include "Obstaculo_Dinamicos.h"

Obstaculo_Dinamicos::Obstaculo_Dinamicos() {
  detectado = false;
}

void Obstaculo_Dinamicos::detectar() {
  detectado = true;
}

bool Obstaculo_Dinamicos::fue_detectado() {
  return detectado;
}

void Obstaculo_Dinamicos::reset() {
  detectado = false;
}