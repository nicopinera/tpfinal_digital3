# Trabajo Práctico — Osciloscopio con LPC1769

## Resumen

Se desarrolló un osciloscopio básico usando la placa LPC1769 aprovechando los periféricos ADC, DAC, TIMER, DMA y UART. El dispositivo adquiere señales analógicas, las procesa y permite su visualización y transmisión serial para monitorización externa, o la generacion por software de una señal seno o rampa.

## Objetivos

- Implementar adquisición de señales por ADC con muestreo periódico, definido en funcion de la frecuencia establecida por el usuario y la cantidad de muestras.
- Transferir datos usando DMA.
- Generar señales de salida con DAC.
- Controlar tiempos de muestreo con TIMER.
- Enviar datos y mensajes de estado por UART.

## Hardware

- Microcontrolador: NXP LPC1769 (placa de desarrollo).
- Entradas analógicas: canal ADC de la placa.
- Salida analógica: DAC integrado.
- Interconexiones: UART para monitoreo/registro en PC.

## Software / Periféricos utilizados

- ADC: adquisición de muestras periódicas.
- DMA: transferencia de buffers a memoria  o perifericos sin CPU.
- TIMER: generación de triggers de muestreo.
- DAC: salida de señal.
- UART: comunicación serie para envío de datos y comandos.
- Entorno de desarrollo: MCUXpresso

## Diseño y funcionamiento

1. El TIMER programa el periodo de muestreo.
2. En cada pulso, el ADC convierte la señal analógica y el DMA transfiere la muestra a un buffer en RAM.
3. Cuando el buffer está completo, se procesa (p. ej. escalado y calibración) y se puede:
   - Enviar por UART a una aplicación en PC.
   - Reproducir o generar una señal con el DAC.
4. Estados y errores se reportan por UART para depuración.

## Resultados y pruebas

- Muestreo estable a la frecuencia objetivo (dependerá de la configuración del TIMER/ADC).
- Transferencia continua con DMA reduciendo uso de CPU.
- Verificación funcional: comparación entre señal de entrada y lectura digital; salida DAC coherente con waveform generada.

## Uso (instrucciones rápidas)

1. Compilar y flashear el firmware en la LPC1769.
2. Conectar la señal de prueba al pin ADC correspondiente.
3. Abrir una terminal serie (configurar baud rate según firmware) para recibir datos y mensajes.
4. Opcional: conectar oscilador/func generator para pruebas y monitorizar salida DAC si aplica.

## Consideraciones y mejoras futuras

- Mejorar filtrado y calibración para mayor precisión.
- Soporte multicanal y buffers circulares para adquisición continua.

## Conclusión

El proyecto demuestra la utilización integrada de ADC, DMA, TIMER, DAC y UART en la LPC1769 para construir una herramienta de adquisición de señales. La arquitectura propuesta maximiza la eficiencia de la CPU y permite posteriores mejoras en visualización y procesamiento.
