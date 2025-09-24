// Trabajo final Digital 3

#ifdef __USE_CMSIS

#include "LPC17xx.h"

#endif

#include <cr_section_macros.h>

void configGPIO(void) {

	LPC_PINCON->PINSEL0 &= ~(0B11 << 8);
	LPC_PINCON->PINSEL1 &= ~(0B11 <<12);
	LPC_PINCON->PINSEL4 &= ~(0B1111<<22);

	LPC_GPIO0->FIODIR |= (1<<22);
	LPC_GPIO0->FIODIR |= (1 << 4);
	LPC_GPIO2->FIODIR |= (1<<12);
	LPC_GPIO2->FIODIR |= (1<<11);

	LPC_GPIO0->FIOCLR= (1<<22);
	LPC_GPIO0->FIOSET = (1 << 4);
	LPC_GPIO2->FIOSET = (0B11<<11);

}

int main(void) {
	SystemInit();
	configGPIO();

	while (1) {

	}

	return 0;

}
