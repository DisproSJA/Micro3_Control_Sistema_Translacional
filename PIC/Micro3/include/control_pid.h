#ifndef CONTROL_PID_H
#define CONTROL_PID_H

#include <stdint.h>

/* PID en mV. Integral en trapezio, derivada con Euler hacia atras.
   La derivada mira la posicion, no el error, para no patear en el escalon. */

void Control_Iniciar(void);
void Control_Reiniciar(void);
uint8_t Control_FijarKp(int16_t kp_milli);
uint8_t Control_FijarKi(int16_t ki_milli);
uint8_t Control_FijarKd(int16_t kd_milli);
int16_t Control_Calcular(int32_t consigna, int32_t posicion);

#endif
