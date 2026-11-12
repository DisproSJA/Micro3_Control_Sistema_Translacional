#include <xc.h>
#include "encoder.h"
#include "configuracion.h"
    
/* El conteo lo hacen los timers, sin interrupciones.
   Timer1 cuenta subidas de A con B en bajo.
   Timer5 cuenta bajadas de A con B en bajo. La CLC1 le da A negada.
   El signo es el del PCNT: con B en bajo, la subida resta y la bajada suma.
   El nivel de A completa la cuenta que cae con B en alto. Queda x2. */

static int32_t cero;

static uint8_t a_inicial;

/* A es RC7 (pata 9), B es RB7 (pata 10). Conector del sensor 2. */
void Encoder_Iniciar(void)
{
    TRISCbits.TRISC7 = 1;
    TRISBbits.TRISB7 = 1;
    ANSELCbits.ANSELC7 = 0;
    ANSELBbits.ANSELB7 = 0;

    /* TTL y pull-up: el encoder en colector abierto igual se ve. */
    INLVLCbits.INLVLC7 = 0;
    INLVLBbits.INLVLB7 = 0;
    WPUCbits.WPUC7 = 1;
    WPUBbits.WPUB7 = 1;

    /* CLC1 = A negada. AND de 4 compuertas: la 1 es A, las otras
       no tienen entrada (dan 0) y se invierten a 1. La salida se niega. */
    CLC1CON = 0x00;
    CLC1SEL0 = 0x00;              /* D1 = CLCIN0PPS = RC7 */
    CLC1SEL1 = 0x00;
    CLC1SEL2 = 0x00;
    CLC1SEL3 = 0x00;
    CLC1GLS0 = 0x02;              /* compuerta 1 = D1 sin negar */
    CLC1GLS1 = 0x00;
    CLC1GLS2 = 0x00;
    CLC1GLS3 = 0x00;
    CLC1POL = 0x8E;               /* salida negada, compuertas 2..4 en 1 */
    CLC1CON = 0x82;               /* encendida, AND de 4 */

    /* Reloj sincronizado con Fosc, lectura de 16 bits de una vez.
       Compuerta activa en bajo: solo cuenta con B = 0. */
    T1CON = 0x02;
    T1CLK = 0x00;                 /* T1CKIPPS = RC7 */
    T1GATE = 0x00;                /* T1GPPS = RB7 */
    T1GCON = 0x80;
    TMR1H = 0;
    TMR1L = 0;

    T5CON = 0x02;
    T5CLK = 0x0D;                 /* CLC1_out = A negada */
    T5GATE = 0x00;                /* T5GPPS = RB7 */
    T5GCON = 0x80;
    TMR5H = 0;
    TMR5L = 0;

    /* El timer pierde la primera subida si arranca con el reloj en bajo.
       Son 2 cuentas como mucho (0.013 mm) y solo al encender. */
    a_inicial = PORTCbits.RC7;
    T1CONbits.ON = 1;
    T5CONbits.ON = 1;

    cero = 0;
}

/* bit0 = A, bit1 = B, como los ve el PIC ahora. */
uint8_t Encoder_Ab(void)
{
    uint8_t a;
    uint8_t b;

    a = PORTCbits.RC7;
    b = PORTBbits.RB7;
    return (uint8_t)(a | (uint8_t)(b << 1));
}

/* Conteo x2 desde el encendido. Si A cambia en medio de la lectura,
   se lee otra vez para que timers y pin sean del mismo instante. */
int32_t Encoder_Cuentas(void)
{
    uint8_t a_antes;
    uint8_t a_despues;
    uint8_t bajo;
    uint16_t sube;
    uint16_t baja;
    int32_t cuentas;

    do {
        a_antes = PORTCbits.RC7;
        bajo = TMR1L;             /* con RD16, leer L congela H */
        sube = (uint16_t)(((uint16_t)TMR1H << 8) | bajo);
        bajo = TMR5L;
        baja = (uint16_t)(((uint16_t)TMR5H << 8) | bajo);
        a_despues = PORTCbits.RC7;
    } while (a_antes != a_despues);

    /* La resta en 16 bits aguanta que los timers den la vuelta. */
    cuentas = 2L * (int32_t)(int16_t)(uint16_t)(baja - sube);
    cuentas += (int32_t)a_despues - (int32_t)a_inicial;

#if INVERTIR_ENCODER
    cuentas = -cuentas;
#endif
    return cuentas;
}

/* Guarda el conteo de este instante. Ahi queda 0 mm. */
void Encoder_FijarCero(void)
{
    cero = Encoder_Cuentas();
}

int32_t Encoder_Posicion(void)
{
    return Encoder_Cuentas() - cero;
}
