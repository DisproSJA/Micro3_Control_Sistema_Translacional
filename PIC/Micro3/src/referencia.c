#include "referencia.h"
#include "configuracion.h"
#include "encoder.h"
#include "adc.h"
#include "control_pid.h"

#define MODO_PARADO                     0U
#define MODO_DIGITAL                    1U
#define MODO_ANALOGA                    2U
#define MODO_HOME                       3U

static uint8_t modo;
static uint8_t homed;
static uint8_t aviso;
static uint16_t muestras;
static int32_t consigna;

/* Pot de 0 a 2.048 V. 0 cuentas del ADC son -100 mm. */
static int32_t consigna_adc(void)
{
    int32_t cuentas;
    uint16_t lectura;

    lectura = Adc_Ver();
    /* 0 del ADC es -100 mm y 2048 es el centro. */
    cuentas = ((int32_t)lectura - 2048L) * CUENTAS_MAX / 2048L;
    if (cuentas > CUENTAS_MAX) {
        cuentas = CUENTAS_MAX;
    } else if (cuentas < CUENTAS_MIN) {
        cuentas = CUENTAS_MIN;
    }
    return cuentas;
}

/* Arranca sin cero. El encoder mide desde el Z, no desde el encendido. */
void Referencia_Iniciar(void)
{
    modo = MODO_PARADO;
    homed = 0;
    aviso = REF_AVISO_NINGUNO;
    muestras = 0;
    consigna = 0;
}

/* Suelta el motor y olvida I y D. */
void Referencia_Parar(void)
{
    modo = MODO_PARADO;
    Control_Reiniciar();
}

/* La consigna pasa a ser la ultima P. Si no hay cero, no corre. */
void Referencia_FuenteDigital(void)
{
    if (!homed) {
        return;
    }
    modo = MODO_DIGITAL;
}

/* La consigna pasa al potenciometro. */
void Referencia_FuenteAnalogica(void)
{
    if (!homed) {
        return;
    }
    modo = MODO_ANALOGA;
}

/* P en decimas de mm. 0 si no hay cero o si se sale de ±100. */
uint8_t Referencia_FijarDecimas(int16_t decimas)
{
    int32_t cuentas;

    if (!homed) {
        return 0;
    }
    if ((decimas < -1000) || (decimas > 1000)) {
        return 0;
    }

    cuentas = (int32_t)decimas * (CUENTAS_POR_MM / 10L);
    consigna = cuentas;
    modo = MODO_DIGITAL;
    return 1;
}

/* Z suelta el PID viejo y sale a buscar CUENTAS_HOME. */
void Referencia_IniciarHome(void)
{
    Control_Reiniciar();
    modo = MODO_HOME;
    homed = 0;
    aviso = REF_AVISO_NINGUNO;
    muestras = 0;
    consigna = 0;
}

/* 1 despues de un Z. */
uint8_t Referencia_HayHome(void)
{
    return homed;
}

/* 1 una sola vez, cuando Z deja el encoder en cero. */
uint8_t Referencia_TomarAviso(void)
{
    uint8_t copia;

    copia = aviso;
    aviso = REF_AVISO_NINGUNO;
    return copia;
}

/* Cada Ts. En home la consigna es 0 mm, o sea el conteo quemado. */
uint8_t Referencia_Actualizar(int32_t *consigna_out, int16_t *milivoltios)
{
    int32_t pos;

    *consigna_out = consigna;
    *milivoltios = 0;

    if (modo == MODO_HOME) {
        pos = Encoder_Posicion();
        muestras++;
        if ((pos <= HOME_TOL_CUENTAS) && (pos >= -HOME_TOL_CUENTAS)) {
            homed = 1;
            modo = MODO_DIGITAL;
            consigna = 0;
            aviso = REF_AVISO_HOME;
        } else if (muestras >= HOME_TIMEOUT_MUESTRAS) {
            modo = MODO_PARADO;
            aviso = REF_AVISO_FALLO;
            return REF_COAST;
        }
        *consigna_out = consigna;
        return REF_PID;
    }

    if (!homed || (modo == MODO_PARADO)) {
        return REF_COAST;
    }

    if (modo == MODO_ANALOGA) {
        consigna = consigna_adc();
        *consigna_out = consigna;
    }

    return REF_PID;
}
