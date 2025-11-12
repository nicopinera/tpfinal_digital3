#include "lpc17xx_adc.h"
#include "lpc17xx_dac.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"

/************************** PRIVATE DEFINITIONS *************************/
/* ADC sample frequency */
#define ADC_CONVERSION_RATE 200000

/* Switch pins */
#define SWITCH1_PORT    0
#define SWITCH1_PIN     2       // P0.2 para Switch 1
#define SWITCH2_PORT    0
#define SWITCH2_PIN     3       // P0.3 para Switch 2

/* LED pins */
#define LED1_PORT       0
#define LED1_PIN        22      // P0.22 LED
#define LED2_PORT       3
#define LED2_PIN        25      // P3.25 LED
#define LED3_PORT       3
#define LED3_PIN        26      // P3.26 LED

/************************** PRIVATE VARIABLES *************************/
/* Operation modes */
typedef enum {
    MODE_CHANNEL1 = 0,    // Solo canal 1 (switches en 00)
    MODE_CHANNEL2,        // Solo canal 2 (switches en 01)
    MODE_SUM,             // Suma CH1 + CH2 (switches en 10)
    MODE_INVERT           // Inversión de CH1 (switches en 11)
} operation_mode_t;

/************************** FUNCTIONS *************************/
uint32_t ADC_ReadChannel(uint8_t channel) {
    uint32_t adc_value;

    // Desactivar todos los canales primero
    ADC_ChannelCmd(LPC_ADC, ADC_CHANNEL_1, DISABLE);
    ADC_ChannelCmd(LPC_ADC, ADC_CHANNEL_2, DISABLE);

    // Activar el canal deseado
    ADC_ChannelCmd(LPC_ADC, channel, ENABLE);

    // Iniciar conversión
    ADC_StartCmd(LPC_ADC, ADC_START_NOW);

    // Esperar a que termine la conversión
    while (!(ADC_ChannelGetStatus(LPC_ADC, channel, ADC_DATA_DONE)));

    // Obtener el valor
    adc_value = ADC_ChannelGetData(LPC_ADC, channel);

    // Desactivar el canal
    ADC_ChannelCmd(LPC_ADC, channel, DISABLE);

    return adc_value;
}

operation_mode_t ReadSwitches(void) {
    // Leer estado de los switches
    // Con pull-up interno: 1 = switch abierto (no presionado), 0 = switch cerrado (presionado)
    uint8_t switch1_pressed = !(GPIO_ReadValue(SWITCH1_PORT) & (1 << SWITCH1_PIN));
    uint8_t switch2_pressed = !(GPIO_ReadValue(SWITCH2_PORT) & (1 << SWITCH2_PIN));


    if (!switch1_pressed && !switch2_pressed) {
        return MODE_CHANNEL1;     // Modo por defecto - más seguro
    } else if (switch1_pressed && !switch2_pressed) {
        return MODE_CHANNEL2;     // Solo SW1
    } else if (!switch1_pressed && switch2_pressed) {
        return MODE_SUM;          // Solo SW2
    } else {
        return MODE_INVERT;       // Ambos switches
    }
}

void UpdateLEDs(operation_mode_t mode) {
    // Primero apagar ambos LEDs
	GPIO_SetValue(LED1_PORT, (1 << LED1_PIN));
	GPIO_SetValue(LED2_PORT, (1 << LED2_PIN));
	GPIO_SetValue(LED3_PORT, (1 << LED3_PIN));
    // Encender LED según el modo activo
    switch (mode) {
        case MODE_CHANNEL1:
        	GPIO_ClearValue(LED1_PORT, (1 << LED1_PIN));  // LED RED ON - CH1
            break;

        case MODE_CHANNEL2:
        	GPIO_ClearValue(LED2_PORT, (1 << LED2_PIN));  // LED BLUE ON - CH2
            break;

        case MODE_SUM:
        	GPIO_ClearValue(LED3_PORT, (1 << LED3_PIN));  // LED GREEN ON - Modo SUMA
            break;

        case MODE_INVERT:
            // LEDs OFF - Modo INVERSIÓN
            break;

        default:
            // Por seguridad, modo por defecto
            break;
    }
}

