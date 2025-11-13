# Trabajo Final - Electrónica Digital 3

Facultad de Ciencias Exactas, Físicas y Naturales - UNC  
Cátedra: Electrónica Digital 3  
Profesor: Ing. Gallardo  
Equipo: 3 alumnos

## Resumen
Este repositorio contiene el trabajo práctico final de la cátedra Electrónica Digital 3. El proyecto está implementado sobre la placa LPC1769 y provee un sistema para generar señales analógicas mediante el DAC, muestrear señales con el ADC y enviar datos por UART para su visualización en una PC.

## Objetivos
- Implementar generadores de señales (seno, triangular, escalonado, escalón) usando DMA + DAC.
- Implementar adquisición ADC periódica mediante Timer y envío por UART para análisis y visualización.
- Proveer una herramienta en PC para visualizar en tiempo real las muestras recibidas por UART.

## Estructura del proyecto
- /tpfinal/final/src
  - main.c: lógica principal, configuración de pines, manejo de interrupciones, modo DMA (DAC) y modo ADC+TIMER (muestreo + UART).
  - constantes.h / constantes.c: constantes, tablas y funciones para generar formas de onda y calcular parámetros del timer.
- /src
  - uart_plotter.py: script Python para recibir por UART las muestras y mostrarlas en tiempo real (gráfica temporal y FFT).

## Descripción del funcionamiento
1. Modos de operación:
   - Modo DMA + DAC: el MCU usa GPDMA para alimentar continuamente la DAC con una tabla (LUT) que genera la forma de onda elegida. Las opciones se seleccionan mediante dipswitches conectados al Puerto 2.
   - Modo ADC + TIMER: al activar un conmutador (EINT2), el sistema deshabilita el DMA y habilita un timer que dispara conversiones ADC periódicas. Los resultados se envían por UART2 (P0.10/P0.11) en formato de bytes seguidos de '\n'.

2. Selección de señal:
   - Los dipswitches en P2.0, P2.1 y P2.2 permiten seleccionar: ninguno, seno, triangular, escalonado o escalón.
   - Se implementa debounce por software usando SysTick (ms).

3. Generación de señales:
   - Funciones en constantes.c crean tablas para cada forma de onda. Para el seno se genera un cuarto de ciclo y luego se refleja para construir el ciclo completo.
   - Las tablas se adaptan al formato requerido por el DAC (10 bits alineados en MSB).

4. DMA y DAC:
   - Se configura una LLI para bucle infinito y se lanza el canal GPDMA conectado al DAC.
   - El timeout DMA/periodo del DAC se calcula en función de la frecuencia deseada y el número de samples.

5. ADC y UART:
   - En modo ADC+TIMER, el Timer0 genera la periodicidad; en su ISR se inicia la conversión ADC del canal seleccionado, se normaliza a 8 bits y se envía por UART2.
   - El script Python en la PC (uart_plotter.py) recibe los bytes, reconstruye la señal, muestra la gráfica en tiempo real y calcula la FFT si hay suficientes muestras.

## Uso y ejecución
- En la placa:
  1. Compilar el proyecto con el entorno para LPC1769 .
  2. Grabar el binario en la placa y conectar el puerto serie correspondiente.
  3. Usar los dipswitches para seleccionar la señal o el conmutador para pasar a modo ADC+TIMER.

- En la PC:
  1. Ejecutar `python3 src/uart_plotter.py -p /dev/ttyUSB0 -b 9600` (ajustar puerto y baudrate).
  2. El script mostrará la señal en tiempo real y un análisis de espectro (requiere backend gráfico como PyQt5 o Tk).

## Limitaciones / Observaciones
- El proyecto asume conexiones y pines según la placa LPC1769; adaptar si el hardware difiere.
- El script Python requiere dependencias (numpy, matplotlib, pyserial, opcionales PyQt).
- Hay que verificar el workspace / toolchain para resolver posibles advertencias o errores de compilación específicos del entorno.

## Integrantes
- Piñera, Nicolas
- Ferraris, Gianluca
- Richter, Juan

Profesor: Ing. Gallardo

Cátedra: Electrónica Digital 3