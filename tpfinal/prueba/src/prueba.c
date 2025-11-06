/**********************************************************************
* $Id$		dac_wave_generate.c				2010-07-16
*//**
* @file		dac_wave_generate.c
* @brief	This example describes how to use DAC to generate a sine wave,
 * 			triangle wave or escalator wave
* @version	1.0
* @date		16. July. 2010
* @author	NXP MCU SW Application Team
*
* Copyright(C) 2010, NXP Semiconductor
* All rights reserved.
*
***********************************************************************
* Software that is described herein is for illustrative purposes only
* which provides customers with programming information regarding the
* products. This software is supplied "AS IS" without any warranties.
* NXP Semiconductors assumes no responsibility or liability for the
* use of the software, conveys no license or title under any patent,
* copyright, or mask work right to the product. NXP Semiconductors
* reserves the right to make changes in the software without
* notification. NXP Semiconductors also make no representation or
* warranty that such application will be suitable for the specified
* use without further testing or modification.
**********************************************************************/
#include "lpc17xx_dac.h"
//#include "lpc17xx_libcfg.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpdma.h"
#include "debug_frmwrk.h"

/* Example group ----------------------------------------------------------- */
/** @defgroup DAC_WaveGenerate		WaveGenerate
 * @ingroup DAC_Examples
 * @{
 */

/************************** PRIVATE MACROS *************************/
/** DMA size of transfer */
#define DMA_SIZE_SINE		60
#define NUM_SAMPLE_SINE		60
#define DMA_SIZE			64
#define NUM_SAMPLE			64

#define SIGNAL_FREQ_IN_HZ	60
#define PCLK_DAC_IN_MHZ	25 //CCLK divided by 4

#define DAC_GENERATE_SINE		1
#define DAC_GENERATE_TRIANGLE	2
#define DAC_GENERATE_ESCALATOR	3
#define DAC_GENERATE_NONE		0

/*-------------------------MAIN FUNCTION------------------------------*/
/*********************************************************************//**
 * @brief		c_entry: Main DAC program body
 * @param[in]	None
 * @return 		int
 **********************************************************************/
