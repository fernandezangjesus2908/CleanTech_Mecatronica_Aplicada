#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <esp_wifi.h>

#include "RumboControl.h"
#include "Obstaculo_Estaticos.h"
#include "Cambio_Ruta_Estaticos.h"
#include "Obstaculo_Dinamicos.h"
#include "Cambio_Ruta_Dinamicos.h"
#include "InspeccionArea.h"
#include "FallaRecoleccion.h"
#include "RecuperacionRecoleccion.h"
#include "Recuperacion_Trayectoria.h"
#include "Comunicacion_IA.h" 

// ==========================================
// CONFIGURACIÓN WIFI
// ==========================================
const char* ssid = "CleanTECH_Robot";
const char* pass = "cleantech2611";
const int   WIFI_CHANNEL = 1;          
AsyncWebServer server(80);

const int FREQ = 10000;
const int RESOLUTION = 8;

// ==========================================
// PINES BTS7960 Y CEPILLO
// ==========================================
const int PIN_DER_RPWM = 7;  const int PIN_DER_LPWM = 6;  
const int PIN_IZQ_RPWM = 5;  const int PIN_IZQ_LPWM = 4;  
const int PIN_DER_EN   = 17; const int PIN_IZQ_EN   = 18;   
const int PIN_CEPILLO_RPWM = 13; const int PIN_CEPILLO_REN  = 14; 

bool cepilloEncendido = false;
const int VELOCIDAD_CEPILLO = 255; 
unsigned long ultima_deteccion_camara_ms = 0;

const unsigned long TIMEOUT_VISTA_CAMARA_MS  = 1500;
const unsigned long TIMEOUT_RECOLECCION_MS   = 12000;
const int           VEL_RECOLECCION          = 100;

bool  buscando_residuo_manual     = false;
bool  persiguiendo_manual         = false;
bool  auto_manual_habilitado      = false;
float heading_inicial_persecucion = 0.0f;
float max_deriva_heading          = 0.0f;
unsigned long inicio_busqueda_ms  = 0;

const float         UMBRAL_DERIVA_HEADING_DEG = 15.0f;
const int           PWM_BUSQUEDA_MANUAL       = 70;
const unsigned long ABORT_BUSQUEDA_VISTA_MS   = 400;
const unsigned long TIMEOUT_BUSQUEDA_360_MS   = 15000;

// ==========================================
// SENSOR BATERÍA
// ==========================================
const int PIN_VOLTAJE = 3; 
float relacionDivisor = 4.93;
float porcentajeSuavizado = 100.0;
int porcentajeBateria = 100;
const float alpha = 0.10;

float calcularPorcentaje(float v) {
  if (v >= 12.60) return 100.0;
  if (v <= 10.50) return 0.0;
  return (v - 10.50) * 100.0 / (12.60 - 10.50);
}

float medirVoltaje() {
  uint32_t suma = 0;
  for (int i = 0; i < 32; i++) {
    suma += analogReadMilliVolts(PIN_VOLTAJE);
    delayMicroseconds(200);
  }
  float vPin = (suma / 32.0) / 1000.0;
  return vPin * relacionDivisor;
}

// ==========================================
// PINES ENCODERS Y I2C
// ==========================================
const int PIN_ENC_DER_A = 15; const int PIN_ENC_DER_B = 16;
const int PIN_ENC_IZQ_A = 10; const int PIN_ENC_IZQ_B = 11;
const int PIN_I2C_SDA = 8; const int PIN_I2C_SCL = 9;

// ==========================================
// MÓDULOS
// ==========================================
RumboControl rumbo;
Obstaculo_Estaticos deteccion_estatica; 
Cambio_Ruta_Estaticos movimiento_estatico; 
Obstaculo_Dinamicos deteccion_dinamica; 
Cambio_Ruta_Dinamicos movimiento_dinamico; 
InspeccionArea inspeccion;
FallaRecoleccion falla;
RecuperacionRecoleccion recuperacion;
Recuperacion_Trayectoria recuperacion_trayectoria; 
Comunicacion_IA vision_ia; 

// ==========================================
// GPS
// ==========================================
const int PIN_GPS_RX = 1; const int PIN_GPS_TX = 2; const uint32_t GPS_BAUD = 115200; 
HardwareSerial GPS_Serial(1); TinyGPSPlus gps;
bool gpsOrigenListo = false; double origenLat = 0.0; double origenLon = 0.0;
float gpsX = 0.0; float gpsY = 0.0; int gpsSats = 0; bool gpsFix = false;

// ==========================================
// ODOMETRÍA Y PARÁMETROS
// ==========================================
const float PULSOS_POR_VUELTA = 495.0;
const float DIAMETRO_RUEDA = 0.0655;
const float DISTANCIA_RUEDAS = 0.220;
const unsigned long INTERVALO_MS = 50;

volatile long pulsosDer = 0; volatile long pulsosIzq = 0;
long ultimosPulsosDer = 0; long ultimosPulsosIzq = 0;

float rpmDer = 0.0; float rpmIzq = 0.0;
float posX = 0.0; float posY = 0.0; 
float distTotal = 0.0;

int velocidadBase = 125; int velocidadActual = 125;  
String comandoActual = "paro";
bool estaQuieto = true;

bool giroManual90Activo = false; float anguloObjetivoGiroManual = 0.0;

const int TRIM_DER = 0; const int TRIM_IZQ = 15; 
const uint8_t CAM_NUM_CLASES = 5;   

enum EstadoOperacion {
  ESTADO_MANUAL = 0, CUADRADO_AVANZAR = 1, CUADRADO_PAUSA_GIRO = 2,
  CUADRADO_GIRAR = 3, CUADRADO_PAUSA_SIG = 4, CUADRADO_COMPLETO = 5,
  NAV_WAYPOINTS = 6, NAV_CALIBRANDO = 8
};
EstadoOperacion estadoRobot = ESTADO_MANUAL;

const float LONGITUD_LADO = 0.80; const float ANGULO_GIRO_DEG = 90.0;
int ladoActual = 0; float distLadoAcumulada = 0.0; float anguloInicialGiro = 0.0;
unsigned long tiempoPausa = 0; unsigned long tiempoAnterior = 0;

struct Waypoint { double lat; double lon; };
const int MAX_WAYPOINTS = 25;
Waypoint rutaMision[MAX_WAYPOINTS]; double wpX[MAX_WAYPOINTS]; double wpY[MAX_WAYPOINTS];
int totalWaypoints = 0; int indiceWPActual = 0;

enum SubEstadoNav {
  WP_CALCULAR_TRAMO, WP_GIRAR, WP_PAUSA_ANTES_AVANCE, WP_AVANZAR_DISTANCIA,
  WP_PAUSA_LLEGADA, WP_RECUPERAR,
  WP_DESVIO_GIRAR, WP_DESVIO_AVANZAR, 
  SUB_CAMBIO_RUTA_ESTATICOS,   
  SUB_CAMBIO_RUTA_DINAMICOS,   
  Inspeccion_Area, Recolectar_Residuos,
  Retroceder_Verificacion,
  Avanzar_Reintento,
  Falla_Recoleccion, Recuperacion_Recoleccion,
  Falla_Trayectoria_residuo,
  Recuperacion_Trayectoria_Residuo 
};
SubEstadoNav subEstadoNav = WP_CALCULAR_TRAMO;

float anguloGiroMetaDeg = 0.0;
const int PWM_GIRO_NAV = 90; const int PWM_GIRO_DINAMICO = 80;
const float TOL_GIRO_DEG = 3.0; 
const float RADIO_LLEGADA_M = 0.15; 
const float UMBRAL_REORIENTAR_DEG = 35.0;
const float DIST_SIN_CORREGIR_M = 0.6;

const unsigned long ATASCO_MS = 1200;
const long UMBRAL_MOV_PULSOS = 3;
const unsigned long TIMEOUT_GIRO_MS = 10000;
const unsigned long TIMEOUT_AVANCE_BASE_MS = 15000; 
const unsigned long TIMEOUT_MS_POR_M = 10000;
const int MAX_REINTENTOS = 3;
const float DIST_RETROCESO_M = 0.20;

int intentosAtasco = 0; int alertaMision = 0; unsigned long contadorAlertas = 0;
unsigned long inicioSubEstadoMs = 0; unsigned long ultimoMovimientoMs = 0;
unsigned long timeoutAvanceMs = 30000; int faseRecuperar = 0;
float distRecuperada = 0.0; unsigned long tiempoInicioRetroceso = 0;

float distVerificacionRecorrida = 0.0;
const float DIST_VERIFICACION_M = 0.30;

const float DESVIO_ANGULO_BASE_DEG = 60.0; const float DESVIO_ANGULO_INC_DEG = 30.0;
const float DESVIO_DIST_BASE_M = 0.60; const float DESVIO_DIST_INC_M = 0.30;
const unsigned long TIMEOUT_DESVIO_MS = 8000;
int ladoDesvio = 1; bool atascoEnAvance = false;
float anguloDesvioMetaDeg = 0.0; float distDesvioMeta = 0.0; float distDesvioAcum = 0.0; int faseDesvio = 0;

const float K_FUSION = 0.08; const float GATE_FUSION_M = 8.0; const int SATS_FUSION = 5;
const float HDOP_MAX = 3.0; const int RECHAZOS_PARA_RESYNC = 15;
const float RADIO_LLEGADA_GPS_M = 1.0; const float DIST_SIN_CORREGIR_GPS_M = 2.0;

bool fusionActiva = false; bool gpsNueva = false; unsigned long gpsRechazadas = 0;
int rechazosSeguidos = 0; float radioLlegadaM = RADIO_LLEGADA_M; float distSinCorregirM = DIST_SIN_CORREGIR_M;
double mPorDegLonMision = 111320.0;

float Ppos = 1.0; const float Q_POR_M = 0.003; const float P_MIN = 0.05;
const float SIGMA_GPS_M = 2.0; const float GATE_SIGMAS = 3.0;
bool fusionHibrida = false; float hibCos = 1.0, hibSin = 0.0;
float hibAncX = 0.0, hibAncY = 0.0;
const float HIB_RADIO_MIN_M = 4.0; const float HIB_RESID_MAX_M = 4.0;
const float HIB_ESCALA_MIN = 0.7; const float HIB_ESCALA_MAX = 1.3;

