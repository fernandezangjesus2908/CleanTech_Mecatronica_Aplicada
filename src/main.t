#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include "camera_pins.h"
#include <LittleFS.h>
#include <esp_now.h>
#include "img_converters.h" 

#include <CleanTECH_-_Vision_inferencing.h> 

const char *ssid = "Mega-2.4G-7BAD";
const char *password = "JUEtg4yu9C";

httpd_handle_t camera_httpd = NULL;

// ==========================================
// VARIABLES PARA LAS COORDENADAS (CANVAS)
// ==========================================
volatile float last_x = 0.0;
volatile float last_y = 0.0;
volatile bool last_detect = false;

// ==========================================
// CONFIGURACIÓN ESP-NOW
// ==========================================
// MAC de tu ESP32-S3
uint8_t receptorMAC[] = {0x1C, 0xDB, 0xD4, 0x40, 0x37,,, 0x64};
esp_now_peer_info_t peerInfo;

void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) { }

void setup_espnow() {
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error inicializando ESP-NOW");
        return;
    }
    esp_now_register_send_cb(OnDataSent);
    memcpy(peerInfo.peer_addr, receptorMAC, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
        Serial.println("Fallo al añadir peer");
        return;
    }
    Serial.println("ESP-NOW Inicializado");
}

// ==========================================
// SERVIDOR WEB Y CÁMARA
// ==========================================
static esp_err_t send_file(httpd_req_t *req, const char *path, const char *mime) {
    File file = LittleFS.open(path, "r");
    if (!file) { httpd_resp_send_404(req); return ESP_FAIL; }
    httpd_resp_set_type(req, mime);
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate"); 
    char buf[512];
    size_t n;
    while ((n = file.readBytes(buf, sizeof(buf))) > 0) {
        if (httpd_resp_send_chunk(req, buf, n) != ESP_OK) { file.close(); return ESP_FAIL; }
    }
    file.close();
    httpd_resp_send_chunk(req, NULL, 0); 
    return ESP_OK;
}

// Endpoint JSON para el Canvas del Frontend
static esp_err_t coords_handler(httpd_req_t *req) {
    char json[100];
    snprintf(json, sizeof(json), "{\"x\": %.4f, \"y\": %.4f, \"detectado\": %s}", 
             last_x, last_y, last_detect ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t index_handler(httpd_req_t *req) { return send_file(req, "/Interfaz_CleanTECH.html", "text/html"); }
static esp_err_t css_handler(httpd_req_t *req) { return send_file(req, "/Interfaz_CleanTECH.css", "text/css"); }
static esp_err_t js_handler(httpd_req_t *req) { return send_file(req, "/Interfaz_CleanTECH.js", "application/javascript"); }

static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    char part_buf[128];
    res = httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=123456789000000000000987654321");
    if (res != ESP_OK) return res;
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) return ESP_FAIL;
        size_t jpg_LEN = fb->len;
        size_t hlen = snprintf(part_buf, 128, "\r\n--123456789000000000000987654321\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", jpg_LEN);
        res = httpd_resp_send_chunk(req, part_buf, hlen);
        if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)fb->buf, jpg_LEN);
        if (res == ESP_OK) res = httpd_resp_send_chunk(req, "\r\n", 2);
        esp_camera_fb_return(fb);
        if (res != ESP_OK) break;
        vTaskDelay(10 / portTICK_PERIOD_MS); 
    }
    return res;
}

void Iniciar_Servidor_Camara() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true; 
    config.max_open_sockets = 4; 
    
    httpd_uri_t stream_uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL };
    httpd_uri_t index_uri  = { .uri = "/",       .method = HTTP_GET, .handler = index_handler,  .user_ctx = NULL };
    httpd_uri_t css_uri    = { .uri = "/Interfaz_CleanTECH.css", .method = HTTP_GET, .handler = css_handler, .user_ctx = NULL };
    httpd_uri_t js_uri     = { .uri = "/Interfaz_CleanTECH.js",  .method = HTTP_GET, .handler = js_handler,  .user_ctx = NULL };
    httpd_uri_t coords_uri = { .uri = "/coords", .method = HTTP_GET, .handler = coords_handler, .user_ctx = NULL };

    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &stream_uri);
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &css_uri);
        httpd_register_uri_handler(camera_httpd, &js_uri);
        httpd_register_uri_handler(camera_httpd, &coords_uri);
    }
}

// ==========================================
// TAREA DE EDGE IMPULSE (Inferencia)
// ==========================================
uint8_t* ei_buffer = NULL; 
uint8_t* rgb_buffer = NULL; 

static int get_signal_data(size_t offset, size_t length, float *out_ptr) {
    size_t out_idx = 0;
    for (size_t i = offset; i < offset + length; i++) {
        out_ptr[out_idx++] = (float)((uint8_t*)ei_buffer)[i]; 
    }
    return EIDSP_OK;
}

