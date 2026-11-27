#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include "camera_pins.h"
#include <LittleFS.h>
#include <esp_now.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include "img_converters.h"
#include <math.h>

#include <CleanTECH_-_Vision_inferencing.h>

// ==========================================
// CONFIGURACIÓN DE RED
// ==========================================
const char *ssid     = "CleanTECH_Robot";
const char *password = "cleantech2611"; 

httpd_handle_t camera_httpd = NULL;

// ==========================================
// DEBUG / TRAZAS
// ==========================================
#define DEBUG_BOXES 1

#if DEBUG_BOXES
  #define DBG_BOX(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
  #define DBG_BOX(fmt, ...) do {} while(0)
#endif

// ==========================================
// VARIABLES PARA LAS COORDENADAS (CANVAS)
// ==========================================
volatile float last_x      = 0.0;
volatile float last_y      = 0.0;
volatile bool  last_detect = false;
portMUX_TYPE coordsMux = portMUX_INITIALIZER_UNLOCKED;

// ==========================================
// PROTOCOLO DE ACCESO EXCLUSIVO A CÁMARA
// ==========================================
SemaphoreHandle_t camMutex = NULL;

const uint32_t CAM_GRAB_VISION_MS  = 500;   // Visión tolera que el stream esté enviando
const uint32_t CAM_GRAB_STREAM_MS  = 100;   // El stream salta el frame si no consigue
const uint32_t CAM_RETRY_DELAY_MS  = 100;
const int      CAM_WARMUP_FRAMES   = 5;

static camera_fb_t* cam_grab(uint32_t timeout_ms) {
    if (!camMutex) return NULL;
    if (xSemaphoreTake(camMutex, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) return NULL;
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) { xSemaphoreGive(camMutex); return NULL; }
    return fb;
}

static void cam_release(camera_fb_t *fb) {
    if (!fb) return;
    esp_camera_fb_return(fb);
    if (camMutex) xSemaphoreGive(camMutex);
}

// ==========================================
// CONFIGURACIÓN ESP-NOW (UNICAST AL ESP32-S3)
// ==========================================
// MAC STA del ESP32-S3 receptor
uint8_t receptorMAC[] = {0x1C, 0xDB, 0xD4, 0x40, 0x37, 0x64}; 
esp_now_peer_info_t peerInfo;

struct __attribute__((packed)) PaqueteVision {
    uint8_t magic;     
    uint8_t activar;   
    uint8_t clase;     
    uint8_t seq;       
    uint8_t dinamico;   // 1 = objeto dinámico en este frame
    uint8_t clase_din;  // clase del dinámico
};
uint8_t seq_paquete = 0;

// ==========================================
// [DIAG] CONTADORES
// ==========================================
uint32_t diag_frames         = 0;
uint32_t diag_det_raw        = 0;
uint32_t diag_desc_label     = 0;
uint32_t diag_desc_inactiva  = 0;
uint32_t diag_desc_conf      = 0;
uint32_t diag_desc_area      = 0;
uint32_t diag_desc_motion    = 0;
uint32_t diag_pos_frame      = 0;
uint32_t diag_pkt_on         = 0;
uint32_t diag_pkt_off        = 0;
volatile uint32_t diag_tx_ok    = 0;
volatile uint32_t diag_tx_fail  = 0;
volatile uint32_t diag_send_err = 0;
uint32_t diag_cam_fail       = 0;   
uint32_t diag_stream_skip    = 0;   
unsigned long lastStatusMs     = 0;

// ==========================================
// CLASES OBJETIVO
// ==========================================
struct ClaseObjetivo { const char *etiqueta; bool activa; float umbral; float areaMin; };

const ClaseObjetivo CLASES[] = {
    {"Bolsas",           true,  0.45, 20.0},
    {"Tapas",            true,  0.45, 20.0},
    {"Rotacion",         true,  0.45, 20.0},
    {"Botella_Vidrio",   true,  0.45, 20.0},
    {"Botella_Plastico", true,  0.45, 20.0},
    {"Hojas",            false, 0.60, 40.0},
};
const int NUM_CLASES = sizeof(CLASES) / sizeof(CLASES[0]);

float umbral_rt[NUM_CLASES];
float area_rt[NUM_CLASES];

const int      FRAMES_APAGAR    = 2;     
const uint32_t VISION_PERIODO_MS = 150;  
const bool CROP_CENTRAL = true;

