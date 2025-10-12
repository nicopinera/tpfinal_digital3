/*
 * Se definen los encabezados de todas las funciones de configuracion y las estructuras de pines disponibles
 *
 * */

#ifndef FUNC_CONFIG_H_
#define FUNC_CONFIG_H_
#include <stdint.h>
// MACROS Y DEFINE
#define FUNC_0 0
#define FUNC_1 1
#define FUNC_2 2
#define FUNC_3 3

// Estructura para configuracion sencilla de puertos
typedef struct {
	uint8_t puerto;
	uint32_t pin;
	uint8_t func;
} Pines;


#endif /* FUNC_CONFIG_H_ */
