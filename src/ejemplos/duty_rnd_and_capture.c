#include "LPC17xx.h"
#include <stdlib.h>

volatile uint32_t ms_count = 0;
volatile uint32_t last_rise = 0, last_fall = 0;
volatile int32_t duty_error = 0;

void TIMER0_IRQHandler(void);

void setupPWM(void) {
    // PWM en P1.29 → MAT0.1
    LPC_PINCON->PINSEL3 &= ~(3 << 26);
    LPC_PINCON->PINSEL3 |=  (2 << 26);

    // Captura en P1.26 → CAP0.0
    LPC_PINCON->PINSEL3 &= ~(3 << 20);
    LPC_PINCON->PINSEL3 |=  (3 << 20);

    LPC_SC->PCONP |= (1 << 1);
    LPC_SC->PCLKSEL0 &= ~(3 << 2);

    LPC_TIM0->PR = 24;
    LPC_TIM0->MR3 = 1000;
    LPC_TIM0->MCR = (1 << 10);

    LPC_TIM0->MR1 = 500;

    LPC_TIM0->EMR &= ~(0xFF << 4);
    LPC_TIM0->EMR |= (1 << 6) | (2 << 10);

    // IRQ por MR3 cada 1ms
    LPC_TIM0->MCR |= (1 << 9);

    // Configurar captura en CAP0.0
    LPC_TIM0->CCR = (1 << 0) | (1 << 1) | (1 << 2);
    // Bits: CAP0RE (flanco subida), CAP0FE (flanco bajada), CAP0I (IRQ)

    NVIC_EnableIRQ(TIMER0_IRQn);

    LPC_TIM0->TCR = 1;
}

void TIMER0_IRQHandler(void) {
    // Interrupción por MR3 (cada 1ms)
    if (LPC_TIM0->IR & (1 << 3)) {
        LPC_TIM0->IR = (1 << 3);
        ms_count++;
        if (ms_count >= 1000) {
            ms_count = 0;
            uint32_t duty = rand() % 101;
            LPC_TIM0->MR1 = (duty * 1000) / 100;
        }
    }

    // Interrupción por CAP0.0
    if (LPC_TIM0->IR & (1 << 4)) {
        uint32_t cap_val = LPC_TIM0->CR0;
        if (LPC_TIM0->CCR & (1 << 0)) {
            // Subida
            last_rise = cap_val;
        }
        if (LPC_TIM0->CCR & (1 << 1)) {
            // Bajada
            last_fall = cap_val;
            uint32_t duty_real = last_fall - last_rise;
            duty_error = (int32_t)duty_real - (int32_t)LPC_TIM0->MR1;
        }
        LPC_TIM0->IR = (1 << 4); // limpiar flag de capture
    }
}

int main(void) {
    setupPWM();
    while (1) {
        // duty_error queda disponible para lectura o debug
    }
}
