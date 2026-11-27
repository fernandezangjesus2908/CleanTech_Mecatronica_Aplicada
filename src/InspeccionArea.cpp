#include "InspeccionArea.h"

InspeccionArea::InspeccionArea() {
  estado_actual = INSPECCION_INACTIVA;
  dist_acumulada = 0.0;
  angulo_previo = 0.0;
  angulo_total_girado = 0.0;
  dist_acercamiento = 0.0;
  tiempo_pausa_recoleccion = 0;
}

bool InspeccionArea::check_necesita_barrido(float deltaDist, float distAlWaypoint) {
  dist_acumulada += deltaDist;
  if (dist_acumulada >= DIST_PARA_BARRIDO && distAlWaypoint > 1.0) {
    return true;
  }
  return false;
}

void InspeccionArea::iniciar_barrido(float heading_actual) {
  estado_actual = INSPECCION_GIRANDO;
  angulo_previo = heading_actual;
  angulo_total_girado = 0.0;
}

float InspeccionArea::calcular_giro(float heading_actual, float objetivo_grados) {
  float diff = heading_actual - angulo_previo;
  while (diff > 180.0) diff -= 360.0;
  while (diff < -180.0) diff += 360.0;
  angulo_total_girado += fabs(diff);
  angulo_previo = heading_actual;
  return angulo_total_girado;
}

bool InspeccionArea::actualizar_barrido(float heading_actual) {
  if (calcular_giro(heading_actual, 360.0) >= 355.0) {
    estado_actual = INSPECCION_INACTIVA;
    dist_acumulada = 0.0; 
    return false; 
  }
  return true;
}

void InspeccionArea::iniciar_recoleccion() {
  estado_actual = INSPECCION_ACERCANDOSE;
  dist_acercamiento = 0.0;
}

bool InspeccionArea::actualizar_recoleccion(float deltaDist, unsigned long ms_actual) {
  if (estado_actual == INSPECCION_ACERCANDOSE) {
    if (dist_acercamiento < 1.0) {
      dist_acercamiento += deltaDist;
    } else {
      estado_actual = INSPECCION_RECOLECTANDO;
      tiempo_pausa_recoleccion = ms_actual;
    }
    return true;
  } 
  else if (estado_actual == INSPECCION_RECOLECTANDO) {
    if (ms_actual - tiempo_pausa_recoleccion >= 1500) {
      estado_actual = INSPECCION_INACTIVA;
      return false; // Terminó la recolección, pasa a verificar
    }
    return true;
  }
  return false;
}

void InspeccionArea::reset() {
  estado_actual = INSPECCION_INACTIVA;
  dist_acumulada = 0.0;
  angulo_total_girado = 0.0;
}

void InspeccionArea::reset_distancia() {
  dist_acumulada = 0.0;
}