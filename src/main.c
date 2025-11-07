#include <stdio.h>
#include <stdint.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MUESTRAS_SIN 18
#define NUM_SAMPLE_SINE		60
#define NUM_SAMPLE 64
#define ESCALONES 5

// Genera 16 muestras de sin() desde 0° hasta 90° (inclusive),
// escaladas por 10000 y redondeadas al entero más cercano.
void generate_sin_0_to_90_16_samples(uint32_t out[])
{
    const double scale = 10000.0;
    const int steps = MUESTRAS_SIN - 1; // 16 muestras => 15 intervalos
    for (int i = 0; i < MUESTRAS_SIN; ++i)
    {
        double angle_deg = (90.0 * i) / steps; // 0,6,12,...,90
        double rad = angle_deg * M_PI / 180.0;
        double v = sin(rad) * scale;
        out[i] = (uint32_t)v;
    }
}

// Funcion de ejemplo
void generar_triangulo(uint32_t out[])
{
    for (int i = 0; i < NUM_SAMPLE; i++)
    {
        if (i < 32)
            out[i] = 32 * i;
        else if (i == 32)
            out[i] = 1023;
        else
            out[i] = 32 * (NUM_SAMPLE - i);
        //out[i] = (out[i] << 6);
    }
}

// Funcion modificada y generalizada para diferentes muestras
void generar_triangulo2(uint32_t out[])
{
    int paso = (int)1024 / NUM_SAMPLE;
    for (int i = 0; i < NUM_SAMPLE; i++)
    {
        if (i < (NUM_SAMPLE / 2))
            out[i] = paso * i;
        else if (i == (NUM_SAMPLE / 2))
            out[i] = 1023;
        else
            out[i] = paso * (NUM_SAMPLE - i);
        //out[i] = (out[i] << 6);
    }
}

// Escalonado generalizado
void generar_escalonado(uint32_t out[])
{
    for (int i = 0; i < NUM_SAMPLE; i++)
    {
        out[i] = (1023 / ESCALONES) * (i / (NUM_SAMPLE / ESCALONES));
        //out[i] = (out[i] << 6);
    }
}

void generar_escalon(uint32_t out[])
{
    for (int i = 0; i < NUM_SAMPLE; i++)
    {
        if(i <= (NUM_SAMPLE/2)){
            out[i] = 1023;
        }
        else{
            out[i] = 0;
        }
    }
}

int main(void)
{
    uint32_t generated[MUESTRAS_SIN];
    uint32_t dac_lut[NUM_SAMPLE_SINE];
    generate_sin_0_to_90_16_samples(generated);

    printf("\n*************************\n");
    printf("Seno: \n");
    printf("*************************\n");

    // Genera los valores del seno modificados
    for (int i = 0; i < MUESTRAS_SIN; ++i)
    {
        printf("%2d: gen=%5u\n", i, generated[i]);
    }

    // Genera el seno de manera correcta
    for (int i = 0; i < NUM_SAMPLE_SINE; i++)
    {
        // Primer Cuarto de ciclo
        if (i <= (NUM_SAMPLE_SINE * 1 / 4))
        {
            dac_lut[i] = 512 + 512 * generated[i] / 10000;
            if (i == (NUM_SAMPLE_SINE * 1 / 4))
                dac_lut[i] = 1023;
        }
        // Segundo cuarto de ciclo
        else if (i <= (NUM_SAMPLE_SINE * 1 / 2))
        {
            dac_lut[i] = 512 + 512 * generated[30 - i] / 10000;
        }

        // Tercer cuarto de ciclo
        else if (i <= (NUM_SAMPLE_SINE * 3 / 4))
        {
            dac_lut[i] = 512 - 512 * generated[i - 30] / 10000;
        }

        // Cuarto cuarto de ciclo
        else
        {
            dac_lut[i] = 512 - 512 * generated[60 - i] / 10000;
        }
        //dac_lut[i] = (dac_lut[i] << 6); -> Corrimiento para el dac
    }

    for (int i = 0; i < NUM_SAMPLE_SINE; ++i)
    {
        printf("%2d: gen=%5u\n", i, dac_lut[i]);
    }

    printf("\n*************************\n");
    printf("Triangulo 1: \n");
    printf("*************************\n");
    uint32_t buffer[NUM_SAMPLE];
    generar_triangulo(buffer);

    for (int i = 0; i < NUM_SAMPLE_SINE; ++i)
    {
        printf("%2d: gen=%5u\n", i, buffer[i]);
    }

    generar_triangulo2(buffer);
    printf("\n*************************\n");
    printf("Triangulo 2: \n");
    printf("*************************\n");
    for (int i = 0; i < NUM_SAMPLE; ++i)
    {
        printf("%2d: gen=%5u\n", i, buffer[i]);
    }

    generar_escalonado(buffer);

    printf("\n*************************\n");
    printf("Escalonado: \n");
    printf("*************************\n");
    for (int i = 0; i < NUM_SAMPLE; ++i)
    {
        printf("%2d: gen=%5u\n", i, buffer[i]);
    }

    generar_escalon(buffer);

    printf("\n*************************\n");
    printf("Escalon: \n");
    printf("*************************\n");
    for (int i = 0; i < NUM_SAMPLE; ++i)
    {
        printf("%2d: gen=%5u\n", i, buffer[i]);
    }

    return 0;
}