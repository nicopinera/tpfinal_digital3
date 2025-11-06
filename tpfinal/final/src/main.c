#include "LPC17xx.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_exti.h"
#include "lpc17xx_gpdma.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_dac.h"
#include "lpc17xx_timer.h"
#include <stdint.h>

// valor en ticks del pre scaler 1
#define PR_TICK_1 4

// hay que cambiarlo a posterior para que sea variable y seteable por el usuario
#define MAX_SAMPLES 100

// Frecuencia de perifericos
#define PCLK 25000000

// define para DMA del ADC
#define S_TRANF_WIDTH (1<<18) // Source transfer width -> 16 bits
#define D_TRANF_WIDTH (1<<21) // Destination transfer width -> 16 bits
#define D_INCREMENT (1<<27) // se incrementa el destino
#define INT_FIN (1<<31)

// opcion de prueba
volatile int opc = 0;

// settear la frecuencia de la onda
volatile int frec = 10000;

volatile int interrupcion_t = 0;

// ALMACENA LA FUNCION GENERADA (Seno, rampa, etc)
uint32_t buffer[MAX_SAMPLES]; // buffer para guardar la señal generada

void configPIN(void) {
	// Configuración de pines
	PINSEL_CFG_Type pin;
	pin.Portnum = 0; // puerto 0
	pin.Pinnum = 22; // pin 22 -> led rojo
	pin.Funcnum = 0; // funcion GPIO
	pin.OpenDrain = 0; // open drain 0
	pin.Pinmode = PINSEL_PINMODE_PULLUP; // Pull up
	PINSEL_ConfigPin(&pin);
	GPIO_SetDir(0, 1 << 22, 1); // salida
	GPIO_ClearValue(0, 1 << 22); // prendo led rojo

	pin.Portnum = 0; // puerto 0
	pin.Pinnum = 24; // AD1
	pin.Funcnum = 1; // Funcion AD1
	pin.OpenDrain = 0; // Open drain 0
	pin.Pinmode = PINSEL_PINMODE_TRISTATE; // tristate
	PINSEL_ConfigPin(&pin);

	pin.Portnum = 0; // puerto 0
	pin.Pinnum = 26; // pin 26
	pin.Funcnum = 2; // Funcion VOUT
	pin.OpenDrain = 0;
	pin.Pinmode = PINSEL_PINMODE_TRISTATE; // Tristate
	PINSEL_ConfigPin(&pin);

}
void configADC(void) {
	ADC_Init(LPC_ADC, 200000); // Se configura a 200KHz
	ADC_IntConfig(LPC_ADC, ADC_ADINTEN1, DISABLE); // Habilito la interrupcion del canal 1
    ADC_BurstCmd(LPC_ADC, DISABLE);
	ADC_ChannelCmd(LPC_ADC, 1, ENABLE); // habilito el canal 1
	//NVIC_EnableIRQ(ADC_IRQn); // habilito la int en el NVIC
}
void configDAC_sinDMA(void) {
	DAC_Init(LPC_DAC); // Inicio el DAC
	DAC_SetBias(LPC_DAC, 0); // seteo la frecuencia en 1MHz -> bias en 0
}

void configTIMER(void) {
	TIM_TIMERCFG_Type config_pre;
	config_pre.PrescaleOption = TIM_PRESCALE_TICKVAL; // Prescaler en ticks
	config_pre.PrescaleValue = PR_TICK_1; // lo pongo en 4

	TIM_Init(LPC_TIM0, TIM_TIMER_MODE, &config_pre); // configuro el pre scaler

	TIM_MATCHCFG_Type config_timer;
	config_timer.MatchChannel = 1; // canal del match 1
	config_timer.IntOnMatch = ENABLE; // habilito la interrupcion
	config_timer.ResetOnMatch = ENABLE; // resetea en match
	config_timer.StopOnMatch = DISABLE; // deshabilito el stop
	config_timer.ExtMatchOutputType = TIM_EXTMATCH_TOGGLE; // external match nothing
	config_timer.MatchValue = 100000; // Match de arranque, se modifica con set frec
	TIM_ConfigMatch(LPC_TIM0, &config_timer);
	NVIC_EnableIRQ(TIMER0_IRQn); // habilito la interrupcion
	TIM_ClearIntPending(LPC_TIM0, TIM_MR0_INT); //Limpio bandera de interrupcion del timer0
}

/* Calcula y actualiza MR1 para que el evento de match ocurra a 'frecuencia' Hz.
 - Si usas PR_TICK como PrescaleValue en TIM_Init, el factor real es (PR_TICK + 1).
 - Usa entero de 64 bits para evitar overflow en la multiplicación (frecuencia * pres).
 - Aplica redondeo al entero más cercano. */
void set_mat_frec(uint32_t frecuencia) {
	if (frecuencia == 0U)
		return; // evitar división por cero

	uint32_t pres = (uint32_t) PR_TICK_1 + 1ULL; // PR + 1
	uint32_t denom = (uint32_t) frecuencia * pres;

	// redondeo: (PCLK + denom/2) / denom - 1
	uint32_t match = 0U;
	if (denom != 0ULL) {
		match = (uint32_t) (((uint32_t) PCLK + (denom / 2ULL)) / denom - 1ULL);
	}

	TIM_UpdateMatchValue(LPC_TIM0, 1, match);
}

/*
 void set_mat_frec(int frecuencia) {
 float match = ((float) PCLK / ((float) frecuencia * (PR_TICK_1 + 1)))
 - 1.0f;
 TIM_UpdateMatchValue(LPC_TIM0, 1, match);
 }
 */
int main(void) {
	SystemInit();
	configPIN();
	configADC();
	configTIMER();
	set_mat_frec(frec);
	configDAC_sinDMA();

	while (1) {
		GPIO_ClearValue(0, 1 << 22);
	}
}

void TIMER0_IRQHandler(void) {
	if (TIM_GetIntStatus(LPC_TIM0, TIM_MR0_INT) == SET) {

		ADC_StartCmd(LPC_ADC, ADC_START_NOW);

		while (!(ADC_ChannelGetStatus(LPC_ADC, ADC_CHANNEL_1, ADC_DATA_DONE)))
			; // espero a que termine la conversion

		uint16_t ADC0Value = 0;
		ADC0Value = ADC_ChannelGetData(LPC_ADC, 1); // tomo el valor
		ADC0Value = ADC0Value << 4;
		DAC_UpdateValue(LPC_DAC, ADC0Value); // lo envio al DAC

		//TIM_Cmd(LPC_TIM0, ENABLE); //inicia el timer
		TIM_ClearIntPending(LPC_TIM0, TIM_MR0_INT); //Limpio bandera de interrupcion del timer0
	}
}

void ADC_IRQHandler() {

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

