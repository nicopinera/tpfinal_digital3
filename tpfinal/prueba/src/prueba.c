/*
 waveform_generator_eint2_adc_timer.c
 Versión corregida e integrada:
 - EINT3 (P2.0..P2.2) para seleccionar forma de onda (ya con debounce diferido).
 - EINT2 (P2.12) para habilitar/deshabilitar modo ADC+TIMER -> DAC.
 - Correcciones: llamada correcta a config_ADC_TIMER(), no mezcla DMA/ADC,
 GPDMACfg.TransferWidth coherente (word), limpieza de flags correcta,
 deshabilita canal DMA cuando se entra en modo ADC/TIMER.
 - Nota: verifica en el datasheet de tu LPC17xx la función PINSEL correcta
 para EINT2 (el ejemplo usa Funcnum = 1 para P2.12, que suele ser el caso,
 pero confirma según tu silicio).
 */

#include "LPC17xx.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_exti.h"
#include "lpc17xx_gpdma.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_dac.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_uart.h"
#include <math.h>
#include <stdint.h>

/* ------------------ Config / constantes ------------------ */
#define PR_TICK_1 4            // Prescale value (PR = 4 -> factor real = PR+1 = 5)
#define PCLK 25000000
#define PCLK_DAC_IN_MHZ    25  // CCLK / 4

#define DMA_SIZE_SINE      60
#define NUM_SAMPLE_SINE    60
#define DMA_SIZE           64
#define NUM_SAMPLE         64

#define SIGNAL_FREQ_IN_HZ  60

#define DAC_GENERATE_SINE      1
#define DAC_GENERATE_TRIANGLE  2
#define DAC_GENERATE_ESCALATOR 3
#define DAC_GENERATE_ESCALON   4
#define DAC_GENERATE_NONE      0

#define MAX_MUESTRAS 18
#define M_PI 3.14159f

#define MUESTRAS_SIN 18
#define ESCALONES 5

/* Debounce */
#define DEBOUNCE_MS 20

/* ------------------ Variables globales ------------------ */
volatile int frec = 4000;
volatile int opc = DAC_GENERATE_NONE; // valor elegido por el DIP (volatile por acceso desde ISR)

static GPDMA_LLI_Type dma_lli;             // LLI persistente (no en pila)
static uint32_t dac_lut[NUM_SAMPLE];       // tabla de salida DAC persistente

/* Debounce / SysTick variables */
volatile uint32_t systick_ms = 0;
volatile uint32_t debounce_event_time = 0;
volatile uint8_t debounce_pending = 0;

/* Flag que indica si estamos en modo ADC+TIMER (leer ADC y pasar al DAC) */
volatile uint8_t adc_timer_mode_enabled = 0;

/* Calcula y actualiza MR1 para la frecuencia solicitada */
uint32_t set_mat_frec(uint32_t frecuencia) {
	if (frecuencia == 0U) {
		return 4999;
	}
	uint32_t pres = (uint32_t) PR_TICK_1 + 1;
	uint32_t denom = (uint32_t) frecuencia * pres;
	uint32_t match = 0;
	if (denom != 0) {
		match = (uint32_t) (((uint32_t) PCLK + (denom / 2)) / denom - 1);
	}

	return match;
}

/* ------------------ Helper / wave generators ------------------ */
void generate_sin_0_to_90_16_samples(uint32_t out[]) {
	const double scale = 10000.0;
	const int steps = MUESTRAS_SIN - 1;
	for (int i = 0; i < MUESTRAS_SIN; ++i) {
		double angle_deg = (90.0 * i) / steps;
		double rad = angle_deg * M_PI / 180.0;
		double v = sin(rad) * scale;
		out[i] = (uint32_t) v;
	}
}

void generar_triangulo(uint32_t out[]) {
	int half = NUM_SAMPLE / 2;
	for (int i = 0; i < NUM_SAMPLE; i++) {
		uint32_t v;
		if (i <= half) {
			v = (uint32_t) (((uint32_t) i * 1023U) / (uint32_t) half);
		} else {
			v = (uint32_t) (((uint32_t) (NUM_SAMPLE - i) * 1023U)
					/ (uint32_t) half);
		}
		if (v > 1023U)
			v = 1023U;
		out[i] = (v << 6); // formato para DAC (10 bits en MSB)
	}
}

