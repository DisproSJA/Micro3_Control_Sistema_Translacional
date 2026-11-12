#ifndef TELEMETRIA_H
#define TELEMETRIA_H

#include <stdint.h>

/* Una linea por muestra: consigna, posicion, error, salida y el ADC. */

void Telemetria_Enviar(int32_t consigna, int32_t posicion, int16_t milivoltios,
                       uint16_t adc);

#endif