// ==========================================
// FILTROS DE MOVIMIENTO DINÁMICO (BALANCEADOS)
// ==========================================
const int   MOTION_PIXEL_THRESHOLD = 25;
const float MAX_MOTION_RATIO       = 0.85f;
const float MAX_BBOX_SPEED         = 0.45f;

// ==========================================
// CALLBACK ESP-NOW
// ==========================================
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) diag_tx_ok++; else diag_tx_fail++;
}
#else
void OnDataSent(const uint8_t *mac, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) diag_tx_ok++; else diag_tx_fail++;
}
#endif

// ==========================================
// TRADUCCION EN VUELO
// ==========================================
int buscarClase(const char *etiqueta) {
    const char *etiqueta_esp = etiqueta;
    if      (strcasecmp(etiqueta, "bolsa")          == 0) etiqueta_esp = "Bolsas";
    else if (strcasecmp(etiqueta, "bottle-caps")    == 0) etiqueta_esp = "Tapas";
    else if (strcasecmp(etiqueta, "canrotation")    == 0) etiqueta_esp = "Rotacion";
    else if (strcasecmp(etiqueta, "glass_bottle")   == 0) etiqueta_esp = "Botella_Vidrio";
    else if (strcasecmp(etiqueta, "plastic_bottle") == 0) etiqueta_esp = "Botella_Plastico";
    else if (strcasecmp(etiqueta, "Leaf")           == 0) etiqueta_esp = "Hojas";

    for (int i = 0; i < NUM_CLASES; i++) {
        if (strcasecmp(etiqueta_esp, CLASES[i].etiqueta) == 0) return i;
    }
    return -1;
}

// ==========================================
// CALIBRACION
// ==========================================
enum ModoCalib { CALIB_OFF = 0, CALIB_REAL = 1, CALIB_AJENO = 2 };
volatile ModoCalib calib_modo = CALIB_OFF;
volatile int       calib_clase = -1;

struct StatsClase {
    uint32_t realN;  float realSumConf;  float realMinConf;  float realMinArea;
    uint32_t ajN;    float ajMaxConf;    float ajMaxArea;
};
StatsClase stats[NUM_CLASES];
portMUX_TYPE statsMux = portMUX_INITIALIZER_UNLOCKED;
Preferences prefs;

void limpiarStats() {
    portENTER_CRITICAL(&statsMux);
    for (int i = 0; i < NUM_CLASES; i++) stats[i] = StatsClase{0, 0.0f, 1.0f, 1e9f, 0, 0.0f, 0.0f};
    portEXIT_CRITICAL(&statsMux);
}

void cargarUmbrales() {
    prefs.begin("cleantech", false);
    for (int i = 0; i < NUM_CLASES; i++) {
        char ku[8], ka[8];
        snprintf(ku, sizeof(ku), "u%d", i);
        snprintf(ka, sizeof(ka), "a%d", i);
        umbral_rt[i] = prefs.getFloat(ku, CLASES[i].umbral);
        area_rt[i]   = prefs.getFloat(ka, CLASES[i].areaMin);
    }
    prefs.end();
}

void guardarUmbrales() {
    prefs.begin("cleantech", false);
    for (int i = 0; i < NUM_CLASES; i++) {
        char ku[8], ka[8];
        snprintf(ku, sizeof(ku), "u%d", i);
        snprintf(ka, sizeof(ka), "a%d", i);
        prefs.putFloat(ku, umbral_rt[i]);
        prefs.putFloat(ka, area_rt[i]);
    }
    prefs.end();
}

void restaurarUmbrales() {
    prefs.begin("cleantech", false);
    prefs.clear();
    prefs.end();
    for (int i = 0; i < NUM_CLASES; i++) {
        umbral_rt[i] = CLASES[i].umbral;
        area_rt[i]   = CLASES[i].areaMin;
    }
    guardarUmbrales();
}

struct Sugerencia { bool valido; float umbral; float area; const char *nota; };

