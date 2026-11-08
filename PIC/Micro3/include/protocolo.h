#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stdint.h>

/* Arma la linea de la UART y ejecuta P, S, Z, D, A, Kp, Ki y Kd. */

void Protocolo_Iniciar(void);
void Protocolo_Procesar(void);
void Protocolo_AvisoHome(uint8_t aviso);

#endif
