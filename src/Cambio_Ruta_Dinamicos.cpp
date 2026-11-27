#include "Cambio_Ruta_Dinamicos.h"

Cambio_Ruta_Dinamicos::Cambio_Ruta_Dinamicos() {
  estado = CAMBIO_DINAMICO_INACTIVO;
  ultimoCambioMs = 0;
  escaneandoIzquierda = true;
  anguloBase = 0.0;
  inicioEscaneoMs = 0;
}

void Cambio_Ruta_Dinamicos::iniciar_escaneo(float headingActual) {
  estado = CAMBIO_DINAMICO_ESCANEANDO;
  anguloBase = headingActual;
  ultimoCambioMs = millis();
  inicioEscaneoMs = millis(); // FIX
  escaneandoIzquierda = true;
}

bool Cambio_Ruta_Dinamicos::actualizar_escaneo(unsigned long msActual, float headingActual, float& anguloObjetivo, bool& girarIzquierda) {
  if (estado == CAMBIO_DINAMICO_ESCANEANDO) {
    // FIX: Timeout de 10 segundos para evitar escaneo infinito
    if (msActual - inicioEscaneoMs > 10000) {
      objetivo_perdido();
      return false;
    }
    
    if (msActual - ultimoCambioMs > 2000) {
      ultimoCambioMs = msActual;
      escaneandoIzquierda = !escaneandoIzquierda;
    }
    
    girarIzquierda = escaneandoIzquierda;
    anguloObjetivo = anguloBase + (escaneandoIzquierda ? 45.0 : -45.0);
    return true; 
  }
  return false;
}

void Cambio_Ruta_Dinamicos::objetivo_perdido() {
  estado = CAMBIO_DINAMICO_INACTIVO;
}

void Cambio_Ruta_Dinamicos::reset() {
  estado = CAMBIO_DINAMICO_INACTIVO;
}