int c_entry(void)
{
	PINSEL_CFG_Type PinCfg;
	DAC_CONVERTER_CFG_Type DAC_ConverterConfigStruct;
	GPDMA_Channel_CFG_Type GPDMACfg;
	GPDMA_LLI_Type DMA_LLI_Struct;
	uint32_t tmp;
	uint8_t i,option;
	uint32_t sin_0_to_90_16_samples[16]={\
			0,1045,2079,3090,4067,\
			5000,5877,6691,7431,8090,\
			8660,9135,9510,9781,9945,10000\
	};
	uint32_t dac_lut[NUM_SAMPLE];

	/*
	 * Init DAC pin connect
	 * AOUT on P0.26
	 */
	PinCfg.Funcnum = 2;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Pinnum = 26;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);

	while(1)
	{
		option = DAC_GENERATE_SINE;

		//Prepare DAC look up table
		switch(option)
		{
		case DAC_GENERATE_SINE:
			for(i=0;i<NUM_SAMPLE_SINE;i++)
			{
				if(i<=15)
				{
					dac_lut[i] = 512 + 512*sin_0_to_90_16_samples[i]/10000;
					if(i==15) dac_lut[i]= 1023;
				}
				else if(i<=30)
				{
					dac_lut[i] = 512 + 512*sin_0_to_90_16_samples[30-i]/10000;
				}
				else if(i<=45)
				{
					dac_lut[i] = 512 - 512*sin_0_to_90_16_samples[i-30]/10000;
				}
				else
				{
					dac_lut[i] = 512 - 512*sin_0_to_90_16_samples[60-i]/10000;
				}
				dac_lut[i] = (dac_lut[i]<<6);
			}
			break;
		case DAC_GENERATE_TRIANGLE:
			for(i=0;i<NUM_SAMPLE;i++)
			{
				if(i<32) dac_lut[i]= 32*i;
				else if (i==32) dac_lut[i]= 1023;
				else dac_lut[i] = 32*(NUM_SAMPLE-i);
				dac_lut[i] = (dac_lut[i]<<6);
			}
			break;
		case DAC_GENERATE_ESCALATOR:
			for(i=0;i<NUM_SAMPLE;i++)
			{
				dac_lut[i] = (1023/3)*(i/16);
				dac_lut[i] = (dac_lut[i]<<6);
			}
			break;
		default: break;
		}

		//Prepare DMA link list item structure
		DMA_LLI_Struct.SrcAddr= (uint32_t)dac_lut;
		DMA_LLI_Struct.DstAddr= (uint32_t)&(LPC_DAC->DACR);
		DMA_LLI_Struct.NextLLI= (uint32_t)&DMA_LLI_Struct;
		DMA_LLI_Struct.Control= ((option==DAC_GENERATE_SINE)?DMA_SIZE_SINE:DMA_SIZE)
								| (2<<18) //source width 32 bit
								| (2<<21) //dest. width 32 bit
								| (1<<26) //source increment
								;


		/* GPDMA block section -------------------------------------------- */
		/* Initialize GPDMA controller */
		GPDMA_Init();

		// Setup GPDMA channel --------------------------------
		// channel 0
		GPDMACfg.ChannelNum = 0;
		// Source memory
		GPDMACfg.SrcMemAddr = (uint32_t)(dac_lut);
		// Destination memory - unused
		GPDMACfg.DstMemAddr = 0;
		// Transfer size
		GPDMACfg.TransferSize = ((option==DAC_GENERATE_SINE)?DMA_SIZE_SINE:DMA_SIZE);
		// Transfer width - unused
		GPDMACfg.TransferWidth = 0;
		// Transfer type
		GPDMACfg.TransferType = GPDMA_TRANSFERTYPE_M2P;
		// Source connection - unused
		GPDMACfg.SrcConn = 0;
		// Destination connection
		GPDMACfg.DstConn = GPDMA_CONN_DAC;
		// Linker List Item - unused
		GPDMACfg.DMALLI = (uint32_t)&DMA_LLI_Struct;
		// Setup channel with given parameter
		GPDMA_Setup(&GPDMACfg);

		DAC_ConverterConfigStruct.CNT_ENA =SET;
		DAC_ConverterConfigStruct.DMA_ENA = SET;
		DAC_Init(LPC_DAC);
		/* set time out for DAC*/
		tmp = (PCLK_DAC_IN_MHZ*1000000)/(SIGNAL_FREQ_IN_HZ*((option==DAC_GENERATE_SINE)?NUM_SAMPLE_SINE:NUM_SAMPLE));
		DAC_SetDMATimeOut(LPC_DAC,tmp);
		DAC_ConfigDAConverterControl(LPC_DAC, &DAC_ConverterConfigStruct);

		// Enable GPDMA channel 0
		GPDMA_ChannelCmd(0, ENABLE);

		// Disable GPDMA channel 0
		GPDMA_ChannelCmd(0, DISABLE);

	}
	return 1;
}
/* With ARM and GHS toolsets, the entry point is main() - this will
   allow the linker to generate wrapper code to setup stacks, allocate
   heap area, and initialize and copy code and data segments. For GNU
   toolsets, the entry point is through __start() in the crt0_gnu.asm
   file, and that startup code will setup stacks and data */
int main(void)
{
    return c_entry();
}

#ifdef  DEBUG
/*******************************************************************************
* @brief		Reports the name of the source file and the source line number
* 				where the CHECK_PARAM error has occurred.
* @param[in]	file Pointer to the source file name
* @param[in]    line assert_param error line source number
* @return		None
*******************************************************************************/
void check_failed(uint8_t *file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	while(1);
}
#endif
/*
 * @}
 */

