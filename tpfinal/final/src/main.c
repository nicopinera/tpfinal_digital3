/*Proyecto: Mini Osciloscopio + Generador de señales

Idea: La PC puede pedir al LPC1769 que:

- Lea una señal analógica (ADC con DMA): si se utiliza la opcion de captuar una señal utilizaria con el adc utilizo un canal de DMA y dos listas enlazadas
para ir guardando datos. El adc se triggerea con el timer para settear una frecuencia de muestreo.

- Genere una onda por DAC seno o rampa (también vía DMA)

En la PC se muestra la señal capturada o generada (por ejemplo en Python). Qué integra

Requisito: Uso

    ADC: Captura la señal
    DAC: Reproduce la señal o genera una que se vera en un osciloscopio
    DMA: Mueve buffers ADC ↔ memoria ↔ DAC
    Timer: Controla frecuencia de muestreo
    UART: Intercambia comandos y muestras con la PC
 *
 * */

#include "LPC17xx.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_exti.h"
#include "func_config.h"
#include <stdint.h>

// Definimos los Pines en un arreglo
Pines pines_uso[] = { { 0, 22, FUNC_0 }, // P2.12 - Funcion GPIO
		{0,23,FUNC_1}, // AD0.0 para capturar la señal
		};

uint16_t buffer1[MAX_SAMPLES]; // buffer 1 para muestras del ADC
uint16_t buffer2[MAX_SAMPLES]; // buffer 2 para muestras del ADC

// Calculo del numero de pines
const int NUM_PINES = sizeof(pines_uso) / sizeof(pines_uso[0]);

void configPIN(void); // Configuracion de GPIO
void configADC(void); // Configuracion del ADC
void configDAC(void); // configuracion del DAC
void configDMA(void); // Configuracion de DMA
void configUART(void); // Configuracion de comunicacion UART

int main(void) {
	SystemInit();
	configPIN();
	configADC();

	while (1) {
		GPIO_ClearValue(pines_uso[0].puerto, 1 << pines_uso[0].pin); // prendo led rojo
	}
}

void configPIN(void) {
	// Configuración de pines
	PINSEL_CFG_Type pin;
	for (int i = 0; i < NUM_PINES; i++) {
		pin.Portnum = pines_uso[i].puerto;
		pin.Pinnum = pines_uso[i].pin;
		pin.Funcnum = pines_uso[i].func;
		pin.Pinmode = 0;
		pin.OpenDrain = 0;
		PINSEL_ConfigPin(&pin);
		GPIO_SetDir(pines_uso[i].puerto, 1 << pines_uso[i].pin, 1);
	}
}

void configADC(void){
	ADC_Init(LPC_ADC, 200000);
	ADC_StartCmd(LPC_ADC, ADC_START_ON_MAT01); // INICIA CON EL TIMER 0 - MATCH 1
	ADC_EdgeStartConfig(LPC_ADC, ADC_START_ON_RISING); // CADA FLANCO DE SUBIDA
	ADC_ChannelCmd(LPC_ADC, ADC_CHANNEL_0, ENABLE); // HABILITO CANAL 0
}

