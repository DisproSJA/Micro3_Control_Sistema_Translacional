#include <xc.h>
#include "pines.h"
#include "uart.h"
#include "adc.h"
#include "temporizador.h"
#include "encoder.h"
#include "control_pid.h"
#include "drv8871_pwm.h"
#include "referencia.h"
#include "protocolo.h"
#include "telemetria.h"
#include "interrupciones.h"

void main(void)
{
    uint8_t salida;
    int32_t consigna;
    int32_t posicion;
    int16_t milivoltios;

    Pines_Iniciar();              /* 32 MHz, pines y PPS */
    Uart_Iniciar();               /* 115200 8N1 */
    Adc_Iniciar();                /* RA2, FVR 2.048 V */
    Pwm_Iniciar();                /* 31.25 kHz, motor suelto */
    Encoder_Iniciar();            /* cuadratura x2 en RC3 */
    Temporizador_Iniciar();       /* muestra de 150 Hz */
    Control_Iniciar();            /* Kp 1.5, Ki 0, Kd 0.07 */
    Referencia_Iniciar();         /* Z busca CUENTAS_HOME */
    Protocolo_Iniciar();
    Interrupciones_Habilitar();   /* encoder, Timer0, RX y ADC */

    Uart_EnviarTexto("RDY\r\n");

    while (1) {
        Protocolo_Procesar();     /* si llego una linea, la ejecuta */

        if (Temporizador_TomarMuestra()) {
            Adc_Disparar();
            salida = Referencia_Actualizar(&consigna, &milivoltios);
            posicion = Encoder_Posicion();

            if (salida == REF_PID) {
                milivoltios = Control_Calcular(consigna, posicion);
                Pwm_AplicarMv(milivoltios);
            } else if (salida == REF_VOLTAJE) {
                Pwm_AplicarMv(milivoltios);
            } else {
                Pwm_Soltar();
                milivoltios = 0;
            }

            Telemetria_Enviar(consigna, posicion, milivoltios);
            Protocolo_AvisoHome(Referencia_TomarAviso());
        }
    }
}
