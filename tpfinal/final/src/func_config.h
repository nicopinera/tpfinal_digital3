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
#define MATH_REG_1 // valor del match register
#define PR_TICK_1 1 // valor en ticks del pre scaler 1
#define MAX_SAMPLES 100

// Estructura para configuracion sencilla de puertos
typedef struct {
	uint8_t puerto;
	uint32_t pin;
	uint8_t func;
} Pines;

#endif /* FUNC_CONFIG_H_ */
