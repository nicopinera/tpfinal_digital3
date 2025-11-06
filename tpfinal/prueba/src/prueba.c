#include "lpc17xx_dac.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpdma.h"
#include "debug_frmwrk.h"
#include <math.h>

// Wave Generation
// DMA size of transfer
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

#define MAX_MUESTRAS 18
#define M_PI 3.14159

void generate_sin_0_to_90_16_samples(uint32_t out[]) {
	const double scale = 10000.0;
	const int steps = MAX_MUESTRAS - 1; // 16 muestras => 15 intervalos
	for (int i = 0; i < MAX_MUESTRAS; ++i) {
		double angle_deg = (90.0 * i) / steps; // 0,6,12,...,90
		double rad = angle_deg * M_PI / 180.0;
		double v = sin(rad) * scale;
		out[i] = (uint32_t)v;
	}
}

void generar_triangulo(uint32_t out[])
{
    int paso = (int) 1024/NUM_SAMPLE;
    for (int i = 0; i < NUM_SAMPLE; i++)
    {
        if (i < (NUM_SAMPLE/2))
            out[i] = paso * i;
        else if (i == (NUM_SAMPLE/2))
            out[i] = 1023;
        else
            out[i] = paso * (NUM_SAMPLE - i);
        out[i] = (out[i] << 6);
    }
}

//MAIN FUNCTION
int c_entry(void)
{
	PINSEL_CFG_Type PinCfg;
	DAC_CONVERTER_CFG_Type DAC_ConverterConfigStruct;
	GPDMA_Channel_CFG_Type GPDMACfg;
	GPDMA_LLI_Type DMA_LLI_Struct;
	uint32_t tmp;
	uint8_t i,option;

	// Valores del seno escalado a 10000
	uint32_t sin_0_to_90_16_samples[MAX_MUESTRAS];
	generate_sin_0_to_90_16_samples(sin_0_to_90_16_samples);
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
				if(i<=(NUM_SAMPLE_SINE* 1/4))
				{
					dac_lut[i] = 512 + 512*sin_0_to_90_16_samples[i]/10000;
					if(i==15) dac_lut[i]= 1023;
				}
				else if(i<=(NUM_SAMPLE_SINE* 1/2))
				{
					dac_lut[i] = 512 + 512*sin_0_to_90_16_samples[30-i]/10000;
				}
				else if(i<=(NUM_SAMPLE_SINE* 3/4))
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
			generar_triangulo(dac_lut);
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

		// Se tranfieren diferentes cantidades en funcion de la opcion
		DMA_LLI_Struct.Control= ((option==DAC_GENERATE_SINE)?DMA_SIZE_SINE:DMA_SIZE)
								| (2<<18) //source width 32 bit
								| (2<<21) //dest. width 32 bit
								| (1<<26) //source increment
								;


		// GPDMA block section
		// Initialize GPDMA controller
		GPDMA_Init();

		// Setup GPDMA channel
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


		// Configuracion del DAC
		DAC_ConverterConfigStruct.CNT_ENA =SET;
		DAC_ConverterConfigStruct.DMA_ENA = SET;
		DAC_Init(LPC_DAC);

		// set time out for DAC
		tmp = (PCLK_DAC_IN_MHZ*1000000)/(SIGNAL_FREQ_IN_HZ*((option==DAC_GENERATE_SINE)?NUM_SAMPLE_SINE:NUM_SAMPLE));
		DAC_SetDMATimeOut(LPC_DAC,tmp);
		DAC_ConfigDAConverterControl(LPC_DAC, &DAC_ConverterConfigStruct);

		// Enable GPDMA channel 0
		GPDMA_ChannelCmd(0, ENABLE);

		while(1);

		// Disable GPDMA channel 0
		//GPDMA_ChannelCmd(0, DISABLE); -> aca se frena el canal, capaz por eso no funcionaba

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

