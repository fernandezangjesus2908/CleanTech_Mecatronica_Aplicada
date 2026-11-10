# Sistema CleanTECH.

El sistema cleanTECH . Es un sistema automata programado con capacidades de inteligencia artificial que le permite hacer una deteccion de diversos datos de adquisicion de las variables del entorno principales enfocadas a la cuestion de reconocimiento de entidades estaticas o en movimiento.

## Nota importante:
Dentro del desglose de los parametros tomados en cuenta en el desarrrollo del sistema cleanTECH se desglosara en esta sección del repositorio de gitHUB los parametros que albergan la ejecución del codigo dentro del sistema CleanTECH
## Componentes utilizados dentro de la construcción del sistema cleanTECH:

- ### Motorreductor JGA25-370 12VDC 330RPM Con Encoder Integrado:
<img width="33%" alt="imagen_2025-09-23_134925293-a82afac2cb254dbe3417586605672271-1024-1024" src="https://github.com/user-attachments/assets/c5fa8177-a76b-410c-856d-26f6f07c72a0" />

- ### XL4015 Convertidor DC-DC Buck Reductor:
![alt text](image.png)
- ### Esp32s3:
- ### ESP32-CAM con Cámara OV2640:
![alt text](image-3.png)
- ### BMI160 Sensor Giroscopio y Acelerómetro De 6DOF.
![alt text](image-1.png)

## ¿Qué es el BMI160?
Es una **unidad de medición inercial (IMU) muy pequeña y de bajo consumo**. Combina en un solo chip:
- Un **acelerómetro** (mide aceleración y orientación).
- Un **giroscopio** (mide velocidad angular o rotación).

Ambos sensores tienen resolución digital de **16 bits**. Su tamaño es de solo **2.5 × 3.0 × 0.8 mm**, en un encapsulado LGA de 14 pines, ideal para dispositivos móviles y wearables.

## ¿Para qué se usa?
Aplicaciones típicas:
- Realidad aumentada y juegos inmersivos.
- Navegación indoor y mapas 3D.
- Podómetros y conteo de pasos.
- Fusión de sensores de 6 y 9 ejes.
- Ratones aéreos, controladores de juego y drones.
- Estabilización óptica de imagen en cámaras.

## Características principales
- **Bajo consumo:** en modo completo (giroscopio + acelerómetro) consume típicamente **950 µA**.
- **Modos de ahorro:** modo suspendido de **3 µA**, detección de movimiento y podómetro con menos de **200 µA**.
- **Alimentación flexible:**
  - VDD: **1.71 V a 3.6 V**.
  - VDDIO: **1.2 V a 3.6 V**.
- **Rangos configurables:**
  - Acelerómetro: **±2g, ±4g, ±8g, ±16g**.
  - Giroscopio: **±125°/s hasta ±2000°/s**.
- **Interfaz principal:** I2C o SPI.
- **Interfaz secundaria:** permite conectar sensores externos, como un magnetómetro (búsqueda de dirección).
- **FIFO integrado de 1024 bytes:** guarda datos temporalmente para reducir consumo y evitar pérdidas.
- **Interrupciones por hardware:** detección de movimiento, doble toque, orientación, caída libre, impacto, etc.
- **Compatible con Android 4.4 y Windows 8.1**.

## Distribución de pines (Página 2)
El documento incluye un diagrama de los 14 pines visto desde abajo. Los pines incluyen:
- Alimentación: VDD, VDDIO, GND, GNDIO.
- Comunicación: SDA/SDI, SCL/SCK, CSB, SDO.
- Interrupciones: INT1 e INT2.
- Interfaz para magnetómetro externo: ASDx y ASCx.
- Interfaz SPI secundaria para estabilización óptica.

- ### Módulo GPS GY-NEO6MV2.
![alt text](image-2.png) {width=300}
## 1. ¿Qué es y cómo funciona el módulo? (Página 1)
* **Precisión:** Tiene una precisión horizontal de **2.5 metros** y una tasa de actualización de 1 vez por segundo (1Hz).
* **Indicador LED:** 
  * **Sin parpadeo:** Está buscando satélites.
  * **Parpadeo cada 1 segundo:** Ha encontrado señal (Position Fix).
* **Consejo importante:** Debe usarse **al aire libre**; no funciona bien en interiores.
* **Alimentación:** Aunque opera a 3.3V, se recomienda conectarlo primero a **5V durante 1 o 2 horas** para cargar su batería interna antes de usarlo.

## 2. Conexión y Materiales Necesarios (Páginas 1 y 2)
Para el proyecto necesitas:
* Un **Arduino UNO**.
* El módulo **GY-NEO6MV2**.
* Un **Shield (placa de expansión)** para Arduino que incluya lector de tarjeta **SD** y un **Reloj en Tiempo Real (RTC)**.
* **Conexiones:** El pin TX del GPS va al pin 4 del Arduino, el RX al pin 3, VCC a 5V y GND a GND.

## 3. El Código y las Librerías (Páginas 2 y 3)
El documento incluye un programa (sketch) en C++ que requiere las librerías `TinyGPS` (para interpretar los datos del GPS) y `RTClib` (para la hora exacta).

**¿Qué hace el código?**
1. Inicializa la tarjeta SD y el reloj de tiempo real.
2. Revisa si el GPS tiene señal.
3. Extrae las coordenadas de **Latitud** y **Longitud**.
4. Muestra las coordenadas en el Monitor Serie de Arduino.
5. **Guarda los datos:** Escribe las coordenadas y la hora exacta en un archivo de texto llamado `gpsLOG.txt` dentro de la tarjeta SD.

## 4. Resultados y Uso en el Mundo Real (Página 4)
* **El archivo final:** Muestra una captura de pantalla del archivo `gpsLOG.txt` abierto en un ordenador, con columnas de fecha, latitud, longitud y hora.
* **Uso autónomo:** La última imagen muestra el proyecto montado sobre una batería de 6V, demostrando que se puede llevar el Arduino, el GPS y la batería en una mochila o vehículo para registrar rutas sin necesidad de un ordenador.
* **Paciencia:** Al encenderlo, hay que esperar entre **40 y 120 segundos** (al aire libre) para que la luz del GPS empiece a parpadear y comience a registrar datos.
