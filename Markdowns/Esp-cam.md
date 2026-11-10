# Descripción del Módulo ESP32-CAM (Wi-Fi + Cámara)

El **ESP32-CAM** es una placa de desarrollo compacta que integra un microcontrolador con conexión inalámbrica y una cámara, ideal para proyectos de visión artificial y transmisión de video.

## ¿Qué es y qué incluye?
* **El Cerebro:** Utiliza el microcontrolador **ESP32** (específicamente el modelo ESP32-DOWDQ6), que tiene un procesador de doble núcleo de 32 bits funcionando hasta a 240 MHz. Esto le da potencia suficiente para procesar imágenes básicas.
* **Conectividad:** Cuenta con **Wi-Fi (2.4 GHz)** y **Bluetooth 4.2**, permitiendo transmitir video en tiempo real a un servidor web, aplicación móvil o a la nube.
* **Cámara:** Incluye de fábrica un sensor **OV2640 de 2 Megapíxeles**. Tiene un conector para cámaras (también acepta la OV7670).
* **Extras:** Tiene un **LED de alta potencia** que sirve como flash para lugares oscuros y una ranura para **tarjeta MicroSD** para guardar fotos o videos.
* **Placa Base (MB):** A diferencia de otros módulos, esta versión incluye una placa base con puerto **Micro-USB** y el chip **CH340**. Esto significa que puedes conectarlo directamente a la computadora para programarlo y ver datos sin necesidad de comprar adaptadores externos.

## Distribución de Pines (Pinout)
La imagen muestra los pines disponibles en los laterales del módulo:
* Alimentación: 5V, 3.3V y GND (Tierra).
* Pines de propósito general (GPIO): Para conectar sensores, botones, etc.
* Pines de comunicación: UART (para enviar/recibir datos), ADC (para leer señales analógicas) y Touch (para detectar el toque de un dedo).
* Conectores para la cámara y la tarjeta MicroSD.

## Posibles Usos y Aplicaciones
Destacan cuatro áreas principales donde este módulo es muy útil:
1. **Domótica:** Para crear soluciones de hogar inteligente, controlando luces, sensores y dispositivos de forma inalámbrica.
2. **IoT y Monitoreo:** Ideal para recopilar datos de sensores y enviarlos a plataformas en la nube para monitoreo remoto.
3. **Robótica:** Permite a estudiantes y *makers* controlar motores y sensores para crear robots o vehículos autónomos con visión.
4. **Automatización:** Para integrar sensores y actuadores que automaticen procesos en la industria o la educación.

Es una plataforma muy popular para quienes quieren empezar en el mundo de la **visión artificial** (usando herramientas como OpenCV o TensorFlow Lite) y el **Internet de las Cosas (IoT)** sin gastar demasiado.