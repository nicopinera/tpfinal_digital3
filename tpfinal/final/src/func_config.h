/*
 * Se definen los encabezados de todas las funciones de configuracion y las estructuras de pines disponibles
 *
 * */

#ifndef FUNC_CONFIG_H_
#define FUNC_CONFIG_H_
#include <stdint.h>
#include "lpc17xx_timer.h"
// MACROS Y DEFINE
#define FUNC_0 0 // funcion 0
#define FUNC_1 1 // funcion 1
#define FUNC_2 2 // funcion 2
#define FUNC_3 3 // funcion 3
#define MODE_0 0
#define MODE_1 1
#define MODE_2 2
#define PR_TICK_1 4 // valor en ticks del pre scaler 1
#define MAX_SAMPLES 100 // hay que cambiarlo a posterior para que sea variable y seteable por el usuario
#define PCLK 25000000

// define para DMA del ADC
#define S_TRANF_WIDTH (1<<18) // Source transfer width -> 16 bits
#define D_TRANF_WIDTH (1<<21) // Destination transfer width -> 16 bits
#define D_INCREMENT (1<<27) // se incrementa el destino
#define INT_FIN (1<<31)

// Estructura para configuracion sencilla de puertos
typedef struct {
	uint8_t puerto;
	uint32_t pin;
	uint8_t func;
	uint8_t mode;
} Pines;

void generar_Seno(uint32_t buffer[], int tam);
void generar_Rampa(uint32_t buffer[], int tam);
void set_mat_frec(int frecuencia);

#endif /* FUNC_CONFIG_H_ */
