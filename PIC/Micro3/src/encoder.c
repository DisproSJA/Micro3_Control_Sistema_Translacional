#include <xc.h>
#include "encoder.h"
#include "configuracion.h"

static volatile int32_t cuentas;

/* Flancos de A en RC3. B en RC6 no interrumpe. */
void Encoder_Iniciar(void)
{
    cuentas = 0;

    IOCCPbits.IOCCP3 = 1;
    IOCCNbits.IOCCN3 = 1;
    IOCCPbits.IOCCP6 = 0;
    IOCCNbits.IOCCN6 = 0;

    (void)PORTC;
    IOCCF = 0;
}

/* Un flanco de A. Si A y B quedaron iguales, el giro es el otro. */
void Encoder_ISR(void)
{
    uint8_t puerto;
    uint8_t a;
    uint8_t b;

    puerto = PORTC;
    if (!IOCCFbits.IOCCF3) {
        IOCCF = 0;
        return;
    }

    a = (uint8_t)((puerto >> 3) & 1U);
    b = (uint8_t)((puerto >> 6) & 1U);
    IOCCFbits.IOCCF3 = 0;

#if INVERTIR_ENCODER
    if (a == b) {
        cuentas++;
    } else {
        cuentas--;
    }
#else
    if (a == b) {
        cuentas--;
    } else {
        cuentas++;
    }
#endif
}

/* Copia el conteo. Son 4 bytes, hay que apagar la ISR para no partirlo. */
int32_t Encoder_Cuentas(void)
{
    int32_t copia;
    uint8_t gie;

    gie = INTCONbits.GIE;
    INTCONbits.GIE = 0;
    copia = cuentas;
    INTCONbits.GIE = gie;
    return copia;
}

/* Cuentas respecto al home quemado en CUENTAS_HOME. Ahi es 0 mm. */
int32_t Encoder_Posicion(void)
{
    return Encoder_Cuentas() - CUENTAS_HOME;
}
