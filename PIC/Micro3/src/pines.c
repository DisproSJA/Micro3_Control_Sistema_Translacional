#include "pines.h"

/* HFINTOSC a 32 MHz. RSTOSC ya lo pide; se vuelve a escribir. */
static void iniciar_oscilador(void)
{
    OSCFRQbits.HFFRQ = 0b110;
    OSCCON1 = 0x60;

    while (!OSCSTATbits.HFOR) {
        ;
    }
    while (OSCCON2bits.COSC != 0b110) {
        ;
    }
}

/* Latch primero. Si TRIS va antes, el pin sale un momento al aire. */
static void iniciar_gpio(void)
{
    /* RA4 en alto: en el zocalo del paso a paso eso deja el driver suelto.
       El DC no pasa por ahi. IN1 e IN2 arrancan en bajo, motor suelto. */
    LATA = 0x10;
    LATB = 0;
    LATC = 0;

    /* RA0/RA1 ICSP, RA2 ADC, RA3 MCLR. RA4 y RA5 salidas. */
    TRISA = 0x0F;
    TRISB = 0xFF;
    /* RC3 encoder A, RC5 RX, RC6 encoder B, RC7 sin usar. */
    TRISC = 0xE8;

    ANSELA = 0x04;
    ANSELB = 0;
    ANSELC = 0;

    /* RB6 queda de entrada. El cero ya no sale de ese pin. */
    WPUB = 0x40;
}

/* Destraba PPSLOCK. La secuencia 0x55, 0xAA tiene que ir seguida. */
static void abrir_pps(void)
{
    asm("BANKSEL PPSLOCK");
    asm("MOVLW 0x55");
    asm("MOVWF PPSLOCK");
    asm("MOVLW 0xAA");
    asm("MOVWF PPSLOCK");
    asm("BCF PPSLOCK, 0");
}

/* Vuelve a trabar PPSLOCK. */
static void cerrar_pps(void)
{
    asm("BANKSEL PPSLOCK");
    asm("MOVLW 0x55");
    asm("MOVWF PPSLOCK");
    asm("MOVLW 0xAA");
    asm("MOVWF PPSLOCK");
    asm("BSF PPSLOCK, 0");
}

/* TX en RC4, PWM6 en RC2, PWM7 en RC1. RX entra por RC5. */
static void iniciar_pps(void)
{
    uint8_t gie;

    gie = INTCONbits.GIE;
    INTCONbits.GIE = 0;

    abrir_pps();

    RX1PPS = 0x15;
    RC4PPS = 0x0F;
    RC2PPS = 0x0D;
    RC1PPS = 0x0E;

    cerrar_pps();
    INTCONbits.GIE = gie;
}

/* Oscilador, GPIO y PPS. */
void Pines_Iniciar(void)
{
    iniciar_oscilador();
    iniciar_gpio();
    iniciar_pps();
}