const bool  CORR_RUMBO_GPS = true; const float SEG_DIST_M = 10.0;
const int   SEG_MUESTRAS = 3; const float SEG_K = 0.25;
const float SEG_MAX_DEG = 4.0; const float SEG_ERR_RUMBO_MAX = 10.0;

float rawX = 0.0, rawY = 0.0;
int segFase = 0; int segN = 0;
float segSgx = 0, segSgy = 0, segSrx = 0, segSry = 0;
float segIniGx = 0, segIniGy = 0, segIniRx = 0, segIniRy = 0;

const float CAL_DIST_M = 8.0; const unsigned long CAL_SETTLE_MS = 1500;
const unsigned long CAL_PROMEDIO_MS = 7500; const int CAL_MIN_MUESTRAS = 4;
const unsigned long CAL_ESPERA_MAX_MS = 25000; const unsigned long CAL_TIMEOUT_MS = 40000;

enum FaseCal { CAL_ESPERA_A, CAL_AVANZA, CAL_ESPERA_B, CAL_ANCLA };
FaseCal faseCal = CAL_ESPERA_A;
float calSumX = 0.0, calSumY = 0.0; int calN = 0;
float calAx = 0.0, calAy = 0.0; float calDistRecorrida = 0.0;
unsigned long calInicioMs = 0;
String payloadMision = "";
bool modoOdomMision = false;

// ==========================================
// CHECK-IN DE SISTEMAS
// ==========================================
enum EstadoCheckin {
  CHECKIN_IDLE,
  CHECKIN_AVANZANDO,
  CHECKIN_RETROCEDIENDO,
  CHECKIN_GIRANDO
};
EstadoCheckin estadoCheckin = CHECKIN_IDLE;
unsigned long checkinInicioMs = 0;
float checkinDistAcum = 0.0;
float checkinThetaPrev = 0.0;
float checkinThetaAcum = 0.0;

bool checkin_adelante_ok = false;
bool checkin_atras_ok = false;
bool checkin_giro360_ok = false;
bool checkin_gps_ok = false;

const float CHECKIN_DIST_M = 0.30;
const float CHECKIN_DIST_MIN = 0.20;
const float CHECKIN_GIRO_MIN = 340.0;
const unsigned long CHECKIN_DIST_TIMEOUT_MS = 8000;
const unsigned long CHECKIN_GIRO_TIMEOUT_MS = 25000;
const int CHECKIN_PWM = 100;

bool checkinCompleto() {
  return checkin_adelante_ok && checkin_atras_ok && checkin_giro360_ok && checkin_gps_ok;
}

// ==========================================
// ISR ENCODERS
// ==========================================
const unsigned long MIN_PULSO_US = 200;
volatile unsigned long ultimoPulsoDerUs = 0; volatile unsigned long ultimoPulsoIzqUs = 0;

void IRAM_ATTR leerEncoderDer() {
  unsigned long t = micros();
  if (t - ultimoPulsoDerUs < MIN_PULSO_US) return;
  ultimoPulsoDerUs = t;
  if (digitalRead(PIN_ENC_DER_B) > 0) pulsosDer = pulsosDer - 1; else pulsosDer = pulsosDer + 1;
}

void IRAM_ATTR leerEncoderIzq() {
  unsigned long t = micros();
  if (t - ultimoPulsoIzqUs < MIN_PULSO_US) return;
  ultimoPulsoIzqUs = t;
  if (digitalRead(PIN_ENC_IZQ_B) > 0) pulsosIzq = pulsosIzq + 1; else pulsosIzq = pulsosIzq - 1;
}

// ==========================================
// CONTROL DEL CEPILLO
// ==========================================
void controlarCepillo(bool encender, int pwm = VELOCIDAD_CEPILLO) {
  cepilloEncendido = encender;
  if (encender) {
    digitalWrite(PIN_CEPILLO_REN, HIGH);
    ledcWrite(PIN_CEPILLO_RPWM, pwm);
  } else {
    ledcWrite(PIN_CEPILLO_RPWM, 0);
    digitalWrite(PIN_CEPILLO_REN, LOW);
  }
}

// ==========================================
// GEODESIA
// ==========================================
void gpsALocal(double lat, double lon, float &xOut, float &yOut) {
  const double mPorDegLat = 111320.0;
  double mPorDegLon = 111320.0 * cos(origenLat * DEG_TO_RAD);
  yOut = (float)((lat - origenLat) * mPorDegLat);
  xOut = (float)((lon - origenLon) * mPorDegLon);
}

void convertirRutaALocal() {
  double lat0 = rutaMision[0].lat; double lon0 = rutaMision[0].lon;
  double mPorDegLon = 111320.0 * cos(lat0 * DEG_TO_RAD);
  mPorDegLonMision = mPorDegLon;
  for (int i = 0; i < totalWaypoints; i++) {
    wpY[i] = (rutaMision[i].lat - lat0) * 111320.0;
    wpX[i] = (rutaMision[i].lon - lon0) * mPorDegLon;
  }
}

void gpsAMision(double lat, double lon, float &x, float &y) {
  y = (float)((lat - rutaMision[0].lat) * 111320.0);
  x = (float)((lon - rutaMision[0].lon) * mPorDegLonMision);
}

void leerGPS() {
  while (GPS_Serial.available() > 0) gps.encode(GPS_Serial.read());
  if (gps.location.isUpdated()) gpsNueva = true;
  
  if (gps.location.isValid() && gps.location.age() < 2000) {
    gpsFix = true; gpsSats = gps.satellites.value();
    double lat = gps.location.lat(); double lon = gps.location.lng();
    if (!gpsOrigenListo && gpsSats >= 4) { origenLat = lat; origenLon = lon; gpsOrigenListo = true; }
    if (gpsOrigenListo) gpsALocal(lat, lon, gpsX, gpsY);
  } else gpsFix = false;
}

// ==========================================
// MOTORES
// ==========================================
void habilitarPuentes(bool en) {
  digitalWrite(PIN_DER_EN, en ? HIGH : LOW);
  digitalWrite(PIN_IZQ_EN, en ? HIGH : LOW);
}

void aplicarMotores(int pwm, String cmd) {
  pwm = constrain(pwm, 0, 255);
  cmd.toLowerCase(); cmd.trim();
  comandoActual = (pwm == 0) ? "paro" : cmd;

  if (pwm > 0) habilitarPuentes(true);

  bool avance = (cmd == "adelante");
  bool retro = (cmd == "atras" || cmd == "reversa");
  if (pwm > 0 && (avance || retro)) {
    int sent = avance ? 1 : -1;
    if (!rumbo.estaActivo() || rumbo.getSentido() != sent) {
      rumbo.setAnguloObjetivo(rumbo.getThetaDeg());
      rumbo.resetIntegral();
    }
    rumbo.activar(sent);
  } else rumbo.desactivar();

  if (pwm == 0 || cmd == "paro") {
    ledcWrite(PIN_DER_RPWM, 0); ledcWrite(PIN_DER_LPWM, 0);
    ledcWrite(PIN_IZQ_RPWM, 0); ledcWrite(PIN_IZQ_LPWM, 0);
  } else if (cmd == "adelante") {
    ledcWrite(PIN_DER_LPWM, 0); ledcWrite(PIN_DER_RPWM, pwm);
    ledcWrite(PIN_IZQ_RPWM, 0); ledcWrite(PIN_IZQ_LPWM, pwm);
  } else if (cmd == "atras" || cmd == "reversa") {
    ledcWrite(PIN_DER_RPWM, 0); ledcWrite(PIN_DER_LPWM, pwm);
    ledcWrite(PIN_IZQ_LPWM, 0); ledcWrite(PIN_IZQ_RPWM, pwm);
  } else if (cmd == "izquierda" || cmd == "giro_izq") {
    ledcWrite(PIN_DER_LPWM, 0); ledcWrite(PIN_DER_RPWM, pwm);
    ledcWrite(PIN_IZQ_LPWM, 0); ledcWrite(PIN_IZQ_RPWM, pwm);
  } else if (cmd == "derecha" || cmd == "giro_der") {
    ledcWrite(PIN_DER_RPWM, 0); ledcWrite(PIN_DER_LPWM, pwm);
    ledcWrite(PIN_IZQ_RPWM, 0); ledcWrite(PIN_IZQ_LPWM, pwm);
  }
}

void moverDiferencial(int pwmD, int pwmI, int sentido) {
  pwmD = constrain(pwmD, 0, 255); pwmI = constrain(pwmI, 0, 255);
  habilitarPuentes(true);
  if (sentido > 0) {
    ledcWrite(PIN_DER_LPWM, 0); ledcWrite(PIN_DER_RPWM, pwmD);
    ledcWrite(PIN_IZQ_RPWM, 0); ledcWrite(PIN_IZQ_LPWM, pwmI);
  } else {
    ledcWrite(PIN_DER_RPWM, 0); ledcWrite(PIN_DER_LPWM, pwmD);
    ledcWrite(PIN_IZQ_LPWM, 0); ledcWrite(PIN_IZQ_RPWM, pwmI);
  }
}

void cambiarSubEstado(SubEstadoNav s) {
  subEstadoNav = s; inicioSubEstadoMs = millis(); ultimoMovimientoMs = millis();
}

void avisarMision(int codigo) {
  alertaMision = codigo; contadorAlertas++;
  Serial.printf(">> Aviso de mision (codigo %d) <<\n", codigo);
}

void cancelarMision(int codigo) {
  aplicarMotores(0, "paro"); 
  controlarCepillo(false);
  estadoRobot = ESTADO_MANUAL;
  totalWaypoints = 0; fusionActiva = false;
  inspeccion.reset(); recuperacion.reset(); recuperacion_trayectoria.reset(); falla.reset_intentos();
  buscando_residuo_manual = false; persiguiendo_manual = false; max_deriva_heading = 0.0f;
  subEstadoNav = WP_CALCULAR_TRAMO; avisarMision(codigo);
  Serial.println(">> MISION CANCELADA <<");
}

bool lecturaGPSBuena(float &gx, float &gy) {
  if (!gpsNueva) return false;
  gpsNueva = false;
  if (!gpsFix || gpsSats < SATS_FUSION || !gps.hdop.isValid() || gps.hdop.hdop() > HDOP_MAX) {
    gpsRechazadas++; return false;
  }
  gpsAMision(gps.location.lat(), gps.location.lng(), gx, gy);
  return true;
}

