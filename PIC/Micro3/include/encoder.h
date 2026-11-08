#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>

/* Cuadratura x2 sobre RC3. RC6 solo dice el sentido. */

void Encoder_Iniciar(void);
void Encoder_ISR(void);
int32_t Encoder_Cuentas(void);
int32_t Encoder_Posicion(void);

#endif
