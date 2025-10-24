/*Proyecto: Mini Osciloscopio + Generador de señales
 * Idea: La PC puede pedir al LPC1769 que:
 * Lea una señal analógica (ADC con DMA): si se utiliza la opcion de captuar una señal
 * utilizaria con el adc utilizo un canal de DMA y dos listas enlazadaspara ir guardando datos.
 * El adc se triggerea con el timer para settear una frecuencia de muestreo. Tiene que haber un comando
 *
 * Genere una onda por DAC seno o rampa (también vía DMA)
 * En la PC se muestra la señal capturada o generada (por ejemplo en Python). Qué integra
 *
 * Requisito: Uso
 * ADC: Captura la señal
 * DAC: Reproduce la señal o genera una que se vera en un osciloscopio
 * DMA: Mueve buffers ADC ↔ memoria ↔ DAC
 * Timer: Controla frecuencia de muestreo
 * UART: Intercambia comandos y muestras con la PC
 * */

#include "LPC17xx.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_exti.h"
#include "lpc17xx_gpdma.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_dac.h"
#include "func_config.h"
#include "lpc17xx_timer.h"
#include <stdint.h>

// Definimos los Pines en un arreglo
Pines pines_uso[] = { { 0, 22, FUNC_0 }, // P2.12 - Funcion GPIO
		{ 0, 23, FUNC_1 }, // AD0.0 para capturar la señal
		{ 0, 26, FUNC_1 }, // AOUT del DAC
		{ 0, 2, FUNC_1 }, // TXD0 UART
		{ 0, 3, FUNC_1 }, // RXD0 UART
		};

// se puede usar un solo buffer?
uint32_t buffer1[MAX_SAMPLES]; // buffer 1 para muestras del ADC
uint32_t buffer2[MAX_SAMPLES]; // buffer 2 para muestras del ADC

// Calculo del numero de pines
const int NUM_PINES = sizeof(pines_uso) / sizeof(pines_uso[0]);

void configPIN(void); // Configuracion de GPIO
void configADC(void); // Configuracion del ADC
void configDAC(void); // configuracion del DAC
void configDMA(void); // Configuracion de DMA
void configTIMER(void); // configuracion de Timer
void configUART(void); // Configuracion de comunicacion UART

int main(void) {
	SystemInit();
	configPIN();
	configADC();
	configDMA();
	configDAC();

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

void configADC(void) {
	ADC_Init(LPC_ADC, 200000);
	ADC_StartCmd(LPC_ADC, ADC_START_ON_MAT01); // INICIA CON EL TIMER 0 - MATCH 1
	ADC_EdgeStartConfig(LPC_ADC, ADC_START_ON_RISING); // CADA FLANCO DE SUBIDA
	ADC_ChannelCmd(LPC_ADC, ADC_CHANNEL_0, ENABLE); // HABILITO CANAL 0
}

void configTIMER(void) {
	// EL periodo de la señal es 1/fseñal
	// el numero de muestras sera de 200 que es el maximo para los dos buffer
	// entonces el timer tiene que habr una muestra cada (1/fseñal*200)
	// Por ende el timer tiene que hacer el toggle a la mitad de ese periodo
}

void configDMA(void) {
	GPDMA_LLI_Type lli_adc_1;
	GPDMA_LLI_Type lli_adc_2;
	GPDMA_Channel_CFG_Type config_dma;
	config_dma.ChannelNum = 0; // canal 0
	config_dma.TransferSize = MAX_SAMPLES; // 100 muestras
	config_dma.TransferWidth = 32; // 32 bits
	config_dma.SrcMemAddr = 0;
	config_dma.DstMemAddr = (uint32_t) &buffer1;
	config_dma.TransferType = GPDMA_TRANSFERTYPE_P2M; // ADC -> MEMORIA
	config_dma.SrcConn = GPDMA_CONN_ADC; // ADC
	config_dma.DstConn = 0;
	config_dma.DMALLI = (uint32_t) &lli_adc_1; // puntero?

	lli_adc_1.SrcAddr = (uint32_t) &(LPC_ADC->ADGDR); // tomo el registro completo
	lli_adc_1.DstAddr = (uint32_t) &buffer1; // faltaria &?
	lli_adc_1.NextLLI = (uint32_t) &lli_adc_2;
	// falta control

	lli_adc_2.SrcAddr = (uint32_t) &(LPC_ADC->ADGDR); // tomo el registro
	lli_adc_2.DstAddr = (uint32_t) &buffer2;
	lli_adc_2.NextLLI = (uint32_t) &lli_adc_1;

	GPDMA_Init();
	GPDMA_Setup(&config_dma);
	GPDMA_ChannelCmd(0, ENABLE); // habilito canal 0

}

void configDAC(void) {
	DAC_CONVERTER_CFG_Type config_dac;
	config_dac.DMA_ENA = SET; // habilito dma
	config_dac.CNT_ENA = SET; // habilito time out
	config_dac.DBLBUF_ENA = 0; // no habilito el buffer interno
	// faltaria configurar el time out o calcularlo en funcion de la frecuencia de la señal
	// Time_out = (25000000)/(Fseñal * N_muestras)
	// tmp = (PCLK_DAC_IN_MHZ*1000000)/(FREC_SEÑAL*MAX_SAMPLES);
	DAC_Init(LPC_DAC);
	DAC_SetBias(LPC_DAC, 0); // seteo la frecuencia en 1MHz
	DAC_ConfigDAConverterControl(LPC_DAC, &config_dac);

}