Sugerencia sugerir(const StatsClase &s) {
    Sugerencia r = {false, 0.75f, 100.0f, "faltan muestras reales (minimo 10 cuadros)"};
    if (s.realN < 10) return r;
    r.valido = true;
    if (s.ajN == 0) {
        r.umbral = s.realMinConf - 0.05f;
        r.nota = "sin falsos positivos registrados: umbral = minimo real - 0.05";
    } else if (s.ajMaxConf < s.realMinConf) {
        r.umbral = (s.ajMaxConf + s.realMinConf) / 2.0f;
        r.nota = "separable";
    } else {
        r.umbral = s.ajMaxConf + 0.02f;
        r.nota = "se traslapan: con este umbral se pierden algunas detecciones reales";
    }
    if (r.umbral < 0.5f)  r.umbral = 0.5f;
    if (r.umbral > 0.99f) r.umbral = 0.99f;
    if (s.ajN > 0 && s.ajMaxArea < s.realMinArea) r.area = (s.ajMaxArea + s.realMinArea) / 2.0f;
    else r.area = s.realMinArea * 0.8f;
    if (r.area < 64.0f) r.area = 64.0f;
    return r;
}

void registrarCalibracion(const bool visto[], const float conf[], const float area[]) {
    if (calib_modo == CALIB_OFF) return;
    portENTER_CRITICAL(&statsMux);
    for (int i = 0; i < NUM_CLASES; i++) {
        if (!CLASES[i].activa || !visto[i]) continue;
        StatsClase &s = stats[i];
        if (calib_modo == CALIB_REAL && i == calib_clase) {
            s.realN++;
            s.realSumConf += conf[i];
            if (conf[i] < s.realMinConf) s.realMinConf = conf[i];
            if (area[i] < s.realMinArea) s.realMinArea = area[i];
        } else {
            s.ajN++;
            if (conf[i] > s.ajMaxConf) s.ajMaxConf = conf[i];
            if (area[i] > s.ajMaxArea) s.ajMaxArea = area[i];
        }
    }
    portEXIT_CRITICAL(&statsMux);
}

// ==========================================
// ESP-NOW — usa el MISMO canal que el AP
// ==========================================
void setup_espnow() {
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error inicializando ESP-NOW");
        return;
    }
    esp_now_register_send_cb(OnDataSent);
    memcpy(peerInfo.peer_addr, receptorMAC, 6);
    peerInfo.channel = 0; // 0 = auto (sincronizado con el AP)
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Fallo al añadir peer ESP-NOW");
        return;
    }
    Serial.printf("ESP-NOW OK. Peer=%02X:%02X:%02X:%02X:%02X:%02X canal=%d (AP=%d)\n",
        receptorMAC[0], receptorMAC[1], receptorMAC[2],
        receptorMAC[3], receptorMAC[4], receptorMAC[5],
        peerInfo.channel, WiFi.channel());
}

// ==========================================
// SERVIDOR WEB Y CÁMARA
// ==========================================
static void make_etag(const char *path, size_t sz, char *out, size_t out_len) {
    uint32_t h = 2166136261u;
    for (const char *p = path; *p; ++p) { h ^= (uint8_t)*p; h *= 16777619u; }
    h ^= (uint32_t)sz;
    snprintf(out, out_len, "\"%08x-%u\"", h, (unsigned)sz);
}