void generar_escalonado(uint32_t out[]) {
	for (int i = 0; i < NUM_SAMPLE; i++) {
		uint32_t step = (1023U / ESCALONES);
		uint32_t bucket = (i / (NUM_SAMPLE / ESCALONES));
		out[i] = (step * bucket) << 6;
	}
}

void generar_escalon(uint32_t out[]) {
	for (int i = 0; i < NUM_SAMPLE; i++) {
		uint32_t v = (i <= (NUM_SAMPLE / 2)) ? 1023U : 0U;
		out[i] = (v << 6);
	}
}

/* ------------------ configurar y lanzar el DMA/DAC ------------------ */
void generar_Func(int option) {
	PINSEL_CFG_Type PinCfg;
	DAC_CONVERTER_CFG_Type DAC_ConverterConfigStruct;
	GPDMA_Channel_CFG_Type GPDMACfg;
	uint32_t tmp;
	uint32_t sin_0_to_90_16_samples[MAX_MUESTRAS];

	// Si estamos en modo ADC+TIMER, no habilitamos DMA (evitar conflicto)
	if (adc_timer_mode_enabled) {
		// Si se pide generar función mientras ADC+TIMER está activo, simplemente salimos.
		return;
	}

	// Init pin DAC P0.26 (AOUT)
	PinCfg.Funcnum = 2;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Pinnum = 26;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);

	// Preparar tabla DAC en buffer estático dac_lut[]
	generate_sin_0_to_90_16_samples(sin_0_to_90_16_samples);

	// Rellenar la tabla según la opción
	switch (option) {
	case DAC_GENERATE_SINE:
		for (int i = 0; i < NUM_SAMPLE_SINE; i++) {
			if (i <= (NUM_SAMPLE_SINE / 4)) {
				dac_lut[i] = 512 + 512 * sin_0_to_90_16_samples[i] / 10000;
				if (i == (NUM_SAMPLE_SINE / 4))
					dac_lut[i] = 1023;
			} else if (i <= (NUM_SAMPLE_SINE / 2)) {
				dac_lut[i] = 512 + 512 * sin_0_to_90_16_samples[30 - i] / 10000;
			} else if (i <= (NUM_SAMPLE_SINE * 3 / 4)) {
				dac_lut[i] = 512 - 512 * sin_0_to_90_16_samples[i - 30] / 10000;
			} else {
				dac_lut[i] = 512 - 512 * sin_0_to_90_16_samples[60 - i] / 10000;
			}
			dac_lut[i] = (dac_lut[i] << 6);
		}
		// asegurar que el resto esté saneado
		for (int i = NUM_SAMPLE_SINE; i < NUM_SAMPLE; i++)
			dac_lut[i] = 0;
		break;

	case DAC_GENERATE_TRIANGLE:
		generar_triangulo(dac_lut);
		break;

	case DAC_GENERATE_ESCALATOR:
		generar_escalonado(dac_lut);
		break;

	case DAC_GENERATE_ESCALON:
		generar_escalon(dac_lut);
		break;

	case DAC_GENERATE_NONE:
	default:
		GPDMA_ChannelCmd(0, DISABLE);
		return;
	}

	// Preparar LLI (usa la variable estática dma_lli)
	dma_lli.SrcAddr = (uint32_t) dac_lut;
	dma_lli.DstAddr = (uint32_t) &(LPC_DAC->DACR);
	dma_lli.NextLLI = (uint32_t) &dma_lli; // bucle infinito

	// Transfer size (coherente con el número de elementos a transferir)
	uint32_t transSize =
			(option == DAC_GENERATE_SINE) ? DMA_SIZE_SINE : DMA_SIZE;

	// Control: usa transSize (si tu driver espera transSize-1, ajusta)
	dma_lli.Control = (transSize) | (2 << 18) | (2 << 21) | (1 << 26);

	// Inicializar GPDMA y configurar canal
	GPDMA_Init();

	GPDMACfg.ChannelNum = 0;
	GPDMACfg.SrcMemAddr = (uint32_t) dac_lut;
	GPDMACfg.DstMemAddr = 0;
	GPDMACfg.TransferSize = transSize;
	GPDMACfg.TransferWidth = 2; // WORD (coincide con (2<<18) en Control)
	GPDMACfg.TransferType = GPDMA_TRANSFERTYPE_M2P;
	GPDMACfg.SrcConn = 0;
	GPDMACfg.DstConn = GPDMA_CONN_DAC;
	GPDMACfg.DMALLI = (uint32_t) &dma_lli;
	GPDMA_Setup(&GPDMACfg);

	// Configurar DAC sin DMA por defecto
	DAC_ConverterConfigStruct.CNT_ENA = SET;
	DAC_ConverterConfigStruct.DMA_ENA = SET;
	DAC_Init(LPC_DAC);

	tmp =
			(PCLK_DAC_IN_MHZ * 1000000U)
					/ (SIGNAL_FREQ_IN_HZ
							* ((option == DAC_GENERATE_SINE) ?
							NUM_SAMPLE_SINE :
																NUM_SAMPLE));
	DAC_SetDMATimeOut(LPC_DAC, tmp);
	DAC_ConfigDAConverterControl(LPC_DAC, &DAC_ConverterConfigStruct);

	// Habilitar canal GPDMA 0
	GPDMA_ChannelCmd(0, ENABLE);
}