void reiniciarSegmento() {
  segFase = 0; segN = 0; segSgx = segSgy = segSrx = segSry = 0.0;
}

void actualizarSegmentoRumbo(float gx, float gy) {
  if (!CORR_RUMBO_GPS) return;
  bool recto = (estadoRobot == NAV_WAYPOINTS) && subEstadoNav == WP_AVANZAR_DISTANCIA && comandoActual == "adelante" && fabsf(rumbo.getErrRumbo()) < SEG_ERR_RUMBO_MAX;
  if (!recto) { reiniciarSegmento(); return; }
  if (segFase == 0) segFase = 1;
  if (segFase == 2) {
    if (hypotf(rawX - segIniRx, rawY - segIniRy) < SEG_DIST_M) return;
    segFase = 3; segN = 0; segSgx = segSgy = segSrx = segSry = 0.0;
  }
  segSgx += gx; segSgy += gy; segSrx += rawX; segSry += rawY; segN++;
  if (segN < SEG_MUESTRAS) return;
  float mgx = segSgx / segN, mgy = segSgy / segN, mrx = segSrx / segN, mry = segSry / segN;
  if (segFase == 1) {
    segIniGx = mgx; segIniGy = mgy; segIniRx = mrx; segIniRy = mry; segFase = 2; return;
  }
  float vgx = mgx - segIniGx, vgy = mgy - segIniGy, vox = mrx - segIniRx, voy = mry - segIniRy;
  float lg = hypotf(vgx, vgy), lo = hypotf(vox, voy);
  if (lo > 0.8f * SEG_DIST_M && lg > 0.5f * lo && lg < 1.5f * lo) {
    float dth = (atan2f(vgy, vgx) - atan2f(voy, vox)) * RAD_TO_DEG;
    while (dth > 180.0) dth -= 360.0; while (dth < -180.0) dth += 360.0;
    float corr = constrain(SEG_K * dth, -SEG_MAX_DEG, SEG_MAX_DEG);
    rumbo.setThetaDeg(rumbo.getThetaDeg() + corr);
    rumbo.setAnguloObjetivo(rumbo.getAnguloObjetivo() + corr);
  }
  reiniciarSegmento();
}

void fusionarGPS() {
  if (!fusionActiva) return;
  float gx, gy;
  if (!lecturaGPSBuena(gx, gy)) return;
  if (fusionHibrida) {
    float ex0 = gx - hibAncX; float ey0 = gy - hibAncY;
    gx =  hibCos * ex0 + hibSin * ey0; gy = -hibSin * ex0 + hibCos * ey0;
  }
  float hd = max(1.0f, (float)gps.hdop.hdop());
  float R = sq(SIGMA_GPS_M * hd);
  float ex = gx - posX; float ey = gy - posY;
  float err = sqrtf(ex * ex + ey * ey);
  if (err > GATE_SIGMAS * sqrtf(Ppos + R)) {
    gpsRechazadas++; rechazosSeguidos++;
    if (rechazosSeguidos >= RECHAZOS_PARA_RESYNC) {
      posX = gx; posY = gy; Ppos = R; rechazosSeguidos = 0; reiniciarSegmento();
    }
    return;
  }
  rechazosSeguidos = 0; actualizarSegmentoRumbo(gx, gy);
  float K = Ppos / (Ppos + R); posX += K * ex; posY += K * ey; Ppos = max(P_MIN, Ppos * (1.0f - K));
}

bool ajustarRotacionHibrida() {
  int n = totalWaypoints; if (n < 2) return false;
  mPorDegLonMision = 111320.0 * cos(rutaMision[0].lat * DEG_TO_RAD);
  double gE[MAX_WAYPOINTS], gN[MAX_WAYPOINTS], mgE = 0, mgN = 0, mox = 0, moy = 0;
  for (int i = 0; i < n; i++) {
    gN[i] = (rutaMision[i].lat - rutaMision[0].lat) * 111320.0; gE[i] = (rutaMision[i].lon - rutaMision[0].lon) * mPorDegLonMision;
    mgE += gE[i]; mgN += gN[i]; mox += wpX[i]; moy += wpY[i];
  }
  mgE /= n; mgN /= n; mox /= n; moy /= n;
  double sCross = 0, sDot = 0, sOO = 0;
  for (int i = 0; i < n; i++) {
    double ox = wpX[i] - mox, oy = wpY[i] - moy, gx = gE[i] - mgE, gy = gN[i] - mgN;
    sCross += ox * gy - oy * gx; sDot += ox * gx + oy * gy; sOO += ox * ox + oy * oy;
  }
  double radio = sqrt(sOO / n);
  if (radio < HIB_RADIO_MIN_M) return false;
  double phi = atan2(sCross, sDot), c = cos(phi), s = sin(phi);
  double escala = (sDot * c + sCross * s) / sOO, res = 0;
  for (int i = 0; i < n; i++) {
    double ox = wpX[i] - mox, oy = wpY[i] - moy, gx = gE[i] - mgE, gy = gN[i] - mgN;
    double rx = gx - (c * ox - s * oy), ry = gy - (s * ox + c * oy);
    res += rx * rx + ry * ry;
  }
  double resRms = sqrt(res / n);
  if (escala < HIB_ESCALA_MIN || escala > HIB_ESCALA_MAX || resRms > HIB_RESID_MAX_M) return false;
  hibCos = (float)c; hibSin = (float)s; return true;
}

void finalizarCalibracion(float bx, float by) {
  float dx = bx - calAx, dy = by - calAy, dGps = sqrtf(dx * dx + dy * dy);
  float ratio = dGps / max(0.1f, calDistRecorrida);
  if (ratio < 0.6 || ratio > 1.4) { cancelarMision(6); return; }
  rumbo.setThetaDeg(atan2f(dy, dx) * RAD_TO_DEG);
  rumbo.setAnguloObjetivo(rumbo.getThetaDeg());
  posX = bx; posY = by; fusionHibrida = false; Ppos = 1.0; reiniciarSegmento();
  fusionActiva = true; rechazosSeguidos = 0; intentosAtasco = 0;
  indiceWPActual = (sqrtf(posX * posX + posY * posY) > 2.0) ? 0 : 1;
  cambiarSubEstado(WP_CALCULAR_TRAMO); estadoRobot = NAV_WAYPOINTS;
}

void iniciarSinFusion(int codigoAviso) {
  fusionActiva = false; fusionHibrida = false; avisarMision(codigoAviso);
  posX = 0.0; posY = 0.0; rawX = 0.0; rawY = 0.0; rumbo.setThetaDeg(0.0); rumbo.setAnguloObjetivo(0.0);
  intentosAtasco = 0; indiceWPActual = 1; cambiarSubEstado(WP_CALCULAR_TRAMO); estadoRobot = NAV_WAYPOINTS;
}

void finalizarAnclaje(float mx, float my) {
  hibAncX = mx; hibAncY = my; posX = 0.0; posY = 0.0; rawX = 0.0; rawY = 0.0;
  rumbo.setThetaDeg(0.0); rumbo.setAnguloObjetivo(0.0); fusionHibrida = true; fusionActiva = true;
  Ppos = 0.3; rechazosSeguidos = 0; reiniciarSegmento(); intentosAtasco = 0;
  indiceWPActual = 1; cambiarSubEstado(WP_CALCULAR_TRAMO); estadoRobot = NAV_WAYPOINTS;
}

void procesarCalibracion(float deltaDist, long deltaDer, long deltaIzq) {
  unsigned long t = millis();
  switch (faseCal) {
    case CAL_ESPERA_A: case CAL_ESPERA_B: case CAL_ANCLA: {
      unsigned long dur = t - calInicioMs;
      if (dur < CAL_SETTLE_MS) { gpsNueva = false; break; }
      float gx, gy;
      if (lecturaGPSBuena(gx, gy)) { calSumX += gx; calSumY += gy; calN++; }
      if (dur >= CAL_PROMEDIO_MS && calN >= CAL_MIN_MUESTRAS) {
        float mx = calSumX / calN, my = calSumY / calN;
        if (faseCal == CAL_ANCLA) finalizarAnclaje(mx, my);
        else if (faseCal == CAL_ESPERA_A) {
          calAx = mx; calAy = my; calDistRecorrida = 0.0; faseCal = CAL_AVANZA;
          calInicioMs = t; ultimoMovimientoMs = t; aplicarMotores(velocidadBase, "adelante");
        } else finalizarCalibracion(mx, my);
      } else if (dur > CAL_ESPERA_MAX_MS) {
        if (faseCal == CAL_ANCLA) iniciarSinFusion(7); else cancelarMision(5);
      }
      break;
    }
    case CAL_AVANZA: {
      calDistRecorrida += deltaDist;
      long mov = labs(deltaDer) + labs(deltaIzq);
      if (mov >= UMBRAL_MOV_PULSOS) ultimoMovimientoMs = t;
      if (calDistRecorrida >= CAL_DIST_M) {
        aplicarMotores(0, "paro"); faseCal = CAL_ESPERA_B; calInicioMs = t; calSumX = 0.0; calSumY = 0.0; calN = 0;
      } else if (t - ultimoMovimientoMs > ATASCO_MS) cancelarMision(3);
      else if (t - calInicioMs > CAL_TIMEOUT_MS) cancelarMision(4);
      break;
    }
  }
}

