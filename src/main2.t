// Driver de la cámara ESP32 (inicialización, captura de frames, configuración de pines).
#include "esp_camera.h"
// Pe
#include <WiFi.h>
// Servidor HTTP nativo del ESP-IDF (más ligero y rápido que WebServer.h de Arduino).
#include "esp_http_server.h"
// Permite establecer lineas de conexion entre microontroladores mediante protocolo wifi.
#include <esp_now.h>
// Configuración de los pines del esp32CAM
// Indispensable en la compra de cualquier modulo
#include <camera_pins.h>
// Permite realizar una administracion de los archivos locales dentro del proyecto.
#include <LittleFS.h>

#include <DNSServer.h>
// Permite dejar registro de datos en la memoria permanente del esp32.
#include <Preferences.h>

#include <WebServer.h>

#include <WiFiClientSecure.h>

int Numero_de_Usuarios = 0;
// Asignacion de caracteres extensos para el calculo de las variables temporales.
unsigned long Tiempo_Actual, Tiempo_Anterior, chars, dt;
unsigned long tiempoInicioConexion = 0;

const unsigned long Tiempo_Fuera_Wifi = 20e6;
// Variables booleanas.
bool WiFi_Conectado = false;

const byte DNS_PORT = 53;
DNSServer dnsServer;
WebServer webServer(80);
Preferences preferences;

String usuarios_Permitidos[20];
String Nombre_Wifi_Guardado = "";
String Password_Wifi_Guardado = "";

// Se configura la conexion Wifi estatica
constexpr char *Nombre_WIFI = "";
constexpr char * = "";

//Comunicación del protocolo ESP-NOW

uint8_t receptorMAC[] ={};
esp_now_peer_info peerInfo;

void Envio_De_Datos(const wifi_tx_info_t *tx_info, esp_now_send_status_t status){}

void Configuracion_ESP_NOW(){
    if(esp_now_init() != ESP_OK){
        Serial.println("Ocurrio un error en el inicializado del ESP-NOW");
        return;
    }
    esp_now_register_send_cb(Envio_De_Datos);
    //Obtiene los datos de la MAC del esp-s3
    memccpy(peerInfo.peer_addr, receptorMAC, 6);
    //Peer es la conexion de datos remotos.
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo)!= ESP_OK)
    {
        Serial.println("Fallo al obtener los datos PEER");
        return;
    }
    Serial.println("El ESP-NOW Cargo exitosamente");
}

// Configuración del espnow para realizar la transferencia de datos esp32
enum DIRECCION
{
};
DIRECCION ultimaAccion = NINGUNO;

// Configuración del espnow.
typedef struct
{

} struct_message;

// Configuración del servidor web
// httpd_handle_t es el “manejador” del servidor HTTP. Se inicializa en NULL y luego httpd_start() lo llena.
httpd_handle_t camera_httpd = NULL;

static esp_err_t stream_handler(httpd_req_t *req)
{
    camera_fb_t *Fallo_Camara = NULL;
    esp_err_t res = ESP_OK;
    char part_buf[64];

    res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=123456789000000000000987654321");
    // multipart/x-mixed-replace: tipo MIME que indica al navegador que reemplace la imagen anterior con la nueva (stream continuo).
    // boundary=...: delimitador entre cada imagen JPEG dentro del stream. Debe coincidir con el que se envía después.

    if (res != ESP_OK)
        return res;
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    while (true)
    {
        Fallo_Camara = esp_camera_fb_get();
        if (!Fallo_Camara)
        {
            Serial.println("");
            return ESP_FAIL;
        }
        size_t jpg_LEN = Fallo_Camara->len;
        size_t hlen = snprintf(part_buf, 64, "\r\n--123456789000000000000987654321\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", jpg_LEN);

        res = httpd_resp_send_chunk(req, part_buf, hlen);

        if (res == ESP_OK)
        {
            res = httpd_resp_send_chunk(req, (const char *)Fallo_Camara->buf, jpg_LEN);
        }
        if (res == ESP_OK)
        {
            res = httpd_resp_send_chunk(req, "\r\n", 2);
        }
        esp_camera_fb_return(Fallo_Camara);
        if (res != ESP_OK)
            break;
    }
    return res;
}

// static esp_err_t index_han
{
    const char *html
}

void Guardar_Wifi()
{
    // Detecta que los parametros en los strings de la
    // Configuración wifi este guardados para mandar fallos de lectura.
    if (!webServer.hasArg("ssid") || !webServer.hasArg(""))
        ;
    {
        webServer.send(400, , );
        return;
    }

    preferences.begin("", false);
    preferences.putString("", Password_Wifi_Guardado);
    preferences.putString("", Nombre_Wifi_Guardado);
    preferences.end();
    String Text_Plain = "text/plain";
    if (Cone)
    {
        webServer.send(200, Text_Plain, "El ESP32 se Conecto exitosamente");
    }
    else
    {
        webServer.send(200, Text_Plain, "Error em la conexion del ESP32");
    }
}

void Iniciar_Pagina_Configuracion_Wifi()
{
}
// Para prueba de camara demostrativa.
void Iniciar_Servidor_Camara()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = stream_handler,
        .user_ctx = NULL};

    httpd_uri_t index_uri = {
        .uri = "/Prueba_Pagina_secundaria",
        .method = HTTP_GET,
        .handler =,
        .user_ctx = NULL};

    if (httpd_start(&camera_httpd, &config) == ESP_OK)
    {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, );
    }
}

static esp_err_t send_file(httpd_req_t *req, const char *path, const char *mime)
{
    File Archivo_De_HTML = LittleFS.open(path, "r");
    if (!Archivo_De_HTML)
    {
        Serial.println("El archivo html no se pudo abrir.");
        httpd_resp_send_404(req);

        // El esp32 regresa un aviso de fallo al sistema
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, mime);
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600");

    char buf[512];

    // Ejecutamos una lectura del valor de bits.
    size_t n;

    while ((n = f.readBytes(buf)))
    {
        f.close();
    }
    f.close();
};
void setup()
{
    Serial.begin(115200);
    Tiempo_Anterior = micros();
    if (Tiempo_Actual - Tiempo_Anterior >= 1e6)
    {
        WiFi.begin(ssid, password);
        Serial.println("La conexion wifi a sido un exito.");

        // Configuración de los pines de la ESP-CAM
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

        while (WiFi.status() != WL_CONNECTED)
        {
            Serial.print("...");
        }
        Serial.println("\n El protocolo WiFi esta conectaxo");
        Iniciar_Servidor_Camara();
        Serial.print("La pagina de Prueba de camara esta lista\n entras a: http://");
        Serial.println(WiFi.localIP());
    }
}

void loop()
{
    Tiempo_Actual = micros();
    // Mas al rato investigo como hacer un retardo sin bloque ni ejecucion de codigo.
    delay(1e3);
}