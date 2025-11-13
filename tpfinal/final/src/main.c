#include "LPC17xx.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_exti.h"
#include "lpc17xx_gpdma.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_dac.h"
#include "lpc17xx_timer.h"
#include <math.h>
#include <stdint.h>
#include "constantes.h"

/* ------------------ Variables globales ------------------ */
volatile int frec = 100; // Frecuencia de la señal que sale por UART
volatile int opc = DAC_GENERATE_NONE; // valor elegido por el DIP por defecto

static GPDMA_LLI_Type dma_lli; // LLI para DMA - DAC
static uint32_t dac_lut[NUM_SAMPLE]; // tabla de salida DAC

// Debounce y SysTick
volatile uint32_t systick_ms = 0;
volatile uint32_t debounce_event_time = 0;
volatile uint8_t debounce_pending = 0;

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
		out[i] = (v << 6); // si tu DAC necesita los 10 bits en posiciones altas
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
/* generar_Func: configura DAC + DMA para la forma de onda indicada y retorna.
 NO bloquea; buffers y LLI están en variables static fuera de la pila. */
void generar_Func(int option) {
	PINSEL_CFG_Type PinCfg;
	DAC_CONVERTER_CFG_Type DAC_ConverterConfigStruct;
	GPDMA_Channel_CFG_Type GPDMACfg;
	uint32_t tmp;
	uint32_t sin_0_to_90_16_samples[MAX_MUESTRAS];

	// Init pin DAC P0.26
	PinCfg.Funcnum = 2;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Pinnum = 26;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);

	// Preparar tabla DAC en buffer estático dac_lut[]
	generate_sin_0_to_90_16_samples(sin_0_to_90_16_samples);

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
		// Si NONE -> deshabilitamos canal DMA y dejamos el DAC quieto
		GPDMA_ChannelCmd(0, DISABLE);
		return;
	}

	// Preparar LLI (usa la variable estática dma_lli)
	dma_lli.SrcAddr = (uint32_t) dac_lut;
	dma_lli.DstAddr = (uint32_t) &(LPC_DAC->DACR);
	dma_lli.NextLLI = (uint32_t) &dma_lli; // bucle infinito

	// Control: tamaño dependiendo de la opción
	dma_lli.Control = ((option == DAC_GENERATE_SINE) ? DMA_SIZE_SINE : DMA_SIZE)
			| (2 << 18)   // source width 32 bit (según tu driver original)
			| (2 << 21)   // dest width 32 bit
			| (1 << 26);  // source increment

	// Inicializar GPDMA y configurar canal
	GPDMA_Init();

	GPDMACfg.ChannelNum = 0;
	GPDMACfg.SrcMemAddr = (uint32_t) dac_lut;
	GPDMACfg.DstMemAddr = 0;
	GPDMACfg.TransferSize = (
			(option == DAC_GENERATE_SINE) ? DMA_SIZE_SINE : DMA_SIZE);
	GPDMACfg.TransferWidth = 0;
	GPDMACfg.TransferType = GPDMA_TRANSFERTYPE_M2P;
	GPDMACfg.SrcConn = 0;
	GPDMACfg.DstConn = GPDMA_CONN_DAC;
	GPDMACfg.DMALLI = (uint32_t) &dma_lli;
	GPDMA_Setup(&GPDMACfg);

	// Configurar DAC
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

/* ------------------ ADC / TIMER ------------------ */

void configEINT2(void) {
	EXTI_Init();
	EXTI_SetMode(EXTI_EINT2, EXTI_MODE_LEVEL_SENSITIVE);
	NVIC_EnableIRQ(EINT2_IRQn);

}

void config_ADC_TIMER(void) {
	PINSEL_CFG_Type pin;

	// Init pin DAC P0.26
	pin.Funcnum = 2;
	pin.OpenDrain = 0;
	pin.Pinmode = 0;
	pin.Pinnum = 26;
	pin.Portnum = 0;
	PINSEL_ConfigPin(&pin);

	DAC_CONVERTER_CFG_Type DAC_ConverterConfigStruct;
	DAC_ConverterConfigStruct.CNT_ENA = 0;
	DAC_ConverterConfigStruct.DMA_ENA = 0;

	DAC_Init(LPC_DAC);
	DAC_ConfigDAConverterControl(LPC_DAC, &DAC_ConverterConfigStruct);


	// ADC pin P0.24 (AD channel 1)
	pin.Portnum = 0;
	pin.Pinnum = 24;
	pin.Funcnum = 1;
	pin.OpenDrain = 0;
	pin.Pinmode = PINSEL_PINMODE_TRISTATE;
	PINSEL_ConfigPin(&pin);

	ADC_Init(LPC_ADC, 200000); // 200 kHz ADC clock
	ADC_BurstCmd(LPC_ADC, DISABLE);
	ADC_ChannelCmd(LPC_ADC, 1, ENABLE);

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
	config_timer.MatchValue = 49999;
	TIM_ConfigMatch(LPC_TIM0, &config_timer);
	TIM_Cmd(LPC_TIM0, ENABLE);
	NVIC_EnableIRQ(TIMER0_IRQn);
	TIM_ClearIntPending(LPC_TIM0, TIM_MR1_INT);
}

