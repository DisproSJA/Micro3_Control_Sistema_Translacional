#ifndef ADC_H
#define ADC_H

#include <stdint.h>

/* RA2 contra VDD. 0 cuentas es un tope, 2048 el centro y 4095 el otro. */

void Adc_Iniciar(void);
void Adc_Disparar(void);
void Adc_ISR(void);
uint16_t Adc_Ver(void);
uint16_t Adc_Filtrar(void);

#endif