void iniciarCuadrado() {
  pulsosDer = 0; ultimosPulsosDer = 0; pulsosIzq = 0; ultimosPulsosIzq = 0;
  posX = 0.0; posY = 0.0; distTotal = 0.0;
  rumbo.setThetaDeg(0.0);
  rumbo.setAnguloObjetivo(rumbo.getThetaDeg()); 
  ladoActual = 0; distLadoAcumulada = 0.0;
  estadoRobot = CUADRADO_AVANZAR;
  aplicarMotores(velocidadBase, "adelante");
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200); delay(500);
  Serial.println("\n========== CleanTECH Robot - Iniciando ==========");

  if (rumbo.begin(PIN_I2C_SDA, PIN_I2C_SCL, 0x69)) Serial.println("[Rumbo] Módulo listo");
  else Serial.println("[Rumbo] ERROR: Sin BMI160");

  GPS_Serial.begin(GPS_BAUD, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  
  ledcAttach(PIN_DER_RPWM, FREQ, RESOLUTION);
  ledcAttach(PIN_DER_LPWM, FREQ, RESOLUTION);
  ledcAttach(PIN_IZQ_RPWM, FREQ, RESOLUTION);
  ledcAttach(PIN_IZQ_LPWM, FREQ, RESOLUTION);
  ledcAttach(PIN_CEPILLO_RPWM, FREQ, RESOLUTION);

  pinMode(PIN_DER_EN, OUTPUT);
  pinMode(PIN_IZQ_EN, OUTPUT);
  habilitarPuentes(true);

  pinMode(PIN_CEPILLO_REN, OUTPUT); 
  digitalWrite(PIN_CEPILLO_REN, LOW); 
  ledcWrite(PIN_CEPILLO_RPWM, 0);

  pinMode(PIN_VOLTAJE, INPUT); analogReadResolution(12); analogSetPinAttenuation(PIN_VOLTAJE, ADC_11db);

  aplicarMotores(0, "paro");

  pinMode(PIN_ENC_DER_A, INPUT_PULLUP); pinMode(PIN_ENC_DER_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_DER_A), leerEncoderDer, RISING);
  pinMode(PIN_ENC_IZQ_A, INPUT_PULLUP); pinMode(PIN_ENC_IZQ_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_IZQ_A), leerEncoderIzq, RISING);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_AP);
  delay(100);
  WiFi.softAP(ssid, pass, WIFI_CHANNEL);
  delay(100);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  Serial.printf("[WiFi] AP '%s' en canal %d\n", ssid, WiFi.channel());
  Serial.printf("[WiFi] AP MAC: %s\n", WiFi.softAPmacAddress().c_str());

  vision_ia.begin();

  // ===================== CHECK-IN =====================
  server.on("/checkinAdelante", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response;
    if (estadoRobot != ESTADO_MANUAL) response = request->beginResponse(400, "text/plain", "Debe estar en manual");
    else if (estadoCheckin != CHECKIN_IDLE) response = request->beginResponse(400, "text/plain", "Test en progreso");
    else {
      checkin_adelante_ok = false;
      checkinDistAcum = 0.0;
      checkinInicioMs = millis();
      estadoCheckin = CHECKIN_AVANZANDO;
      velocidadActual = CHECKIN_PWM;
      aplicarMotores(velocidadActual, "adelante");
      Serial.println("[CHECKIN] Test adelante iniciado");
      response = request->beginResponse(200, "text/plain", "OK");
    }
    response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
  });

  server.on("/checkinAtras", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response;
    if (estadoRobot != ESTADO_MANUAL) response = request->beginResponse(400, "text/plain", "Debe estar en manual");
    else if (estadoCheckin != CHECKIN_IDLE) response = request->beginResponse(400, "text/plain", "Test en progreso");
    else {
      checkin_atras_ok = false;
      checkinDistAcum = 0.0;
      checkinInicioMs = millis();
      estadoCheckin = CHECKIN_RETROCEDIENDO;
      velocidadActual = CHECKIN_PWM;
      aplicarMotores(velocidadActual, "atras");
      Serial.println("[CHECKIN] Test atras iniciado");
      response = request->beginResponse(200, "text/plain", "OK");
    }
    response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
  });

  server.on("/checkinGiro360", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response;
    if (estadoRobot != ESTADO_MANUAL) response = request->beginResponse(400, "text/plain", "Debe estar en manual");
    else if (estadoCheckin != CHECKIN_IDLE) response = request->beginResponse(400, "text/plain", "Test en progreso");
    else {
      checkin_giro360_ok = false;
      checkinThetaAcum = 0.0;
      checkinThetaPrev = rumbo.getThetaDeg();
      checkinInicioMs = millis();
      estadoCheckin = CHECKIN_GIRANDO;
      aplicarMotores(CHECKIN_PWM, "izquierda");
      Serial.println("[CHECKIN] Test giro 360 iniciado");
      response = request->beginResponse(200, "text/plain", "OK");
    }
    response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
  });

  server.on("/checkinGPS", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response;
    if (gpsFix && gpsSats >= 6) {
      checkin_gps_ok = true;
      Serial.printf("[CHECKIN] GPS OK (%d sats)\n", gpsSats);
      response = request->beginResponse(200, "text/plain", "OK");
    } else {
      checkin_gps_ok = false;
      Serial.printf("[CHECKIN] GPS FAIL (fix=%d sats=%d)\n", gpsFix, gpsSats);
      response = request->beginResponse(400, "text/plain", "GPS insuficiente");
    }
    response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
  });

  server.on("/resetCheckin", HTTP_GET, [](AsyncWebServerRequest *request){
    checkin_adelante_ok = false;
    checkin_atras_ok = false;
    checkin_giro360_ok = false;
    checkin_gps_ok = false;
    estadoCheckin = CHECKIN_IDLE;
    if (comandoActual != "paro") aplicarMotores(0, "paro");
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
    response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
  });

  // ===================== CONTROL ROBOT =====================
  server.on("/setRobot", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("speed") && request->hasParam("cmd")) {
      String cmd = request->getParam("cmd")->value();
      
      if (cmd == "paro" && estadoCheckin != CHECKIN_IDLE) {
        aplicarMotores(0, "paro");
        estadoCheckin = CHECKIN_IDLE;
        velocidadActual = velocidadBase;
      }
      
      if (cmd != "paro" && !checkinCompleto()) {
        AsyncWebServerResponse *response = request->beginResponse(403, "text/plain", "Check-in incompleto");
        response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
        return;
      }
      
      if (estadoCheckin != CHECKIN_IDLE && cmd != "paro") {
        AsyncWebServerResponse *response = request->beginResponse(400, "text/plain", "Test en progreso");
        response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
        return;
      }
      
      if (cmd == "giro_90_izq" || cmd == "giro_90_der") {
        if (estadoRobot == ESTADO_MANUAL) {
          aplicarMotores(0, "paro");
          giroManual90Activo = true;
          anguloObjetivoGiroManual = rumbo.getThetaDeg() + (cmd == "giro_90_izq" ? 90.0 : -90.0);
          while (anguloObjetivoGiroManual > 180.0) anguloObjetivoGiroManual -= 360.0;
          while (anguloObjetivoGiroManual < -180.0) anguloObjetivoGiroManual += 360.0;
          buscando_residuo_manual = false; persiguiendo_manual = false; max_deriva_heading = 0.0f;
          inspeccion.reset();
        }
      } else {
        giroManual90Activo = false;
        if (cmd == "paro") {
          estadoRobot = ESTADO_MANUAL; fusionActiva = false; aplicarMotores(0, "paro");
          buscando_residuo_manual = false; persiguiendo_manual = false; max_deriva_heading = 0.0f;
          ultima_deteccion_camara_ms = 0; inspeccion.reset(); controlarCepillo(false);
        }
        else if (estadoRobot == ESTADO_MANUAL) {
          velocidadBase = request->getParam("speed")->value().toInt();
          velocidadActual = velocidadBase;
          aplicarMotores(velocidadActual, cmd);
          ultima_deteccion_camara_ms = 0; buscando_residuo_manual = false;
          persiguiendo_manual = false; max_deriva_heading = 0.0f; inspeccion.reset();
        }
      }
      AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
      response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
    } else request->send(400, "text/plain", "Faltan parametros");
  });

  server.on("/setAutoManual", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("state")) {
      auto_manual_habilitado = (request->getParam("state")->value().toInt() == 1);
      AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
      response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
    } else request->send(400, "text/plain", "Falta parametro");
  });

  server.on("/setCepillo", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("state")) {
      controlarCepillo(request->getParam("state")->value().toInt() == 1);
      AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
      response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
    } else request->send(400, "text/plain", "Falta parametro");
  });

  server.on("/autoCuadrado", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkinCompleto()) {
      AsyncWebServerResponse *response = request->beginResponse(403, "text/plain", "Check-in incompleto");
      response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
      return;
    }
    iniciarCuadrado();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
    response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
  });

  server.on("/simularObstaculo", HTTP_GET, [](AsyncWebServerRequest *request){
    if (estadoRobot == NAV_WAYPOINTS) {
      if (subEstadoNav == Inspeccion_Area) {
        aplicarMotores(0, "paro"); falla.reset_intentos();
        ultima_deteccion_camara_ms = millis(); avisarMision(14); 
        cambiarSubEstado(Recolectar_Residuos); controlarCepillo(true); 
      } else if (subEstadoNav == WP_AVANZAR_DISTANCIA) {
        aplicarMotores(0, "paro"); deteccion_estatica.detectar(); 
        movimiento_estatico.iniciar_evasion(rumbo.getThetaDeg()); 
        avisarMision(10); cambiarSubEstado(SUB_CAMBIO_RUTA_ESTATICOS);
      }
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
    response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
  });

  server.on("/recoleccionExitosa", HTTP_GET, [](AsyncWebServerRequest *request){
    if (subEstadoNav == Falla_Recoleccion) falla.confirmar_exito();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
    response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
  });

  server.on("/recoleccionFallida", HTTP_GET, [](AsyncWebServerRequest *request){
    if (subEstadoNav == Falla_Recoleccion) falla.confirmar_fallo();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
    response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
  });

  server.on("/objetivoDespejado", HTTP_GET, [](AsyncWebServerRequest *request){
    if (subEstadoNav == SUB_CAMBIO_RUTA_ESTATICOS) { 
      movimiento_estatico.camino_despejado(); deteccion_estatica.reset(); avisarMision(11); 
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
    response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
  });

  server.on("/simularAnimado", HTTP_GET, [](AsyncWebServerRequest *request){
    if (estadoRobot == NAV_WAYPOINTS) {
      aplicarMotores(0, "paro"); deteccion_dinamica.detectar(); 
      movimiento_dinamico.iniciar_escaneo(rumbo.getThetaDeg()); avisarMision(12); 
      cambiarSubEstado(SUB_CAMBIO_RUTA_DINAMICOS);
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
    response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
  });

  server.on("/objetivoPerdido", HTTP_GET, [](AsyncWebServerRequest *request){
    if (subEstadoNav == SUB_CAMBIO_RUTA_DINAMICOS) { 
      movimiento_dinamico.objetivo_perdido(); deteccion_dinamica.reset(); avisarMision(13); 
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
    response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
  });

  server.on("/simularGolpe", HTTP_GET, [](AsyncWebServerRequest *request){
    if (subEstadoNav == Recolectar_Residuos) {
      aplicarMotores(0, "paro"); controlarCepillo(false);
      avisarMision(22); cambiarSubEstado(Falla_Trayectoria_residuo); 
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
    response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
  });

  server.on("/residuoVueltoAVer", HTTP_GET, [](AsyncWebServerRequest *request){
    if (subEstadoNav == Recuperacion_Trayectoria_Residuo) {
      recuperacion_trayectoria.confirmar_exito(); avisarMision(23); 
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
    response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
  });

  // ===================== SET MISSION =====================
  server.on("/setMission", HTTP_OPTIONS, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });

  server.on("/setMission", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!checkinCompleto()) {
      AsyncWebServerResponse *response = request->beginResponse(403, "text/plain", "Check-in incompleto");
      response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
      return;
    }
  },
  NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    if (index == 0) payloadMision = "";
    for (size_t i = 0; i < len; i++) payloadMision += (char)data[i];

    if (index + len == total) {
      DynamicJsonDocument doc(6144); DeserializationError error = deserializeJson(doc, payloadMision);
      bool procesado = false;
      
      if (!error && doc.is<JsonArray>()) {
        JsonArray array = doc.as<JsonArray>(); int n = 0; bool modoOdom = false; bool todosGps = true;
        bool valido = true;
        for (JsonObject item : array) {
          if (n < MAX_WAYPOINTS) {
            if (item.containsKey("x") && item.containsKey("y")) {
              modoOdom = true; 
              wpX[n] = item["x"].as<double>(); wpY[n] = item["y"].as<double>();
              if (!isfinite(wpX[n]) || !isfinite(wpY[n])) { valido = false; break; }
              if (item.containsKey("lat") && item.containsKey("lon")) { 
                rutaMision[n].lat = item["lat"].as<double>(); rutaMision[n].lon = item["lon"].as<double>(); 
              } else todosGps = false;
            } else if (item.containsKey("lat") && item.containsKey("lon")) { 
              rutaMision[n].lat = item["lat"].as<double>(); rutaMision[n].lon = item["lon"].as<double>(); 
              if (!isfinite(rutaMision[n].lat) || !isfinite(rutaMision[n].lon)) { valido = false; break; }
            } else { valido = false; break; }
            n++;
          }
        }
        
        if (valido && n >= 2) {
          totalWaypoints = n; if (!modoOdom) convertirRutaALocal();
          modoOdomMision = modoOdom;
          aplicarMotores(0, "paro"); pulsosDer = 0; ultimosPulsosDer = 0; pulsosIzq = 0; ultimosPulsosIzq = 0;
          posX = 0.0; posY = 0.0; distTotal = 0.0; intentosAtasco = 0; fusionActiva = false; rechazosSeguidos = 0;
          radioLlegadaM = RADIO_LLEGADA_M; distSinCorregirM = DIST_SIN_CORREGIR_M; fusionHibrida = false;
          Ppos = 1.0; reiniciarSegmento(); rawX = 0.0; rawY = 0.0; inspeccion.reset(); recuperacion.reset();
          buscando_residuo_manual = false; persiguiendo_manual = false;
          max_deriva_heading = 0.0f; ultima_deteccion_camara_ms = 0;
          
          bool hibrido = modoOdom && todosGps && gpsFix && (gpsSats >= SATS_FUSION); int avisoHib = 0;
          if (hibrido && !ajustarRotacionHibrida()) { hibrido = false; avisoHib = 8; }
          bool calibrar = (!modoOdom) && request->hasParam("cal") && gpsFix && (gpsSats >= SATS_FUSION);

          if (modoOdom) { wpX[0] = 0.0; wpY[0] = 0.0; rumbo.setThetaDeg(0.0); }
          else { radioLlegadaM = RADIO_LLEGADA_GPS_M; distSinCorregirM = DIST_SIN_CORREGIR_GPS_M; rumbo.setThetaDeg((float)(atan2(wpY[1], wpX[1]) * RAD_TO_DEG)); }
          rumbo.setAnguloObjetivo(rumbo.getThetaDeg());

          if (calibrar) {
            faseCal = CAL_ESPERA_A; calSumX = 0.0; calSumY = 0.0; calN = 0; calDistRecorrida = 0.0;
            calInicioMs = millis(); gpsNueva = false; estadoRobot = NAV_CALIBRANDO;
          } else if (hibrido) {
            faseCal = CAL_ANCLA; calSumX = 0.0; calSumY = 0.0; calN = 0;
            calInicioMs = millis(); gpsNueva = false; estadoRobot = NAV_CALIBRANDO;
          } else {
            if (!modoOdom && gpsFix && gpsSats >= SATS_FUSION) fusionActiva = true; else fusionActiva = false;
            if (avisoHib) avisarMision(avisoHib);
            indiceWPActual = 1; cambiarSubEstado(WP_CALCULAR_TRAMO); estadoRobot = NAV_WAYPOINTS;
          }
          procesado = true;
        }
      }
      
      AsyncWebServerResponse *response = request->beginResponse(procesado ? 200 : 400, "text/plain", procesado ? "OK" : "Ruta invalida");
      response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
    }
  });

  // ===================== TELEMETRÍA =====================
  server.on("/telemetry", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<2048> doc;
    doc["cmd"] = comandoActual; doc["pwm"] = velocidadActual; doc["estado_auto"] = (int)estadoRobot;
    doc["lado"] = (estadoRobot == NAV_WAYPOINTS) ? indiceWPActual : (ladoActual + 1);
    doc["x"] = serialized(String(posX, 2)); doc["y"] = serialized(String(posY, 2));
    doc["theta_deg"] = serialized(String(rumbo.getThetaDeg(), 1)); doc["dist_total"] = serialized(String(distTotal, 2));
    doc["err_rumbo"] = serialized(String(rumbo.getErrRumbo(), 1));
    doc["alerta"] = alertaMision; doc["alerta_n"] = contadorAlertas; doc["fusion"] = fusionActiva;
    doc["hdop"] = serialized(String(gps.hdop.isValid() ? gps.hdop.hdop() : 0.0, 1));
    doc["gps_rech"] = gpsRechazadas; doc["sigma_pos"] = serialized(String(sqrtf(Ppos), 2)); doc["hib"] = fusionHibrida;
    doc["m_der_rpm"] = serialized(String(abs(rpmDer), 1)); doc["m_der_pulses"] = pulsosDer;
    doc["m_izq_rpm"] = serialized(String(abs(rpmIzq), 1)); doc["m_izq_pulses"] = pulsosIzq;
    doc["gps_fix"] = gpsFix; doc["gps_sats"] = gpsSats;
    doc["gps_x"] = serialized(String(gpsX, 2)); doc["gps_y"] = serialized(String(gpsY, 2));
    doc["cepillo"] = cepilloEncendido; doc["buscando_manual"] = buscando_residuo_manual;
    doc["auto_manual"] = auto_manual_habilitado;

    // NUEVO: indicadores de cámara (solo booleanos)
    doc["objeto_dinamico"]   = vision_ia.hay_objeto_dinamico();
    doc["objeto_reconocido"] = vision_ia.hay_objeto_reconocido();

    // CHECK-IN
    doc["checkin_adelante"] = checkin_adelante_ok;
    doc["checkin_atras"] = checkin_atras_ok;
    doc["checkin_giro360"] = checkin_giro360_ok;
    doc["checkin_gps"] = checkin_gps_ok;
    doc["checkin_completo"] = checkinCompleto();
    doc["checkin_running"] = (int)estadoCheckin;

    bool maniobra_estable = !(
        subEstadoNav == WP_RECUPERAR || subEstadoNav == WP_DESVIO_GIRAR ||
        subEstadoNav == WP_DESVIO_AVANZAR || subEstadoNav == SUB_CAMBIO_RUTA_ESTATICOS ||
        subEstadoNav == SUB_CAMBIO_RUTA_DINAMICOS || subEstadoNav == Inspeccion_Area ||
        subEstadoNav == Falla_Recoleccion || subEstadoNav == Recuperacion_Recoleccion ||
        subEstadoNav == Falla_Trayectoria_residuo || subEstadoNav == Recuperacion_Trayectoria_Residuo ||
        giroManual90Activo || buscando_residuo_manual || estadoCheckin != CHECKIN_IDLE ||
        comandoActual == "izquierda" || comandoActual == "derecha" || 
        comandoActual == "giro_izq" || comandoActual == "giro_der"
    );
    doc["maniobra_estable"] = maniobra_estable;

    float vBat = medirVoltaje();
    float pct = calcularPorcentaje(vBat);
    porcentajeSuavizado = alpha * pct + (1 - alpha) * porcentajeSuavizado;
    porcentajeBateria = round(porcentajeSuavizado); 

    doc["bat_v"] = serialized(String(vBat, 2)); doc["bat_pct"] = porcentajeBateria;
    doc["marker_active"] = recuperacion.hay_marcador();
    if (recuperacion.hay_marcador()) { 
        doc["marker_x"] = serialized(String(recuperacion.get_x(), 2)); 
        doc["marker_y"] = serialized(String(recuperacion.get_y(), 2)); 
    }
    
    if (gps.location.isValid() && gps.location.age() < 2000) {
      doc["lat"] = serialized(String(gps.location.lat(), 6)); doc["lon"] = serialized(String(gps.location.lng(), 6));
    }
    String jsonString; serializeJson(doc, jsonString);
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonString);
    response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
  });

  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
    estadoRobot = ESTADO_MANUAL; totalWaypoints = 0; fusionActiva = false;
    pulsosDer = 0; ultimosPulsosDer = 0; pulsosIzq = 0; ultimosPulsosIzq = 0;
    posX = 0.0; posY = 0.0; distTotal = 0.0; rumbo.setThetaDeg(0.0); distLadoAcumulada = 0.0;
    inspeccion.reset(); recuperacion.reset(); recuperacion_trayectoria.reset(); falla.reset_intentos();
    movimiento_estatico.reset(); movimiento_dinamico.reset(); subEstadoNav = WP_CALCULAR_TRAMO; 
    controlarCepillo(false); giroManual90Activo = false; 
    buscando_residuo_manual = false; persiguiendo_manual = false; max_deriva_heading = 0.0f;
    ultima_deteccion_camara_ms = 0; auto_manual_habilitado = false; modoOdomMision = false;
    checkin_adelante_ok = false; checkin_atras_ok = false; 
    checkin_giro360_ok = false; checkin_gps_ok = false; estadoCheckin = CHECKIN_IDLE;
    aplicarMotores(0, "paro");
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Reset OK");
    response->addHeader("Access-Control-Allow-Origin", "*"); request->send(response);
  });

  server.begin();
  tiempoAnterior = millis();
  Serial.println("========== Sistema listo ==========\n");
}

