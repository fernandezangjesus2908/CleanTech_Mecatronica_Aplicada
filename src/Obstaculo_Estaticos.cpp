#include "Obstaculo_Estaticos.h"

Obstaculo_Estaticos::Obstaculo_Estaticos() {
  detectado = false;
}

void Obstaculo_Estaticos::detectar() {
  detectado = true;
}

bool Obstaculo_Estaticos::fue_detectado() {
  return detectado;
}

void Obstaculo_Estaticos::reset() {
  detectado = false;
}