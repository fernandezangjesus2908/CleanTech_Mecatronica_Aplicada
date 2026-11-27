#ifndef INSPECCION_AREA_H
#define INSPECCION_AREA_H

#include <Arduino.h>

enum EstadoInspeccion {
  INSPECCION_INACTIVA,
  INSPECCION_GIRANDO,
  INSPECCION_ACERCANDOSE,
  INSPECCION_RECOLECTANDO
};

class InspeccionArea {
public:
  InspeccionArea();
  
  bool check_necesita_barrido(float deltaDist, float distAlWaypoint);
  void iniciar_barrido(float heading_actual);
  bool actualizar_barrido(float heading_actual);
  
  void iniciar_recoleccion();
  bool actualizar_recoleccion(float deltaDist, unsigned long ms_actual);
  
  void reset();
  void reset_distancia();
  
  EstadoInspeccion get_estado() { return estado_actual; }

private:
  EstadoInspeccion estado_actual;
  float dist_acumulada;
  const float DIST_PARA_BARRIDO = 2.0;
  float angulo_previo;
  float angulo_total_girado;
  float dist_acercamiento;
  unsigned long tiempo_pausa_recoleccion;
  float calcular_giro(float heading_actual, float objetivo_grados);
};

#endif