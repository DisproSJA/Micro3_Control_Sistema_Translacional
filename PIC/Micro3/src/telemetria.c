#include "telemetria.h"
#include "configuracion.h"
#include "uart.h"

/* 15 cuentas son 0.1 mm. Se arma el texto a mano, sin printf. */
static uint8_t poner_char(char *buf, uint8_t n, char c)
{
    if (n < 40U) {
        buf[n] = c;
    }
    return (uint8_t)(n + 1U);
}

static uint8_t poner_u16(char *buf, uint8_t n, uint16_t valor)
{
    char tmp[5];
    uint8_t i;

    i = 0;
    do {
        tmp[i++] = (char)('0' + (valor % 10U));
        valor = (uint16_t)(valor / 10U);
    } while ((valor > 0U) && (i < 5U));

    while (i > 0U) {
        i--;
        n = poner_char(buf, n, tmp[i]);
    }
    return n;
}

/* mm con un decimal, a partir de las cuentas del tornillo. */
static uint8_t poner_mm(char *buf, uint8_t n, int32_t cuentas)
{
    int32_t decimas;
    uint16_t entero;
    uint8_t frac;

    decimas = cuentas / (CUENTAS_POR_MM / 10L);
    if (decimas < 0) {
        n = poner_char(buf, n, '-');
        decimas = -decimas;
    }
    entero = (uint16_t)(decimas / 10L);
    frac = (uint8_t)(decimas % 10L);
    n = poner_u16(buf, n, entero);
    n = poner_char(buf, n, '.');
    n = poner_char(buf, n, (char)('0' + frac));
    return n;
}

/* Voltios con dos decimales. */
static uint8_t poner_voltios(char *buf, uint8_t n, int16_t milivoltios)
{
    int16_t centi;
    uint16_t entero;
    uint8_t frac;

    centi = (int16_t)(milivoltios / 10);
    if (centi < 0) {
        n = poner_char(buf, n, '-');
        centi = (int16_t)(-centi);
    }
    entero = (uint16_t)(centi / 100);
    frac = (uint8_t)(centi % 100);
    n = poner_u16(buf, n, entero);
    n = poner_char(buf, n, '.');
    n = poner_char(buf, n, (char)('0' + (frac / 10U)));
    n = poner_char(buf, n, (char)('0' + (frac % 10U)));
    return n;
}

/* consigna, posicion, error, salida, ADC. Si no cabe, se salta. */
void Telemetria_Enviar(int32_t consigna, int32_t posicion, int16_t milivoltios,
                       uint16_t adc)
{
    char buf[48];
    uint8_t n;
    uint8_t i;

    n = 0;
    n = poner_mm(buf, n, consigna);
    n = poner_char(buf, n, ',');
    n = poner_mm(buf, n, posicion);
    n = poner_char(buf, n, ',');
    n = poner_mm(buf, n, consigna - posicion);
    n = poner_char(buf, n, ',');
    n = poner_voltios(buf, n, milivoltios);
    n = poner_char(buf, n, ',');
    n = poner_u16(buf, n, adc);
    n = poner_char(buf, n, '\r');
    n = poner_char(buf, n, '\n');
    if (n > 47U) {
        n = 47U;
    }
    buf[n] = '\0';

    if (Uart_Espacio() < n) {
        return;
    }
    for (i = 0; i < n; i++) {
        Uart_EscribirByte((uint8_t)buf[i]);
    }
}
