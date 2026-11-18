- ### Puente-H BTS7960 IBT_2:
![alt text](img/image-4.png)

El **BTS 7960** es un módulo de potencia diseñado para el control de motores de corriente continua (DC) que requieren mucha energía. Integra en un solo encapsulado un interruptor de alta potencia superior (MOSFET de canal P), un interruptor inferior (MOSFET de canal N) y su propio circuito de control (driver). Es ideal para construir puentes H o sistemas de control de motores en robótica y automoción.

## Características principales
- **Alta corriente:** Soporta picos de hasta **43 A** y una corriente continua de 40 A.
- **Baja resistencia:** Tiene una resistencia de paso muy baja (típicamente 16 mΩ), lo que significa que pierde muy poca energía en forma de calor.
- **Control directo:** Se conecta fácilmente a microcontroladores (Arduino, ESP32, PIC, etc.) gracias a sus entradas de nivel lógico.
- **PWM Integrado:** Permite controlar la velocidad del motor mediante modulación por ancho de pulso (PWM) de hasta **25 kHz**.
- **Diagnóstico de corriente:** Incluye un pin de salida (IS) que informa al microcontrolador sobre la cantidad de corriente que está pasando por el motor y detecta fallos.
- **Protecciones integradas:** Evita daños por sobretemperatura, sobretensión, subtensión, sobrecorriente y cortocircuitos.
- **Reducción de ruido:** Al usar un MOSFET de canal P, no necesita una "bomba de carga" externa, lo que reduce las interferencias electromagnéticas (EMI). Además, permite ajustar la velocidad de conmutación (slew rate) con una resistencia externa.

## Distribución de Pines (Encapsulado de 7 u 8 pines)
El chip se conecta a la placa de la siguiente manera:
- **VS:** Alimentación de potencia (generalmente de 8V a 18V, con un máximo de 27.5V).
- **OUT:** Salida de potencia hacia el motor.
- **IN:** Entrada de control (decide si se activa el interruptor superior o el inferior).
- **INH:** Inhibidor (si se pone en bajo, el chip entra en modo de reposo y apaga ambas salidas).
- **SR:** Ajuste de la velocidad de conmutación (se conecta a tierra mediante una resistencia).
- **IS:** Salida de corriente y diagnóstico de errores (se conecta a una resistencia para medir el voltaje).
- **GND:** Tierra.

## ¿Cómo funciona internamente?
El BTS 7960 está diseñado para trabajar en configuraciones de medio puente. Para controlar un motor en ambos sentidos (adelante y atrás), normalmente se usan **dos** de estos chips juntos para formar un **Puente H**. 
El chip genera un "tiempo muerto" automático para asegurarse de que el interruptor superior y el inferior nunca se enciendan al mismo tiempo (lo cual causaría un cortocircuito). Si detecta un fallo, como una sobrecorriente, reduce la energía mediante un modo de conmutación rápida en lugar de calentarse peligrosamente.

## Aplicaciones comunes
- Control de motores de corriente continua en robótica.
- Sistemas de dirección y tracción en vehículos (automoción).
- Control de motores en impresoras 3D o máquinas CNC.
- Construcción de puentes H para inversión de giro.