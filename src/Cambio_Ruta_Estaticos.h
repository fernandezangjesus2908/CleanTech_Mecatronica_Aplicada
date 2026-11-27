#ifndef CAMBIO_RUTA_ESTATICOS_H
#define CAMBIO_RUTA_ESTATICOS_H

enum EstadoCambioEstatico {
  CAMBIO_ESTATICO_INACTIVO,
  CAMBIO_ESTATICO_TRAZANDO_ARCO,
  CAMBIO_ESTATICO_ESPERANDO_RECHECK,
  CAMBIO_ESTATICO_COMPLETO
};

class Cambio_Ruta_Estaticos {
public:
  Cambio_Ruta_Estaticos();
  
  void iniciar_evasion(float headingActual);
  EstadoCambioEstatico actualizar_evasion(float deltaDist, float headingActual, float& anguloObjetivo);
  void camino_despejado();
  void reset();

private:
  EstadoCambioEstatico estado;
  float distArcoAcum;
  float distArcoMeta;
  float anguloArcoMeta;
  int direccionArco;
  float anguloArcoFinal; // NUEVO: Para evitar la espiral infinita
};

#endif