static esp_err_t send_file(httpd_req_t *req, const char *path, const char *mime) {
    File file = LittleFS.open(path, "r");
    if (!file) { httpd_resp_send_404(req); return ESP_FAIL; }

    size_t fsize = file.size();
    char etag[24];
    make_etag(path, fsize, etag, sizeof(etag));

    char if_none[32];
    if (httpd_req_get_hdr_value_str(req, "If-None-Match", if_none, sizeof(if_none)) == ESP_OK
        && strcmp(if_none, etag) == 0) {
        file.close();
        httpd_resp_set_status(req, "304 Not Modified");
        httpd_resp_set_hdr(req, "ETag", etag);
        httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=3600");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    httpd_resp_set_type(req, mime);
    httpd_resp_set_hdr(req, "ETag", etag);
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=3600");

    if (fsize > 0 && fsize <= 131072) {
        char *buf = (char *)ps_malloc(fsize);
        if (buf) {
            size_t rd = file.readBytes(buf, fsize);
            file.close();
            if (rd != fsize) { free(buf); httpd_resp_send_500(req); return ESP_FAIL; }
            httpd_resp_send(req, buf, fsize);
            free(buf);
            return ESP_OK;
        }
    }

    char buf[4096];
    size_t n;
    while ((n = file.readBytes(buf, sizeof(buf))) > 0) {
        if (httpd_resp_send_chunk(req, buf, n) != ESP_OK) { file.close(); return ESP_FAIL; }
    }
    file.close();
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t coords_handler(httpd_req_t *req) {
    portENTER_CRITICAL(&coordsMux);
    float x = last_x; float y = last_y; bool det = last_detect;
    portEXIT_CRITICAL(&coordsMux);

    char json[100];
    snprintf(json, sizeof(json), "{\"x\": %.4f, \"y\": %.4f, \"detectado\": %s}", x, y, det ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t calib_handler(httpd_req_t *req) {
    char query[96] = {0}; char val[32];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (httpd_query_key_value(query, "reset", val, sizeof(val)) == ESP_OK) limpiarStats();
        if (httpd_query_key_value(query, "default", val, sizeof(val)) == ESP_OK) restaurarUmbrales();
        if (httpd_query_key_value(query, "fase", val, sizeof(val)) == ESP_OK) {
            if (strcmp(val, "off") == 0) calib_modo = CALIB_OFF;
            else if (strcmp(val, "ajeno") == 0) calib_modo = CALIB_AJENO;
            else if (strcmp(val, "real") == 0) {
                char cl[32];
                if (httpd_query_key_value(query, "clase", cl, sizeof(cl)) == ESP_OK) {
                    int i = buscarClase(cl); 
                    if (i >= 0 && CLASES[i].activa) { calib_clase = i; calib_modo = CALIB_REAL; }
                }
            }
        }
        if (httpd_query_key_value(query, "aplicar", val, sizeof(val)) == ESP_OK) {
            StatsClase c[NUM_CLASES];
            portENTER_CRITICAL(&statsMux);
            memcpy(c, stats, sizeof(stats));
            portEXIT_CRITICAL(&statsMux);
            for (int i = 0; i < NUM_CLASES; i++) {
                if (!CLASES[i].activa) continue;
                Sugerencia sg = sugerir(c[i]);
                if (sg.valido) { umbral_rt[i] = sg.umbral; area_rt[i] = sg.area; }
            }
            guardarUmbrales();
        }
    }

    StatsClase c[NUM_CLASES];
    portENTER_CRITICAL(&statsMux);
    memcpy(c, stats, sizeof(stats));
    portEXIT_CRITICAL(&statsMux);

    size_t cap = 400 + NUM_CLASES * 520;
    char *buf = (char *)malloc(cap);
    if (!buf) { httpd_resp_send_500(req); return ESP_FAIL; }

    const char *modoTxt  = (calib_modo == CALIB_REAL) ? "real" : (calib_modo == CALIB_AJENO ? "ajeno" : "off");
    const char *claseTxt = (calib_modo == CALIB_REAL && calib_clase >= 0) ? CLASES[calib_clase].etiqueta : "-";
    int n = snprintf(buf, cap, "{\"modo\":\"%s\",\"clase_prueba\":\"%s\",\"clases\":[", modoTxt, claseTxt);

    bool primero = true; bool truncado = false;
    for (int i = 0; i < NUM_CLASES && !truncado; i++) {
        if (!CLASES[i].activa) continue;
        Sugerencia sg = sugerir(c[i]);
        float media = c[i].realN ? c[i].realSumConf / c[i].realN : 0.0f;
        float rmin  = c[i].realN ? c[i].realMinConf : 0.0f;
        float amin  = c[i].realN ? c[i].realMinArea : 0.0f;

        if (n < 0 || n >= (int)cap) { truncado = true; break; }
        int w = snprintf(buf + n, cap - n,
            "%s{\"clase\":\"%s\",\"real_n\":%u,\"real_conf_min\":%.2f,\"real_conf_media\":%.2f,\"real_area_min\":%.0f,"
            "\"ajeno_n\":%u,\"ajeno_conf_max\":%.2f,\"ajeno_area_max\":%.0f,"
            "\"umbral_actual\":%.2f,\"area_actual\":%.0f,\"umbral_sugerido\":%.2f,\"area_sugerida\":%.0f,\"nota\":\"%s\"}",
            primero ? "" : ",", CLASES[i].etiqueta,
            (unsigned)c[i].realN, rmin, media, amin,
            (unsigned)c[i].ajN, c[i].ajMaxConf, c[i].ajMaxArea,
            umbral_rt[i], area_rt[i], sg.valido ? sg.umbral : 0.0f, sg.valido ? sg.area : 0.0f, sg.nota);
        if (w < 0) { truncado = true; break; }
        n += w;
        primero = false;
    }
    if (n < 0) n = 0;
    if ((size_t)n >= cap) n = cap - 1;
    int w2 = snprintf(buf + n, cap - n, "]}");
    if (w2 > 0) n += w2;
    if ((size_t)n >= cap) n = cap - 1;
    buf[n] = '\0';

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
    free(buf);
    return ESP_OK;
}

static esp_err_t index_handler(httpd_req_t *req) { return send_file(req, "/Interfaz_CleanTECH.html", "text/html"); }
static esp_err_t css_handler(httpd_req_t *req)   { return send_file(req, "/Interfaz_CleanTECH.css", "text/css"); }
static esp_err_t js_handler(httpd_req_t *req)    { return send_file(req, "/Interfaz_CleanTECH.js", "application/javascript"); }

// ==========================================
// STREAM HANDLER (Con protocolo de mutex)
// ==========================================
static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    char part_buf[128];
    res = httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=123456789000000000000987654321");
    if (res != ESP_OK) return res;
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    while (true) {
        fb = cam_grab(CAM_GRAB_STREAM_MS);
        if (!fb) {
            // La IA está usando la cámara, saltamos este frame
            diag_stream_skip++;
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        size_t jpg_LEN = fb->len;
        size_t hlen = snprintf(part_buf, 128, "\r\n--123456789000000000000987654321\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", jpg_LEN);
        res = httpd_resp_send_chunk(req, part_buf, hlen);
        if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)fb->buf, jpg_LEN);
        if (res == ESP_OK) res = httpd_resp_send_chunk(req, "\r\n", 2);
        
        cam_release(fb);
        fb = NULL;

        if (res != ESP_OK) break;
        vTaskDelay(pdMS_TO_TICKS(80)); 
    }
    return res;
}

void Iniciar_Servidor_Camara() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true; 
    config.max_open_sockets = 7; 
    config.max_uri_handlers = 12;
    config.stack_size = 8192;
    config.recv_wait_timeout = 3;
    config.send_wait_timeout = 5;
    
    httpd_uri_t stream_uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL };
    httpd_uri_t index_uri  = { .uri = "/",       .method = HTTP_GET, .handler = index_handler,  .user_ctx = NULL };
    httpd_uri_t css_uri    = { .uri = "/Interfaz_CleanTECH.css", .method = HTTP_GET, .handler = css_handler, .user_ctx = NULL };
    httpd_uri_t js_uri     = { .uri = "/Interfaz_CleanTECH.js",  .method = HTTP_GET, .handler = js_handler,  .user_ctx = NULL };
    httpd_uri_t coords_uri = { .uri = "/coords", .method = HTTP_GET, .handler = coords_handler, .user_ctx = NULL };
    httpd_uri_t calib_uri  = { .uri = "/calib",  .method = HTTP_GET, .handler = calib_handler,  .user_ctx = NULL };

    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &stream_uri);
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &css_uri);
        httpd_register_uri_handler(camera_httpd, &js_uri);
        httpd_register_uri_handler(camera_httpd, &coords_uri);
        httpd_register_uri_handler(camera_httpd, &calib_uri);
    }
}

