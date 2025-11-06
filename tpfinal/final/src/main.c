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
#define PR_TICK_1 4            // Prescale value (PR = 4 -> factor real = PR+1 = 5)

// hay que cambiarlo a posterior para que sea variable y seteable por el usuario
#define MAX_SAMPLES 100

// Frecuencia de perifericos
#define PCLK 25000000

#define PCLK_DAC_IN_MHZ	25 //CCLK divided by 4

/** DMA size of transfer */
volatile int dma_size = 60;
volatile int num_sample = 255;
volatile int frec = 2000;
volatile int opc = 1;

void generar_Func() {
	GPDMA_Channel_CFG_Type GPDMACfg;
	PINSEL_CFG_Type PinCfg;
	DAC_CONVERTER_CFG_Type DAC_ConverterConfigStruct;
	GPDMA_LLI_Type DMA_LLI_Struct;
	uint32_t tmp;
	uint32_t i;

	// Valores del seno normalizado
	uint32_t sin_0_to_90_16_samples[16] = { 0, 1045, 2079, 3090, 4067, 5000,
			5877, 6691, 7431, 8090, 8660, 9135, 9510, 9781, 9945, 10000\
 };
	uint32_t dac_out[num_sample];

	// Configuracion del pin DAC
	PinCfg.Funcnum = 2;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Pinnum = 26;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	switch (opc) {
	case 0:
		for (i = 0; i < num_sample; i++) {
			if (i <= 15) {
				dac_out[i] = 512 + 512 * sin_0_to_90_16_samples[i] / 10000;
				if (i == 15)
					dac_out[i] = 1023;
			} else if (i <= 30) {
				dac_out[i] = 512 + 512 * sin_0_to_90_16_samples[30 - i] / 10000;
			} else if (i <= 45) {
				dac_out[i] = 512 - 512 * sin_0_to_90_16_samples[i - 30] / 10000;
			} else {
				dac_out[i] = 512 - 512 * sin_0_to_90_16_samples[60 - i] / 10000;
			}
			dac_out[i] = (dac_out[i] << 6);
		}
		break;
	case 1:
		for (i = 0; i < num_sample; i++) {
			if (i < 32)
				dac_out[i] = 32 * i;
			else if (i == 32)
				dac_out[i] = 1023;
			else
				dac_out[i] = 32 * (num_sample - i);
			dac_out[i] = (dac_out[i] << 6);
		}
		break;
	case 2:
		for (i = 0; i < num_sample; i++) {
			dac_out[i] = (1023 / 3) * (i / 16);
			dac_out[i] = (dac_out[i] << 6);
		}
		break;
	default:
		break;
	}

	//Prepare DAC sine look up table
	/**/
	//Prepare DMA link list item structure
	DMA_LLI_Struct.SrcAddr = (uint32_t) dac_out;
	DMA_LLI_Struct.DstAddr = (uint32_t) &(LPC_DAC->DACR);
	DMA_LLI_Struct.NextLLI = (uint32_t) &DMA_LLI_Struct;
	DMA_LLI_Struct.Control = dma_size | (2 << 18) //source width 32 bit
			| (2 << 21) //dest. width 32 bit
			| (1 << 26) //source increment
			;

	// GPDMA block section
	// Initialize GPDMA controller
	GPDMA_Init();

	// Setup GPDMA channel
	// channel 0
	GPDMACfg.ChannelNum = 0;
	// Source memory
	GPDMACfg.SrcMemAddr = (uint32_t) (dac_out);
	// Destination memory - unused
	GPDMACfg.DstMemAddr = 0;
	// Transfer size
	GPDMACfg.TransferSize = dma_size;
	// Transfer width - unused
	GPDMACfg.TransferWidth = 0;
	// Transfer type
	GPDMACfg.TransferType = GPDMA_TRANSFERTYPE_M2P;
	// Source connection - unused
	GPDMACfg.SrcConn = 0;
	// Destination connection
	GPDMACfg.DstConn = GPDMA_CONN_DAC;
	// Linker List Item - unused
	GPDMACfg.DMALLI = (uint32_t) &DMA_LLI_Struct;
	// Setup channel with given parameter
	GPDMA_Setup(&GPDMACfg);

	DAC_ConverterConfigStruct.CNT_ENA = SET;
	DAC_ConverterConfigStruct.DMA_ENA = SET;
	DAC_Init(LPC_DAC);
	/* set time out for DAC*/
	tmp = (PCLK_DAC_IN_MHZ * 1000000) / (frec * num_sample);
	DAC_SetDMATimeOut(LPC_DAC, tmp);
	DAC_ConfigDAConverterControl(LPC_DAC, &DAC_ConverterConfigStruct);

	// Enable GPDMA channel 0
	GPDMA_ChannelCmd(0, ENABLE);

	while (1)
		; // se queda aca

	return;

}

// Funcion para configurar pin (led)
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

}
void configADC(void) {
	ADC_Init(LPC_ADC, 200000); // Se configura a 200KHz
	ADC_BurstCmd(LPC_ADC, DISABLE);
	ADC_ChannelCmd(LPC_ADC, 1, ENABLE); // habilito el canal 1
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
	config_timer.MatchValue = 4999; // Match de arranque, se modifica con set frec
	TIM_ConfigMatch(LPC_TIM0, &config_timer);
	TIM_Cmd(LPC_TIM0, ENABLE);
	NVIC_EnableIRQ(TIMER0_IRQn); // habilito la interrupcion
	TIM_ClearIntPending(LPC_TIM0, TIM_MR1_INT); //Limpio bandera de interrupcion del timer0
}

/* Calcula y actualiza MR1 para que el evento de match ocurra a 'frecuencia' Hz.
 - Si usas PR_TICK como PrescaleValue en TIM_Init, el factor real es (PR_TICK + 1).
 - Usa entero de 64 bits para evitar overflow en la multiplicación (frecuencia * pres).
 - Aplica redondeo al entero más cercano. */
void set_mat_frec(uint32_t frecuencia) {
	if (frecuencia == 0U)
		return; // evitar división por cero

	uint32_t pres = (uint32_t) PR_TICK_1 + 1; // PR + 1
	uint32_t denom = (uint32_t) frecuencia * pres;

	// redondeo: (PCLK + denom/2) / denom - 1
	uint32_t match = 0;
	if (denom != 0) {
		match = (uint32_t) (((uint32_t) PCLK + (denom / 2)) / denom - 1);
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
	//configADC();
	configTIMER();
	while (1) {
		generar_Func();
		//GPIO_ClearValue(0, 1 << 22);
	}
}

void TIMER0_IRQHandler(void) {
	if (TIM_GetIntStatus(LPC_TIM0, TIM_MR1_INT) == SET) {

		ADC_StartCmd(LPC_ADC, ADC_START_NOW);

		while (!(ADC_ChannelGetStatus(LPC_ADC, ADC_CHANNEL_1, ADC_DATA_DONE)))
			; // espero a que termine la conversion

		uint16_t raw = ADC_ChannelGetData(LPC_ADC, ADC_CHANNEL_1); // 12-bit típico
		// mapear 12-bit ADC -> 10-bit DAC (simple truncamiento)
		uint32_t dac_val = (uint32_t) (raw >> 2) & 0x3FFU;
		DAC_UpdateValue(LPC_DAC, dac_val);

		//TIM_Cmd(LPC_TIM0, ENABLE); //inicia el timer
		TIM_ClearIntPending(LPC_TIM0, TIM_MR0_INT); //Limpio bandera de interrupcion del timer0
	}
}

