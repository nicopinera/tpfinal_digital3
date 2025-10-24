/*
 * Se definen los encabezados de todas las funciones de configuracion y las estructuras de pines disponibles
 *
 * */

#ifndef FUNC_CONFIG_H_
#define FUNC_CONFIG_H_
#include <stdint.h>
// MACROS Y DEFINE
#define FUNC_0 0 // funcion 0
#define FUNC_1 1 // funcion 1
#define FUNC_2 2 // funcion 2
#define FUNC_3 3 // funcion 3
#define PR_TICK_1 1 // valor en ticks del pre scaler 1
#define MAX_SAMPLES 100
#define PCLK_DAC_IN_MHZ 25

// define para DMA del ADC
#define T_SIZE 100 // transfer size
#define S_TRANF_WIDTH (1<<18) // Source transfer width -> 16 bits, descarto el resto o que coloque todos en 0
#define D_TRANF_WIDTH (1<<21) // Destination transfer width -> 16 bits
#define D_INCREMENT (1<<27) // se incrementa el destino

// Estructura para configuracion sencilla de puertos
typedef struct {
	uint8_t puerto;
	uint32_t pin;
	uint8_t func;
} Pines;

#endif /* FUNC_CONFIG_H_ */