// ==========================================
// TAREA DE VISION (Edge Impulse)
// ==========================================
uint8_t* ei_buffer = NULL; 
uint8_t* rgb_buffer = NULL; 
uint8_t* prev_gray_buffer = NULL; 

static int get_signal_data(size_t offset, size_t length, float *out_ptr) {
    size_t out_idx = 0;
    for (size_t i = offset; i < offset + length; i++) out_ptr[out_idx++] = (float)ei_buffer[i];
    return EIDSP_OK;
}

void vision_task(void *pvParameters) {
    Serial.println(">>> INICIANDO TAREA DE VISION <<<");
    size_t ei_len = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;

    if (ei_buffer == NULL) ei_buffer = (uint8_t*)ps_malloc(ei_len);
    if (prev_gray_buffer == NULL) prev_gray_buffer = (uint8_t*)ps_malloc(ei_len);
    
    if (ei_buffer == NULL || prev_gray_buffer == NULL) {
        Serial.println("ERROR CRITICO: No hay PSRAM suficiente. Abortando.");
        vTaskDelete(NULL);
    }
    memset(prev_gray_buffer, 0, ei_len); 

    // Warmup
    for (int i = 0; i < CAM_WARMUP_FRAMES; i++) {
        camera_fb_t *w = cam_grab(1000);
        if (w) { Serial.printf("[VISION] warmup %d OK len=%u\n", i, w->len); cam_release(w); }
        else   { Serial.printf("[VISION] warmup %d FALLO\n", i); }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    int      framesSinDeteccion = 0;
    bool     motorActivo        = false;
    uint8_t  claseFinal         = 255;
    bool     last_envio         = false;

    float prev_frame_cx = -1.0;
    float prev_frame_cy = -1.0;

    while (1) {
        // --- PASO 1: Capturar frame con Mutex ---
        camera_fb_t *fb = cam_grab(CAM_GRAB_VISION_MS);
        if (!fb) {
            diag_cam_fail++;
            if (diag_cam_fail % 10 == 1) {
                Serial.printf("[VISION] Cámara ocupada (cam_fail=%u). Reintentando...\n", diag_cam_fail);
            }
            vTaskDelay(pdMS_TO_TICKS(CAM_RETRY_DELAY_MS));
            continue;
        }

        if (rgb_buffer == NULL) {
            rgb_buffer = (uint8_t*)ps_malloc(fb->width * fb->height * 3);
            if (rgb_buffer == NULL) {
                cam_release(fb);
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
        }

        // --- PASO 2: Convertir y guardar dimensiones ---
        bool converted = fmt2rgb888(fb->buf, fb->len, fb->format, rgb_buffer);
        int fbW = fb->width;
        int fbH = fb->height;

        // --- PASO 3: LIBERAR CÁMARA antes de la inferencia ---
        cam_release(fb);
        fb = NULL;

        if (!converted) {
            Serial.println("Error convirtiendo JPEG a RGB");
            vTaskDelay(pdMS_TO_TICKS(VISION_PERIODO_MS));
            continue;
        }

        // --- PASO 4: Procesamiento (Grises) ---
        const int W = EI_CLASSIFIER_INPUT_WIDTH;
        const int H = EI_CLASSIFIER_INPUT_HEIGHT;
        int src_w = fbW, src_h = fbH, off_x = 0, off_y = 0;
        if (CROP_CENTRAL) {
            int lado = (fbW < fbH) ? fbW : fbH;
            src_w = lado; src_h = lado;
            off_x = (fbW - lado) / 2;
            off_y = (fbH - lado) / 2;
        }

        for (int i = 0; i < H; i++) {
            for (int j = 0; j < W; j++) {
                int x2 = off_x + (j * src_w) / W;
                int y2 = off_y + (i * src_h) / H;
                int pixel_idx = (y2 * fbW + x2) * 3;
                uint8_t r = rgb_buffer[pixel_idx];
                uint8_t g = rgb_buffer[pixel_idx + 1];
                uint8_t b = rgb_buffer[pixel_idx + 2];
                ei_buffer[i * W + j] = (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
            }
        }

        // --- PASO 5: Inferencia ---
        signal_t signal;
        signal.total_length = ei_len;
        signal.get_data = &get_signal_data;

        ei_impulse_result_t result = { 0 };
        EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

        if (res == EI_IMPULSE_OK) {
            diag_frames++;
            bool positivo = false;
            bool visto[NUM_CLASES] = {false};
            float cmax[NUM_CLASES] = {0};
            float amax[NUM_CLASES] = {0};
            float mejor = 0.0;
            float current_best_cx = -1.0;
            float current_best_cy = -1.0;
            uint8_t claseDet = 255;
            bool dinamico_detectado = false;
            uint8_t clase_dinamica_idx = 255;

            for (size_t ix = 0; ix < result.bounding_boxes_count; ix++) {
                auto bb = result.bounding_boxes[ix];
                if (bb.value == 0) continue;
                diag_det_raw++;

                int idx = buscarClase(bb.label); 
                if (idx < 0) { diag_desc_label++; DBG_BOX("  [X] Descartada: Etiqueta desconocida '%s'\n", bb.label); continue; }
                if (!CLASES[idx].activa) { diag_desc_inactiva++; DBG_BOX("  [X] Descartada: Clase '%s' inactiva\n", CLASES[idx].etiqueta); continue; }
                if (bb.value < umbral_rt[idx]) { diag_desc_conf++; DBG_BOX("  [X] Descartada [%s]: Confianza %.2f < Umbral %.2f\n", CLASES[idx].etiqueta, bb.value, umbral_rt[idx]); continue; }

                float area = (float)bb.width * (float)bb.height;
                if (bb.value > cmax[idx]) { visto[idx] = true; cmax[idx] = bb.value; amax[idx] = area; }
                if (area < area_rt[idx]) { diag_desc_area++; DBG_BOX("  [X] Descartada [%s]: Área %.0f < Mínima %.0f\n", CLASES[idx].etiqueta, area, area_rt[idx]); continue; }

                // --- FILTRO 1: PIXELES EN MOVIMIENTO ---
                bool es_dinamico_pixeles = false;
                if (prev_gray_buffer != NULL) {
                    int moving_pixels = 0; int total_pixels = 0;
                    int start_x = max(0, (int)bb.x); int end_x = min(W, (int)(bb.x + bb.width));
                    int start_y = max(0, (int)bb.y); int end_y = min(H, (int)(bb.y + bb.height));
                    for (int y = start_y; y < end_y; y++) {
                        int row_offset = y * W;
                        for (int x = start_x; x < end_x; x++) {
                            int pixel_idx = row_offset + x;
                            int diff = abs((int)ei_buffer[pixel_idx] - (int)prev_gray_buffer[pixel_idx]);
                            if (diff > MOTION_PIXEL_THRESHOLD) moving_pixels++;
                            total_pixels++;
                        }
                    }
                    if (total_pixels > 0) {
                        float motion_ratio = (float)moving_pixels / (float)total_pixels;
                        if (motion_ratio > MAX_MOTION_RATIO) { es_dinamico_pixeles = true; diag_desc_motion++; }
                    }
                }
                if (es_dinamico_pixeles) {
                    DBG_BOX("  [X] Descartada [%s]: Movimiento de píxeles detectado.\n", CLASES[idx].etiqueta);
                    if (!dinamico_detectado) { dinamico_detectado = true; clase_dinamica_idx = (uint8_t)idx; }
                    continue; 
                }

                float temp_cx = (off_x + (bb.x + bb.width / 2.0f) * src_w / (float)W) / (float)fbW;
                float temp_cy = (off_y + (bb.y + bb.height / 2.0f) * src_h / (float)H) / (float)fbH;
                
                // --- FILTRO 2: VELOCIDAD DE LA CAJA ---
                if (prev_frame_cx != -1.0 && prev_frame_cy != -1.0) {
                    float dx = temp_cx - prev_frame_cx;
                    float dy = temp_cy - prev_frame_cy;
                    float velocidad = sqrt((dx * dx) + (dy * dy));
                    if (velocidad > MAX_BBOX_SPEED) { 
                        diag_desc_motion++; 
                        DBG_BOX("  [X] Descartada [%s]: Velocidad alta (%.2f > %.2f)\n", CLASES[idx].etiqueta, velocidad, MAX_BBOX_SPEED);
                        if (!dinamico_detectado) { dinamico_detectado = true; clase_dinamica_idx = (uint8_t)idx; }
                        continue; 
                    }
                }

                if (bb.value > mejor) {
                    mejor = bb.value; positivo = true; claseDet = (uint8_t)idx;
                    current_best_cx = temp_cx; current_best_cy = temp_cy;
                    Serial.printf("  [✓] DETECCIÓN VÁLIDA: %s (Conf: %.2f)\n", CLASES[idx].etiqueta, bb.value);
                }
            }
            
            if (positivo) { prev_frame_cx = current_best_cx; prev_frame_cy = current_best_cy; diag_pos_frame++; }
            registrarCalibracion(visto, cmax, amax);

            if (positivo) { framesSinDeteccion = 0; motorActivo = true; claseFinal = claseDet; }
            else { if (framesSinDeteccion < FRAMES_APAGAR) framesSinDeteccion++; if (framesSinDeteccion >= FRAMES_APAGAR) motorActivo = false; }
            bool activar = motorActivo;

            portENTER_CRITICAL(&coordsMux);
            last_detect = activar;
            if (positivo) { 
                if (last_x == 0.0 && last_y == 0.0) { last_x = current_best_cx; last_y = current_best_cy; }
                else { last_x = (last_x * 0.70f) + (current_best_cx * 0.30f); last_y = (last_y * 0.70f) + (current_best_cy * 0.30f); }
            }
            portEXIT_CRITICAL(&coordsMux);

            bool enviar = activar && (calib_modo == CALIB_OFF); 

            Serial.printf("  [WIN] pos=%d sinDet=%d/%d activar=%d enviar=%d modo=%d clase=%d conf=%.2f din=%d\n",
                          positivo ? 1 : 0, framesSinDeteccion, FRAMES_APAGAR,
                          activar ? 1 : 0, enviar ? 1 : 0, (int)calib_modo,
                          (int)claseFinal, mejor, dinamico_detectado ? 1 : 0);

            if (enviar != last_envio) {
                if (enviar) diag_pkt_on++; else diag_pkt_off++;
                Serial.printf("  [ESP-NOW TX] %s (Clase: %s, Seq: %u)\n", 
                              enviar ? "ON" : "OFF", 
                              (enviar && claseFinal < NUM_CLASES) ? CLASES[claseFinal].etiqueta : "-", seq_paquete);
                last_envio = enviar;
            }

            PaqueteVision pkt;
            pkt.magic     = 0xCE;
            pkt.activar   = enviar ? 1 : 0;
            pkt.clase     = enviar ? claseFinal : 255;
            pkt.seq       = seq_paquete++;
            pkt.dinamico  = dinamico_detectado ? 1 : 0;
            pkt.clase_din = dinamico_detectado ? clase_dinamica_idx : 255;

            if (dinamico_detectado) { Serial.printf("  [DIN] Objeto dinámico (idx %u)\n", clase_dinamica_idx); }
            
            esp_err_t err = esp_now_send(receptorMAC, (uint8_t*)&pkt, sizeof(pkt));
            if (err != ESP_OK) { diag_send_err++; Serial.printf("  [ESP-NOW ERROR] Fallo al enviar: %d\n", err); }

            if (millis() - lastStatusMs > 5000) {
                lastStatusMs = millis();
                Serial.printf("[STAT] frames=%u det=%u pos=%u pktON=%u pktOFF=%u sendErr=%u txOk=%u txFail=%u camFail=%u strSkip=%u\n",
                              diag_frames, diag_det_raw, diag_pos_frame,
                              diag_pkt_on, diag_pkt_off, (unsigned)diag_send_err, 
                              (unsigned)diag_tx_ok, (unsigned)diag_tx_fail,
                              diag_cam_fail, diag_stream_skip);
            }
        } else {
            Serial.println("Error en la vision de EI");
        }

        if (prev_gray_buffer != NULL) memcpy(prev_gray_buffer, ei_buffer, ei_len);
        vTaskDelay(pdMS_TO_TICKS(VISION_PERIODO_MS));
    }
}

// ==========================================
// SETUP Y LOOP
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(500);

    cargarUmbrales();
    limpiarStats();

    if (!LittleFS.begin(true)) Serial.println("Error LittleFS");

    // Crear Mutex de Cámara
    camMutex = xSemaphoreCreateMutex();
    if (!camMutex) {
        Serial.println("ERROR CRITICO: no se pudo crear camMutex.");
        return;
    }

    // Limpieza de credenciales WiFi viejas
    WiFi.persistent(false);
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(200);

    WiFi.setTxPower(WIFI_POWER_19_5dBm); 
    WiFi.mode(WIFI_STA); 
    WiFi.setSleep(false); 
    WiFi.begin(ssid, password);

    int intentos = 0;
    while (WiFi.status() != WL_CONNECTED && intentos < 30) {
        delay(500); intentos++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[OK] WiFi conectado. SSID: %s, IP: %s, Canal: %d\n", 
                      WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.channel());
    } else {
        Serial.println("\n[ERROR] No se pudo conectar al AP.");
    }

    // NO forzamos esp_wifi_set_channel(1). Usamos el canal del AP automáticamente.
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
    
    config.frame_size = FRAMESIZE_QVGA; 
    config.jpeg_quality = 14; 
    config.fb_count = 2;                       // Estable para compartir
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY; // Estable para compartir

    esp_err_t cam_err = esp_camera_init(&config);
    Serial.printf("[CAM] esp_camera_init -> %d (%s)\n", cam_err, esp_err_to_name(cam_err));
    if (cam_err != ESP_OK) return;

    sensor_t *s = esp_camera_sensor_get();
    if (s) Serial.printf("[CAM] sensor PID=0x%02X\n", s->id.PID);
    
    // Warmup inicial antes de lanzar tareas concurrentes
    for (int i = 0; i < CAM_WARMUP_FRAMES; i++) {
        camera_fb_t *w = cam_grab(1000);
        if (w) { Serial.printf("[CAM] warmup %d OK len=%u\n", i, w->len); cam_release(w); }
        else   { Serial.printf("[CAM] warmup %d FALLO\n", i); }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    Iniciar_Servidor_Camara();
    xTaskCreatePinnedToCore(vision_task, "Vision", 16384, NULL, 1, NULL, 1);
}

void loop() { 
    delay(1000); 
}