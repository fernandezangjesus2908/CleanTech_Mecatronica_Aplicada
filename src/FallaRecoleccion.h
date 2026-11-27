#ifndef FALLA_RECOLECCION_H
#define FALLA_RECOLECCION_H

#include <Arduino.h>

enum EstadoFalla {
  FALLA_INACTIVA,
  FALLA_GIRANDO_180,     // Verificando
  FALLA_ESPERANDO_IA,    // Esperando confirmación (o timeout)
  FALLA_REINTENTANDO     // Giro de vuelta para volver a intentar
};

class FallaRecoleccion {
public:
  FallaRecoleccion();
  
  void iniciar(float heading_actual);
  void confirmar_fallo();
  void confirmar_exito();
  
  // Retorna true si sigue en proceso de giro o espera
  bool actualizar(float heading_actual, unsigned long ms_actual, bool& girar_izq, bool& girar_der);
  
  int get_intentos() { return intentos; }
  bool fue_exitoso() { return exito; }
  bool fallo_definitivo() { return (intentos >= 3 && !exito); }
  void reset_intentos() { intentos = 0; exito = false; }

private:
  EstadoFalla estado;
  int intentos;
  bool exito;
  float angulo_previo;
  float angulo_girado;
  unsigned long tiempo_inicio_espera;
};

#endif