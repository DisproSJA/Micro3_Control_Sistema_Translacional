#include <xc.h>
#include "adc.h"

static volatile uint16_t lectura_adc;
static uint32_t acum_adc;

/* RA2, 12 bits a la derecha, reloj FRC, referencia VDD.
   El pot de 10k va de 0 a 5 V: un tope, la mitad y el otro tope. */
void Adc_Iniciar(void)
{
    /* ADPREF = VDD, ADNREF = VSS. Toda la vuelta entra en 0..4095. */
    ADREF = 0x00;
    ADPCH = 0x02;
    ADACT = 0x00;
    ADCON1 = 0x00;
    ADCON2 = 0x00;
    ADCON3 = 0x00;
    ADACQ = 32;                   /* el cursor es de 10k, hay que esperar */
    ADCON0bits.FM = 1;
    ADCON0bits.CS = 1;
    ADCON0bits.CONT = 0;
    ADCON0bits.ON = 1;

    lectura_adc = 2048;
    acum_adc = 2048UL * 8UL;
    PIR1bits.ADIF = 0;
}

/* Pide una conversion. Si la anterior sigue, no la pisa. */
void Adc_Disparar(void)
{
    if (ADCON0bits.GO) {
        return;
    }
    ADCON0bits.GO = 1;
}

/* Guarda ADRES. */
void Adc_ISR(void)
{
    lectura_adc = ADRES;
    PIR1bits.ADIF = 0;
}

/* Ultima lectura, sin consumirla. */
uint16_t Adc_Ver(void)
{
    uint16_t lectura;
    uint8_t gie;

    gie = INTCONbits.GIE;
    INTCONbits.GIE = 0;
    lectura = lectura_adc;
    INTCONbits.GIE = gie;
    return lectura;
}

/* Promedio corto para que el ruido de 5 V no mueva la consigna. */
uint16_t Adc_Filtrar(void)
{
    uint16_t lectura;

    lectura = Adc_Ver();
    if (lectura > 4095U) {
        lectura = 4095U;
    }
    acum_adc -= acum_adc >> 3;
    acum_adc += lectura;
    return (uint16_t)(acum_adc >> 3);
}
