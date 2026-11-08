#include "protocolo.h"
#include "configuracion.h"
#include "uart.h"
#include "referencia.h"
#include "control_pid.h"
#include "encoder.h"

static char linea[LARGO_LINEA];
static uint8_t largo;

/* Mayuscula, para no depender de como llegue el comando. */
static char mayus(char c)
{
    if ((c >= 'a') && (c <= 'z')) {
        return (char)(c - ('a' - 'A'));
    }
    return c;
}

static void responder(const char *texto)
{
    Uart_EnviarTexto(texto);
}

/* Lee un numero con hasta 3 decimales y lo deja en milesimas. */
static uint8_t leer_milesimas(const char *s, int32_t *milesimas)
{
    uint32_t entero;
    uint32_t frac;
    uint8_t digitos;
    uint8_t alguno;
    uint8_t negativo;

    entero = 0;
    frac = 0;
    digitos = 0;
    alguno = 0;
    negativo = 0;

    while (*s == ' ') {
        s++;
    }
    if (*s == '+') {
        s++;
    } else if (*s == '-') {
        negativo = 1;
        s++;
    }

    while ((*s >= '0') && (*s <= '9')) {
        alguno = 1;
        entero = (entero * 10UL) + (uint32_t)(*s - '0');
        if (entero > 100UL) {
            return 0;
        }
        s++;
    }

    if (*s == '.') {
        s++;
        while ((*s >= '0') && (*s <= '9') && (digitos < 3U)) {
            alguno = 1;
            frac = (frac * 10UL) + (uint32_t)(*s - '0');
            digitos++;
            s++;
        }
        while (digitos < 3U) {
            frac = frac * 10UL;
            digitos++;
        }
        /* Si mandan mas de 3 decimales, se ignoran. */
        while ((*s >= '0') && (*s <= '9')) {
            s++;
        }
    }

    while (*s == ' ') {
        s++;
    }
    if (!alguno || (*s != '\0')) {
        return 0;
    }

    *milesimas = (int32_t)((entero * 1000UL) + frac);
    if (negativo) {
        *milesimas = -(*milesimas);
    }
    return 1;
}

/* P10.5 -> 105 decimas. Se redondea si vienen mas decimales. */
static void comando_p(const char *s)
{
    int32_t milesimas;
    int16_t decimas;

    if (!Referencia_HayHome()) {
        responder("ERR HOME\r\n");
        return;
    }
    if (!leer_milesimas(s, &milesimas)) {
        responder("ERR VALOR\r\n");
        return;
    }

    if (milesimas >= 0) {
        decimas = (int16_t)((milesimas + 50L) / 100L);
    } else {
        decimas = (int16_t)(-(((-milesimas) + 50L) / 100L));
    }

    if (!Referencia_FijarDecimas(decimas)) {
        responder("ERR RANGO\r\n");
        return;
    }
    responder("OK\r\n");
}

/* Kp:1.5 guarda 1500. Kd no pasa de 2. */
static void comando_ganancia(char cual, const char *s)
{
    int32_t milesimas;
    int16_t milli;
    uint8_t bien;

    if (!leer_milesimas(s, &milesimas) || (milesimas < 0)) {
        responder("ERR VALOR\r\n");
        return;
    }
    if (milesimas > 20000L) {
        responder("ERR RANGO\r\n");
        return;
    }

    milli = (int16_t)milesimas;
    if (cual == 'P') {
        bien = Control_FijarKp(milli);
    } else if (cual == 'I') {
        bien = Control_FijarKi(milli);
    } else {
        bien = Control_FijarKd(milli);
    }

    if (!bien) {
        responder("ERR RANGO\r\n");
        return;
    }
    responder("OK\r\n");
}

/* ENC: y el conteo crudo. Ese numero se pega en CUENTAS_HOME. */
static void responder_conteo(int32_t valor)
{
    char buf[20];
    char digitos[11];
    uint8_t n;
    uint8_t k;
    uint32_t mag;

    n = 0;
    buf[n++] = 'E';
    buf[n++] = 'N';
    buf[n++] = 'C';
    buf[n++] = ':';

    if (valor < 0) {
        buf[n++] = '-';
        mag = (uint32_t)(-valor);
    } else {
        mag = (uint32_t)valor;
    }

    k = 0;
    do {
        digitos[k++] = (char)('0' + (mag % 10UL));
        mag = mag / 10UL;
    } while ((mag > 0UL) && (k < 11U));

    while (k > 0U) {
        k--;
        buf[n++] = digitos[k];
    }

    buf[n++] = '\r';
    buf[n++] = '\n';
    buf[n] = '\0';
    responder(buf);
}

/* Una linea ya cerrada con enter. */
static void ejecutar(void)
{
    char c0;
    char c1;

    if (largo == 0U) {
        return;
    }
    linea[largo] = '\0';

    c0 = mayus(linea[0]);
    c1 = mayus(linea[1]);

    if (c0 == 'P') {
        comando_p(&linea[1]);
    } else if ((c0 == 'S') && (linea[1] == '\0')) {
        Referencia_Parar();
        responder("OK\r\n");
    } else if ((c0 == 'C') && (linea[1] == '\0')) {
        responder_conteo(Encoder_Cuentas());
    } else if ((c0 == 'Z') && (linea[1] == '\0')) {
        Referencia_IniciarHome();
        responder("OK\r\n");
    } else if ((c0 == 'D') && (linea[1] == '\0')) {
        if (!Referencia_HayHome()) {
            responder("ERR HOME\r\n");
        } else {
            Referencia_FuenteDigital();
            responder("OK\r\n");
        }
    } else if ((c0 == 'A') && (linea[1] == '\0')) {
        if (!Referencia_HayHome()) {
            responder("ERR HOME\r\n");
        } else {
            Referencia_FuenteAnalogica();
            responder("OK\r\n");
        }
    } else if ((c0 == 'K') && (largo >= 3U) && (linea[2] == ':') &&
               ((c1 == 'P') || (c1 == 'I') || (c1 == 'D'))) {
        comando_ganancia(c1, &linea[3]);
    } else {
        responder("ERR VALOR\r\n");
    }

    largo = 0;
}

/* Nada que armar todavia. */
void Protocolo_Iniciar(void)
{
    largo = 0;
}

/* Saca bytes hasta armar una linea. \r se bota. */
void Protocolo_Procesar(void)
{
    uint8_t dato;

    while (Uart_LeerByte(&dato)) {
        if (dato == '\n') {
            ejecutar();
        } else if (dato == '\r') {
            ;
        } else if (largo < (LARGO_LINEA - 1U)) {
            linea[largo++] = (char)dato;
        } else {
            largo = 0;
            responder("ERR VALOR\r\n");
        }
    }
}

/* El OK del Z ya salio. Esto avisa si llego al conteo o se rindio. */
void Protocolo_AvisoHome(uint8_t aviso)
{
    if (aviso == REF_AVISO_HOME) {
        responder("OK HOME\r\n");
    } else if (aviso == REF_AVISO_FALLO) {
        responder("ERR HOME\r\n");
    }
}
