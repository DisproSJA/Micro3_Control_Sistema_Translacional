#include <xc.h>
#include "uart.h"
#include "configuracion.h"

static volatile uint8_t buf_rx[TAM_RX];
static volatile uint8_t buf_tx[TAM_TX];
static volatile uint8_t cabeza_rx;
static volatile uint8_t cola_rx;
static volatile uint8_t cabeza_tx;
static volatile uint8_t cola_tx;

/* Es macro porque la usan la ISR y el main. Con -mno-stackcall
   una funcion compartida se pisa. */
#define SIGUIENTE(indice, tam) \
    ((uint8_t)((((indice) + 1U) >= (tam)) ? 0U : ((indice) + 1U)))

/* 115200 8N1. SPBRG = FOSC/(4*baudios) - 1 = 68. */
void Uart_Iniciar(void)
{
    uint16_t spbrg;

    spbrg = (uint16_t)((FOSC_HZ / (4UL * BAUDIOS_UART)) - 1UL);

    TX1STAbits.SYNC = 0;
    TX1STAbits.BRGH = 1;
    BAUD1CONbits.BRG16 = 1;
    SP1BRGH = (uint8_t)(spbrg >> 8);
    SP1BRGL = (uint8_t)spbrg;

    TX1STAbits.TX9 = 0;
    RC1STAbits.RX9 = 0;

    RC1STAbits.SPEN = 1;
    TX1STAbits.TXEN = 1;
    RC1STAbits.CREN = 1;

    cabeza_rx = 0;
    cola_rx = 0;
    cabeza_tx = 0;
    cola_tx = 0;
}

/* Mete el byte en el buffer. Si se llena, se bota. */
void Uart_ISR_Recepcion(void)
{
    uint8_t dato;
    uint8_t siguiente;

    if (RC1STAbits.OERR) {
        RC1STAbits.CREN = 0;
        RC1STAbits.CREN = 1;
    }

    dato = RC1REG;

    if (RC1STAbits.FERR) {
        return;
    }

    siguiente = SIGUIENTE(cabeza_rx, TAM_RX);
    if (siguiente == cola_rx) {
        return;
    }

    buf_rx[cabeza_rx] = dato;
    cabeza_rx = siguiente;
}

/* Saca un byte. Apaga TXIE cuando el buffer queda vacio. */
void Uart_ISR_Transmision(void)
{
    if (cabeza_tx == cola_tx) {
        PIE3bits.TX1IE = 0;
        return;
    }

    TX1REG = buf_tx[cola_tx];
    cola_tx = SIGUIENTE(cola_tx, TAM_TX);
}

/* Saca un byte recibido. 0 si no hay. */
uint8_t Uart_LeerByte(uint8_t *dato)
{
    if (cabeza_rx == cola_rx) {
        return 0;
    }

    *dato = buf_rx[cola_rx];
    cola_rx = SIGUIENTE(cola_rx, TAM_RX);
    return 1;
}

/* Bytes libres en TX. Se mira desde el main, con la ISR apagada un momento. */
uint8_t Uart_Espacio(void)
{
    uint8_t usados;
    uint8_t gie;

    gie = INTCONbits.GIE;
    INTCONbits.GIE = 0;
    if (cabeza_tx >= cola_tx) {
        usados = (uint8_t)(cabeza_tx - cola_tx);
    } else {
        usados = (uint8_t)(TAM_TX - (cola_tx - cabeza_tx));
    }
    INTCONbits.GIE = gie;

    return (uint8_t)((TAM_TX - 1U) - usados);
}

/* Encola un byte y prende la interrupcion de TX. 0 si no cupo. */
uint8_t Uart_EscribirByte(uint8_t dato)
{
    uint8_t siguiente;

    siguiente = SIGUIENTE(cabeza_tx, TAM_TX);
    if (siguiente == cola_tx) {
        return 0;
    }

    buf_tx[cabeza_tx] = dato;
    cabeza_tx = siguiente;
    PIE3bits.TX1IE = 1;
    return 1;
}

/* Encola un texto. 0 si el buffer se lleno a medias. */
uint8_t Uart_EnviarTexto(const char *texto)
{
    while (*texto != '\0') {
        if (!Uart_EscribirByte((uint8_t)*texto)) {
            return 0;
        }
        texto++;
    }
    return 1;
}
