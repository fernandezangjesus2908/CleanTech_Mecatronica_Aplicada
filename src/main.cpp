#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <camera_pins.h>

unsigned long Tiempo_Actual, Tiempo_Anterior;

typedef struct struct_message
{
    int label_id;
    float x, y, value;
} struct_message;

struct_message myData;

void setup_camera()
{
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_RGB565; // FOMO necesita RGB565
    config.frame_size = FRAMESIZE_96X96;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_PSRAM;

    if (esp_camera_init(&config) != ESP_OK)
    {
        Serial.println("Error al inicializar la cámara");
        return;
    }
}

void OnDataSent(const uint8_t *mac, esp_now_send_status_t status)
{
}

void Configuracion_esp_now()
{
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Error al iniciar Esp NOW");
        return;
    }
    esp_now_register_send_cb(OnDataSent());
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, receiverMA, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
}

void setup()
{
    Serial.begin(115200);
    setup_camera();
    Configuracion_esp_now();
    Tiempo_Anterior = millis();
}

void loop()
{
    Tiempo_Actual = millis();

    if (Tiempo_Actual - Tiempo_Anterior >= 50)
    {
        Tiempo_Anterior = Tiempo_Actual;
        camera_fb_t *fb = esp_camera_fb_get();
        if(!fb) return;
    }
}