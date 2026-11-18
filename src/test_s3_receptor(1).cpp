// ==========================================================
// PRUEBA - ESP32-S3 (receptor)
// Un solo BTS7960, un solo sentido, 100 %, activado por ESP-NOW
// ==========================================================
// Cableado del BTS7960:
//   RPWM -> GPIO6
//   R_EN -> GPIO7
//   L_EN -> puenteado con R_EN (o a 3.3 V): AMBAS mitades del puente deben estar habilitadas
//   LPWM -> sin conectar (o a GND)
//   VCC -> 5 V   |   GND -> GND del ESP32 (comun con la bateria)
//   B+ / B- -> bateria   |   M+ / M- -> motor
// OJO: GPIO6 y 7 son las ruedas derechas del robot completo; esto es SOLO para la prueba.
//
// Monitor serial (115200): '1' = forzar ON, '0' = forzar OFF, 'a' = automatico (ESP-NOW)

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

const int PIN_RPWM = 6;
const int PIN_EN   = 7;    // R_EN (con L_EN puenteado a este mismo pin)

// >>> CANAL IGUAL AL DE LA CAMARA <<<
// La camara imprime "Canal WiFi de la Camara: X" al arrancar. Ponlo aqui.
// Si el router cambia de canal, esto se rompe: fija el canal del router (1, 6 u 11).
const uint8_t CANAL = 7;

const unsigned long ENLACE_MAX_MS = 1500;      // sin paquetes -> se apaga

struct __attribute__((packed)) PaqueteVision {
  uint8_t magic;     // 0xCE
  uint8_t activar;   // 1 = encender
  uint8_t clase;
  uint8_t seq;
};

volatile unsigned long ultimoPaqueteMs = 0;
volatile bool ultimoActivar = false;
volatile uint32_t paquetes = 0;
int modoManual = -1;                           // -1 auto | 0 OFF | 1 ON
bool encendido = false;

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
#else
void onRecv(const uint8_t *mac, const uint8_t *data, int len) {
#endif
  if (len != (int)sizeof(PaqueteVision)) return;
  PaqueteVision p;
  memcpy(&p, data, sizeof(p));
  if (p.magic != 0xCE) return;
  ultimoActivar = (p.activar == 1);
  ultimoPaqueteMs = millis();
  paquetes++;
}

void setMotor(bool on) {
  if (on == encendido) return;
  encendido = on;
  if (on) {
    digitalWrite(PIN_RPWM, HIGH);   // HIGH fijo = 100 % de ciclo de trabajo
    digitalWrite(PIN_EN, HIGH);
  } else {
    digitalWrite(PIN_EN, LOW);
    digitalWrite(PIN_RPWM, LOW);
  }
  Serial.println(on ? ">> MOTOR ON (100%)" : ">> MOTOR OFF");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(PIN_EN, OUTPUT);
  pinMode(PIN_RPWM, OUTPUT);
  digitalWrite(PIN_EN, LOW);
  digitalWrite(PIN_RPWM, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();               // no nos conectamos a ningun AP

  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR: ESP-NOW no inicio");
    return;
  }
  esp_now_register_recv_cb(onRecv);

  // IMPORTANTE: fijar el canal DESPUES de esp_now_init().
  // esp_now_init() suele dejar el canal en 1 (default) y machaca el set_channel previo.
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(CANAL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  // Verificacion: debe imprimir el mismo numero que CANAL
  uint8_t ch; wifi_second_chan_t sc;
  esp_wifi_get_channel(&ch, &sc);
  Serial.printf("Canal real del receptor: %d (esperado %d)\n", ch, CANAL);

  Serial.print("MAC de esta placa (ponla en la camara): ");
  Serial.println(WiFi.macAddress());
  Serial.printf("Canal configurado: %d\n", CANAL);
  Serial.println("Serial: '1' ON | '0' OFF | 'a' automatico");
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '1') { modoManual = 1; Serial.println("Modo manual: ON"); }
    else if (c == '0') { modoManual = 0; Serial.println("Modo manual: OFF"); }
    else if (c == 'a') { modoManual = -1; Serial.println("Modo automatico (ESP-NOW)"); }
  }

  if (modoManual >= 0) {
    setMotor(modoManual == 1);
  } else {
    bool enlace = paquetes > 0 && (millis() - ultimoPaqueteMs) < ENLACE_MAX_MS;
    setMotor(enlace && ultimoActivar);
  }

  static unsigned long t = 0;
  if (millis() - t > 2000) {
    t = millis();
    Serial.printf("paquetes=%u  ultimo=%s  motor=%s\n", (unsigned)paquetes,
                  ultimoActivar ? "ON" : "OFF", encendido ? "ON" : "OFF");
  }
  delay(10);
}