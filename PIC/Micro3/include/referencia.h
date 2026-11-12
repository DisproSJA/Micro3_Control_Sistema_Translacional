#ifndef REFERENCIA_H
#define REFERENCIA_H

#include <stdint.h>

/* Que tiene que hacer el main con el motor en esta muestra. */
#define REF_COAST                   0U
#define REF_PID                     1U
#define REF_VOLTAJE                 2U

#define REF_AVISO_NINGUNO           0U
#define REF_AVISO_HOME              1U
#define REF_AVISO_FALLO             2U
#define REF_AVISO_PROTECCION        3U

#define REF_Z_OK                    0U
#define REF_Z_FALLO                 1U
#define REF_Z_MOTOR                 2U
#define REF_Z_MOV                   3U

void Referencia_Iniciar(void);

/* 1 si seguia en falla: el motor queda frenado. */
uint8_t Referencia_Parar(void);

/* 1 si habia falla y se borro. El cero sigue siendo el del encendido. */
uint8_t Referencia_BorrarFallo(void);
void Referencia_FuenteDigital(void);
void Referencia_FuenteAnalogica(void);
uint8_t Referencia_FijarDecimas(int16_t decimas);

/* Finales de carrera, en decimas de mm. 1 si el minimo queda debajo del maximo. */
uint8_t Referencia_FijarLimites(int16_t min_d, int16_t max_d);

/* 0 V si en el tope el PID empujaria mas hacia afuera. */
int16_t Referencia_AplicarFin(int32_t posicion, int16_t milivoltios);

/* Posicion del extremo del carrito: el centro medido, mas o menos 19.8 mm. */
int32_t Referencia_Posicion(int32_t cruda);

/* Z manda el carro al punto donde estaba al encender. */
uint8_t Referencia_IrAlHome(void);
uint8_t Referencia_HayHome(void);

/* Una muestra. adc es la lectura ya filtrada del potenciómetro. */
uint8_t Referencia_Actualizar(int32_t *consigna, int16_t *milivoltios,
                              uint16_t adc);

/* 8 V en analogica, 24 V en digital. */
int16_t Referencia_TopeMv(void);
uint8_t Referencia_TomarAviso(void);

#endif
