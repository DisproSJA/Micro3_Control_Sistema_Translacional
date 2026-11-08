#include <xc.h>
#include "temporizador.h"
#include "configuracion.h"

static volatile uint8_t muestra_lista;

/* Timer0 a 16 bits, Fosc/4, sin prescaler. Periodo 53333 cuentas. */
void Temporizador_Iniciar(void)
{
    T0CON1bits.T0CS = 0b010;
    T0CON1bits.T0CKPS = 0;
    T0CON0bits.T016BIT = 1;
    T0CON0bits.T0OUTPS = 0;

    TMR0H = (uint8_t)(TMR0_RECARGA >> 8);
    TMR0L = (uint8_t)TMR0_RECARGA;

    muestra_lista = 0;
    PIR0bits.TMR0IF = 0;
    T0CON0bits.T0EN = 1;
}

/* Suma el periodo a lo que ya corrio, para no comerse la latencia. */
void Temporizador_ISR(void)
{
    uint16_t ahora;

    ahora = TMR0L;
    ahora |= (uint16_t)((uint16_t)TMR0H << 8);
    ahora = (uint16_t)(ahora + TMR0_RECARGA);
    TMR0H = (uint8_t)(ahora >> 8);
    TMR0L = (uint8_t)ahora;

    muestra_lista = 1;
    PIR0bits.TMR0IF = 0;
}

/* 1 si ya cumplio un Ts. Se lleva una sola, aunque se hayan juntado. */
uint8_t Temporizador_TomarMuestra(void)
{
    uint8_t gie;
    uint8_t lista;

    gie = INTCONbits.GIE;
    INTCONbits.GIE = 0;
    lista = muestra_lista;
    muestra_lista = 0;
    INTCONbits.GIE = gie;
    return lista;
}
