- ### Motorreductor JGA25-370 12VDC 330RPM Con Encoder Integrado:
<img width="33%" alt="imagen_2025-09-23_134925293-a82afac2cb254dbe3417586605672271-1024-1024" src="https://github.com/user-attachments/assets/c5fa8177-a76b-410c-856d-26f6f07c72a0" />

El **JGA25-370** es un motorreductor de corriente continua (12V) que incluye un **encoder incremental** para medir con precisión la velocidad y la posición del giro. Sus engranajes metálicos lo hacen resistente y duradero, y su encoder permite conectarlo a microcontroladores como Arduino, ESP32, PIC o Raspberry Pi para sistemas de control realimentado (PID). Es ideal para robótica, automatización y proyectos educativos avanzados.

## Características principales
- **Modelo:** JGA25-370 DC + Encoder.
- **Voltaje nominal:** 12 VDC.
- **Rango de velocidad (sin carga):** 60 – 350 RPM (según la relación de engranajes).
- **Torque máximo:** 2.5 – 5 kg-cm (según modelo y carga).
- **Corriente:** 80 – 200 mA sin carga; hasta 1.5 A en carga máxima.
- **Relación de reducción:** 1:30 a 1:300.
- **Engranajes:** Metálicos de alta precisión.
- **Eje de salida:** 4 mm tipo D (longitud 10–12 mm).
- **Dimensiones:** Longitud total 70–85 mm (con encoder), peso 90–110 g.
- **Encoder:** Incremental de 2 canales (A y B), resolución de 11–13 pulsos por vuelta del motor (multiplicable por la reducción).
- **Alimentación del encoder:** 3.3 – 5 VDC (compatible con microcontroladores).
- **Ruido:** Bajo a moderado.

## Funcionamiento del encoder
El encoder genera dos señales digitales (A y B) desfasadas 90°. Esto permite medir tanto la **velocidad** como la **dirección** de giro. Se conecta directamente a pines digitales del microcontrolador, usando interrupciones para el canal A y una entrada auxiliar para el B. Alimentación: VCC (5V) y GND.

## Curvas de rendimiento típicas (a 12V)
| Relación | Velocidad sin carga (RPM) | Corriente sin carga (mA) | Torque máx. (kg-cm) |
|----------|---------------------------|--------------------------|----------------------|
| 1:30     | 350                       | 180                      | 2.5                  |
| 1:50     | 200                       | 160                      | 3.2                  |
| 1:100    | 120                       | 130                      | 4.0                  |
| 1:210    | 60                        | 110                      | 5.0                  |

## Aplicaciones comunes
- Robots móviles y seguidores de línea.
- Sistemas de control de posición y velocidad.
- Cámaras motorizadas y mecanismos de dirección.
- Impresoras 3D, CNC miniatura y automatización ligera.
- Vehículos autónomos y proyectos educativos de robótica.

## Ventajas
- Encoder integrado para control preciso.
- Engranes metálicos resistentes.
- Funcionamiento silencioso y estable.
- Amplio rango de velocidad y torque.
- Bajo consumo energético.

## Recomendaciones de uso
- No exceder los 12V DC.
- Verificar polaridad antes de conectar.
- Fijar el motor correctamente para evitar vibraciones.
- Usar drivers compatibles (L298N, TB6612FNG, L9110).
- Filtrar las señales del encoder si hay ruido eléctrico.

## Observaciones finales
El JGA25-370 es una solución confiable para proyectos que requieren precisión y durabilidad. Su encoder permite una retroalimentación efectiva para sistemas PID y control digital, siendo ideal para aplicaciones educativas, de automatización o experimentales.