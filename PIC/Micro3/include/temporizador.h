#ifndef TEMPORIZADOR_H
#define TEMPORIZADOR_H

#include <stdint.h>

/* Timer0 a 150 Hz. La ISR solo marca que toca una muestra. */

void Temporizador_Iniciar(void);
void Temporizador_ISR(void);
uint8_t Temporizador_TomarMuestra(void);

#endif
