// #include <Arduino.h>
// #include <esp_now.h>
// #include <WiFi.h>
// #include <esp_camera.h>
// #include <camera_pins.h>

// unsigned long Tiempo_Actual, Tiempo_Anterior;

// typedef struct struct_message
// {
//     int label_id;
//     float x, y, value;
// } struct_message;

// struct_message myData;

// void setup_camera()
// {
//     camera_config_t config;
//     config.ledc_channel = LEDC_CHANNEL_0;
//     config.ledc_timer = LEDC_TIMER_0;
//     config.pin_d0 = Y2_GPIO_NUM;
//     config.pin_d1 = Y3_GPIO_NUM;
//     config.pin_d2 = Y4_GPIO_NUM;
//     config.pin_d3 = Y5_GPIO_NUM;
//     config.pin_d4 = Y6_GPIO_NUM;
//     config.pin_d5 = Y7_GPIO_NUM;
//     config.pin_d6 = Y8_GPIO_NUM;
//     config.pin_d7 = Y9_GPIO_NUM;
//     config.pin_xclk = XCLK_GPIO_NUM;
//     config.pin_pclk = PCLK_GPIO_NUM;
//     config.pin_vsync = VSYNC_GPIO_NUM;
//     config.pin_href = HREF_GPIO_NUM;
//     config.pin_sscb_sda = SIOD_GPIO_NUM;
//     config.pin_sscb_scl = SIOC_GPIO_NUM;
//     config.pin_pwdn = PWDN_GPIO_NUM;
//     config.pin_reset = RESET_GPIO_NUM;
//     config.xclk_freq_hz = 20000000;
//     config.pixel_format = PIXFORMAT_RGB565; // FOMO necesita RGB565
//     config.frame_size = FRAMESIZE_96X96;
//     config.fb_count = 1;
//     config.fb_location = CAMERA_FB_IN_PSRAM;

//     if (esp_camera_init(&config) != ESP_OK)
//     {
//         Serial.println("Error al inicializar la cámara");
//         return;
//     }
// }

// void OnDataSent(const uint8_t *mac, esp_now_send_status_t status)
// {
// }

// void Configuracion_esp_now()
// {
//     WiFi.mode(WIFI_STA);
//     if (esp_now_init() != ESP_OK)
//     {
//         Serial.println("Error al iniciar Esp NOW");
//         return;
//     }
//     esp_now_register_send_cb(OnDataSent());
//     esp_now_peer_info_t peerInfo = {};
//     memcpy(peerInfo.peer_addr, receiverMA, 6);
//     peerInfo.channel = 0;
//     peerInfo.encrypt = false;
//     esp_now_add_peer(&peerInfo);
// }

// void setup()
// {
//     Serial.begin(115200);
//     setup_camera();
//     Configuracion_esp_now();
//     Tiempo_Anterior = millis();
// }

// void loop()
// {
//     Tiempo_Actual = millis();

//     if (Tiempo_Actual - Tiempo_Anterior >= 50)
//     {
//         Tiempo_Anterior = Tiempo_Actual;
//         camera_fb_t *fb = esp_camera_fb_get();
//         if(!fb) return;

//         ei_impulse_result_t result = 0;
        
//     }
// }

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// ==========================================
// CONFIGURACIÓN DE WIFI
// ==========================================
// ==========================================
// CONFIGURACIÓN DE WIFI (TU HOTSPOT)
// ==========================================
const char* ssid = "Redmi Note 12 Pro 5G";
const char* password = "Leobardo"; // <-- CAMBIA ESTO

// ==========================================
// PINES PARA ESP32 CLÁSICO (AI-Thinker / HW-818)
// ==========================================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ==========================================
// SERVIDOR WEB
// ==========================================
httpd_handle_t camera_httpd = NULL;

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  char part_buf[64];

  res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=123456789000000000000987654321");
  if (res != ESP_OK) return res;
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return ESP_FAIL;
    }

    size_t jpg_len = fb->len;
    size_t hlen = snprintf(part_buf, 64, "\r\n--123456789000000000000987654321\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", jpg_len);

    res = httpd_resp_send_chunk(req, part_buf, hlen);
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)fb->buf, jpg_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, "\r\n", 2);
    }
    
    esp_camera_fb_return(fb);
    if (res != ESP_OK) break;
  }
  return res;
}

static esp_err_t index_handler(httpd_req_t *req) {
  const char* html = "<html><body style='text-align:center;'><h1>ESP32-CAM</h1><img src='/stream' style='max-width:100%;'></body></html>";
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, html, strlen(html));
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &stream_uri);
  }
}

// ... configuración de cámara, pines y WiFi igual que antes ...

// Lee un archivo del FS y lo envía al cliente
static esp_err_t send_file(httpd_req_t *req, const char *path, const char *mime) {
  File f = LittleFS.open(path, "r");
  if (!f) {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, mime);
  httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600");

  char buf[512];
  size_t n;
  while ((n = f.readBytes(buf, sizeof(buf))) > 0) {
    if (httpd_resp_send_chunk(req, buf, n) != ESP_OK) {
      f.close();
      return ESP_FAIL;
    }
  }
  f.close();
  httpd_resp_send_chunk(req, NULL, 0);   // cierra el chunked
  return ESP_OK;
}

static esp_err_t index_handler(httpd_req_t *req) {
  return send_file(req, "/index.html", "text/html; charset=utf-8");
}

// Bonus: sirve CSS, JS, favicon, etc. desde /data
static esp_err_t static_handler(httpd_req_t *req) {
  char path[64];
  snprintf(path, sizeof(path), "%s", req->uri);

  const char *mime = "text/plain";
  if (strstr(path, ".html")) mime = "text/html; charset=utf-8";
  else if (strstr(path, ".css")) mime = "text/css";
  else if (strstr(path, ".js"))  mime = "application/javascript";
  else if (strstr(path, ".png")) mime = "image/png";
  else if (strstr(path, ".ico")) mime = "image/x-icon";

  return send_file(req, path, mime);
}

// ==========================================
// SETUP PRINCIPAL
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nIniciando ESP32-CAM Clasico...");

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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Usar PSRAM si está disponible (el HW-818 suele tener 4MB de PSRAM)
  if(psramFound()){
    config.frame_size = FRAMESIZE_SVGA; 
    config.jpeg_quality = 12;
    config.fb_count = 2;
    Serial.println("PSRAM detectada. Usando resolucion SVGA.");
  } else {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    Serial.println("Sin PSRAM. Usando resolucion VGA.");
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Error al iniciar la camara: 0x%x\n", err);
    return;
  }
  Serial.println("Camara inicializada correctamente.");

  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado.");

  startCameraServer();

  Serial.print("Servidor listo! Entra a: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  delay(10000);
}