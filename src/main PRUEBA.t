#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// Pines del cepillo
const int PIN_CEPILLO_RPWM = 13;
const int PIN_CEPILLO_REN  = 14;

// Estructura IDÉNTICA a la de tu cámara original
struct __attribute__((packed)) PaqueteVision {
    uint8_t magic;     
    uint8_t activar;   
    uint8_t clase;     
    uint8_t seq;       
};

unsigned long ultimo_paquete_ms = 0;
bool cepillo_encendido = false;

// Callback ESP-NOW
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
#else
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
#endif
    if (len == sizeof(PaqueteVision)) {
        PaqueteVision pkt;
        memcpy(&pkt, incomingData, sizeof(pkt));
        if (pkt.magic == 0xCE && pkt.activar == 1) {
            ultimo_paquete_ms = millis();
            Serial.printf(">>> [RX] Objeto detectado por la cámara! Clase: %d\n", pkt.clase);
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    // 1. Configurar PWM del cepillo (API nueva ESP-IDF 5.x)
    ledcAttach(PIN_CEPILLO_RPWM, 10000, 8);
    pinMode(PIN_CEPILLO_REN, OUTPUT);
    digitalWrite(PIN_CEPILLO_REN, LOW);

    // 2. Crear WiFi AP en Canal 1
    WiFi.mode(WIFI_AP);
    WiFi.softAP("CleanTECH_Robot", NULL, 1);
    
    Serial.println("\n--- MODO PRUEBA DE CEPILLO ---");
    Serial.printf("WiFi AP: CleanTECH_Robot | Canal: 1\n");
    // ⚠️ COPIA ESTA MAC Y PÉGALA EN receptorMAC DE TU CÁMARA
    Serial.printf("MAC del Robot: %s\n\n", WiFi.softAPmacAddress().c_str());

    // 3. Iniciar ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error ESP-NOW");
        return;
    }
    esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
    // Si la cámara mandó un paquete en el último 1.5 segundos, mantener cepillo encendido
    if (millis() - ultimo_paquete_ms < 1500) {
        if (!cepillo_encendido) {
            digitalWrite(PIN_CEPILLO_REN, HIGH); // Activar driver
            ledcWrite(PIN_CEPILLO_RPWM, 255);    // Giro en un solo sentido
            cepillo_encendido = true;
            Serial.println("--- CEPILLO ENCENDIDO ---");
        }
    } else {
        if (cepillo_encendido) {
            ledcWrite(PIN_CEPILLO_RPWM, 0);
            digitalWrite(PIN_CEPILLO_REN, LOW);
            cepillo_encendido = false;
            Serial.println("--- CEPILLO APAGADO (Timeout) ---");
        }
    }
}