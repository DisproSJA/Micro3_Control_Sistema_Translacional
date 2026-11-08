#include <xc.h>
#include "drv8871_pwm.h"
#include "configuracion.h"

/* 10 bits sobre PR2 = 255. 1024 cuentas = 100 % del periodo. */
#define CUENTAS_PWM                     1024L

/* Escribe el duty. Primero la parte baja, que es la que no se traba. */
static void poner_duty(volatile uint8_t *bajo, volatile uint8_t *alto, uint16_t cuentas)
{
    if (cuentas > 1023U) {
        cuentas = 1023U;
    }
    *bajo = (uint8_t)((cuentas & 0x03U) << 6);
    *alto = (uint8_t)(cuentas >> 2);
}

/* Timer2 a 31.25 kHz y los dos PWM en cero. El motor queda suelto. */
void Pwm_Iniciar(void)
{
    T2CLKCONbits.CS = 0b0001;
    T2CONbits.CKPS = 0;
    T2CONbits.OUTPS = 0;
    T2HLTbits.MODE = 0;
    T2HLTbits.PSYNC = 1;
    T2PR = 255;
    T2TMR = 0;

    poner_duty(&PWM6DCL, &PWM6DCH, 0);
    poner_duty(&PWM7DCL, &PWM7DCH, 0);
    PWM6CONbits.PWM6POL = 0;
    PWM7CONbits.PWM7POL = 0;
    PWM6CONbits.PWM6EN = 1;
    PWM7CONbits.PWM7EN = 1;

    PIR4bits.TMR2IF = 0;
    T2CONbits.ON = 1;
}

/* IN1 e IN2 en bajo: rueda libre. Es el comando S. */
void Pwm_Soltar(void)
{
    poner_duty(&PWM6DCL, &PWM6DCH, 0);
    poner_duty(&PWM7DCL, &PWM7DCH, 0);
}

/* Decaimiento lento. El pin modulado va en alto cuando frena
   y en bajo cuando aplica VM, por eso el duty es el complemento.
   Cero voltios deja las dos entradas en alto y el eje frenado. */
void Pwm_AplicarMv(int16_t milivoltios)
{
    int32_t mag;
    uint16_t alto;
    uint16_t fijo;

    if (milivoltios > VM_MV) {
        milivoltios = VM_MV;
    } else if (milivoltios < -VM_MV) {
        milivoltios = -VM_MV;
    }

#if INVERTIR_GIRO
    milivoltios = (int16_t)(-milivoltios);
#endif

    if (milivoltios < 0) {
        mag = -milivoltios;
    } else {
        mag = milivoltios;
    }

    mag = (mag * CUENTAS_PWM) / VM_MV;
    if (mag > CUENTAS_PWM) {
        mag = CUENTAS_PWM;
    }
    alto = (uint16_t)(CUENTAS_PWM - mag);
    fijo = 1023U;

    if (milivoltios > 0) {
        poner_duty(&PWM6DCL, &PWM6DCH, fijo);
        poner_duty(&PWM7DCL, &PWM7DCH, alto);
    } else if (milivoltios < 0) {
        poner_duty(&PWM6DCL, &PWM6DCH, alto);
        poner_duty(&PWM7DCL, &PWM7DCH, fijo);
    } else {
        poner_duty(&PWM6DCL, &PWM6DCH, fijo);
        poner_duty(&PWM7DCL, &PWM7DCH, fijo);
    }
}