/* ------------------ Configuración pines / interrupciones ------------------ */
void configPIN(void) {
	PINSEL_CFG_Type pin;
	// LED P0.22 as GPIO output
	pin.Portnum = 0;
	pin.Pinnum = 22;
	pin.Funcnum = 0;
	pin.OpenDrain = 0;
	pin.Pinmode = PINSEL_PINMODE_PULLUP;
	PINSEL_ConfigPin(&pin);
	GPIO_SetDir(0, 1 << 22, 1);
	GPIO_ClearValue(0, 1 << 22);
}

/* Configuración de interrupciones por puerto 2 (pines de dipswitch) */
void configPIN_INT(void) {
	PINSEL_CFG_Type PinCfg;

	PinCfg.Funcnum = 0; // GPIO
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = PINSEL_PINMODE_PULLUP; // habilitar pull-up interno
	PinCfg.Portnum = 2;

	PinCfg.Pinnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	GPIO_SetDir(2, (1u << 0), 0); // entrada

	PinCfg.Pinnum = 1;
	PINSEL_ConfigPin(&PinCfg);
	GPIO_SetDir(2, (1u << 1), 0);

	PinCfg.Pinnum = 2;
	PINSEL_ConfigPin(&PinCfg);
	GPIO_SetDir(2, (1u << 2), 0);

	// Habilitar interrupciones por flanco de subida y bajada en IO2
	LPC_GPIOINT->IO2IntEnR |= (1u << 0) | (1u << 1) | (1u << 2);
	LPC_GPIOINT->IO2IntEnF |= (1u << 0) | (1u << 1) | (1u << 2);

	// Limpiar cualquier interrupción pendiente
	LPC_GPIOINT->IO2IntClr = (1u << 0) | (1u << 1) | (1u << 2);

	NVIC_EnableIRQ(EINT3_IRQn);
}

/* ------------------ ADC + TIMER (habilitación por EINT2) ------------------ */

/* Configura P2.12 como EINT2 (asegúrate en tu datasheet el Funcnum correcto).
 Aquí se usa Funcnum = 1 (verifica para tu variante). */
void configEINT2(void) {
	PINSEL_CFG_Type p;
	p.Portnum = 2;
	p.Pinnum = 12;
	p.Funcnum = 1; // normalmente EINT2 está en función 1 para P2.12 (verificar)
	p.OpenDrain = 0;
	p.Pinmode = PINSEL_PINMODE_PULLUP; // pull-up para detectar nivel
	PINSEL_ConfigPin(&p);

	/* Inicializar EXTI EINT2 (nivel sensible en este ejemplo) */
	EXTI_Init();
	EXTI_SetMode(EXTI_EINT2, EXTI_MODE_LEVEL_SENSITIVE);
	NVIC_EnableIRQ(EINT2_IRQn);
}

