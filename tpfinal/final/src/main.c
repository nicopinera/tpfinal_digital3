/*Proyecto: Mini Osciloscopio + Generador de señales

 Idea: La PC puede pedir al LPC1769 que:

 - Lea una señal analógica (ADC con DMA): si se utiliza la opcion de captuar una señal utilizaria con el adc utilizo un canal de DMA y dos listas enlazadas
 para ir guardando datos. El adc se triggerea con el timer para settear una frecuencia de muestreo. Tiene que haber un comando

 - Genere una onda por DAC seno o rampa (también vía DMA):

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
#include "lpc17xx_gpdma.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_dac.h"
#include "func_config.h"
#include "lpc17xx_timer.h"
#include <stdint.h>

volatile int adc_on = 1;
volatile int banco = 0; // 0 -> buffer 1 | 1 -> buffer 2
volatile int frec = 10000; // setear la frecuencia de la onda

// Definimos los Pines en un arreglo
Pines pines_uso[] = { { 0, 22, FUNC_0 }, // P0.22 - Funcion GPIO
		{ 0, 23, FUNC_1 }, // AD0.0 para capturar la señal
		{ 3, 25, FUNC_0 }, // led
		{ 0, 26, FUNC_2 }, };

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
void set_mat_frec(int frecuencia);

int main(void) {
	SystemInit();
	configPIN();
	configADC();
	configDMA();
	configDAC();

	while (1) {
		switch (adc_on) {
		case 0: // prendo el adc
			set_mat_frec(frec);
			TIM_Cmd(LPC_TIM0, 1); // habilito timer 0
			ADC_ChannelCmd(LPC_ADC, 0, 1); // habilito el canal 0
			GPDMA_ChannelCmd(0, 1); // habilito canal 0
			GPIO_ClearValue(3, 1 << 25);
			break;
		case 1: // apago el adc
			TIM_Cmd(LPC_TIM0, DISABLE);
			ADC_ChannelCmd(LPC_ADC, 0, DISABLE); // Deshabilito el canal 0
			GPDMA_ChannelCmd(0, DISABLE); // habilito canal 0
			GPIO_ClearValue(0, 1 << 22);
			break;
		}
	}
}

void configPIN(void) {
	// Configuración de pines
	PINSEL_CFG_Type pin;
	for (int i = 0; i < NUM_PINES; i++) {
		pin.Portnum = pines_uso[i].puerto;
		pin.Pinnum = pines_uso[i].pin;
		pin.Funcnum = pines_uso[i].func;
		pin.OpenDrain = 0;

		if (pines_uso[i].func == FUNC_0) { // GPIO
			pin.Pinmode = 0;
			GPIO_SetDir(pines_uso[i].puerto, 1 << pines_uso[i].pin, 1); // salida
			PINSEL_ConfigPin(&pin);

		} else {
			pin.Pinmode = 2; // tristate
			PINSEL_ConfigPin(&pin);
		}

	}
}

void configADC(void) {
	ADC_Init(LPC_ADC, 200000);
	ADC_StartCmd(LPC_ADC, ADC_START_ON_MAT01); // INICIA CON EL TIMER 0 - MATCH 1
	ADC_EdgeStartConfig(LPC_ADC, ADC_START_ON_RISING); // CADA FLANCO DE SUBIDA
}

void set_mat_frec(int frecuencia) {
	float t_match = 1.0f / (frecuencia * (MAX_SAMPLES * 2.0f));
	int match_value = (int) ((t_match * (float) PCLK / (float) (PR_TICK_1 + 1))
			- 1.0f);
	TIM_UpdateMatchValue(LPC_TIM0, 1, match_value);
}

void configTIMER(void) {
	TIM_TIMERCFG_Type config_pre;
	config_pre.PrescaleOption = TIM_PRESCALE_TICKVAL;
	config_pre.PrescaleValue = PR_TICK_1;

	TIM_Init(LPC_TIM0, TIM_TIMER_MODE, &config_pre); // configuro el pre scaler

	TIM_MATCHCFG_Type config_timer;
	config_timer.MatchChannel = 1;
	config_timer.IntOnMatch = DISABLE;
	config_timer.ResetOnMatch = ENABLE; // resetea en match
	config_timer.StopOnMatch = DISABLE;
	config_timer.ExtMatchOutputType = TIM_EXTMATCH_TOGGLE;
	config_timer.MatchValue = 0;
	TIM_ConfigMatch(LPC_TIM0, &config_timer);
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
	lli_adc_1.Control = MAX_SAMPLES | S_TRANF_WIDTH | D_TRANF_WIDTH
			| D_INCREMENT | INT_FIN;
	// falta control

	lli_adc_2.SrcAddr = (uint32_t) &(LPC_ADC->ADGDR); // tomo el registro
	lli_adc_2.DstAddr = (uint32_t) &buffer2;
	lli_adc_2.NextLLI = (uint32_t) &lli_adc_1;
	lli_adc_2.Control = MAX_SAMPLES | S_TRANF_WIDTH | D_TRANF_WIDTH
			| D_INCREMENT | INT_FIN;

	GPDMA_Init();
	GPDMA_Setup(&config_dma);

	GPDMA_LLI_Type lli_dac_1;
	GPDMA_LLI_Type lli_dac_2;
	config_dma.ChannelNum = 1; // canal 1
	config_dma.TransferSize = MAX_SAMPLES; // 100 muestras
	config_dma.TransferWidth = 32; // 32 bits
	config_dma.SrcMemAddr = (uint32_t)&buffer1;
	config_dma.DstMemAddr = 0;
	config_dma.TransferType = GPDMA_TRANSFERTYPE_M2P; // MEMORIA -> DAC
	config_dma.SrcConn = 0;
	config_dma.DstConn = GPDMA_CONN_DAC;
	config_dma.DMALLI = (uint32_t)&lli_dac_1;

	lli_dac_1.SrcAddr = (uint32_t) &buffer1; // tomo el buffer
	lli_dac_1.DstAddr = (uint32_t) &(LPC_DAC->DACR);
	lli_dac_1.NextLLI = (uint32_t) &lli_dac_2;
	lli_dac_1.Control = MAX_SAMPLES | (2 << 18) // ancho de origen 32 bits
			| (2 << 21) // ancho de destino 32 bits
			| (1 << 26); // incremento de origen;
	// falta control

	lli_dac_2.SrcAddr = (uint32_t) &buffer2; // tomo el registro
	lli_dac_2.DstAddr = (uint32_t) &(LPC_DAC->DACR);
	lli_dac_2.NextLLI = (uint32_t) &lli_dac_1;
	lli_dac_2.Control = MAX_SAMPLES | (2 << 18) // ancho de origen 32 bits
			| (2 << 21) // ancho de destino 32 bits
			| (1 << 26);
	GPDMA_Setup(&config_dma);
	GPDMA_Init();

}

void configDAC(void) {
	DAC_CONVERTER_CFG_Type config_dac;
	config_dac.DMA_ENA = 1; // habilito dma
	config_dac.CNT_ENA = 1; // habilito time out
	config_dac.DBLBUF_ENA = 0; // no habilito el buffer interno
	// faltaria configurar el time out o calcularlo en funcion de la frecuencia de la señal
	// Time_out = (25000000)/(Fseñal * N_muestras)
	DAC_Init(LPC_DAC);
	DAC_SetBias(LPC_DAC, 0); // seteo la frecuencia en 1MHz
	float t_out = 1.0f / (frec * (MAX_SAMPLES * 2.0f));
	int time_out_value = (int) (t_out * (float) PCLK );
	DAC_SetDMATimeOut(LPC_DAC, time_out_value);
	DAC_ConfigDAConverterControl(LPC_DAC, &config_dac);

}

void DMA_IRQHandler() {
	if (GPDMA_IntGetStatus(GPDMA_STAT_INT, 0) == 1) {
		if (GPDMA_IntGetStatus(GPDMA_STAT_INTTC, 0)) {
			switch (banco) {
			case 0:
				for (int i = 0; i < MAX_SAMPLES; i++) {
					buffer1[i] = (buffer1[i] >> 4) & 0xFFF; // desplazo y limpio
					buffer1[i] = (buffer1[i] << 6);
				}
				GPDMA_ChannelCmd(1, ENABLE); // habilito canal 1
				break;
			case 1:
				for (int i = 0; i < MAX_SAMPLES; i++) {
					buffer2[i] = (buffer2[i] >> 4) & 0xFFF;
					buffer2[i] = (buffer2[i] << 6);

				}
				break;
			}
			banco ^= 1;
			GPDMA_ClearIntPending(GPDMA_STATCLR_INTTC, 0);
		}
	}

}

