#include <xc.h>
#include "adc.h"

static volatile uint16_t lectura_adc;

/* RA2, 12 bits a la derecha, reloj FRC, referencia 2.048 V. */
void Adc_Iniciar(void)
{
    uint16_t espera;

    FVRCONbits.ADFVR = 0b10;
    FVRCONbits.FVREN = 1;
    espera = 0;
    while (!FVRCONbits.FVRRDY && (espera < 40000U)) {
        espera++;
    }

    /* ADPREF = FVR, ADNREF = VSS. */
    ADREF = 0x03;
    ADPCH = 0x02;
    ADACT = 0x00;
    ADCON1 = 0x00;
    ADCON2 = 0x00;
    ADCON3 = 0x00;
    ADCON0bits.FM = 1;
    ADCON0bits.CS = 1;
    ADCON0bits.CONT = 0;
    ADCON0bits.ON = 1;

    lectura_adc = 2048;
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
