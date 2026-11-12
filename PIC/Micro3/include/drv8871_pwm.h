#ifndef DRV8871_PWM_H
#define DRV8871_PWM_H

#include <stdint.h>

/* IN1 = PWM6 en RC0, IN2 = PWM7 en RA5. 31.25 kHz. */

void Pwm_Iniciar(void);
void Pwm_AplicarMv(int16_t milivoltios);
void Pwm_Soltar(void);

#endif
