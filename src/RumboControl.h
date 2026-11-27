#ifndef RUMBO_CONTROL_H
#define RUMBO_CONTROL_H

#include <Arduino.h>
#include <Wire.h>
#include <DFRobot_BMI160.h>

class RumboControl {
public:
  RumboControl();
    // Detecta si hubo un golpe repentino (aceleración o giro brusco)
  bool detectarGolpe();
  
  // Inicialización del sensor
  bool begin(uint8_t sda, uint8_t scl, uint8_t bmiAddr = 0x69);
  void calibrarOffset();
  
  // Actualización de sensores
  void actualizarGyro(bool quieto);
  void actualizarFiltroEncoder(long deltaDer, long deltaIzq, float distDer, float distIzq, float distEntreRuedas, bool enGiroSitio);
  void cerrarTick(); // Se llama al final del ciclo de 50ms
  
  // Lectura de la orientación
  float getThetaDeg() const { return thetaDeg; }
  float getThetaRad() const { return thetaDeg * (PI / 180.0f); }
  void setThetaDeg(float deg);
  
  // Control PID de rumbo
  void activar(int sentido);
  void desactivar();
  bool estaActivo() const { return rumboActivo; }
  int getSentido() const { return sentidoRumbo; }
  void setAnguloObjetivo(float deg) { anguloObjetivo = deg; }
  float getAnguloObjetivo() const { return anguloObjetivo; }
  void resetIntegral() { integralRumbo = 0.0f; }
  bool calcularPID(float dt, float velocidadBase, int& magDer, int& magIzq);
  
  // Diagnóstico
  float getGzDps() const { return gzDps; }
  float getErrRumbo() const { return errRumbo; }

private:
  DFRobot_BMI160 bmi;
  uint8_t addr;
  uint8_t pinSDA, pinSCL;
  
  float thetaDeg;             // Ángulo actual
  float thetaGuardada;        // Referencia para el filtro
  
  float gyroZ_offset;
  float gzDps;
  float gzRaw;
  unsigned long ultimoGyroUs;
  int fallosI2C;
  
  float anguloObjetivo;
  float integralRumbo;
  float errRumbo;
  bool rumboActivo;
  int sentidoRumbo;
  
  // Coeficientes PID (Ajustados a los que te funcionaron)
  const float kp = 2.0f;
  const float ki = 0.30f;
  const float kd = 0.0f;
  const float corrMax = 25.0f;
  const float intMax = 30.0f;
  const int pwmMin = 50;
  
  // Constantes del sensor
  static constexpr float ZONA_MUERTA_DPS = 0.2f;
  static constexpr float GYRO_LSB_POR_DPS = 16.4f;
  static constexpr int   GZ_IDX = 2;
  static constexpr float SIGNO_YAW = -1.0f;
  
  void reiniciarWire();
};

#endif