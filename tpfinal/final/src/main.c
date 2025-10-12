#include "LPC17xx.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_exti.h"
#include "func_config.h"
#include <stdint.h>

// Definimos los Pines en un arreglo
Pines pines_uso[] = { { 2, 12, FUNC_0 }, // P2.12 - Funcion GPIO
		{ 2, 13, FUNC_0 }, // P2.13 - Funcion GPIO
		{ 2, 11, FUNC_0 }, // P2.11 - Funcion GPIO
		{ 0, 21, FUNC_0 },  // P0.21 - Funcion GPIO
		{ 0, 3, FUNC_0 }, // P0.3 - Funcion GPIO
		{ 0, 2, FUNC_0 }, // P0.2 - Funcion GPIO
		};

// Calculo del numero de pines
const int NUM_PINES = sizeof(pines_uso) / sizeof(pines_uso[0]);

void configPIN(void); // Configuracion de GPIO
void configADC(void); // Configuracion del ADC
void configDAC(void); // configuracion del DAC
void configDMA(void); // Configuracion de DMA
void configUART(void); // Configuracion de comunicacion UART

int main(void) {
	SystemInit();
	configPIN();
	//configADC();

	while (1) {
	}
}

void configPIN(void) {
	// Configuración de pines
	PINSEL_CFG_Type pin;
	for (int i = 0; i < NUM_PINES; i++) {
		pin.Portnum = pines_uso[i].puerto;
		pin.Pinnum = pines_uso[i].pin;
		pin.Funcnum = pines_uso[i].func;
		pin.Pinmode = 0;
		pin.OpenDrain = 0;
		PINSEL_ConfigPin(&pin);
	}
}
/*
 * void prenderTodos(void) {
 for (int i = 0; i < NUM_LEDS; i++) {
 GPIO_SetValue(leds[i].port, 1 << leds[i].pin);
 }
 }

 void apagarTodos(void) {
 for (int i = 0; i < NUM_LEDS; i++) {
 GPIO_ClearValue(leds[i].port, 1 << leds[i].pin);
 }
 }

 void desplazamiento(void) {
 // ida
 for (int i = 0; i < NUM_LEDS; i++) {
 apagarTodos();
 GPIO_SetValue(leds[i].port, 1 << leds[i].pin);
 delay();
 }
 // vuelta
 for (int i = NUM_LEDS - 1; i >= 0; i--) {
 apagarTodos();
 GPIO_SetValue(leds[i].port, 1 << leds[i].pin);
 delay();
 }
 banderaFin = 1;
 }
 * */
