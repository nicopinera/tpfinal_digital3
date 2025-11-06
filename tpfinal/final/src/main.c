#include "LPC17xx.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_exti.h"
#include "lpc17xx_gpdma.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_dac.h"
#include "lpc17xx_timer.h"
#include <stdint.h>

#define PR_TICK_1 4 // valor en ticks del pre scaler 1
#define MAX_SAMPLES 100 // hay que cambiarlo a posterior para que sea variable y seteable por el usuario
#define PCLK 25000000

// define para DMA del ADC
#define S_TRANF_WIDTH (1<<18) // Source transfer width -> 16 bits
#define D_TRANF_WIDTH (1<<21) // Destination transfer width -> 16 bits
#define D_INCREMENT (1<<27) // se incrementa el destino
#define INT_FIN (1<<31)

volatile int opc = 0;
volatile int frec = 10000; // setear la frecuencia de la onda

// ALMACENA LA FUNCION GENERADA (Seno, rampa, etc)
uint32_t buffer[MAX_SAMPLES]; // buffer para guardar la señal generada

void configPIN(void) {
	// Configuración de pines
	PINSEL_CFG_Type pin;
	pin.Portnum = 0;
	pin.Pinnum = 22;
	pin.Funcnum = 0;
	pin.OpenDrain = 0;
	pin.Pinmode = PINSEL_PINMODE_PULLUP;
	PINSEL_ConfigPin(&pin);
	GPIO_SetDir(0, 1 << 22, 1); // salida
	GPIO_ClearValue(0, 1 << 22); // prendo led rojo

	pin.Portnum = 0;
	pin.Pinnum = 24; // AD1
	pin.Funcnum = 1;
	pin.OpenDrain = 0;
	pin.Pinmode = PINSEL_PINMODE_TRISTATE;
	PINSEL_ConfigPin(&pin);

	pin.Portnum = 0;
	pin.Pinnum = 26;
	pin.Funcnum = 1;
	pin.OpenDrain = 0;
	pin.Pinmode = PINSEL_PINMODE_TRISTATE;
	PINSEL_ConfigPin(&pin);

}
void configADC(void) {
	ADC_Init(LPC_ADC, 200000);
	//ADC_StartCmd(LPC_ADC, ADC_START_ON_MAT01); // INICIA CON EL TIMER 0 - MATCH 1
	//ADC_EdgeStartConfig(LPC_ADC, ADC_START_ON_RISING); // CADA FLANCO DE SUBIDA
	ADC_IntConfig(LPC_ADC, ADC_ADINTEN1, ENABLE);
	ADC_ChannelCmd(LPC_ADC, 1, ENABLE);
	NVIC_EnableIRQ(ADC_IRQn); // habilito la int
}
void configDAC_sinDMA(void) {
	DAC_Init(LPC_DAC);
	DAC_SetBias(LPC_DAC, 0); // seteo la frecuencia en 1MHz
}

void configTIMER(void) {
	TIM_TIMERCFG_Type config_pre;
	config_pre.PrescaleOption = TIM_PRESCALE_TICKVAL;
	config_pre.PrescaleValue = PR_TICK_1;

	TIM_Init(LPC_TIM0, TIM_TIMER_MODE, &config_pre); // configuro el pre scaler

	TIM_MATCHCFG_Type config_timer;
	config_timer.MatchChannel = 1;
	config_timer.IntOnMatch = ENABLE;
	config_timer.ResetOnMatch = ENABLE; // resetea en match
	config_timer.StopOnMatch = DISABLE;
	config_timer.ExtMatchOutputType = TIM_EXTMATCH_TOGGLE;
	config_timer.MatchValue = 100000;
	TIM_ConfigMatch(LPC_TIM0, &config_timer);
	NVIC_EnableIRQ(TIMER0_IRQn);
}

void set_mat_frec(int frecuencia) {
	float match = ((float) PCLK / ((float) frecuencia * (PR_TICK_1 + 1)))
			- 1.0f;
	TIM_UpdateMatchValue(LPC_TIM0, 1, match);
}

int main(void) {
	SystemInit();
	configPIN();
	configADC();
	configTimer();
	set_mat_frec(frec);
	configDAC_sinDMA();

	while (1) {
		GPIO_ClearValue(0, 1 << 22);
	}
}

void TIMER0_IRQHandler(void) {
	ADC_StartCmd(LPC_ADC, ADC_START_NOW); // activo el adc por cada interrupcion del timer
	TIM_ClearIntPending(LPC_TIM0, TIM_MR0_INT); // limpio bandera
}

void ADC_IRQHandler() {
	uint16_t ADC0Value = 0;
	ADC0Value = ADC_ChannelGetData(LPC_ADC, 1);
	ADC0Value = ADC0Value << 4;
	DAC_UpdateValue(LPC_DAC, ADC0Value);
	LPC_ADC->ADGDR &= LPC_ADC->ADGDR;
	return;
}

void configDMA(void) {

	GPDMA_LLI_Type lli_dac_1;
	GPDMA_Channel_CFG_Type config_dma;
	config_dma.ChannelNum = 1; // canal 1
	config_dma.TransferSize = MAX_SAMPLES; // 100 muestras
	config_dma.TransferWidth = 32; // 32 bits
	config_dma.SrcMemAddr = (uint32_t) &buffer;
	config_dma.DstMemAddr = 0;
	config_dma.TransferType = GPDMA_TRANSFERTYPE_M2P; // MEMORIA -> DAC
	config_dma.SrcConn = 0;
	config_dma.DstConn = GPDMA_CONN_DAC;
	config_dma.DMALLI = (uint32_t) &lli_dac_1;

	lli_dac_1.SrcAddr = (uint32_t) &buffer; // tomo el buffer
	lli_dac_1.DstAddr = (uint32_t) &(LPC_DAC->DACR);
	lli_dac_1.NextLLI = (uint32_t) &lli_dac_1;
	lli_dac_1.Control = MAX_SAMPLES | (2 << 18) // ancho de origen 32 bits
			| (2 << 21) // ancho de destino 32 bits
			| (1 << 26); // incremento de origen;

	GPDMA_Init();
	GPDMA_Setup(&config_dma);
	//GPDMA_ChannelCmd(channel, ENABLE);

}

void configDAC_conDMA(void) {
	DAC_CONVERTER_CFG_Type config_dac;
	config_dac.DMA_ENA = SET; // habilito dma
	config_dac.CNT_ENA = SET; // habilito time out
	config_dac.DBLBUF_ENA = 0; // no habilito el buffer interno
	// faltaria configurar el time out o calcularlo en funcion de la frecuencia de la señal
	//Time_out = (25000000)/(Fseñal * N_muestras)
	uint32_t t_out = (uint32_t) ((float) PCLK / (frec * (float) MAX_SAMPLES)); // calculo del time_out en funcion de los valores de la señal
	DAC_SetDMATimeOut(LPC_DAC, t_out); // Hay que ponerle el valor en TICKS
	DAC_ConfigDAConverterControl(LPC_DAC, &config_dac);
}

void generar_Seno(uint32_t buffer[], int tam) {

}

void generar_Rampa(uint32_t buffer[], int tam) {

}

