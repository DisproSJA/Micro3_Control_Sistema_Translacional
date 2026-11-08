#ifndef UART_H
#define UART_H

#include <stdint.h>

/* 115200 8N1. La interrupcion solo mueve bytes; el main arma la linea. */

void Uart_Iniciar(void);
uint8_t Uart_LeerByte(uint8_t *dato);
uint8_t Uart_EscribirByte(uint8_t dato);
uint8_t Uart_EnviarTexto(const char *texto);
uint8_t Uart_Espacio(void);

void Uart_ISR_Recepcion(void);
void Uart_ISR_Transmision(void);

#endif
