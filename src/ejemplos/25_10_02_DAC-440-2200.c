/**
 * @file    25_10_02_DAC-440-2200.c
 * @brief   Genera una señal senoidal de entre 440Hz y 2200Hz
 *
 * Utiliza el ADC para leer el voltaje de un potenciometro conectado a P0.23 (ADC0)
 * El valor del ADC (0-4095) se mapea a una frecuencia entre 440Hz y 2200Hz
 * Se utiliza el DAC para generar una señal en el pin P0.26
 * La señal es una onda senoidal almacenada en un array
 * Se utiliza el Timer0 para generar interrupciones a la frecuencia deseada
 * En cada interrupción se actualiza el valor del DAC con el siguiente valor de la onda
 * Tambien se utiliza el Timer1 para generar interrupciones cada 100ms
 * En cada interrupción se lee el valor del ADC y se actualiza la frecuencia del Timer0
 */

#include "E:\Electronica-Digital-3\common\cmsis\CMSISv2p00_LPC17xx\Drivers\inc\lpc17xx_adc.h"
#include "E:\Electronica-Digital-3\common\cmsis\CMSISv2p00_LPC17xx\Drivers\inc\lpc17xx_dac.h"
#include "E:\Electronica-Digital-3\common\cmsis\CMSISv2p00_LPC17xx\Drivers\inc\lpc17xx_pinsel.h"
#include "E:\Electronica-Digital-3\common\cmsis\CMSISv2p00_LPC17xx\Drivers\inc\lpc17xx_timer.h"

/* ADC sample frequency */
#define ADC_CONVERSION_RATE 200000
#define N_SAMPLES       454
#define ADC_BUFFER      32

/* Array de valores para un periodo completo de la onda senoidal */
volatile uint16_t sine_bank[N_SAMPLES] = {
 512, 519, 526, 533, 540, 547, 554, 561, 568, 575, 582, 589,
 596, 603, 610, 617, 624, 631, 638, 644, 651, 658, 665, 672,
 678, 685, 692, 698, 705, 711, 718, 724, 731, 737, 743, 750,
 756, 762, 768, 774, 780, 786, 792, 798, 804, 810, 816, 821,
 827, 832, 838, 843, 849, 854, 859, 864, 869, 874, 879, 884,
 889, 894, 898, 903, 908, 912, 916, 921, 925, 929, 933, 937,
 941, 945, 948, 952, 956, 959, 962, 966, 969, 972, 975, 978,
 981, 984, 986, 989, 991, 994, 996, 998, 1001, 1003, 1004, 1006,
 1008, 1010, 1011, 1013, 1014, 1015, 1017, 1018, 1019, 1019, 1020, 1021,
 1022, 1022, 1022, 1023, 1023, 1023, 1023, 1023, 1023, 1022, 1022, 1022,
 1021, 1020, 1019, 1019, 1018, 1017, 1015, 1014, 1013, 1011, 1010, 1008,
 1006, 1004, 1003, 1001, 998, 996, 994, 991, 989, 986, 984, 981,
 978, 975, 972, 969, 966, 962, 959, 956, 952, 948, 945, 941,
 937, 933, 929, 925, 921, 916, 912, 908, 903, 898, 894, 889,
 884, 879, 874, 869, 864, 859, 854, 849, 843, 838, 832, 827,
 821, 816, 810, 804, 798, 792, 786, 780, 774, 768, 762, 756,
 750, 743, 737, 731, 724, 718, 711, 705, 698, 692, 685, 678,
 672, 665, 658, 651, 644, 638, 631, 624, 617, 610, 603, 596,
 589, 582, 575, 568, 561, 554, 547, 540, 533, 526, 519, 512,
 504, 497, 490, 483, 476, 469, 462, 455, 448, 441, 434, 427,
 420, 413, 406, 399, 392, 385, 379, 372, 365, 358, 351, 345,
 338, 331, 325, 318, 312, 305, 299, 292, 286, 280, 273, 267,
 261, 255, 249, 243, 237, 231, 225, 219, 213, 207, 202, 196,
 191, 185, 180, 174, 169, 164, 159, 154, 149, 144, 139, 134,
 129, 125, 120, 115, 111, 107, 102, 98, 94, 90, 86, 82,
 78, 75, 71, 67, 64, 61, 57, 54, 51, 48, 45, 42,
 39, 37, 34, 32, 29, 27, 25, 22, 20, 19, 17, 15,
 13, 12, 10, 9, 8, 6, 5, 4, 4, 3, 2, 1,
 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 2,
 3, 4, 4, 5, 6, 8, 9, 10, 12, 13, 15, 17,
 19, 20, 22, 25, 27, 29, 32, 34, 37, 39, 42, 45,
 48, 51, 54, 57, 61, 64, 67, 71, 75, 78, 82, 86,
 90, 94, 98, 102, 107, 111, 115, 120, 125, 129, 134, 139,
 144, 149, 154, 159, 164, 169, 174, 180, 185, 191, 196, 202,
 207, 213, 219, 225, 231, 237, 243, 249, 255, 261, 267, 273,
 280, 286, 292, 299, 305, 312, 318, 325, 331, 338, 345, 351,
 358, 365, 372, 379, 385, 392, 399, 406, 413, 420, 427, 434,
 441, 448, 455, 462, 469, 476, 483, 490, 497, 504 };
