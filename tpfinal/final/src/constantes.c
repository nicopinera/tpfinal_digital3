#include "constantes.h"

#include <math.h>
#include <stdint.h>

/* ------------------ Helper / wave generators ------------------ */
void generate_sin_0_to_90_16_samples(uint32_t out[])
{
	const double scale = 10000.0;
	const int steps = MUESTRAS_SIN - 1;
	for (int i = 0; i < MUESTRAS_SIN; ++i)
	{
		double angle_deg = (90.0 * i) / steps;
		double rad = angle_deg * M_PI / 180.0;
		double v = sin(rad) * scale;
		out[i] = (uint32_t)v;
	}
}

void generar_triangulo(uint32_t out[])
{
	int half = NUM_SAMPLE / 2;
	for (int i = 0; i < NUM_SAMPLE; i++)
	{
		uint32_t v;
		if (i <= half)
		{
			v = (uint32_t)(((uint32_t)i * 1023U) / (uint32_t)half);
		}
		else
		{
			v = (uint32_t)(((uint32_t)(NUM_SAMPLE - i) * 1023U) / (uint32_t)half);
		}
		if (v > 1023U)
			v = 1023U;
		out[i] = (v << 6); // formato para DAC (10 bits en MSB)
	}
}

void generar_escalonado(uint32_t out[])
{
	for (int i = 0; i < NUM_SAMPLE; i++)
	{
		uint32_t step = (1023U / ESCALONES);
		uint32_t bucket = (i / (NUM_SAMPLE / ESCALONES));
		out[i] = (step * bucket) << 6;
	}
}

void generar_escalon(uint32_t out[])
{
	for (int i = 0; i < NUM_SAMPLE; i++)
	{
		uint32_t v = (i <= (NUM_SAMPLE / 2)) ? 1023U : 0U;
		out[i] = (v << 6);
	}
}

/* Calcula y actualiza MR1 para la frecuencia solicitada */
uint32_t set_mat_frec(uint32_t frecuencia)
{
	if (frecuencia == 0U)
	{
		return 4999;
	}
	uint32_t pres = (uint32_t)PR_TICK_1 + 1;
	uint32_t denom = (uint32_t)frecuencia * pres;
	uint32_t match = 0;
	if (denom != 0)
	{
		match = (uint32_t)(((uint32_t)PCLK + (denom / 2)) / denom - 1);
	}

	return match;
}
