# Sistema de vision del sistema CleanTECH.

<img width="487" height="265" alt="image" src="https://github.com/user-attachments/assets/aec2b8d5-8f1f-40ca-9207-6712cad8a68b" />

Para llevar a cabo los subprocesos que integran.

<img width="432" height="273" alt="image" src="https://github.com/user-attachments/assets/f2f0870b-40a2-4096-a1fc-2762934897e9" />

## Componentes utilizados dentro de la construcción del sistema de visión CleanTECH:

- ### ESP32-CAM:
![alt text](image.png)

La **ESP32-CAM** es una placa de desarrollo de bajo costo basada en el microcontrolador ESP32, con cámara integrada y tamaño reducido. Es ideal para proyectos de Internet de las Cosas (IoT), prototipos y proyectos DIY.

## ¿Qué incluye y qué puede hacer?
- Integra **Wi-Fi**, Bluetooth clásico y Bluetooth de bajo consumo (BLE).
- Tiene **dos núcleos de 32 bits LX6** con arquitectura de 7 etapas, frecuencia ajustable de **80 MHz a 240 MHz**.
- Incluye sensor Hall, sensor de temperatura y otros periféricos internos.
- Compatible con estándares **Wi-Fi 802.11 b/g/n/e/i** y **Bluetooth 4.2**.
- Puede funcionar como controlador de red independiente (modo maestro) o como esclavo de otro microcontrolador para añadir conectividad.

## Aplicaciones típicas
- Dispositivos inteligentes para el hogar.
- Control industrial inalámbrico.
- Monitoreo inalámbrico y videovigilancia.
- Identificación inalámbrica de códigos QR.
- Sistemas de posicionamiento inalámbrico.
- Proyectos IoT en general.

## Características principales
- Hasta **160 MHz** de reloj y potencia de cómputo de hasta **600 DMIPS**.
- **520 KB de SRAM interna** + **4 MB de PSRAM externa**.
- Soporta interfaces **UART, SPI, I2C, PWM, ADC y DAC**.
- Compatible con cámaras **OV2640** y **OV7670**, con flash incorporado.
- Permite subida de imágenes por Wi-Fi.
- Soporta tarjeta **TF (microSD)** de hasta **4 GB**.
- Varios modos de sueño para bajo consumo.
- Incluye **Lwip** y **FreeRTOS** embebidos.
- Soporta modos **STA / AP / STA+AP**.
- Compatible con **Smart Config / AirKiss**.
- Permite actualización de firmware por puerto serie y **FOTA** (por Wi-Fi).

## Especificaciones técnicas
- **Flash SPI:** 32 Mbit por defecto.
- **RAM:** 520 KB interna + 4 MB PSRAM externa.
- **Dimensiones:** 27 × 40.5 × 4.5 mm (±0.2 mm).
- **Bluetooth:** 4.2 BR/EDR y BLE.
- **Wi-Fi:** 802.11 b/g/n/e/i.
- **Puerto serie:** 115200 bps por defecto.
- **Formatos de imagen:** JPEG (solo con OV2640), BMP y escala de grises.
- **Rango de frecuencia:** 2412 – 2484 MHz.
- **Antena:** PCB integrada, ganancia de 2 dBi.
- **Alimentación:** 5V.
- **Temperatura de operación:** -20 °C a 85 °C.
- **Peso:** 10 g.

## Notas importantes de uso
1. La fuente de alimentación debe ser de **al menos 5V y 2A**; de lo contrario, pueden aparecer ondulaciones o ruido en la imagen.
2. El pin **GPIO32** controla la alimentación de la cámara: debe ponerse en **nivel bajo** cuando la cámara esté en funcionamiento.
3. El pin **IO** está conectado al reloj XCLK de la cámara; debe dejarse **flotante** y no conectarse a nivel alto o bajo.
4. La placa ya incluye firmware de fábrica. Se recomienda precaución al grabar otros firmwares, ya que no se proporcionan archivos adicionales para descarga.

## Consumo energético aproximado
- Flash apagado: **180 mA a 5V**.
- Flash encendido al máximo: **310 mA a 5V**.
- Modo deep-sleep: hasta **6 mA a 5V**.
- Modo modern-sleep: hasta **20 mA a 5V**.
- Modo light-sleep: hasta **6.7 mA a 5V**.

- ### Cámara OV2640:
![alt text](image-1.png)
La **OV2640** es un sensor de imagen CMOS (fotografía digital) de bajo voltaje que integra en un solo chip una cámara de 2 Megapíxeles y un procesador de imágenes. Es muy popular en el mundo de la electrónica y el Internet de las Cosas (IoT), siendo la cámara estándar para módulos como el ESP32-CAM. 

## Características principales
- **Resolución:** Captura imágenes en formato **UXGA** (1632 x 1232 píxeles, equivalente a 2 MP).
- **Tamaño del píxel:** 2.2 µm x 2.2 µm.
- **Frecuencia de imagen:** Hasta 15 cuadros por segundo (fps) en resolución máxima, y hasta 30 fps en resoluciones menores (SVGA).
- **Alta sensibilidad:** Funciona bien en condiciones de baja iluminación (0.6 V/Lux-sec).
- **Formatos de salida:** Soporta YUV, RGB, Raw RGB y YCbCr.
- **Compresión integrada:** Incluye un motor de compresión JPEG interno, lo que ahorra memoria y ancho de banda.
- **Controles automáticos:** Ajuste automático de exposición (AEC), ganancia (AGC) y balance de blancos (AWB), además de reducción de ruido y control de saturación.
- **Voltajes de alimentación:** Núcleo de 1.2V, analógico de 2.5V a 3.0V, y entrada/salida de 1.7V a 3.3V.
- **Consumo de energía:** 125 mW en funcionamiento activo y 600 µA en modo de espera.

## ¿Cómo se comunica? (Pines principales)
El sensor tiene un encapsulado compacto (CSP de 38 pines) y se comunica mediante dos vías:
1. **Interfaz SCCB (Serial Camera Control Bus):** Muy similar al I2C. Utiliza los pines `SIO_C` (reloj) y `SIO_D` (datos) para configurar el sensor (resolución, brillo, efectos, etc.).
2. **Puerto de Video Digital:** Envía los datos de la imagen a través de 8 o 10 bits (`Y2` a `Y9`), sincronizados por los pines `PCLK` (reloj de píxel), `HREF` (referencia horizontal) y `VSYNC` (sincronización vertical).

También cuenta con pines clave como `XVCLK` (reloj del sistema), `PWDN` (para apagar el sensor y ahorrar energía) y `RESETB` (para reiniciarlo).

## Funcionamiento Interno
1. **Captura de luz:** La matriz de sensores convierte la luz en señales eléctricas (analógicas).
2. **Conversión:** Dos convertidores Analógico-Digital (A/D) de 10 bits convierten esas señales en datos digitales.
3. **Procesamiento (DSP):** El chip ajusta el color, aplica corrección gamma, elimina el ruido y mejora los bordes.
4. **Compresión y Salida:** Si se activa, el motor de compresión convierte la imagen a JPEG, y luego se envía al microcontrolador o dispositivo externo a través del puerto de video.

## Aplicaciones Comunes
- Teléfonos móviles y cámaras de vigilancia doméstica.
- Juguetes interactivos y sistemas de visión en robótica.
- Multimedia para PC y cámaras fotográficas digitales.
- Proyectos de reconocimiento de imágenes, visión artificial y *streaming* de video con microcontroladores como Arduino o ESP32.