volatile uint16_t sine_index = 0;

volatile uint16_t adcBuffer[ADC_BUFFER];
volatile uint32_t adcSum = 0;
volatile uint8_t adcIdx = 0;
volatile uint16_t adcAvg = 0;


// Periodo del Timer0 (ticks)
volatile uint32_t delayTicks = 100;


/**
 * Configura el ADC para leer el valor del potenciometro en P0.23 (ADC0)
 */
void configADC(void) {
    PINSEL_CFG_Type PinCfg;

    /* Initialize ADC pins */
    PinCfg.Funcnum = PINSEL_FUNC_1;
    PinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
    PinCfg.Pinmode = PINSEL_PINMODE_PULLUP;
    PinCfg.Portnum = PINSEL_PORT_0;

    // P0.23 para ADC canal 0
    PinCfg.Pinnum = PINSEL_PIN_23;
    PINSEL_ConfigPin(&PinCfg);

    /* ADC Init */
    ADC_Init(LPC_ADC, ADC_CONVERSION_RATE);
    ADC_ChannelCmd(LPC_ADC, 0, ENABLE);
    ADC_BurstCmd(LPC_ADC, ENABLE);  // Burst mode 
}
/**
 * Configura el DAC para generar la señal en P0.26 (DAC)
 */
void configDAC(void) {
    PINSEL_CFG_Type PinCfg;
    /* Configuration for DAC */
    PinCfg.Funcnum = 2;
    PinCfg.Pinnum = 26;
    PinCfg.Portnum = PINSEL_PORT_0;
    PINSEL_ConfigPin(&PinCfg);
    DAC_Init(LPC_DAC);
}
/*
 * Actualiza el valor promedio del ADC, para suavizar la lectura del potenciometro
 */
void actualizarAdcAvg(void) {
    uint16_t adcNew = (LPC_ADC -> ADDR0 >>4) & 0xFFF;
    adcSum -= adcBuffer[adcIdx];    // Le resto el valor más viejo a la sumatoria
    adcBuffer[adcIdx] = adcNew;     // Reemplazo por el valor nuevo
    adcSum += adcNew;               //
    adcIdx = (adcIdx + 1) % ADC_BUFFER;
    adcAvg = adcSum / ADC_BUFFER;
}

void configTimer0(void) {
    TIM_TIMERCFG_Type timerCfg;
    TIM_MATCHCFG_Type matchCfg;

    TIM_ConfigStructInit(TIM_TIMER_MODE, &timerCfg);
    TIM_Init(LPC_TIM0, TIM_TIMER_MODE, &timerCfg);

    matchCfg.MatchChannel = 0;
    matchCfg.IntOnMatch = ENABLE;
    matchCfg.StopOnMatch = DISABLE;
    matchCfg.ResetOnMatch = ENABLE;
    matchCfg.ExtMatchOutputType = TIM_EXTMATCH_NOTHING;
    matchCfg.MatchValue = delayTicks;   // Variable segun la frecuencia deseafa
    TIM_ConfigMatch(LPC_TIM0, &matchCfg);

    NVIC_EnableIRQ(TIMER0_IRQn);
    TIM_Cmd(LPC_TIM0, ENABLE);
}

void configTimer1(void) {
    TIM_TIMERCFG_Type timerCfg;
    TIM_MATCHCFG_Type matchCfg;

    TIM_ConfigStructInit(TIM_TIMER_MODE, &timerCfg);
    TIM_Init(LPC_TIM1, TIM_TIMER_MODE, &timerCfg);

    matchCfg.MatchChannel = 0;
    matchCfg.IntOnMatch = ENABLE;
    matchCfg.StopOnMatch = DISABLE;
    matchCfg.ResetOnMatch = ENABLE;
    matchCfg.ExtMatchOutputType = TIM_EXTMATCH_NOTHING;
    matchCfg.MatchValue = 50000;   // cada 0.5 ms
    TIM_ConfigMatch(LPC_TIM1, &matchCfg);

    NVIC_EnableIRQ(TIMER1_IRQn);
    TIM_Cmd(LPC_TIM1, ENABLE);
}


int main(void) {

    // Inicializar buffer ADC para promedio
    for (int i = 0; i < ADC_BUFFER; i++) adcBuffer[i] = 0;
    adcSum = 0;
    adcIdx = 0;

    configDAC();
    configADC();
    configTimer0();
    configTimer1();

    while(1){}
}
/* Esta rutina va muy justa de tiempo */
void TIMER0_IRQHandler(void) {
    LPC_TIM0->IR = 1;  // limpiar flag

    // Actualizar DAC
    LPC_DAC->DACR = (sine_bank[sine_index] & 0x3FF) << 6;
    sine_index = (sine_index + 1) % N_SAMPLES;

    // Avanzar MR0 con delay calculado
    LPC_TIM0->MR0 += delayTicks; // sugerencia de GPT
}

void TIMER1_IRQHandler(void) {
    LPC_TIM1->IR = 1;

    // Actualizar promedio ADC
    updateADCAverage();

    // Calcular nuevo delayTicks según adcAvg
    // Escala lineal: 0 -> 5 µs, 4095 -> 1 µs
    delayTicks = 500 - ((adcAvg * 400) / 4095);  // 1 tick = 0.01 µs
    if(delayTicks < 100) delayTicks = 100;  // 1 µs mínimo por las dudas
}