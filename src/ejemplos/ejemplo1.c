// Ejemplo 2
#define DMA_SIZE 60
#define NUM_SINE_SAMPLE 255
#define SINE_FREQ_IN_HZ 20000
#define PCLK_DAC_IN_MHZ 25 // CCLK divided by 4

GPDMA_Channel_CFG_Type GPDMACfg;

int c_entry(void)
{
    PINSEL_CFG_Type PinCfg;
    DAC_CONVERTER_CFG_Type DAC_ConverterConfigStruct;
    GPDMA_LLI_Type DMA_LLI_Struct;
    uint32_t tmp;
    uint32_t i;
    uint32_t sin_0_to_90_16_samples[16] = {
        0, 1045, 2079, 3090, 4067,
        5000, 5877, 6691, 7431, 8090,
        8660, 9135, 9510, 9781, 9945, 10000};
    uint32_t dac_sine_lut[NUM_SINE_SAMPLE];
    PinCfg.Funcnum = 2;
    PinCfg.OpenDrain = 0;
    PinCfg.Pinmode = 0;
    PinCfg.Pinnum = 26;
    PinCfg.Portnum = 0;
    PINSEL_ConfigPin(&PinCfg);

    // Prepare DAC sine look up table
    for (i = 0; i < NUM_SINE_SAMPLE; i++)
    {
        if (i <= 15)
        {
            dac_sine_lut[i] = 512 + 512 * sin_0_to_90_16_samples[i] / 10000;
            if (i == 15)
                dac_sine_lut[i] = 1023;
        }
        else if (i <= 30)
        {
            dac_sine_lut[i] = 512 + 512 * sin_0_to_90_16_samples[30 - i] / 10000;
        }
        else if (i <= 45)
        {
            dac_sine_lut[i] = 512 - 512 * sin_0_to_90_16_samples[i - 30] / 10000;
        }
        else
        {
            dac_sine_lut[i] = 512 - 512 * sin_0_to_90_16_samples[60 - i] / 10000;
        }
        dac_sine_lut[i] = (dac_sine_lut[i] << 6);
    }
    // Prepare DMA link list item structure
    DMA_LLI_Struct.SrcAddr = (uint32_t)dac_sine_lut;
    DMA_LLI_Struct.DstAddr = (uint32_t)&(LPC_DAC->DACR);
    DMA_LLI_Struct.NextLLI = (uint32_t)&DMA_LLI_Struct;
    DMA_LLI_Struct.Control = DMA_SIZE | (2 << 18) // source width 32 bit
                             | (2 << 21)          // dest. width 32 bit
                             | (1 << 26)          // source increment
        ;

    // GPDMA block section --------------------------------------------
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

    DAC_ConverterConfigStruct.CNT_ENA = SET;
    DAC_ConverterConfigStruct.DMA_ENA = SET;
    DAC_Init(LPC_DAC);
    // set time out for DAC
    tmp = (PCLK_DAC_IN_MHZ * 1000000) / (SINE_FREQ_IN_HZ * NUM_SINE_SAMPLE);
    DAC_SetDMATimeOut(LPC_DAC, tmp);
    DAC_ConfigDAConverterControl(LPC_DAC, &DAC_ConverterConfigStruct);

    // Enable GPDMA channel 0
    GPDMA_ChannelCmd(0, ENABLE);

    while (1)
        ;

    return 1;
}
// With ARM and GHS toolsets, the entry point is main() - this will
//   allow the linker to generate wrapper code to setup stacks, allocate
//   heap area, and initialize and copy code and data segments. For GNU
//   toolsets, the entry point is through __start() in the crt0_gnu.asm
//   file, and that startup code will setup stacks and data
int main(void)
{
    return c_entry();
}