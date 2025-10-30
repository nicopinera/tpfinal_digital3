#include "func_config.h"
#include "lpc17xx_timer.h"

void generar_Seno(uint32_t buffer[], int tam){

}

void generar_Rampa(uint32_t buffer[], int tam){

}

void set_mat_frec(int frecuencia) {
	float t_match = 1.0f / (frecuencia * (MAX_SAMPLES * 2.0f));
	int match_value = (int) ((t_match * (float) PCLK / (float) (PR_TICK_1 + 1))
			- 1.0f);
	TIM_UpdateMatchValue(LPC_TIM0, 1, match_value);
}
