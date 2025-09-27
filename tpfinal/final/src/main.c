#include "LPC17xx.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_exti.h"
#define DELAY 5000000

volatile int banderaFin = 0;
volatile int banderaComienzo = 0;
//1.31 para dar tension
// 2.10 para EINT0 -> flanco descendente

typedef struct {
	uint8_t port;
	uint32_t pin;
} Led_t;

// Definimos los LEDs en un arreglo
Led_t leds[] = { { 2, 12 }, // LED1
		{ 2, 13 }, // LED2
		{ 2, 11 }, // LED3
		{ 0, 21 },  // LED4
		{ 0, 3 }, // LED 5
		{ 0, 2 }, // LED 6
		};
const int NUM_LEDS = sizeof(leds) / sizeof(leds[0]);

void delay(void) {
	for (int i = 0; i < DELAY; i++)
		;
}

void prenderTodos(void) {
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

void EINT0_IRQHandler() {
	banderaComienzo ^= 1;
	// banderaComienzo = 1;
	EXTI_ClearEXTIFlag(0); // limpia bandera
}

int main(void) {
	SystemInit();

	// Configuración de pines
	PINSEL_CFG_Type pin;
	for (int i = 0; i < NUM_LEDS; i++) {
		pin.Portnum = leds[i].port;
		pin.Pinnum = leds[i].pin; // obtiene el número del pin
		pin.Funcnum = 0;
		pin.Pinmode = 0;
		pin.OpenDrain = 0;
		PINSEL_ConfigPin(&pin);

		GPIO_SetDir(leds[i].port, 1 << leds[i].pin, 1);
	}
	pin.Portnum = 1;
	pin.Pinnum = 31;
	pin.Funcnum = 0;
	pin.Pinmode = 0;
	pin.OpenDrain = 0;
	PINSEL_ConfigPin(&pin);
	pin.Portnum = 2;
	pin.Pinnum = 10; // EINT0
	pin.Funcnum = 1;
	pin.Pinmode = 0;
	pin.OpenDrain = 0;
	PINSEL_ConfigPin(&pin);
	LPC_PINCON->PINMODE4 &= ~(1 << 20);
	LPC_PINCON->PINMODE4 |= (1 << 21);

	// EXTI_InitTypeDef externa;
	// externa.EXTI_Line = 0;
	// externa.EXTI_Mode = 1;
	// externa.EXTI_polarity = 0; // bajada
	// EXTI_Config(&externa); // Configuracion
	// EXTI_SetMode(0, 1);
	// EXTI_SetPolarity(0, 0);

	//NVIC_EnableIRQ(EINT0_IRQn); // habilito

	while (1) {
		if (banderaComienzo) {
			desplazamiento();
			if (banderaFin) {
				for (int i = 0; i < 3; i++) {
					prenderTodos();
					delay();
					apagarTodos();
					delay();
				}
				banderaFin = 0;
				// banderaComienzo = 0;
			}
		} else {
			apagarTodos();
		}
	}
}