// ==========================================
// LOOP PRINCIPAL
// ==========================================
void loop() {
  leerGPS();
  rumbo.actualizarGyro(estaQuieto);

  unsigned long ahora = millis();
  if (ahora - tiempoAnterior >= INTERVALO_MS) {
    float dt = (ahora - tiempoAnterior) / 1000.0;
    tiempoAnterior = ahora;

    if (vision_ia.hay_deteccion() && estadoCheckin == CHECKIN_IDLE) {
      uint8_t clase_detectada = vision_ia.get_clase();
      vision_ia.limpiar_deteccion();

      Serial.printf("[VISION RX] clase=%d estado=%d sub=%d\n", clase_detectada, (int)estadoRobot, (int)subEstadoNav);

      if (clase_detectada < CAM_NUM_CLASES) {
        ultima_deteccion_camara_ms = millis(); 
        if (estadoRobot == NAV_WAYPOINTS && 
            (subEstadoNav == WP_AVANZAR_DISTANCIA || subEstadoNav == Inspeccion_Area)) {
          aplicarMotores(0, "paro"); falla.reset_intentos();
          avisarMision(14); cambiarSubEstado(Recolectar_Residuos); controlarCepillo(true);
        }
        if (estadoRobot == ESTADO_MANUAL && !cepilloEncendido) controlarCepillo(true);
      } 
      else if (clase_detectada >= CAM_NUM_CLASES && estadoRobot == NAV_WAYPOINTS) {
        if (subEstadoNav == WP_AVANZAR_DISTANCIA) {
          aplicarMotores(0, "paro"); deteccion_estatica.detectar();
          movimiento_estatico.iniciar_evasion(rumbo.getThetaDeg());
          avisarMision(10); cambiarSubEstado(SUB_CAMBIO_RUTA_ESTATICOS);
        }
      }
    }

    if (estadoRobot == ESTADO_MANUAL && estadoCheckin == CHECKIN_IDLE) {
      if (buscando_residuo_manual) {
        bool vio_ahora   = (millis() - ultima_deteccion_camara_ms < ABORT_BUSQUEDA_VISTA_MS);
        bool timeout_bus = (millis() - inicio_busqueda_ms > TIMEOUT_BUSQUEDA_360_MS);
        if (vio_ahora) {
          Serial.println("[MANUAL] Residuo re-detectado");
          buscando_residuo_manual = false; persiguiendo_manual = false;
          max_deriva_heading = 0.0f; inspeccion.reset(); controlarCepillo(true);
        }
        else if (timeout_bus) {
          Serial.println("[MANUAL] Timeout busqueda 360");
          aplicarMotores(0, "paro"); controlarCepillo(false);
          buscando_residuo_manual = false; persiguiendo_manual = false;
          max_deriva_heading = 0.0f; inspeccion.reset(); ultima_deteccion_camara_ms = 0;
        }
        else {
          if (inspeccion.actualizar_barrido(rumbo.getThetaDeg())) {
            if (comandoActual != "izquierda") aplicarMotores(PWM_BUSQUEDA_MANUAL, "izquierda");
            if (!cepilloEncendido) controlarCepillo(true);
          } else {
            Serial.println("[MANUAL] Busqueda 360 completa");
            aplicarMotores(0, "paro"); controlarCepillo(false);
            buscando_residuo_manual = false; persiguiendo_manual = false;
            max_deriva_heading = 0.0f; ultima_deteccion_camara_ms = 0;
          }
        }
      }
      else if (auto_manual_habilitado && ultima_deteccion_camara_ms != 0) { 
        unsigned long tiempo_sin_ver = millis() - ultima_deteccion_camara_ms;
        if (!persiguiendo_manual) {
          persiguiendo_manual = true;
          heading_inicial_persecucion = rumbo.getThetaDeg();
          max_deriva_heading = 0.0f;
        }
        float deriva = fabsf(rumbo.getThetaDeg() - heading_inicial_persecucion);
        if (deriva > 180.0f) deriva = 360.0f - deriva;
        if (deriva > max_deriva_heading) max_deriva_heading = deriva;

        if (tiempo_sin_ver < 1500) {
          velocidadActual = 125;
          aplicarMotores(velocidadActual, "adelante");
          if (!cepilloEncendido) controlarCepillo(true);
        } else if (tiempo_sin_ver > 3000) {
          aplicarMotores(0, "paro");
          if (max_deriva_heading > UMBRAL_DERIVA_HEADING_DEG && !buscando_residuo_manual && !giroManual90Activo) {
            Serial.printf("[MANUAL] Deriva %.1f -> busqueda 360\n", max_deriva_heading);
            buscando_residuo_manual = true;
            inicio_busqueda_ms = millis();
            inspeccion.iniciar_barrido(rumbo.getThetaDeg());
            controlarCepillo(true);
          } else {
            controlarCepillo(false); ultima_deteccion_camara_ms = 0;
          }
          persiguiendo_manual = false; max_deriva_heading = 0.0f;
        } else {
          if (comandoActual != "paro") aplicarMotores(0, "paro");
          if (!cepilloEncendido) controlarCepillo(true);
        }
      }
    }

    long deltaDer = pulsosDer - ultimosPulsosDer;
    long deltaIzq = pulsosIzq - ultimosPulsosIzq;
    ultimosPulsosDer = pulsosDer; ultimosPulsosIzq = pulsosIzq;

    bool anclando = (estadoRobot == NAV_CALIBRANDO && faseCal == CAL_ANCLA);
    estaQuieto = (comandoActual == "paro" && deltaDer == 0 && deltaIzq == 0 && (estadoRobot == ESTADO_MANUAL || anclando));

    rpmDer = ((float)deltaDer / PULSOS_POR_VUELTA) * (60.0 / dt);
    rpmIzq = ((float)deltaIzq / PULSOS_POR_VUELTA) * (60.0 / dt);

    float distDer = ((float)deltaDer / PULSOS_POR_VUELTA) * (PI * DIAMETRO_RUEDA);
    float distIzq = ((float)deltaIzq / PULSOS_POR_VUELTA) * (PI * DIAMETRO_RUEDA);
    float deltaDist = (distDer + distIzq) / 2.0;
    if (comandoActual == "izquierda" || comandoActual == "derecha" || comandoActual == "giro_izq" || comandoActual == "giro_der") deltaDist = 0.0;

    bool enGiroSitio = (subEstadoNav == WP_GIRAR || subEstadoNav == WP_DESVIO_GIRAR ||
                        subEstadoNav == Inspeccion_Area || subEstadoNav == Falla_Recoleccion || 
                        subEstadoNav == Recuperacion_Trayectoria_Residuo || subEstadoNav == SUB_CAMBIO_RUTA_DINAMICOS ||
                        estadoRobot == CUADRADO_GIRAR || giroManual90Activo ||
                        buscando_residuo_manual || estadoCheckin == CHECKIN_GIRANDO ||
                        comandoActual == "izquierda" || comandoActual == "derecha" || comandoActual == "giro_izq" || comandoActual == "giro_der");
    rumbo.actualizarFiltroEncoder(deltaDer, deltaIzq, distDer, distIzq, DISTANCIA_RUEDAS, enGiroSitio);

    distTotal += abs(deltaDist);
    posX += deltaDist * cos(rumbo.getThetaRad());
    posY += deltaDist * sin(rumbo.getThetaRad());
    rawX += deltaDist * cos(rumbo.getThetaRad());
    rawY += deltaDist * sin(rumbo.getThetaRad());
    if (fusionActiva) Ppos += Q_POR_M * fabsf(deltaDist);

    fusionarGPS();

    if (estadoCheckin != CHECKIN_IDLE) {
      unsigned long elapsed = millis() - checkinInicioMs;
      
      if (estadoCheckin == CHECKIN_AVANZANDO) {
        checkinDistAcum += deltaDist;
        if (checkinDistAcum >= CHECKIN_DIST_M || elapsed > CHECKIN_DIST_TIMEOUT_MS) {
          aplicarMotores(0, "paro");
          checkin_adelante_ok = (checkinDistAcum >= CHECKIN_DIST_MIN);
          Serial.printf("[CHECKIN] Adelante: %s (%.2fm)\n", checkin_adelante_ok ? "OK" : "FAIL", checkinDistAcum);
          estadoCheckin = CHECKIN_IDLE;
          velocidadActual = velocidadBase;
        }
      }
      else if (estadoCheckin == CHECKIN_RETROCEDIENDO) {
        checkinDistAcum += -deltaDist;
        if (checkinDistAcum >= CHECKIN_DIST_M || elapsed > CHECKIN_DIST_TIMEOUT_MS) {
          aplicarMotores(0, "paro");
          checkin_atras_ok = (checkinDistAcum >= CHECKIN_DIST_MIN);
          Serial.printf("[CHECKIN] Atras: %s (%.2fm)\n", checkin_atras_ok ? "OK" : "FAIL", checkinDistAcum);
          estadoCheckin = CHECKIN_IDLE;
          velocidadActual = velocidadBase;
        }
      }
      else if (estadoCheckin == CHECKIN_GIRANDO) {
        float diff = rumbo.getThetaDeg() - checkinThetaPrev;
        while (diff > 180.0) diff -= 360.0;
        while (diff < -180.0) diff += 360.0;
        checkinThetaAcum += fabs(diff);
        checkinThetaPrev = rumbo.getThetaDeg();
        
        if (checkinThetaAcum >= 355.0 || elapsed > CHECKIN_GIRO_TIMEOUT_MS) {
          aplicarMotores(0, "paro");
          checkin_giro360_ok = (checkinThetaAcum >= CHECKIN_GIRO_MIN);
          Serial.printf("[CHECKIN] Giro360: %s (%.1f deg)\n", checkin_giro360_ok ? "OK" : "FAIL", checkinThetaAcum);
          estadoCheckin = CHECKIN_IDLE;
        } else {
          if (comandoActual != "izquierda") aplicarMotores(CHECKIN_PWM, "izquierda");
        }
      }
    }

    if (giroManual90Activo) {
      float diff = anguloObjetivoGiroManual - rumbo.getThetaDeg();
      while (diff > 180.0) diff -= 360.0; while (diff < -180.0) diff += 360.0;
      if (fabs(diff) <= 1.5) { aplicarMotores(0, "paro"); giroManual90Activo = false; }
      else { int pwmG = constrain((int)(1.5 * fabsf(diff)) + 50, 65, 110); aplicarMotores(pwmG, diff > 0 ? "izquierda" : "derecha"); }
    } else if (estadoCheckin == CHECKIN_IDLE) {
      int magDer, magIzq;
      if (rumbo.calcularPID(dt, velocidadActual, magDer, magIzq)) {
        int magDerFinal = constrain(magDer + TRIM_DER, 50, 255); int magIzqFinal = constrain(magIzq + TRIM_IZQ, 50, 255);
        moverDiferencial(magDerFinal, magIzqFinal, rumbo.getSentido());
      }
    }

    if (estadoRobot == NAV_CALIBRANDO) procesarCalibracion(deltaDist, deltaDer, deltaIzq);

    if (estadoRobot == NAV_WAYPOINTS) {
      if (indiceWPActual < totalWaypoints) {
        {
          long movTick = labs(deltaDer) + labs(deltaIzq);
          bool motoresActivos = (subEstadoNav == WP_GIRAR || subEstadoNav == WP_AVANZAR_DISTANCIA || 
                                 subEstadoNav == WP_DESVIO_GIRAR || subEstadoNav == WP_DESVIO_AVANZAR ||
                                 subEstadoNav == SUB_CAMBIO_RUTA_ESTATICOS || subEstadoNav == SUB_CAMBIO_RUTA_DINAMICOS ||
                                 subEstadoNav == Inspeccion_Area || subEstadoNav == Recolectar_Residuos ||
                                 subEstadoNav == Retroceder_Verificacion || subEstadoNav == Avanzar_Reintento ||
                                 subEstadoNav == Falla_Recoleccion || subEstadoNav == Recuperacion_Recoleccion ||
                                 subEstadoNav == Recuperacion_Trayectoria_Residuo) && comandoActual != "paro";
          if (!motoresActivos || movTick >= UMBRAL_MOV_PULSOS) ultimoMovimientoMs = millis();
          bool atascado = motoresActivos && (millis() - ultimoMovimientoMs > ATASCO_MS);
          bool tiempoAgotado = false;
          if (subEstadoNav == WP_GIRAR) tiempoAgotado = (millis() - inicioSubEstadoMs > TIMEOUT_GIRO_MS);
          else if (subEstadoNav == WP_AVANZAR_DISTANCIA) tiempoAgotado = (millis() - inicioSubEstadoMs > timeoutAvanceMs);
          else if (subEstadoNav == WP_DESVIO_GIRAR) tiempoAgotado = (millis() - inicioSubEstadoMs > TIMEOUT_GIRO_MS);
          else if (subEstadoNav == WP_DESVIO_AVANZAR) tiempoAgotado = (millis() - inicioSubEstadoMs > TIMEOUT_DESVIO_MS);
          else if (subEstadoNav == Inspeccion_Area) tiempoAgotado = (millis() - inicioSubEstadoMs > 15000); 
          else if (subEstadoNav == Retroceder_Verificacion) tiempoAgotado = (millis() - inicioSubEstadoMs > 5000);
          else if (subEstadoNav == Avanzar_Reintento) tiempoAgotado = (millis() - inicioSubEstadoMs > 5000);
          else if (subEstadoNav == Falla_Recoleccion) tiempoAgotado = (millis() - inicioSubEstadoMs > 15000); 
          else if (subEstadoNav == Recuperacion_Recoleccion) tiempoAgotado = (millis() - inicioSubEstadoMs > 5000);
          else if (subEstadoNav == Recuperacion_Trayectoria_Residuo) tiempoAgotado = (millis() - inicioSubEstadoMs > 20000);

          if (atascado || tiempoAgotado) {
            atascoEnAvance = (subEstadoNav == WP_AVANZAR_DISTANCIA || subEstadoNav == WP_DESVIO_AVANZAR || subEstadoNav == Falla_Recoleccion);
            intentosAtasco++; aplicarMotores(0, "paro");
            if (intentosAtasco > MAX_REINTENTOS) cancelarMision(atascado ? 3 : 4);
            else { avisarMision(atascado ? 1 : 2); faseRecuperar = 0; tiempoPausa = millis(); cambiarSubEstado(WP_RECUPERAR); }
          }
        }

        switch (subEstadoNav) {
          case WP_CALCULAR_TRAMO: { aplicarMotores(0, "paro"); cambiarSubEstado(WP_GIRAR); break; }

          case WP_GIRAR: {
            float dx = (float)wpX[indiceWPActual] - posX, dy = (float)wpY[indiceWPActual] - posY;
            anguloGiroMetaDeg = atan2(dy, dx) * RAD_TO_DEG;
            float diffAng = anguloGiroMetaDeg - rumbo.getThetaDeg();
            while (diffAng >  180.0) diffAng -= 360.0; while (diffAng < -180.0) diffAng += 360.0;
            if (fabs(diffAng) <= TOL_GIRO_DEG) { aplicarMotores(0, "paro"); tiempoPausa = millis(); cambiarSubEstado(WP_PAUSA_ANTES_AVANCE); }
            else { int pwmG = constrain((int)(1.0 * fabsf(diffAng)) + 45, 50, PWM_GIRO_NAV); aplicarMotores(pwmG, diffAng > 0 ? "izquierda" : "derecha"); }
            break;
          }

          case WP_PAUSA_ANTES_AVANCE: {
            if (millis() - tiempoPausa >= 500) {
              float dx = (float)wpX[indiceWPActual] - posX, dy = (float)wpY[indiceWPActual] - posY;
              float d = sqrt(dx * dx + dy * dy); 
              timeoutAvanceMs = TIMEOUT_AVANCE_BASE_MS + (unsigned long)(d * TIMEOUT_MS_POR_M);
              velocidadActual = velocidadBase; aplicarMotores(velocidadActual, "adelante"); 
              rumbo.setAnguloObjetivo(anguloGiroMetaDeg); cambiarSubEstado(WP_AVANZAR_DISTANCIA);
            }
            break;
          }

          case WP_AVANZAR_DISTANCIA: {
            float dx = (float)wpX[indiceWPActual] - posX, dy = (float)wpY[indiceWPActual] - posY;
            float dist = sqrt(dx * dx + dy * dy);
            if (inspeccion.check_necesita_barrido(deltaDist, dist)) { 
              aplicarMotores(0, "paro"); inspeccion.iniciar_barrido(rumbo.getThetaDeg()); avisarMision(16); cambiarSubEstado(Inspeccion_Area); break; 
            }
            if (dist < radioLlegadaM) { 
              aplicarMotores(0, "paro"); tiempoPausa = millis(); cambiarSubEstado(WP_PAUSA_LLEGADA); 
            }
            else {
              float brg = atan2(dy, dx) * RAD_TO_DEG, e = brg - rumbo.getThetaDeg();
              while (e > 180.0) e -= 360.0; while (e < -180.0) e += 360.0;
              if (fabs(e) > UMBRAL_REORIENTAR_DEG) { aplicarMotores(0, "paro"); cambiarSubEstado(WP_GIRAR); }
              else if (dist > distSinCorregirM) {
                float diffObj = brg - rumbo.getAnguloObjetivo();
                while (diffObj > 180.0) diffObj -= 360.0; while (diffObj < -180.0) diffObj += 360.0;
                diffObj = constrain(diffObj, -15.0f, 15.0f); float newObj = rumbo.getAnguloObjetivo() + 0.05f * diffObj;
                rumbo.setAnguloObjetivo(newObj);
              }
              int velAvance = velocidadBase; 
              if (dist < 1.0f) { 
                int velMin = min(95, velocidadBase);
                velAvance = constrain((int)(dist * 150.0f), velMin, velocidadBase); 
              }
              if (comandoActual != "adelante" || velocidadActual != velAvance) { velocidadActual = velAvance; aplicarMotores(velocidadActual, "adelante"); }
            }
            break;
          }

          case WP_PAUSA_LLEGADA: {
            if (millis() - tiempoPausa >= 800) {
              indiceWPActual++; intentosAtasco = 0; inspeccion.reset_distancia();
              if (indiceWPActual >= totalWaypoints) { 
                avisarMision(25); indiceWPActual = 1; estadoRobot = NAV_WAYPOINTS;
                if (modoOdomMision) posX = 0.0, posY = 0.0, rawX = 0.0, rawY = 0.0;
                Serial.println(">> Vuelta completada. Reiniciando ruta infinita... <<");
                cambiarSubEstado(WP_CALCULAR_TRAMO);
              } else cambiarSubEstado(WP_CALCULAR_TRAMO);
            }
            break;
          }

          case WP_RECUPERAR: {
            controlarCepillo(false); 
            if (faseRecuperar == 0) {
              if (millis() - tiempoPausa >= 300) { distRecuperada = 0.0; tiempoInicioRetroceso = millis(); velocidadActual = velocidadBase; aplicarMotores(velocidadActual, "atras"); faseRecuperar = 1; }
            } else if (faseRecuperar == 1) {
              distRecuperada += -deltaDist;
              if (distRecuperada >= DIST_RETROCESO_M || millis() - tiempoInicioRetroceso > 2000) { aplicarMotores(0, "paro"); tiempoPausa = millis(); faseRecuperar = 2; }
            } else {
              if (millis() - tiempoPausa >= 300) {
                if (atascoEnAvance) {
                  int n = max(intentosAtasco - 1, 0);
                  float ang = (DESVIO_ANGULO_BASE_DEG + DESVIO_ANGULO_INC_DEG * n) * ladoDesvio;
                  anguloDesvioMetaDeg = rumbo.getThetaDeg() + ang;
                  while (anguloDesvioMetaDeg > 180.0) anguloDesvioMetaDeg -= 360.0;
                  while (anguloDesvioMetaDeg < -180.0) anguloDesvioMetaDeg += 360.0;
                  distDesvioMeta = DESVIO_DIST_BASE_M + DESVIO_DIST_INC_M * n;
                  distDesvioAcum = 0.0; faseDesvio = 0; ladoDesvio = -ladoDesvio; avisarMision(9); cambiarSubEstado(WP_DESVIO_GIRAR);
                } else cambiarSubEstado(WP_CALCULAR_TRAMO);
              }
            }
            break;
          }

          case WP_DESVIO_GIRAR: {
            float diff = anguloDesvioMetaDeg - rumbo.getThetaDeg();
            while (diff > 180.0) diff -= 360.0; while (diff < -180.0) diff += 360.0;
            if (fabs(diff) <= TOL_GIRO_DEG) { aplicarMotores(0, "paro"); tiempoPausa = millis(); faseDesvio = 0; cambiarSubEstado(WP_DESVIO_AVANZAR); }
            else { int pwmG = constrain((int)(1.0 * fabsf(diff)) + 45, 50, PWM_GIRO_NAV); aplicarMotores(pwmG, diff > 0 ? "izquierda" : "derecha"); }
            break;
          }

          case WP_DESVIO_AVANZAR: {
            if (faseDesvio == 0) {
              if (millis() - tiempoPausa >= 400) { distDesvioAcum = 0.0; velocidadActual = velocidadBase; aplicarMotores(velocidadActual, "adelante"); rumbo.setAnguloObjetivo(anguloDesvioMetaDeg); faseDesvio = 1; }
            } else if (faseDesvio == 1) {
              distDesvioAcum += deltaDist;
              if (distDesvioAcum >= distDesvioMeta) { aplicarMotores(0, "paro"); tiempoPausa = millis(); faseDesvio = 2; }
            } else if (millis() - tiempoPausa > 300) cambiarSubEstado(WP_CALCULAR_TRAMO);
            break;
          }

          case Inspeccion_Area: {
            if (inspeccion.actualizar_barrido(rumbo.getThetaDeg())) { if (comandoActual != "izquierda") aplicarMotores(80, "izquierda"); }
            else { aplicarMotores(0, "paro"); inspeccion.reset_distancia(); cambiarSubEstado(WP_CALCULAR_TRAMO); } 
            break;
          }

          case Recolectar_Residuos: {
            if (rumbo.detectarGolpe()) {
              aplicarMotores(0, "paro"); controlarCepillo(false); avisarMision(22); cambiarSubEstado(Falla_Trayectoria_residuo); break;
            }
            bool viendo_ahora  = (millis() - ultima_deteccion_camara_ms < TIMEOUT_VISTA_CAMARA_MS);
            bool timeout_total = (millis() - inicioSubEstadoMs > TIMEOUT_RECOLECCION_MS);
            if (!viendo_ahora || timeout_total) {
              aplicarMotores(0, "paro"); distVerificacionRecorrida = 0.0; cambiarSubEstado(Retroceder_Verificacion);
            } else {
              if (comandoActual != "adelante" || velocidadActual != VEL_RECOLECCION) {
                velocidadActual = VEL_RECOLECCION; aplicarMotores(velocidadActual, "adelante");
              }
              if (!cepilloEncendido) controlarCepillo(true);
            }
            break;
          }

          case Retroceder_Verificacion: {
            if (comandoActual != "atras") { velocidadActual = velocidadBase; aplicarMotores(velocidadActual, "atras"); }
            distVerificacionRecorrida += -deltaDist;
            if (distVerificacionRecorrida >= DIST_VERIFICACION_M) { 
              aplicarMotores(0, "paro"); tiempoPausa = millis(); falla.iniciar(rumbo.getThetaDeg()); avisarMision(20); cambiarSubEstado(Falla_Recoleccion); 
            }
            break;
          }

          case Falla_Recoleccion: {
            if (vision_ia.hay_deteccion() && vision_ia.get_clase() < CAM_NUM_CLASES) {
                vision_ia.limpiar_deteccion(); falla.confirmar_exito();
            }
            bool izq = false, der = false;
            if (falla.actualizar(rumbo.getThetaDeg(), millis(), izq, der)) {
              if (izq) aplicarMotores(80, "izquierda"); else if (der) aplicarMotores(80, "derecha"); else aplicarMotores(0, "paro");
            } else {
              if (falla.fallo_definitivo()) { 
                controlarCepillo(false); recuperacion.iniciar(posX, posY); avisarMision(19); cambiarSubEstado(Recuperacion_Recoleccion); 
              }
              else if (falla.fue_exitoso()) { avisarMision(17); controlarCepillo(false); cambiarSubEstado(WP_CALCULAR_TRAMO); }
              else { distVerificacionRecorrida = 0.0; cambiarSubEstado(Avanzar_Reintento); }
            }
            break;
          }

          case Avanzar_Reintento: {
            if (comandoActual != "adelante") { velocidadActual = velocidadBase; aplicarMotores(velocidadActual, "adelante"); }
            distVerificacionRecorrida += deltaDist;
            if (distVerificacionRecorrida >= DIST_VERIFICACION_M) {
              aplicarMotores(0, "paro"); tiempoPausa = millis();
              ultima_deteccion_camara_ms = millis(); controlarCepillo(true);
              avisarMision(14); cambiarSubEstado(Recolectar_Residuos);
            }
            break;
          }

          case Recuperacion_Recoleccion: {
            if (recuperacion.actualizar(millis())) { if (comandoActual != "paro") aplicarMotores(0, "paro"); }
            else { cambiarSubEstado(WP_CALCULAR_TRAMO); }
            break;
          }

          case Falla_Trayectoria_residuo: { 
            aplicarMotores(0, "paro"); recuperacion_trayectoria.iniciar(rumbo.getThetaDeg()); cambiarSubEstado(Recuperacion_Trayectoria_Residuo); break;
          }

          case Recuperacion_Trayectoria_Residuo: {
            bool girarIzq = false;
            if (recuperacion_trayectoria.actualizar(rumbo.getThetaDeg(), millis(), girarIzq)) {
              if (girarIzq) { if (comandoActual != "izquierda") aplicarMotores(80, "izquierda"); } 
              else { if (comandoActual != "paro") aplicarMotores(0, "paro"); }
            } else {
              bool exito = recuperacion_trayectoria.fue_exitoso(); recuperacion_trayectoria.reset();
              if (exito) { controlarCepillo(true); ultima_deteccion_camara_ms = millis(); cambiarSubEstado(WP_CALCULAR_TRAMO); }
              else { avisarMision(24); cambiarSubEstado(WP_CALCULAR_TRAMO); }
            }
            break;
          }
          
          case SUB_CAMBIO_RUTA_ESTATICOS: {
            float angObj = rumbo.getAnguloObjetivo(); 
            EstadoCambioEstatico estEv = movimiento_estatico.actualizar_evasion(deltaDist, rumbo.getThetaDeg(), angObj);
            if (estEv == CAMBIO_ESTATICO_TRAZANDO_ARCO) { 
              if (comandoActual != "adelante") { velocidadActual = velocidadBase; aplicarMotores(velocidadActual, "adelante"); } 
              rumbo.setAnguloObjetivo(angObj); 
            }
            else if (estEv == CAMBIO_ESTATICO_ESPERANDO_RECHECK) { if (comandoActual != "paro") aplicarMotores(0, "paro"); }
            else if (estEv == CAMBIO_ESTATICO_COMPLETO) { movimiento_estatico.reset(); cambiarSubEstado(WP_CALCULAR_TRAMO); }
            break;
          }

          case SUB_CAMBIO_RUTA_DINAMICOS: {
            float angObj = rumbo.getAnguloObjetivo(); bool girarIzq = false;
            if (movimiento_dinamico.actualizar_escaneo(millis(), rumbo.getThetaDeg(), angObj, girarIzq)) {
              if (comandoActual != (girarIzq ? "izquierda" : "derecha")) { aplicarMotores(PWM_GIRO_DINAMICO, girarIzq ? "izquierda" : "derecha"); }
              rumbo.setAnguloObjetivo(angObj);
            } else { movimiento_dinamico.reset(); aplicarMotores(0, "paro"); cambiarSubEstado(WP_CALCULAR_TRAMO); }
            break;
          }
        }
      }
    }

    if (estadoRobot == CUADRADO_AVANZAR) {
      distLadoAcumulada += deltaDist;
      if (distLadoAcumulada >= LONGITUD_LADO) { aplicarMotores(0, "paro"); tiempoPausa = millis(); estadoRobot = CUADRADO_PAUSA_GIRO; }
    }
    else if (estadoRobot == CUADRADO_PAUSA_GIRO) {
      if (millis() - tiempoPausa >= 500) { 
        anguloGiroMetaDeg = rumbo.getThetaDeg() + ANGULO_GIRO_DEG;
        while (anguloGiroMetaDeg > 180.0) anguloGiroMetaDeg -= 360.0; while (anguloGiroMetaDeg < -180.0) anguloGiroMetaDeg += 360.0;
        estadoRobot = CUADRADO_GIRAR;
      }
    }
    else if (estadoRobot == CUADRADO_GIRAR) {
      float diff = anguloGiroMetaDeg - rumbo.getThetaDeg();
      while (diff > 180.0) diff -= 360.0; while (diff < -180.0) diff += 360.0;
      if (fabs(diff) <= 3.0) { aplicarMotores(0, "paro"); tiempoPausa = millis(); estadoRobot = CUADRADO_PAUSA_SIG; }
      else { int pwmG = constrain((int)(1.0 * fabsf(diff)) + 45, 50, PWM_GIRO_NAV); aplicarMotores(pwmG, diff > 0 ? "izquierda" : "derecha"); }
    }
    else if (estadoRobot == CUADRADO_PAUSA_SIG) {
      if (millis() - tiempoPausa >= 500) {
        ladoActual++;
        if (ladoActual >= 4) { estadoRobot = CUADRADO_COMPLETO; aplicarMotores(0, "paro"); }
        else {
          rumbo.setAnguloObjetivo(rumbo.getThetaDeg());
          distLadoAcumulada = 0.0; velocidadActual = velocidadBase; aplicarMotores(velocidadActual, "adelante"); estadoRobot = CUADRADO_AVANZAR;
        }
      }
    }

    rumbo.cerrarTick();
  }
}