/* Configura ADC + Timer para empezar a muestrear y pasar al DAC en el IRQ del timer */
void config_ADC_TIMER(void) {

	PINSEL_CFG_Type pinCfg;

	// TXD2 (P0.10) y RXD2 (P0.11)
	pinCfg.Portnum = 0;
	pinCfg.Pinnum = 10;
	pinCfg.Funcnum = 1;
	pinCfg.Pinmode = PINSEL_PINMODE_PULLUP;
	pinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
	PINSEL_ConfigPin(&pinCfg);

	pinCfg.Pinnum = 11;
	PINSEL_ConfigPin(&pinCfg);

	UART_CFG_Type uart_cfg;
	UART_ConfigStructInit(&uart_cfg);
	uart_cfg.Baud_rate = 9600;
	UART_Init(LPC_UART2, &uart_cfg);

	UART_FIFO_CFG_Type uart_fifo;
	UART_FIFOConfigStructInit(&uart_fifo);
	UART_FIFOConfig(LPC_UART2, &uart_fifo);
	UART_TxCmd(LPC_UART2, ENABLE);
	NVIC_DisableIRQ(UART2_IRQn);

	PINSEL_CFG_Type pin;

	// Asegurar DAC pin P0.26 configurado como AOUT
	pin.Funcnum = 2;
	pin.OpenDrain = 0;
	pin.Pinmode = 0;
	pin.Pinnum = 26;
	pin.Portnum = 0;
	PINSEL_ConfigPin(&pin);

	// Inicializar DAC (modo no DMA, update por software)
	DAC_CONVERTER_CFG_Type DAC_Config;
	DAC_Config.CNT_ENA = SET;    // Enable counter (not strictly needed)
	DAC_Config.DMA_ENA = RESET;  // NO DMA: we'll update from timer ISR
	DAC_Init(LPC_DAC);
	DAC_ConfigDAConverterControl(LPC_DAC,
			(DAC_CONVERTER_CFG_Type*) &DAC_Config);

	// ADC: P0.24 -> AD1.1
	pin.Portnum = 0;
	pin.Pinnum = 24;
	pin.Funcnum = 1; // AD1.1 typical
	pin.OpenDrain = 0;
	pin.Pinmode = PINSEL_PINMODE_TRISTATE;
	PINSEL_ConfigPin(&pin);

	ADC_Init(LPC_ADC, 200000);
	ADC_BurstCmd(LPC_ADC, DISABLE);
	ADC_ChannelCmd(LPC_ADC, 1, ENABLE); // canal AD1

	// Timer0: configura con prescaler en ticks
	TIM_TIMERCFG_Type config_pre;
	config_pre.PrescaleOption = TIM_PRESCALE_TICKVAL;
	config_pre.PrescaleValue = PR_TICK_1;
	TIM_Init(LPC_TIM0, TIM_TIMER_MODE, &config_pre);

	TIM_MATCHCFG_Type config_timer;
	config_timer.MatchChannel = 1;
	config_timer.IntOnMatch = ENABLE;
	config_timer.ResetOnMatch = ENABLE;
	config_timer.StopOnMatch = DISABLE;
	config_timer.ExtMatchOutputType = TIM_EXTMATCH_TOGGLE;
	uint32_t match = set_mat_frec(frec);
	config_timer.MatchValue = match; // valor inicial (ajustable)
	TIM_ConfigMatch(LPC_TIM0, &config_timer);

	TIM_Cmd(LPC_TIM0, ENABLE);
	NVIC_EnableIRQ(TIMER0_IRQn);
	TIM_ClearIntPending(LPC_TIM0, TIM_MR1_INT);
}

/* Detener ADC+TIMER de forma segura */
void stop_ADC_TIMER(void) {
	// Deshabilitar timer y ADC
	TIM_Cmd(LPC_TIM0, DISABLE);
	TIM_ClearIntPending(LPC_TIM0, TIM_MR1_INT);
	ADC_ChannelCmd(LPC_ADC, 1, DISABLE);
	// Opcional: Deinit timer/adc si tu librería lo soporta
	adc_timer_mode_enabled = 0;
	// Asegurarse de que DMA esté deshabilitado (no mezclar modos)
	GPDMA_ChannelCmd(0, DISABLE);
}

/* ------------------ ISRs ------------------ */

/* EINT2 IRQ: habilita/deshabilita ADC+TIMER.
 Se asume pull-up: estado alto = inactivo, bajo = activo (dependiendo wiring).
 Ajusta la lógica si tu interruptor invierte polaridad.
 */
