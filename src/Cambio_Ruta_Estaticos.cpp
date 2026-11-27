#include "Cambio_Ruta_Estaticos.h"

Cambio_Ruta_Estaticos::Cambio_Ruta_Estaticos() {
  estado = CAMBIO_ESTATICO_INACTIVO;
  distArcoAcum = 0.0;
  distArcoMeta = 1.0;
  anguloArcoMeta = 60.0;
  direccionArco = 1;
  anguloArcoFinal = 0.0;
}

void Cambio_Ruta_Estaticos::iniciar_evasion(float headingActual) {
  estado = CAMBIO_ESTATICO_TRAZANDO_ARCO;
  distArcoAcum = 0.0;
  direccionArco = -direccionArco; 
  
  // FIX: Calcular el ángulo de destino FIJO una sola vez
  anguloArcoFinal = headingActual + (anguloArcoMeta * direccionArco);
  while (anguloArcoFinal > 180.0) anguloArcoFinal -= 360.0;
  while (anguloArcoFinal < -180.0) anguloArcoFinal += 360.0;
}

EstadoCambioEstatico Cambio_Ruta_Estaticos::actualizar_evasion(float deltaDist, float headingActual, float& anguloObjetivo) {
  if (estado == CAMBIO_ESTATICO_TRAZANDO_ARCO) {
    distArcoAcum += deltaDist;
    
    // FIX: Usar el ángulo fijo, no sumarle el headingActual
    anguloObjetivo = anguloArcoFinal; 
    
    if (distArcoAcum >= distArcoMeta) {
      estado = CAMBIO_ESTATICO_ESPERANDO_RECHECK;
    }
  }
  return estado;
}

void Cambio_Ruta_Estaticos::camino_despejado() {
  if (estado == CAMBIO_ESTATICO_ESPERANDO_RECHECK) {
    estado = CAMBIO_ESTATICO_COMPLETO;
  }
}

void Cambio_Ruta_Estaticos::reset() {
  estado = CAMBIO_ESTATICO_INACTIVO;
  distArcoAcum = 0.0;
}