void inference_task(void *pvParameters) {
    Serial.println("Tarea de Inferencia Iniciada");
    size_t ei_len = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    
    if (ei_buffer == NULL) ei_buffer = (uint8_t*)ps_malloc(ei_len); 
    if (ei_buffer == NULL) {
        Serial.println("ERROR CRITICO: No hay PSRAM para ei_buffer. Abortando.");
        vTaskDelete(NULL); 
    }

    while (1) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (rgb_buffer == NULL) {
            // QVGA: 320x240x3 = ~230 KB
            rgb_buffer = (uint8_t*)ps_malloc(fb->width * fb->height * 3);
            if(rgb_buffer == NULL){
                 Serial.println("ERROR: No hay PSRAM suficiente para rgb_buffer.");
                 esp_camera_fb_return(fb);
                 vTaskDelay(pdMS_TO_TICKS(1000));
                 continue;
            }
        }

        if (fmt2rgb888(fb->buf, fb->len, fb->format, rgb_buffer)) {
            int x_ratio = (int)((fb->width << 16) / EI_CLASSIFIER_INPUT_WIDTH) + 1;
            int y_ratio = (int)((fb->height << 16) / EI_CLASSIFIER_INPUT_HEIGHT) + 1;
            
            for (int i = 0; i < EI_CLASSIFIER_INPUT_HEIGHT; i++) {
                for (int j = 0; j < EI_CLASSIFIER_INPUT_WIDTH; j++) {
                    int x2 = ((j * x_ratio) >> 16);
                    int y2 = ((i * y_ratio) >> 16);
                    int pixel_idx = (y2 * fb->width + x2) * 3;
                    uint8_t r = rgb_buffer[pixel_idx];
                    uint8_t g = rgb_buffer[pixel_idx + 1];
                    uint8_t b = rgb_buffer[pixel_idx + 2];
                    ei_buffer[i * EI_CLASSIFIER_INPUT_WIDTH + j] = (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
                }
            }

            signal_t signal;
            signal.total_length = ei_len;
            signal.get_data = &get_signal_data;
            
            ei_impulse_result_t result = { 0 };
            EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);
            
            if (res == EI_IMPULSE_OK) {
                bool detectado = false;
                float temp_x = 0.0;
                float temp_y = 0.0;

                // --- DEBUG EN SERIAL ---
                if (result.bounding_boxes_count > 0) {
                    Serial.printf("Detecciones: %d -> ", result.bounding_boxes_count);
                }

                for (size_t ix = 0; ix < result.bounding_boxes_count; ix++) {
                    auto bb = result.bounding_boxes[ix];
                    if (bb.value == 0) continue;
                    
                    Serial.printf("[%s: %.2f] ", bb.label, bb.value);

                    // Detectamos Bolsas o Botellas con >30% de confianza
                    if ((strcmp(bb.label, "Bolsas") == 0 || strcmp(bb.label, "Botellas") == 0) && bb.value >= 0.30) {
                        detectado = true;
                        if(temp_x == 0.0 && temp_y == 0.0) {
                            temp_x = (float)bb.x / (float)EI_CLASSIFIER_INPUT_WIDTH;
                            temp_y = (float)bb.y / (float)EI_CLASSIFIER_INPUT_HEIGHT;
                        }
                    }
                }
                
                if (result.bounding_boxes_count > 0) Serial.println();
                // -----------------------

                last_detect = detectado;
                if (detectado) {
                    last_x = temp_x;
                    last_y = temp_y;
                    
                    uint8_t comando = 1; 
                    esp_now_send(receptorMAC, &comando, sizeof(comando));
                } else {
                    uint8_t comando = 0; 
                    esp_now_send(receptorMAC, &comando, sizeof(comando));
                }
            } else {
                Serial.println("Error en la inferencia de EI");
            }
        } else {
            Serial.println("Error convirtiendo JPEG a RGB");
        }
        
        esp_camera_fb_return(fb);
        // Regresamos a los 2 segundos (QVGA es más rápido)
        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }
}

// ==========================================
// SETUP Y LOOP
// ==========================================
void setup() {
    Serial.begin(115200);
    if (!LittleFS.begin(true)) Serial.println("Error LittleFS");

    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println("\nWiFi conectado. IP: " + WiFi.localIP().toString());
    Serial.println("Canal WiFi de la Camara: " + String(WiFi.channel()));

    setup_espnow();

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM; config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM; config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM; config.pin_pclk = PCLK_GPIO_NUM; config.pin_vsync = VSYNC_GPIO_NUM; config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM; config.pin_sccb_scl = SIOC_GPIO_NUM; config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000; 
    config.pixel_format = PIXFORMAT_JPEG;
    
    // --- RESOLUCIÓN QVGA (320x240) RESTAURADA ---
    config.frame_size = FRAMESIZE_QVGA; 
    
    config.jpeg_quality = 12;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;

    if (esp_camera_init(&config) != ESP_OK) {
        Serial.println("Error cámara");
        return;
    }
    
    Iniciar_Servidor_Camara();

    xTaskCreatePinnedToCore(
        inference_task,   
        "Inferencia",     
        16384,             
        NULL,             
        1,                
        NULL,             
        1                 
    );
}

void loop() { 
    delay(1000); 
}