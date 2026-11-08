#ifndef ADC_H
#define ADC_H

#include <stdint.h>

/* RA2 contra la FVR de 2.048 V. 1.024 V es el cero de la carrera. */

void Adc_Iniciar(void);
void Adc_Disparar(void);
void Adc_ISR(void);
uint16_t Adc_Ver(void);

#endif
