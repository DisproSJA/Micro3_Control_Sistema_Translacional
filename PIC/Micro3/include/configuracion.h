#ifndef CONFIGURACION_H
#define CONFIGURACION_H

/* Numeros del preinforme. Fs, tornillo, encoder y ganancias salen de ahi. */

#define FOSC_HZ                         32000000UL
#define _XTAL_FREQ                      FOSC_HZ

#define FS_HZ                           150UL
#define BAUDIOS_UART                    115200UL

/* 600 PPR, cuadratura x2: solo los flancos de A. 8 mm de avance. */
#define CUENTAS_POR_VUELTA              1200L
#define AVANCE_MM                       8L
#define CUENTAS_POR_MM                  (CUENTAS_POR_VUELTA / AVANCE_MM)

/* Si al probar Z el carro se aleja del 0, se cambia uno de estos dos. */
#define INVERTIR_GIRO                   0
#define INVERTIR_ENCODER                0

#define POS_MM_MAX                      120L
#define POS_MM_MIN                      (-120L)
#define CUENTAS_MAX                     (POS_MM_MAX * CUENTAS_POR_MM)
#define CUENTAS_MIN                     (POS_MM_MIN * CUENTAS_POR_MM)

/* La mitad del carrito queda 19.8 mm adelante del punto que mide el encoder. */
#define DESPLAZAMIENTO_CUENTAS          ((198L * CUENTAS_POR_MM) / 10L)

/* PD del paso 0 a 100 mm. Kp = 1.5 V/mm, Ki = 0, Kd = 0.07, los de la simulacion.
   Van en milesimas: 1.5 -> 1500, 0.07 -> 70. */
#define KP_MILLI_INICIAL                1500
#define KI_MILLI_INICIAL                0
#define KD_MILLI_INICIAL                70

/* La fuente es de 24 V. El PID no pasa de 12 V. El PWM sigue
   calculado sobre los 24 V. */
#define VM_MV                           24000
#define VLIM_MV                         12000

/* 65536 - 8000000/150. El timer cuenta Fosc/4. */
#define TMR0_RECARGA                    12203U

#define TAM_RX                          48U
#define TAM_TX                          96U
#define LARGO_LINEA                     28U

#endif