/*
#define DMA_SIZE		60
#define NUM_SINE_SAMPLE	255
#define SINE_FREQ_IN_HZ	20000
#define PCLK_DAC_IN_MHZ	25 //CCLK divided by 4

GPDMA_Channel_CFG_Type GPDMACfg;

 * @brief		c_entry: Main DAC program body
 * @param[in]	None
 * @return 		int
int c_entry(void)
{
	PINSEL_CFG_Type PinCfg;
	DAC_CONVERTER_CFG_Type DAC_ConverterConfigStruct;
	GPDMA_LLI_Type DMA_LLI_Struct;
	uint32_t tmp;
	uint32_t i;
	uint32_t sin_0_to_90_16_samples[16]={\
			0,1045,2079,3090,4067,\
			5000,5877,6691,7431,8090,\
			8660,9135,9510,9781,9945,10000\
	};
	uint32_t dac_sine_lut[NUM_SINE_SAMPLE];
	PinCfg.Funcnum = 2;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Pinnum = 26;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);

	//Prepare DAC sine look up table
	for(i=0;i<NUM_SINE_SAMPLE;i++)
	{
		if(i<=15)
		{
			dac_sine_lut[i] = 512 + 512*sin_0_to_90_16_samples[i]/10000;
			if(i==15) dac_sine_lut[i]= 1023;
		}
		else if(i<=30)
		{
			dac_sine_lut[i] = 512 + 512*sin_0_to_90_16_samples[30-i]/10000;
		}
		else if(i<=45)
		{
			dac_sine_lut[i] = 512 - 512*sin_0_to_90_16_samples[i-30]/10000;
		}
		else
		{
			dac_sine_lut[i] = 512 - 512*sin_0_to_90_16_samples[60-i]/10000;
		}
		dac_sine_lut[i] = (dac_sine_lut[i]<<6);
	}
	//Prepare DMA link list item structure
	DMA_LLI_Struct.SrcAddr= (uint32_t)dac_sine_lut;
	DMA_LLI_Struct.DstAddr= (uint32_t)&(LPC_DAC->DACR);
	DMA_LLI_Struct.NextLLI= (uint32_t)&DMA_LLI_Struct;
	DMA_LLI_Struct.Control= DMA_SIZE
							| (2<<18) //source width 32 bit
							| (2<<21) //dest. width 32 bit
							| (1<<26) //source increment
							;

	/* GPDMA block section --------------------------------------------
	GPDMA_Init();

	// Setup GPDMA channel --------------------------------
	// channel 0
	GPDMACfg.ChannelNum = 0;
	// Source memory
	GPDMACfg.SrcMemAddr = (uint32_t)(dac_sine_lut);
	// Destination memory - unused
	GPDMACfg.DstMemAddr = 0;
	// Transfer size
	GPDMACfg.TransferSize = DMA_SIZE;
	// Transfer width - unused
	GPDMACfg.TransferWidth = 0;
	// Transfer type
	GPDMACfg.TransferType = GPDMA_TRANSFERTYPE_M2P;
	// Source connection - unused
	GPDMACfg.SrcConn = 0;
	// Destination connection
	GPDMACfg.DstConn = GPDMA_CONN_DAC;
	// Linker List Item - unused
	GPDMACfg.DMALLI = (uint32_t)&DMA_LLI_Struct;
	// Setup channel with given parameter
	GPDMA_Setup(&GPDMACfg);

	DAC_ConverterConfigStruct.CNT_ENA =SET;
	DAC_ConverterConfigStruct.DMA_ENA = SET;
	DAC_Init(LPC_DAC);
	/* set time out for DAC
	tmp = (PCLK_DAC_IN_MHZ*1000000)/(SINE_FREQ_IN_HZ*NUM_SINE_SAMPLE);
	DAC_SetDMATimeOut(LPC_DAC,tmp);
	DAC_ConfigDAConverterControl(LPC_DAC, &DAC_ConverterConfigStruct);

	// Enable GPDMA channel 0
	GPDMA_ChannelCmd(0, ENABLE);

	while (1);

	return 1;
}
/* With ARM and GHS toolsets, the entry point is main() - this will
   allow the linker to generate wrapper code to setup stacks, allocate
   heap area, and initialize and copy code and data segments. For GNU
   toolsets, the entry point is through __start() in the crt0_gnu.asm
   file, and that startup code will setup stacks and data
int main(void)
{
    return c_entry();
}

#ifdef  DEBUG
/*******************************************************************************
* @brief		Reports the name of the source file and the source line number
* 				where the CHECK_PARAM error has occurred.
* @param[in]	file Pointer to the source file name
* @param[in]    line assert_param error line source number
* @return		None
******************************************************************************
void check_failed(uint8_t *file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop
	while(1);
}
#endif
/*
 * @}
 */

