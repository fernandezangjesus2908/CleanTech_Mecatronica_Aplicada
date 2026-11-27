#ifndef COMUNICACION_IA_H
#define COMUNICACION_IA_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>

struct __attribute__((packed)) PaqueteVision {
    uint8_t magic;      // 0xCE
    uint8_t activar;    // 1 = habilitar colector
    uint8_t clase;      // indice de la clase (255 = ninguna)
    uint8_t seq;        // contador
    uint8_t dinamico;   // 1 = objeto dinámico detectado
    uint8_t clase_din;  // clase del dinámico (protocolo, no se usa en UI)
};

class Comunicacion_IA {
public:
    Comunicacion_IA();
    void begin();

    // Objeto válido para recolectar (edge-triggered)
    bool hay_deteccion();
    uint8_t get_clase();
    void limpiar_deteccion();

    // Para UI: solo booleanos, auto-expiran a los 3s
    bool hay_objeto_reconocido();
    bool hay_objeto_dinamico();

private:
    static volatile bool _deteccion_activa;
    static volatile uint8_t _clase_detectada;

    static volatile unsigned long _ultimo_reconocido_ms;
    static volatile unsigned long _ultimo_dinamico_ms;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    static void OnDataRecv(const esp_now_recv_info_t *info,
                           const uint8_t *incomingData, int len);
#else
    static void OnDataRecv(const uint8_t *mac,
                           const uint8_t *incomingData, int len);
#endif
};

#endif