void EINT2_IRQHandler(void) {
	static uint32_t estado_anterior = 1; // suponemos pull-up -> comienza en alto (1)
	uint32_t estado_actual;

	// Leer el pin P2.12 (bit 12)
	estado_actual = (GPIO_ReadValue(2) >> 12) & 0x1;

	if (estado_actual != estado_anterior) {
		if (estado_actual == 0) {
			// ejemplo: switch cerrado (activo) -> habilitar ADC+TIMER
			adc_timer_mode_enabled = 1;
			// asegurarse de que DMA esté deshabilitado
			GPDMA_ChannelCmd(0, DISABLE);
			config_ADC_TIMER();
			// setear frecuencia deseada (ejemplo: frec variable global)
			set_mat_frec((uint32_t) frec);
		} else {
			// switch abierto -> deshabilitar ADC+TIMER y volver a modo DMA si opc lo pide
			stop_ADC_TIMER();
			// opc puede permanecer igual; la parte principal decidirá si vuelve a generar_Func(opc)
		}
		estado_anterior = estado_actual;
	}

	// Limpiar bandera de interrupción externa EINT2
	LPC_SC->EXTINT = (1 << 2);
}

/* SysTick: contador ms para debounce */
void SysTick_Handler(void) {
	systick_ms++;
}

/* Timer0 IRQ: usado para disparar conversiones ADC y pasar el valor al DAC (modo ADC+TIMER) */
void TIMER0_IRQHandler(void) {
    if (TIM_GetIntStatus(LPC_TIM0, TIM_MR1_INT) == SET) {
        if (adc_timer_mode_enabled) {
            /* Inicio de conversión */
            ADC_StartCmd(LPC_ADC, ADC_START_NOW);

            while (!(ADC_ChannelGetStatus(LPC_ADC, ADC_CHANNEL_1, ADC_DATA_DONE)))
                ; // esperar fin de conversión

            uint16_t raw = ADC_ChannelGetData(LPC_ADC, ADC_CHANNEL_1); // 12 bits (0..4095)
            uint8_t valor = (uint16_t)(raw >> 4) & 0xFFU;

            /* Enviar con formato: valor + salto de línea */
            UART_SendByte(LPC_UART2, valor);
            UART_SendByte(LPC_UART2, '\n');  // Agregar salto de línea

            /* Esperar que THR esté vacío */
            while (!(LPC_UART2->LSR & 0x20)) {
            }
        }
    }

    /* Limpiar la bandera MR1 */
    TIM_ClearIntPending(LPC_TIM0, TIM_MR1_INT);
}

/* EINT3 IRQ: puerto 2 IRQ. Debounce diferido para dip switches */
void EINT3_IRQHandler(void) {
	const uint32_t mask = (1u << 0) | (1u << 1) | (1u << 2);

	debounce_event_time = systick_ms;
	debounce_pending = 1;

	// Limpiar la fuente de interrupción del puerto 2
	LPC_GPIOINT->IO2IntClr = mask;
}

/* ------------------ resto del sistema (set_mat_frec, main) ------------------ */

int main(void) {
	SystemInit();

	// Pines y NVIC
	configPIN();
	configPIN_INT();

	// Configurar EINT2 pin + EXTI
	configEINT2();

	// SysTick 1 ms (necesario para debounce diferido)
	SysTick_Config(SystemCoreClock / 1000);

	int last_opc = -1; // para detectar cambios y reconfigurar solo cuando cambie

	while (1) {
		// Debounce diferido: procesar evento cuando hayan pasado DEBOUNCE_MS
		if (debounce_pending) {
			if ((systick_ms - debounce_event_time) >= DEBOUNCE_MS) {
				uint32_t mask = 0x7u;
				uint32_t valor_p = (~GPIO_ReadValue(2)) & mask; // leer P2.0..P2.2, invertir si usas pull-up
				// Mapea (puedes ajustar la lógica de mapeo a tu necesidad)
				switch (valor_p) {
				case 0:
					opc = DAC_GENERATE_NONE;
					break;
				case 1:
					opc = DAC_GENERATE_SINE;
					break;
				case 2:
					opc = DAC_GENERATE_TRIANGLE;
					break;
				case 3:
					opc = DAC_GENERATE_ESCALATOR;
					break;
				case 4:
					opc = DAC_GENERATE_ESCALON;
					break;
				default:
					opc = DAC_GENERATE_NONE;
					break;
				}
				debounce_pending = 0;
			}
		}

		// Si cambió la opción y no estamos en modo ADC+TIMER, reconfiguramos (DMA)
		if (!adc_timer_mode_enabled && (opc != last_opc)) {
			last_opc = opc;
			if (opc == DAC_GENERATE_NONE) {
				GPDMA_ChannelCmd(0, DISABLE);
			} else {
				generar_Func(opc);
			}
		}

		// Cuando esté activo adc_timer_mode_enabled, el TIMER0_IRQHandler hace ADC->DAC.
	}

	return 0;
}
