#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>


const char* ssid = "CleanTECH_Robot";
AsyncWebServer server(80);


const int PIN_RPWM = 18;
const int PIN_LPWM = 16;  

const int FREQ = 10000;       
const int RESOLUTION = 8;     
const int CH_RPWM = 0;        
const int CH_LPWM = 1;        


const int PIN_ENC_A = 25; 
const int PIN_ENC_B = 26; 


const float PULSOS_POR_VUELTA = 495.0;


volatile long pulsosEncoder = 0;
long ultimosPulsos = 0;
unsigned long tiempoAnterior = 0;
const unsigned long INTERVALO_MS = 200;


float rpmActuales = 0.0;
float vueltasEje = 0.0;
long deltaPulsosUltimo = 0;
int velocidadActual = 0;
String direccionActual = "adelante";


void IRAM_ATTR leerEncoder() {
    int estadoB = digitalRead(PIN_ENC_B);
    if (estadoB > 0) {
        pulsosEncoder--;
    } else {
        pulsosEncoder++;
    }
}


void aplicarMotor(int pwm, String dir) {
    pwm = constrain(pwm, 0, 255);
    dir.toLowerCase();
    dir.trim();

    if (pwm == 0) {
        ledcWrite(CH_RPWM, 0);
        ledcWrite(CH_LPWM, 0);
    } else if (dir == "adelante" || dir == "forward") {
        ledcWrite(CH_LPWM, 0);
        ledcWrite(CH_RPWM, pwm);
    } else if (dir == "atras" || dir == "reversa" || dir == "reverse") {
        ledcWrite(CH_RPWM, 0);
        ledcWrite(CH_LPWM, pwm);
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- CleanTECH Motor Web Server con Telemetria ---");


    ledcSetup(CH_RPWM, FREQ, RESOLUTION);
    ledcSetup(CH_LPWM, FREQ, RESOLUTION);
    ledcAttachPin(PIN_RPWM, CH_RPWM);
    ledcAttachPin(PIN_LPWM, CH_LPWM);
    aplicarMotor(0, "adelante");


    pinMode(PIN_ENC_A, INPUT_PULLUP);
    pinMode(PIN_ENC_B, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), leerEncoder, RISING);


    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    WiFi.mode(WIFI_AP);
    delay(100);

    WiFi.softAP(ssid);
    Serial.println(">> Wi-Fi AP listo <<");
    Serial.print("SSID: "); Serial.println(ssid);
    Serial.print("IP: ");   Serial.println(WiFi.softAPIP());


    server.on("/setMotor", HTTP_GET, [](AsyncWebServerRequest *request){
        if (request->hasParam("speed") && request->hasParam("dir")) {
            velocidadActual = request->getParam("speed")->value().toInt();
            direccionActual = request->getParam("dir")->value();

            aplicarMotor(velocidadActual, direccionActual);
            
            AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
            response->addHeader("Access-Control-Allow-Origin", "*");
            request->send(response);
        } else {
            request->send(400, "text/plain", "Faltan parametros");
        }
    });


    server.on("/telemetry", HTTP_GET, [](AsyncWebServerRequest *request){
        StaticJsonDocument<256> doc;
        doc["rpm"] = rpmActuales;
        doc["pulses"] = pulsosEncoder;
        doc["turns"] = vueltasEje;
        doc["delta"] = deltaPulsosUltimo;
        doc["pwm"] = velocidadActual;
        doc["dir"] = (velocidadActual == 0) ? "Paro" : direccionActual;

        String jsonString;
        serializeJson(doc, jsonString);

        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonString);
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });


    server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
        pulsosEncoder = 0;
        ultimosPulsos = 0;
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Reset OK");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });

    server.begin();
    Serial.println("Servidor Web listo.");
}

void loop() {
    unsigned long ahora = millis();
    if (ahora - tiempoAnterior >= INTERVALO_MS) {
        unsigned long dt = ahora - tiempoAnterior;
        deltaPulsosUltimo = pulsosEncoder - ultimosPulsos;
        
        ultimosPulsos = pulsosEncoder;
        tiempoAnterior = ahora;


        rpmActuales = ((float)deltaPulsosUltimo / PULSOS_POR_VUELTA) * (60000.0 / dt);
        vueltasEje = (float)pulsosEncoder / PULSOS_POR_VUELTA;
    }
}