void configADC(void) {
    PINSEL_CFG_Type PinCfg;

    /* Initialize ADC pins */
    PinCfg.Funcnum = PINSEL_FUNC_1;
    PinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
    PinCfg.Pinmode = PINSEL_PINMODE_PULLUP;
    PinCfg.Portnum = PINSEL_PORT_0;

    // P0.24 para ADC canal 1
    PinCfg.Pinnum = PINSEL_PIN_24;
    PINSEL_ConfigPin(&PinCfg);

    // P0.25 para ADC canal 2
    PinCfg.Pinnum = PINSEL_PIN_25;
    PINSEL_ConfigPin(&PinCfg);

    /* ADC Init */
    ADC_Init(LPC_ADC, ADC_CONVERSION_RATE);

}

void configDAC(void) {
    PINSEL_CFG_Type PinCfg;
    /* Configuration for DAC */
    PinCfg.Funcnum = 2;
    PinCfg.Pinnum = 26;
    PinCfg.Portnum = PINSEL_PORT_0;
    PINSEL_ConfigPin(&PinCfg);
    DAC_Init(LPC_DAC);
}


void configGPIO(void){
    PINSEL_CFG_Type PinCfg;
    // Configurar pines como GPIO con pull-up interno
    PinCfg.Funcnum = PINSEL_FUNC_0;      // Función GPIO
    PinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
    PinCfg.Pinmode = PINSEL_PINMODE_PULLUP;  // Pull-up interno habilitado
    PinCfg.Portnum = PINSEL_PORT_0;

    // Configurar P0.2 como entrada con pull-up (Switch 1)
    PinCfg.Pinnum = SWITCH1_PIN;
    PINSEL_ConfigPin(&PinCfg);
    GPIO_SetDir(SWITCH1_PORT, (1 << SWITCH1_PIN), 0);  // Configurar como entrada

    // Configurar P0.3 como entrada con pull-up (Switch 2)
    PinCfg.Pinnum = SWITCH2_PIN;
    PINSEL_ConfigPin(&PinCfg);
    GPIO_SetDir(SWITCH2_PORT, (1 << SWITCH2_PIN), 0);  // Configurar como entrada

    /* LEDs configuration como salidas */
    GPIO_SetDir(LED1_PORT, (1 << LED1_PIN), 1);  // Output
    GPIO_SetDir(LED2_PORT, (1 << LED2_PIN), 1);  // Output
    GPIO_SetDir(LED3_PORT, (1 << LED3_PIN), 1);  // Output


    GPIO_SetValue(LED1_PORT, (1 << LED1_PIN)); //
    GPIO_SetValue(LED2_PORT, (1 << LED2_PIN));
    GPIO_SetValue(LED3_PORT, (1 << LED3_PIN));

}

int main(void) {
    configADC();
    configDAC();
    configGPIO();

    uint32_t adc_value1;
    uint32_t adc_value2;
    uint32_t result = 0;

    operation_mode_t current_mode;


    while (1) {
        // Leer estado de switches
        current_mode = ReadSwitches();

        // Actualizar LEDs según el modo
        UpdateLEDs(current_mode);

        // Leer canales ADC
        adc_value1 = ADC_ReadChannel(ADC_CHANNEL_1);
        adc_value2 = ADC_ReadChannel(ADC_CHANNEL_2);

        // Procesar según la operación seleccionada
        switch (current_mode) {
            case MODE_CHANNEL1:
                result = adc_value1;
                break;

            case MODE_CHANNEL2:
                result = adc_value2;
                break;

            case MODE_SUM:
                result = adc_value1 + adc_value2;
                break;

            case MODE_INVERT:
                // Inversión del canal 1 (4095 - valor_actual)
                result = 0x0FFF - adc_value1;
                break;

            default:
                result = adc_value1;
                break;
        }
        // Limitar a 12 bits máximo
        if(result > 0x0FFF) {
            result = 0x0FFF;
        }

        // Enviar el valor al DAC (mapear de 12 bits a 10 bits)
        DAC_UpdateValue(LPC_DAC, (uint16_t)(result >> 2));

        // Pequeño delay para estabilidad
        for(volatile int i = 0; i < 10000; i++);
    }


    ADC_DeInit(LPC_ADC);  // nunca se alcanza pero es buena práctica
    return 0;
}

