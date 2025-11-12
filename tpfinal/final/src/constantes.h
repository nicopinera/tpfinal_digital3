#ifndef CONSTANTES_H_
#define CONSTANTES_H_


/* ------------------ Config / constantes ------------------ */
#define PR_TICK_1 4            // Prescale value (PR = 4 -> factor real = PR+1 = 5)
#define PCLK 25000000
#define PCLK_DAC_IN_MHZ    25  // CCLK / 4

#define DMA_SIZE_SINE      60
#define NUM_SAMPLE_SINE    60
#define DMA_SIZE           64
#define NUM_SAMPLE         64

#define SIGNAL_FREQ_IN_HZ  60

#define DAC_GENERATE_SINE      1
#define DAC_GENERATE_TRIANGLE  2
#define DAC_GENERATE_ESCALATOR 3
#define DAC_GENERATE_ESCALON   4
#define DAC_GENERATE_NONE      0

#define MAX_MUESTRAS 18
#define M_PI 3.14159f

#define MUESTRAS_SIN 18
#define ESCALONES 10

/* Debounce */
#define DEBOUNCE_MS 20

/* Toggle this to enable ADC+TIMER sampling mode (then do NOT use DMA simultaneously) */
#define ENABLE_ADC_TIMER_MODE 0

#endif /* CONSTANTES_H_ */
