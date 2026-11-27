#include "Comunicacion_IA.h"

volatile bool Comunicacion_IA::_deteccion_activa = false;
volatile uint8_t Comunicacion_IA::_clase_detectada = 255;
volatile unsigned long Comunicacion_IA::_ultimo_reconocido_ms = 0;
volatile unsigned long Comunicacion_IA::_ultimo_dinamico_ms = 0;

Comunicacion_IA::Comunicacion_IA() {}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void Comunicacion_IA::OnDataRecv(const esp_now_recv_info_t *info,
                                 const uint8_t *incomingData, int len) {
    const uint8_t *mac = info->src_addr;
#else
void Comunicacion_IA::OnDataRecv(const uint8_t *mac,
                                 const uint8_t *incomingData, int len) {
#endif
    PaqueteVision pkt;
    if (len == (int)sizeof(PaqueteVision)) {
        memcpy(&pkt, incomingData, sizeof(pkt));
        if (pkt.magic == 0xCE) {
            unsigned long ahora = millis();

            if (pkt.activar == 1) {
                _deteccion_activa = true;
                _clase_detectada  = pkt.clase;
                _ultimo_reconocido_ms = ahora;
            }
            if (pkt.dinamico == 1) {
                _ultimo_dinamico_ms = ahora;
            }
        }
    }
    (void)mac;
}

void Comunicacion_IA::begin() {
    if (esp_now_init() != ESP_OK) {
        Serial.println("[IA] Error inicializando ESP-NOW");
        return;
    }
    esp_now_register_recv_cb(OnDataRecv);
    Serial.printf("[IA] ESP-NOW listo. Escuchando a la cámara en canal %d\n", WiFi.channel());
}

bool Comunicacion_IA::hay_deteccion() { return _deteccion_activa; }

uint8_t Comunicacion_IA::get_clase() { return _clase_detectada; }

void Comunicacion_IA::limpiar_deteccion() {
    _deteccion_activa = false;
    _clase_detectada  = 255;
}

bool Comunicacion_IA::hay_objeto_reconocido() {
    return (_ultimo_reconocido_ms > 0) && (millis() - _ultimo_reconocido_ms < 3000);
}

bool Comunicacion_IA::hay_objeto_dinamico() {
    return (_ultimo_dinamico_ms > 0) && (millis() - _ultimo_dinamico_ms < 3000);
}