#include "control_pid.h"
#include "configuracion.h"

/* N = 1/6.8983e-5 y Ts = 1/150. El polo queda en 1/(1+N*Ts) ≈ 8/781.
   14496 es N en rad/s, para armar el termino Kd*N*dq en mV. */
#define DEN_NUM                         8L
#define DEN_DEN                         781L
#define N_RAD_S                         14496L
#define DOS_SOBRE_TS                    (2L * (int32_t)FS_HZ)

static int16_t kp_milli;
static int16_t ki_milli;
static int16_t kd_milli;
static int32_t i_mv;
static int32_t d_mv;
static int32_t e_prev;
static int32_t y_prev;
static uint8_t primera;

/* Deja el estado en cero y carga las ganancias del preinforme. */
void Control_Iniciar(void)
{
    kp_milli = KP_MILLI_INICIAL;
    ki_milli = KI_MILLI_INICIAL;
    kd_milli = KD_MILLI_INICIAL;
    Control_Reiniciar();
}

/* Borra I, D y el sample anterior. Se llama en S y al fijar el cero. */
void Control_Reiniciar(void)
{
    i_mv = 0;
    d_mv = 0;
    e_prev = 0;
    y_prev = 0;
    primera = 1;
}

/* 0 si se sale de 0 a 20 V/mm. */
uint8_t Control_FijarKp(int16_t valor)
{
    if ((valor < 0) || (valor > 20000)) {
        return 0;
    }
    kp_milli = valor;
    return 1;
}

/* 0 si se sale de 0 a 20. Al pasar a cero se tira el integrador. */
uint8_t Control_FijarKi(int16_t valor)
{
    if ((valor < 0) || (valor > 20000)) {
        return 0;
    }
    ki_milli = valor;
    if (valor == 0) {
        i_mv = 0;
    }
    return 1;
}

/* 0 si se sale de 0 a 2. Mas alto y el entero del filtro no da. */
uint8_t Control_FijarKd(int16_t valor)
{
    if ((valor < 0) || (valor > 2000)) {
        return 0;
    }
    kd_milli = valor;
    return 1;
}

/* Una muestra. Devuelve voltios en mV, ya recortados a ±tope. */
int16_t Control_Calcular(int32_t consigna, int32_t posicion, int16_t tope)
{
    int32_t error;
    int32_t p_mv;
    int32_t coef;
    int32_t dq;
    int32_t suma;
    int32_t u;
    int32_t i_antes;
    uint8_t congelar;

    error = consigna - posicion;
    if (error > (2L * CUENTAS_MAX)) {
        error = 2L * CUENTAS_MAX;
    } else if (error < (2L * CUENTAS_MIN)) {
        error = 2L * CUENTAS_MIN;
    }
    p_mv = ((int32_t)kp_milli * error) / CUENTAS_POR_MM;

    /* Si se acaba de bajar el tope, el integrador no se queda por encima. */
    if (i_mv > tope) {
        i_mv = tope;
    } else if (i_mv < -tope) {
        i_mv = -tope;
    }

    i_antes = i_mv;
    if (ki_milli == 0) {
        i_mv = 0;
    } else {
        i_mv += ((int32_t)ki_milli * (error + e_prev)) / (DOS_SOBRE_TS * CUENTAS_POR_MM);
        if (i_mv > tope) {
            i_mv = tope;
        } else if (i_mv < -tope) {
            i_mv = -tope;
        }
    }

    if (primera) {
        dq = 0;
        primera = 0;
    } else {
        /* q = -y, asi que el incremento es la posicion al reves. */
        dq = y_prev - posicion;
        if (dq > 600) {
            dq = 600;
        } else if (dq < -600) {
            dq = -600;
        }
    }

    coef = ((int32_t)kd_milli * N_RAD_S) / CUENTAS_POR_MM;
    suma = d_mv + (coef * dq);
    if (suma > 200000000L) {
        suma = 200000000L;
    } else if (suma < -200000000L) {
        suma = -200000000L;
    }
    d_mv = (suma * DEN_NUM) / DEN_DEN;

    u = p_mv + i_mv + d_mv;
    congelar = 0;
    if (u > tope) {
        u = tope;
        if (error > 0) {
            congelar = 1;
        }
    } else if (u < -tope) {
        u = -tope;
        if (error < 0) {
            congelar = 1;
        }
    }

    /* Si el integrador empujaba mas contra el tope, se devuelve. */
    if (congelar) {
        i_mv = i_antes;
        u = p_mv + i_mv + d_mv;
        if (u > tope) {
            u = tope;
        } else if (u < -tope) {
            u = -tope;
        }
    }

    e_prev = error;
    y_prev = posicion;
    return (int16_t)u;
}
