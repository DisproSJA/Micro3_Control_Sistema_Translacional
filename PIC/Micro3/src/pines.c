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
       Motor 1 (RC0, RA5) y motor 2 (RC1, RC2) arrancan en bajo, sueltos. */
    LATA = 0x10;
    LATB = 0;
    LATC = 0;

    /* RA0/RA1 ICSP, RA2 ADC, RA3 MCLR. RA4 enable, RA5 IN2 del motor 1. */
    TRISA = 0x0F;
    /* RB7 encoder B. RB4..RB6 sensores 5, 4 y 3, sin usar. */
    TRISB = 0xFF;
    /* RC0 IN1 motor 1, RC1/RC2 motor 2 sin usar, RC4 TX: salidas.
       RC5 RX, RC7 encoder A. RC3 y RC6 (sensor 1) sin usar. */
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

/* TX en RC4, PWM6 en RC0 (IN1), PWM7 en RA5 (IN2). RX entra por RC5.
   T1CKI y CLCIN0 solo aceptan puertos A y C; T1G y T5G, B y C.
   RC1 y RC2 quedan con su latch en bajo. */
static void iniciar_pps(void)
{
    uint8_t gie;

    gie = INTCONbits.GIE;
    INTCONbits.GIE = 0;

    abrir_pps();

    RX1PPS = 0x15;

    /* Encoder: A (RC7) a Timer1 y a la CLC1, B (RB7) a las compuertas. */
    T1CKIPPS = 0x17;
    CLCIN0PPS = 0x17;
    T1GPPS = 0x0F;
    T5GPPS = 0x0F;
    RC4PPS = 0x0F;
    RC0PPS = 0x0D;
    RA5PPS = 0x0E;
    RC1PPS = 0x00;
    RC2PPS = 0x00;

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
