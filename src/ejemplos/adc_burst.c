#define ADC_CHANNEL     2               // Usaremos AD0.2 (P0.25)
#define DMA_CHANNEL     0
#define ADC_NUM_SAMPLES 256

// Buffer circular
static uint16_t adc_buffer[ADC_NUM_SAMPLES];

static DMA_CHDESC_T dmaDesc;

static void setupADC(void) {
    ADC_CLOCK_SETUP_T adcSetup;

    Chip_ADC_Init(LPC_ADC, &adcSetup);

    // PCLK a 25 MHz, ADC clock máx = 13 MHz
    adcSetup.adcRate = 200000;         // 200 ks/s máx (seguro)
    adcSetup.bitsAccuracy = ADC_10BITS;
    adcSetup.burstMode = true;
    Chip_ADC_SetSampleRate(LPC_ADC, &adcSetup, adcSetup.adcRate);

    // Configuro canal 2
    Chip_IOCON_PinMux(LPC_IOCON, 0, 25, IOCON_MODE_INACT, IOCON_FUNC1);
    Chip_ADC_EnableChannel(LPC_ADC, ADC_CH2, ENABLE);

    // Burst Mode ON
    Chip_ADC_SetBurstCmd(LPC_ADC, ENABLE);
}

/* --- Configuración DMA circular --- */
static void setupDMA(void) {
    Chip_GPDMA_Init(LPC_GPDMA);

    // Limpia configuración previa
    Chip_GPDMA_Stop(LPC_GPDMA, DMA_CHANNEL);

    // Configuración del descriptor
    dmaDesc.src = (uint32_t) &LPC_ADC->GDR;            // Fuente: registro ADC
    dmaDesc.dest = (uint32_t) adc_buffer;               // Destino: buffer RAM
    dmaDesc.next = (uint32_t) &dmaDesc;                 // Circular
    dmaDesc.ctrl = DMA_XFERCFG_CFGVALID
                 | DMA_XFERCFG_SETINTA
                 | DMA_XFERCFG_WIDTH_HALFWORD
                 | DMA_XFERCFG_SRCINC_NONE
                 | DMA_XFERCFG_DSTINC_1
                 | (ADC_NUM_SAMPLES & 0x3FF);

    // Configuro canal
    Chip_GPDMA_Transfer(LPC_GPDMA,
                        DMA_CHANNEL,
                        GPDMA_CONN_ADC,
                        (uint32_t) adc_buffer,
                        GPDMA_TRANSFERTYPE_P2M_CONTROLLER_DMA,
                        ADC_NUM_SAMPLES);
}

int main(void) {
    SystemCoreClockUpdate();
    setupADC();
    setupDMA();

    // Arranca el DMA
    Chip_GPDMA_ChannelCmd(LPC_GPDMA, DMA_CHANNEL, ENABLE);

    while (1) {
        // El buffer se llena automáticamente
        // Si se desea procesar cada X muestras:
        // leer puntero de DMA o usar interrupción de fin de bloque
        __WFI(); // Ahorra energía
    }
}