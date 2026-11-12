#include "referencia.h"
#include "configuracion.h"
#include "encoder.h"
#include "control_pid.h"

#define MODO_PARADO                     0U
#define MODO_DIGITAL                    1U
#define MODO_ANALOGA                    2U

static uint8_t modo;
static uint8_t homed;
static uint8_t aviso;
static int32_t consigna;
static int32_t lim_min;
static int32_t lim_max;

/* Deja la consigna entre los dos finales de carrera. */
static int32_t acotar(int32_t cuentas)
{
    if (cuentas > lim_max) {
        return lim_max;
    }
    if (cuentas < lim_min) {
        return lim_min;
    }
    return cuentas;
}

/* 0 cuentas = -120 mm, 2048 = 0 mm, 4095 = +120 mm. Una vuelta del pot. */
static int32_t consigna_adc(uint16_t lectura)
{
    int32_t cuentas;

    if (lectura > 4095U) {
        lectura = 4095U;
    }
    cuentas = ((int32_t)lectura * (2L * CUENTAS_MAX)) / 4095L;
    cuentas -= CUENTAS_MAX;
    if (cuentas > CUENTAS_MAX) {
        cuentas = CUENTAS_MAX;
    } else if (cuentas < CUENTAS_MIN) {
        cuentas = CUENTAS_MIN;
    }
    return cuentas;
}

/* El punto donde esta el carro al encender queda en 0 mm. */
void Referencia_Iniciar(void)
{
    Encoder_FijarCero();
    modo = MODO_PARADO;
    homed = 1;
    aviso = REF_AVISO_NINGUNO;
    consigna = 0;
    lim_min = CUENTAS_MIN;
    lim_max = CUENTAS_MAX;
}

/* Suelta el motor y olvida I y D. */
uint8_t Referencia_Parar(void)
{
    modo = MODO_PARADO;
    Control_Reiniciar();
    return 0;
}

/* Ya no hay falla que borrar. Se deja por el comando X. */
uint8_t Referencia_BorrarFallo(void)
{
    return 0;
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
    if ((decimas < -1200) || (decimas > 1200)) {
        return 0;
    }

    cuentas = (int32_t)decimas * (CUENTAS_POR_MM / 10L);
    consigna = acotar(cuentas);
    modo = MODO_DIGITAL;
    return 1;
}

/* Los topes nuevos. El minimo tiene que quedar por debajo del maximo. */
uint8_t Referencia_FijarLimites(int16_t min_d, int16_t max_d)
{
    if ((min_d < -1200) || (max_d > 1200) || (min_d >= max_d)) {
        return 0;
    }
    lim_min = (int32_t)min_d * (CUENTAS_POR_MM / 10L);
    lim_max = (int32_t)max_d * (CUENTAS_POR_MM / 10L);
    consigna = acotar(consigna);
    return 1;
}

/* El encoder mide el centro. Hacia el lado de la consigna se suman 19.8 mm. */
int32_t Referencia_Posicion(int32_t cruda)
{
    if (consigna > 0) {
        return cruda + DESPLAZAMIENTO_CUENTAS;
    }
    if (consigna < 0) {
        return cruda - DESPLAZAMIENTO_CUENTAS;
    }
    return cruda;
}

/* En el tope no se empuja mas hacia afuera. Hacia adentro si se puede. */
int16_t Referencia_AplicarFin(int32_t posicion, int16_t milivoltios)
{
    if ((posicion >= lim_max) && (milivoltios > 0)) {
        return 0;
    }
    if ((posicion <= lim_min) && (milivoltios < 0)) {
        return 0;
    }
    return milivoltios;
}

/* Z lleva el carro a 0 mm, el punto que se guardo al encender. */
uint8_t Referencia_IrAlHome(void)
{
    consigna = acotar(0);
    modo = MODO_DIGITAL;
    return REF_Z_OK;
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

/* Cada Ts. En analogica la consigna sale del potenciometro. */
uint8_t Referencia_Actualizar(int32_t *consigna_out, int16_t *milivoltios,
                              uint16_t adc)
{
    *consigna_out = consigna;
    *milivoltios = 0;

    if (!homed || (modo == MODO_PARADO)) {
        return REF_COAST;
    }

    if (modo == MODO_ANALOGA) {
        consigna = acotar(consigna_adc(adc));
        *consigna_out = consigna;
    }

    return REF_PID;
}
