#include <xc.h>
#include "interrupciones.h"
#include "temporizador.h"
#include "uart.h"
#include "adc.h"

/* Una sola entrada. El encoder ya no interrumpe: lo cuentan los timers. */
void __interrupt() ISR(void)
{
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

/* Prende Timer0, RX y ADC. TX se prende solo al enviar. */
void Interrupciones_Habilitar(void)
{
    PIR0bits.TMR0IF = 0;
    PIR1bits.ADIF = 0;
    PIR3bits.RC1IF = 0;
    PIR3bits.TX1IF = 0;

    PIE0bits.TMR0IE = 1;
    PIE1bits.ADIE = 1;
    PIE3bits.RC1IE = 1;
    PIE3bits.TX1IE = 0;

    INTCONbits.PEIE = 1;
    INTCONbits.GIE = 1;
}
