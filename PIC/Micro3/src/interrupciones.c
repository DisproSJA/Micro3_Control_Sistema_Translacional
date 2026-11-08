#include <xc.h>
#include "interrupciones.h"
#include "encoder.h"
#include "temporizador.h"
#include "uart.h"
#include "adc.h"

/* Una sola entrada. El encoder va primero: es la que mas se repite. */
void __interrupt() ISR(void)
{
    if (PIE0bits.IOCIE && PIR0bits.IOCIF) {
        Encoder_ISR();
    }
    if (PIE0bits.TMR0IE && PIR0bits.TMR0IF) {
        Temporizador_ISR();
    }
    if (PIE3bits.RC1IE && PIR3bits.RC1IF) {
        Uart_ISR_Recepcion();
    }
    if (PIE3bits.TX1IE && PIR3bits.TX1IF) {
        Uart_ISR_Transmision();
    }
    if (PIE1bits.ADIE && PIR1bits.ADIF) {
        Adc_ISR();
    }
}

/* Prende encoder, Timer0, RX y ADC. TX se prende solo al enviar. */
void Interrupciones_Habilitar(void)
{
    PIR0bits.IOCIF = 0;
    PIR0bits.TMR0IF = 0;
    PIR1bits.ADIF = 0;
    PIR3bits.RC1IF = 0;
    PIR3bits.TX1IF = 0;

    PIE0bits.IOCIE = 1;
    PIE0bits.TMR0IE = 1;
    PIE1bits.ADIE = 1;
    PIE3bits.RC1IE = 1;
    PIE3bits.TX1IE = 0;

    INTCONbits.PEIE = 1;
    INTCONbits.GIE = 1;
}
