#include "RumboControl.h"

RumboControl::RumboControl()
  : addr(0x69), pinSDA(8), pinSCL(9),
    thetaDeg(0.0f), thetaGuardada(0.0f),
    gyroZ_offset(0.0f), gzDps(0.0f), gzRaw(0.0f),
    ultimoGyroUs(0), fallosI2C(0),
    anguloObjetivo(0.0f), integralRumbo(0.0f), errRumbo(0.0f),
    rumboActivo(false), sentidoRumbo(1) {}

bool RumboControl::begin(uint8_t sda, uint8_t scl, uint8_t bmiAddr) {
  pinSDA = sda;
  pinSCL = scl;
  addr   = bmiAddr;

  Wire.begin(pinSDA, pinSCL);
  Wire.setClock(100000);

  if (bmi.softReset() != BMI160_OK) {
    Serial.println("[Rumbo] Aviso: SoftReset BMI160 fallo");
  }

  if (bmi.I2cInit(addr) != BMI160_OK) {
    if (bmi.I2cInit(0x68) != BMI160_OK) {
      Serial.println("[Rumbo] ERROR: BMI160 no responde");
      return false;
    }
    addr = 0x68;
  }
  Serial.printf("[Rumbo] BMI160 listo en 0x%02X\n", addr);
  calibrarOffset();
  return true;
}

void RumboControl::calibrarOffset() {
  Serial.println("[Rumbo] Calibrando offset giroscopio (NO MOVER)...");
  delay(600);

  float suma = 0.0f, suma2 = 0.0f;
  int n = 0;
  int16_t d[6] = {0};
  for (int i = 0; i < 400; i++) {
    if (bmi.getAccelGyroData(d) == 0) {
      float gz = SIGNO_YAW * (float)d[GZ_IDX] / GYRO_LSB_POR_DPS;
      suma  += gz;
      suma2 += gz * gz;
      n++;
    }
    delay(5);
  }
  if (n > 0) {
    gyroZ_offset = suma / (float)n;
    float ruido = sqrtf(fmaxf(0.0f, suma2 / (float)n - gyroZ_offset * gyroZ_offset));
    Serial.printf("[Rumbo] Offset: %.3f dps | ruido: %.3f dps | n=%d\n", gyroZ_offset, ruido, n);
  }
}

void RumboControl::actualizarGyro(bool quieto) {
  unsigned long ahora = micros();
  if (ahora - ultimoGyroUs < 5000) return; // ~200 Hz
  float dtG = (ahora - ultimoGyroUs) / 1000000.0f;
  ultimoGyroUs = ahora;

  int16_t d[6] = {0};
  int res = bmi.getAccelGyroData(d);

  if (res == 0) {
    fallosI2C = 0;
    gzRaw = SIGNO_YAW * (float)d[GZ_IDX] / GYRO_LSB_POR_DPS;
    float gz_dps = gzRaw - gyroZ_offset;

    if (quieto) {
      if (fabsf(gz_dps) < 3.0f) gyroZ_offset += 0.005f * (gzRaw - gyroZ_offset);
      gz_dps = 0.0f;
    } else {
      if (fabsf(gz_dps) < ZONA_MUERTA_DPS) gz_dps = 0.0f;
    }
    gzDps = gz_dps;

    if (!quieto) {
      // CORREGIDO: Factor neutro (1.0). El problema no era el sensor, era la inercia.
      const float FACTOR_CORRECCION_GIRO = 1.0f; 
      thetaDeg += (gz_dps * dtG) * FACTOR_CORRECCION_GIRO;
      
      while (thetaDeg >  180.0f) thetaDeg -= 360.0f;
      while (thetaDeg < -180.0f) thetaDeg += 360.0f;
    }
  } else {
    fallosI2C++;
    if (fallosI2C > 5) {
      reiniciarWire();
      fallosI2C = 0;
    }
  }
}

bool RumboControl::detectarGolpe() {
  if (fabsf(gzDps) > 150.0f) {
    return true;
  }
  return false;
}

void RumboControl::actualizarFiltroEncoder(long deltaDer, long deltaIzq,
                                           float distDer, float distIzq,
                                           float distEntreRuedas,
                                           bool enGiroSitio) {
  if (enGiroSitio) return;
  if (labs(deltaDer) + labs(deltaIzq) < 3) return; 

  float deltaYawEnc = (distDer - distIzq) / distEntreRuedas;
  float thetaEnc    = thetaGuardada + deltaYawEnc * RAD_TO_DEG;
  float errF = thetaEnc - thetaDeg;
  while (errF >  180.0f) errF -= 360.0f;
  while (errF < -180.0f) errF += 360.0f;

  thetaDeg += (1.0f - 0.985f) * errF;
  while (thetaDeg >  180.0f) thetaDeg -= 360.0f;
  while (thetaDeg < -180.0f) thetaDeg += 360.0f;
}

void RumboControl::cerrarTick() {
  thetaGuardada = thetaDeg;
}

void RumboControl::setThetaDeg(float deg) {
  thetaDeg = deg;
  while (thetaDeg >  180.0f) thetaDeg -= 360.0f;
  while (thetaDeg < -180.0f) thetaDeg += 360.0f;
  thetaGuardada = thetaDeg;
}

void RumboControl::reiniciarWire() {
  Wire.begin(pinSDA, pinSCL);
  Wire.setClock(100000);
}

void RumboControl::activar(int sentido) {
  sentidoRumbo = (sentido >= 0) ? 1 : -1;
  rumboActivo  = true;
}

void RumboControl::desactivar() {
  rumboActivo = false;
}

bool RumboControl::calcularPID(float dt, float velocidadBase, int& magDer, int& magIzq) {
  if (!rumboActivo) {
    errRumbo = 0.0f;
    return false;
  }

  float err = anguloObjetivo - thetaDeg;
  while (err >  180.0f) err -= 360.0f;
  while (err < -180.0f) err += 360.0f;

  integralRumbo = constrain(integralRumbo + err * dt, -intMax, intMax);
  float corr = kp * err + ki * integralRumbo - kd * gzDps;
  corr = constrain(corr, -corrMax, corrMax);
  errRumbo = err;

  int base = (int)velocidadBase;
  magDer = constrain(base + sentidoRumbo * (int)lroundf(corr), pwmMin, 255);
  magIzq = constrain(base - sentidoRumbo * (int)lroundf(corr), pwmMin, 255);
  return true;
}