/*
#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif

#include "lpc17xx_gpio.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_nvic.h"
#include "lpc17xx_uart.h"
#include "string.h"
#include "stdio.h"


volatile uint32_t adc_value = 0; //Aqui se guardara el valor de la conversion
static const uint8_t CHANNEL_ADC = 7;
static const uint32_t RATE_ADC = 200000; //200Khz
static const uint8_t MATCH_CHANNEL = 0;
static const uint32_t prescale_value = 24;
static const uint32_t match_value = 5000000;

void uart3_SendADC(uint32_t value);

void TIMER0_IRQHandler(void){
    if(TIM_GetIntStatus(LPC_TIM0, TIM_MR0_INT) == SET){

    	ADC_StartCmd(LPC_ADC, ADC_START_NOW);

    	while (!(ADC_ChannelGetStatus(LPC_ADC, ADC_CHANNEL_7, ADC_DATA_DONE)));

    	adc_value = ADC_ChannelGetData(LPC_ADC, ADC_CHANNEL_7);
        //uart3_SendADC(adc_value);
    	// Ejemplo: LED encendido si potenciómetro > 50%
    	if(adc_value > 2048){
    	    LPC_GPIO0->FIOCLR = (1 << 22); // Enciende LED
    	} else {
    	    LPC_GPIO0->FIOSET = (1 << 22); // Apaga LED
    	}

    	//TIM_Cmd(LPC_TIM0, ENABLE); //inicia el timer
    	TIM_ClearIntPending(LPC_TIM0,TIM_MR0_INT); //Limpio bandera de interrupcion del timer0
    }
}


void config_LED(void){
    //Configuro el pin P0.22 como salida GPIO para el LED
    PINSEL_CFG_Type pinsel_led;
    pinsel_led.Portnum = 0;
    pinsel_led.Pinnum = 22;
    pinsel_led.Funcnum = 0; //GPIO
    pinsel_led.Pinmode = 1;
    pinsel_led.OpenDrain = 0;
    PINSEL_ConfigPin(&pinsel_led);

    //Configuro el pin como salida
    LPC_GPIO0->FIODIR |= (1 << 22);
    LPC_GPIO0->FIOSET |= (1 << 22);
}


void config_ADC(void){

    //Se configura el ADC7 para la entrada pin P0.2
    PINSEL_CFG_Type pinsel_adc;
    pinsel_adc.Portnum = 0;
    pinsel_adc.Pinnum = 2;
    pinsel_adc.Funcnum = 2;
    pinsel_adc.Pinmode = 1;
    PINSEL_ConfigPin(&pinsel_adc);


    ADC_Init(LPC_ADC, RATE_ADC);
    ADC_ChannelCmd(LPC_ADC, CHANNEL_ADC, ENABLE);
    ADC_BurstCmd(LPC_ADC, DISABLE);
}

void config_TIMER(void){
    //Configuramos el timer
    TIM_TIMERCFG_Type struct_timer;
    TIM_MATCHCFG_Type struct_match;
    struct_timer.PrescaleOption = TIM_PRESCALE_TICKVAL;
    struct_timer.PrescaleValue = prescale_value;

    //Inicializo el Timer 0
    TIM_Init(LPC_TIM0, TIM_TIMER_MODE, &struct_timer);

    //Configuramos el match para que haga match cada 5 segundos

    struct_match.MatchChannel = MATCH_CHANNEL;
    struct_match.IntOnMatch = ENABLE;
    struct_match.ResetOnMatch = ENABLE;
    struct_match.StopOnMatch = DISABLE;
    struct_match.ExtMatchOutputType = TIM_EXTMATCH_NOTHING;
    struct_match.MatchValue = match_value; //5 segundos

    //Inicializo el Match 0 del Timer
    TIM_ConfigMatch(LPC_TIM0, &struct_match);
    TIM_ResetCounter(LPC_TIM0);
    //Habilito el Timer0
    TIM_Cmd(LPC_TIM0, ENABLE);

    TIM_ClearIntPending(LPC_TIM0,TIM_MR0_INT); //Limpio bandera de interrupcion del timer0
    NVIC_SetPriority(TIMER0_IRQn, 1);
    NVIC_EnableIRQ(TIMER0_IRQn);
}

/*
void config_Uart(uint32_t baud){
    PINSEL_CFG_Type uart_config;
    uart_config.Portnum = 0;
    uart_config.Pinnum = 0;
    uart_config.Funcnum = 2;
    uart_config.Pinmode = 1;
    uart_config.OpenDrain = 0;
    PINSEL_ConfigPin(&uart_config);

    UART_CFG_Type uart_cfg;
    UART_ConfigStructInit(&uart_cfg);
    uart_cfg.Baud_rate = baud;
    UART_Init(LPC_UART3, &uart_cfg);

    UART_FIFO_CFG_Type uart_fifo;
    UART_FIFOConfigStructInit(&uart_fifo);
    UART_FIFOConfig(LPC_UART3, &uart_fifo);

    UART_TxCmd(LPC_UART3, ENABLE);
}
*/

/*
void uart3_SendADC(uint32_t value){
    char buf[32];
    int n = sprintf(buf, "ADC=%lu\r\n", (unsigned long)value);
    UART_Send(LPC_UART3, (uint8_t*)buf, (uint32_t)n, BLOCKING);
}


int main(void)
{
    config_LED();
    config_ADC();
    config_TIMER();
    //config_Uart(9600);
    while(1){
    }

    return 0;
}
*/