/* Calcula y actualiza MR1 para la frecuencia solicitada */
void set_mat_frec(uint32_t frecuencia) {
	if (frecuencia == 0U)
		return;
	uint32_t pres = (uint32_t) PR_TICK_1 + 1;
	uint32_t denom = (uint32_t) frecuencia * pres;
	uint32_t match = 0;
	if (denom != 0) {
		match = (uint32_t) (((uint32_t) PCLK + (denom / 2)) / denom - 1);
	}
	TIM_UpdateMatchValue(LPC_TIM0, 1, match);
}

/* ------------------ ISRs ------------------ */

void EINT2_IRQHandler(void) {
	static uint32_t estado_anterior = 1; // suponemos pull-up → comienza en alto (1)
	uint32_t estado_actual;

	// Leer el pin asociado al EINT2, por ejemplo P2.12
	estado_actual = (GPIO_ReadValue(2) >> 12) & 0x1;

	if (estado_actual != estado_anterior) {
		if (estado_actual) {
			opc = DAC_GENERATE_NONE; // CAMBIAR OPC A NONE
			config_adc_timer(); // CONFIGURACION ADC - TIMER
		} else {
			TIM_DeInit(LPC_TIM0);
			ADC_ChannelCmd(LPC_ADC, 1, DISABLE); // APAGAR ADC Y TIMER
		}
		estado_anterior = estado_actual;
	}

	// Limpiar bandera de interrupción externa
	LPC_SC->EXTINT = (1 << 2);
}

/* SysTick: contador ms para debounce */
void SysTick_Handler(void) {
	systick_ms++;
}

/* Timer0 IRQ: solo si usa ADC+TIMER sampling mode.
 Nota: si usas DMA para DAC, NO habilites configADC/configTIMER simultáneamente. */
void TIMER0_IRQHandler(void) {
	if (TIM_GetIntStatus(LPC_TIM0, TIM_MR1_INT) == SET) {
		ADC_StartCmd(LPC_ADC, ADC_START_NOW);
		while (!(ADC_ChannelGetStatus(LPC_ADC, ADC_CHANNEL_1, ADC_DATA_DONE)))
			;
		uint16_t raw = ADC_ChannelGetData(LPC_ADC, ADC_CHANNEL_1);
		uint32_t dac_val = (uint32_t) (raw >> 2) & 0x3FFU;
		DAC_UpdateValue(LPC_DAC, dac_val);
		// Limpiar la bandera correcta MR1 (antes estaba MR0 en tu código)
		TIM_ClearIntPending(LPC_TIM0, TIM_MR1_INT);
	}
}

/* EINT3 IRQ: puerto 2 IRQ. Hacemos debounce diferido: registramos el evento y limpiamos la fuente. */
void EINT3_IRQHandler(void) {
	const uint32_t mask = (1u << 0) | (1u << 1) | (1u << 2);

	debounce_event_time = systick_ms;
	debounce_pending = 1;

	// Limpiar la fuente de interrupción del puerto 2
	LPC_GPIOINT->IO2IntClr = mask;
}

/* ------------------ main ------------------ */
int main(void) {
	SystemInit();

	// Pines y NVIC
	configPIN();
	configPIN_INT();

	// COnfiguracion EINT2
	configEINT2();

	// SysTick 1 ms (necesario para debounce diferido)
	SysTick_Config(SystemCoreClock / 1000);

	int last_opc = -1; // para detectar cambios y reconfigurar solo cuando cambie

	while (1) {
		// Debounce diferido: procesar evento cuando hayan pasado DEBOUNCE_MS
		if (debounce_pending) {
			if ((systick_ms - debounce_event_time) >= DEBOUNCE_MS) {
				uint32_t mask = 0x7u;
				uint32_t valor_p = (~GPIO_ReadValue(2)) & mask;	// Interpretar valor_p como código 0..7
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

		// Si cambió la opción, reconfiguramos (genera la forma de onda con DMA)
		if (opc != last_opc) {
			last_opc = opc;
			if (opc == DAC_GENERATE_NONE) {
				GPDMA_ChannelCmd(0, DISABLE); // Deshabilitar DMA
			} else {
				generar_Func(opc);
			}
		}
	}

	return 0;
}
