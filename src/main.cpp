#include <Arduino.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <esp_now.h>
#include <WiFi.h>
#include <string.h>
#include <math.h>
#include <DFRobot_BMI160.h>

#define Sensor_Encoder ;
#define Sensor_BMI ;
#define EnableA 6
#define EnableA_DER 7
#define RotAIz 4
#define RotADer 5

#define EnableB 17
#define EnableB_DER 18
#define RotBIz 15
#define RotBDer 16

#define PWM_Torque 9
#define PWM_Torque_DER 10
#define RotTorqueIZ 8
#define RotTorqueDER 3

int Agno, i, Inicio, Fin, rslt, modo = 2;

// Tiempos de retardo
int Tiempo_Retardo_odometria;

// Variables enteras de odometria.
int Sensor_Izquierdo, Sensor_Derecho, Contador_Izquierdo = 0, Contador_Derecho = 0, Frecuencia_de_torque = 13500;
bool Estado_Izquierdo, Estado_Derecho;
float Diametro, Resolución_del_Encoder, Distancia_Entre_Ruedas, Factor_de_Conversion = (PI * Diametro) / Resolución_del_Encoder;
float Distancia_Rueda_Izquierda, Distancia_Rueda_Derecha, Distancia_Centro_Robot;
float theta, x, y, deltaPulsosUltimo, pulsosEncoder;
const int IN_RPWM = 18, PIN_LPWM = 16, FREQ = 15000,RESOLUTION = 8; 
const int CH_RPWM = 0, CH_LPWM = 1, PIN_ENC_A = 25, PIN_ENC_B = 26; 

unsigned long Tiempo_Actual, Tiempo_Anterior, chars, dt;
unsigned short Sentencias, Fallo_Revision;
byte Mes, Dia, Hora, Minuto, Segundo;
DFRobot_BMI160 bmi160;

enum DIRECCION
{
    NINGUNO,
    IZQUIERDA,
    DERECHA
};
DIRECCION ultimaAccion = NINGUNO;

typedef struct
{
    int ValX_Der;
    int ValX_Iz;
    int ValY_Arriba;
    int ValY_Abajo;
    bool EjeMazoDer;
    bool EjeMazoIz;
} struct_message;

struct_message incomingData;
volatile bool newDataReceived = false;

uint8_t broadcastAdress[] = {0x1C, 0xDB, 0xD4, 0x40, 0x37, 0x64};
const int8_t i2c_addr = 0x69;
uint16_t accelGyro[6] = {0};
// Manda indicacióne de como se debe comportar el robot segun que entrada se le mande

void controlarMotor(int pwmIzq, int pwmDer, int dirIzq, int dirDer, int velocidad, bool girarIzq, bool girarDer)
{
    if (girarIzq && !girarDer)
    {
        digitalWrite(dirIzq, HIGH);
        digitalWrite(dirDer, LOW);
        ledcWrite(pwmIzq, velocidad);
        ledcWrite(pwmDer, 0);
    }
    else if (!girarIzq && girarDer)
    {
        digitalWrite(dirIzq, LOW);
        digitalWrite(dirDer, HIGH);
        ledcWrite(pwmIzq, 0);
        ledcWrite(pwmDer, velocidad);
    }
    else
    {
        digitalWrite(dirIzq, LOW);
        digitalWrite(dirDer, LOW);
        ledcWrite(pwmIzq, 0);
        ledcWrite(pwmDer, 0);
    }
}

void LogicaPuenteH()
{
    int velX = 0, velY = 0;
    bool moverXDer = (incomingData.ValX_Der > 0);
    bool moverXIz = (incomingData.ValX_Iz > 0);
    bool moverYArr = (incomingData.ValY_Arriba > 0);
    bool moverYAbj = (incomingData.ValY_Abajo > 0);

    if ((moverYArr && moverXDer) || (moverYArr && moverXIz) ||
        (moverYAbj && moverXDer) || (moverYAbj && moverXIz))
    {
        controlarMotor(0, 3, RotAIz, RotADer, 0, false, false);
        controlarMotor(1, 4, RotBIz, RotBDer, 0, false, false);
        return;
    }

    if (moverXDer)
    {
        int vel = incomingData.ValX_Der;
        controlarMotor(0, 3, RotAIz, RotADer, vel, false, true);
        controlarMotor(1, 4, RotBIz, RotBDer, vel, true, false);
    }
    else if (moverXIz)
    {
        int vel = incomingData.ValX_Iz;
        controlarMotor(0, 3, RotAIz, RotADer, vel, true, false);
        controlarMotor(1, 4, RotBIz, RotBDer, vel, false, true);
    }
    else if (moverYArr)
    {
        int vel = incomingData.ValY_Arriba;
        controlarMotor(0, 3, RotAIz, RotADer, vel, true, false);
        controlarMotor(1, 4, RotBIz, RotBDer, vel, true, false);
    }
    else if (moverYAbj)
    {
        int vel = incomingData.ValY_Abajo;
        controlarMotor(0, 3, RotAIz, RotADer, vel, false, true);
        controlarMotor(1, 4, RotBIz, RotBDer, vel, false, true);
    }
    else
    {
        controlarMotor(0, 3, RotAIz, RotADer, 0, false, false);
        controlarMotor(1, 4, RotBIz, RotBDer, 0, false, false);
    }
}

