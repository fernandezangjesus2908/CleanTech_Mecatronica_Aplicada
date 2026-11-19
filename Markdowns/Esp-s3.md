- ### ESP32-S3-DevKitC-1:

## 1. Distribución y "Multiplexación" de Pines.
El chip tiene **56 pines** distribuidos en un encapsulado cuadrado. Algo muy importante es que el ESP32-S3 usa **"multiplexación"**. 
* **¿Qué significa esto?** Que un solo pin físico puede hacer varias funciones diferentes (como enviar datos, leer un sensor, o encender un LED) dependiendo de cómo lo programes por software. Esto permite que el chip sea pequeño sin perder capacidades.

## 2. Tipos de Funciones de los Pines.
Los pines se dividen en tres grupos principales:
* **Pines IO (Entrada/Salida):** Son los pines estándar para conectar componentes (botones, pantallas, relés, etc.). Tienen funciones predefinidas para comunicaciones como UART (puerto serie), SPI (para tarjetas SD y pantallas) y JTAG (para depuración).
* **Funciones RTC (Real-Time Clock):** Algunos pines siguen funcionando incluso cuando el chip está en modo de "sueño profundo" (Deep-sleep) consumiendo muy poca energía. Son ideales para despertar el chip con un sensor de movimiento o un botón.
* **Funciones Analógicas:** Ciertos pines pueden leer voltajes variables (Convertidor Analógico-Digital o ADC) o detectar toques capacitivos (Touch Sensor).

## 3. Advertencias Importantes: ¡Cuidado con estos pines!
El documento advierte sobre varios detalles críticos que pueden arruinar tu proyecto si no los tienes en cuenta:
* **Pines de Memoria (Flash/PSRAM):** Los pines del 26 al 32 (aproximadamente) están conectados a la memoria interna del chip. **NUNCA** los uses para conectar otras cosas, o el chip se colgará y no arrancará.
* **Pines de Configuración (Strapping Pins):** Pines como GPIO0, GPIO3, GPIO45 y GPIO46 deciden cómo arranca el chip (por ejemplo, si entra en modo de programación o modo normal). Deben estar en niveles de voltaje específicos al encender.
* **"Glitches" al Encender:** Algunos pines (como GPIO19 y GPIO20) emiten pequeños pulsos eléctricos (picos) de unos 60 microsegundos justo cuando se enciende el chip. Si tienes conectado un relé o un motor, podría activarse brevemente por error.
* **Pines de USB y UART0:** Normalmente están reservados para programar el chip y ver los mensajes en la computadora.

## 4. Alimentación y Encendido.
* **Voltajes:** El chip no usa un solo voltaje. Tiene reguladores internos que convierten la energía para diferentes partes: 1.1V para el procesador digital, 1.1V para el reloj de tiempo real (RTC) y 1.8V para la memoria flash.
* **El botón de encendido (CHIP_PU):** El pin **CHIP_PU** es el encargado de encender o resetear el chip. Para que funcione, debe recibir un voltaje alto (High). El datasheet muestra un gráfico de tiempo: hay que esperar al menos **50 microsegundos** después de que llegue la energía para activar este pin, de lo contrario el chip podría no arrancar correctamente.