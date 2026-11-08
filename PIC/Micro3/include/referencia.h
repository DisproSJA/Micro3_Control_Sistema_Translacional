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

void Referencia_Iniciar(void);
void Referencia_Parar(void);
void Referencia_FuenteDigital(void);
void Referencia_FuenteAnalogica(void);
uint8_t Referencia_FijarDecimas(int16_t decimas);
void Referencia_IniciarHome(void);
uint8_t Referencia_HayHome(void);

/* Una muestra. Llena la consigna en cuentas del encoder. */
uint8_t Referencia_Actualizar(int32_t *consigna, int16_t *milivoltios);
uint8_t Referencia_TomarAviso(void);

#endif
