#ifndef CONSTANTES_H_
#define CONSTANTES_H_

#include <math.h>
#include <stdint.h>

/* ------------------ Config / constantes ------------------ */
#define PR_TICK_1 4        // Prescale value
#define PCLK 25000000      // Clock de perifericos
#define PCLK_DAC_IN_MHZ 25 // CCLK / 4

#define DMA_SIZE_SINE 60   // Tamaño DMA para SENO
#define NUM_SAMPLE_SINE 60 // Numero de Muestras para SENO

#define DMA_SIZE 64   // Tamaño DMA para resto de señales
#define NUM_SAMPLE 64 // Numero de Muestras para el resto de señales

#define SIGNAL_FREQ_IN_HZ 60 // Frecuencia de las señales

#define DAC_GENERATE_SINE 1      // Generar SENO
#define DAC_GENERATE_TRIANGLE 2  // Generar TRIANGULAR
#define DAC_GENERATE_ESCALATOR 3 // Generar ESCALADOR
#define DAC_GENERATE_ESCALON 4   // Generar ESCALON
#define DAC_GENERATE_NONE 0      // Nada

#define MAX_MUESTRAS 18 // Maximas muestras del seno
#define M_PI 3.14159f   // PI

#define MUESTRAS_SIN 18
#define ESCALONES 5 // Escalones o divisiones del escalador

/* Debounce */
#define DEBOUNCE_MS 20

void generate_sin_0_to_90_16_samples(uint32_t out[]);
void generar_triangulo(uint32_t out[]);
void generar_escalonado(uint32_t out[]);
void generar_escalon(uint32_t out[]);
uint32_t set_mat_frec(uint32_t frecuencia);

#endif /* CONSTANTES_H_ */
