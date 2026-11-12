#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>

/* Cuadratura x2 por hardware: Timer1, Timer5 y CLC1. A en RC7, B en RB7. */

void Encoder_Iniciar(void);
void Encoder_FijarCero(void);
uint8_t Encoder_Ab(void);
int32_t Encoder_Cuentas(void);
int32_t Encoder_Posicion(void);

#endif
