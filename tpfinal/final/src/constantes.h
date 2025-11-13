#ifndef CONSTANTES_H_
#define CONSTANTES_H_

#include <math.h>
#include <stdint.h>

/* ------------------ Config / constantes ------------------ */
#define PR_TICK_1 4            // Prescale (PR = 4 -> factor real = PR+1 = 5)
#define PCLK 25000000			// Valor de la frecuencia de perifericos
#define PCLK_DAC_IN_MHZ    25  // CCLK / 4

#define DMA_SIZE_SINE      60 // Tamaño del DMA para señal SENO
#define NUM_SAMPLE_SINE    60 // Numero de samples para SENO
#define DMA_SIZE           64 // Tamaño del DMA para otras señales
#define NUM_SAMPLE         64 // Numero de Samples para otras señales

#define SIGNAL_FREQ_IN_HZ  60 // Frecuencia de la señal Generada

#define DAC_GENERATE_SINE      1 // Generacion de señal SENO
#define DAC_GENERATE_TRIANGLE  2 // Generacion de señal TRIANGULAR
#define DAC_GENERATE_ESCALATOR 3 // Generacion de señal ESCALADOR
#define DAC_GENERATE_ESCALON   4 // Generacion de señal ESCALON
#define DAC_GENERATE_NONE      0 // Generacion de ninguna señal

#define MAX_MUESTRAS 18 // Maximo de muestras para seno normalizado
#define M_PI 3.14159f // PI

#define MUESTRAS_SIN 18 // Muestras del seno normalizado
#define ESCALONES 10 // Escalones para el ESCALADOR

/* Debounce */
#define DEBOUNCE_MS 20 // Tiempo en MS para el antirebote

void generate_sin_0_to_90_16_samples(uint32_t out[]);
void generar_triangulo(uint32_t out[]);
void generar_escalonado(uint32_t out[]);
void generar_escalon(uint32_t out[]);
uint32_t set_mat_frec(uint32_t frecuencia);

#endif /* CONSTANTES_H_ */