void Odometria()
{
    if ((Tiempo_Actual - Tiempo_Anterior) >= Tiempo_Retardo_odometria)
    {
        Serial.print("x = ");
        Serial.println(x);
        Serial.print("y = ");
        Serial.println(y);
        Serial.print("cont_izq = ");
        Serial.println(Contador_Izquierdo);
        Serial.print("cont_der = ");
        Serial.println(Contador_Derecho);
    }
}

void puenteHTorque(bool Golpe_izquierdo, bool Golpe_Derecho)
{

}

void alerta_prueba(bool Falla){
if(Falla){
//En esta sección del codigo se realiza el aviso al usuario de la detección de la respectiva falla.
}
else{
//En esta sección del codigo se notifica al usuario que todos los parametros registrados estan correctos.
}
}

void Deteccion_Pruebas(){

}

void Esp_cam(){

}

void setup()
{
    Serial.begin(115200);
    Tiempo_Anterior = micros();
    pinMode(2, INPUT);
    pinMode(3, INPUT);

    if (bmi160.softReset() != BMI160_OK)
    {
        Serial.println("Reset en False");
    }
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Error iniciando Esp_now");
        return;
    }

    ledcSetup(0, 5000, 12);
    ledcAttachPin(EnableA, 0);
    ledcSetup(3, 5000, 12);
    ledcAttachPin(EnableA_DER, 3);

    ledcSetup(1, 5000, 12);
    ledcAttachPin(EnableB, 1);
    ledcSetup(4, 5000, 12);
    ledcAttachPin(EnableB_DER, 4);

    ledcSetup(2, Frecuencia_de_torque, 12);
    ledcAttachPin(PWM_Torque, 2);
    ledcSetup(5, Frecuencia_de_torque, 12);
    ledcAttachPin(PWM_Torque_DER, 5);
}
void loop()
{
    Tiempo_Actual = micros();
    if (Tiempo_Actual - Tiempo_Anterior >= INTERVALO_MS) {
        dt = Tiempo_Actual - Tiempo_Anterior;
        deltaPulsosUltimo = pulsosEncoder - ultimosPulsos;
        
        ultimosPulsos = pulsosEncoder;
        tiempoAnterior = Tiempo_Actual;


        rpmActuales = (deltaPulsosUltimo / PULSOS_POR_VUELTA) * (60000.0 / dt);
        vueltasEje = pulsosEncoder / PULSOS_POR_VUELTA;
    }

    if ((Tiempo_Actual - Tiempo_Anterior) > 4e6)
    {
        Sensor_Izquierdo = digitalRead(2);
    }

    rslt = bmi160.getAccelGyroData(accelGyro);
    switch (modo)
    {
    case 0:
        Inicio = 0;
        Fin = 6;
        break;
    case 1:
        Inicio = 0;
        Fin = 3;
        break;
    case 2:
        Inicio = 3;
        Fin = 6;
        break;
    default:
        Serial.println("Modo incorrecto de uso");
        break;
    }

    if ((Tiempo_Actual - Tiempo_Anterior) >= 100000 && rslt == 0)
    {
        Tiempo_Anterior = Tiempo_Actual;
        for (i = 0; i < Fin; i++)
        {
            if (i < 3)
            {
                Serial.print(accelGyro[i] * (PI / 180));
            }
            else
            {
                Serial.print((accelGyro[i]) / 16384.0);
            }
        }
        Serial.println();
    }
    else
    {
        Serial.println("Error de lectura");
    }
    if ((Tiempo_Actual - Tiempo_Anterior) > 5e5)
    {
        Tiempo_Anterior = Tiempo_Actual;
    }
    if (newDataReceived)
    {
        newDataReceived = false;
        LogicaPuenteH();
        //puenteHTorque(incomingData.EjeMazoIz, incomingData.EjeMazoDer);
    }
}