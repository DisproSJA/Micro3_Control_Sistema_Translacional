# Micro 3 Sistema de Control de Posición Translacional (Proyecto Diseño Mecatrónica)

Control de posición de un carro que se mueve sobre un tornillo. Un PIC16F18446 cierra el lazo; desde el PC se manda la consigna y se ve la respuesta.

Integrantes: Juan Andrés Sánchez, Sofía Vega y Andrés Felipe Trujillo.

El carro lleva 1 kg. El tornillo es TR8×8 de 4 entradas, así que avanza 8 mm por vuelta. El motor es un DC de escobillas (XD-3420, 24 V) manejado por un DRV8871. La posición se mide con un encoder incremental E38S6 de 600 pulsos por vuelta, montado en el tornillo. El recorrido útil es ±100.0 mm, con resolución de 0.1 mm.

La meta del lazo es seguir una consigna y, en un escalón de 0 a 50 mm, quedarse dentro de 1 mm en menos de 50 muestras. A 150 Hz eso son 0.333 s.

## Carpetas


| Carpeta      | Qué es                                |
| ------------ | ------------------------------------- |
| `Insumos`    | Enunciado de la práctica y preinforme |
| `PIC/Micro3` | Firmware del PIC16F18446              |
| `GUI`        | Interfaz en Python                    |


`Insumos/Practica_3_2026 2.pdf` es el enunciado. `Insumos/Preinforme_Micro_3_Guia_Lineal.pdf` es el preinforme: planta, simulación, pines y el plan de software. Los números del firmware (150 Hz, ganancias, 150 cuentas/mm) salen de ahí.

## Firmware (`PIC/Micro3`)

Se abre la carpeta `PIC/Micro3` con la extensión MPLAB. Dispositivo PIC16F18446, compilador XC8 v4.00, programador SNAP. Para compilar: **Ctrl + Shift + P** y `MPLAB CMake: Build`.

El oscilador es el interno a 32 MHz. Hay una sola interrupción. Atiende, en este orden, el encoder, el Timer0, la UART y el ADC. El `main` solo mira banderas y corre un paso del lazo cuando el timer marca muestra.


| Archivo                | Qué hace                                        |
| ---------------------- | ----------------------------------------------- |
| `main.c`               | Arranque y lazo de 150 Hz                       |
| `bits_configuracion.c` | Bits de configuración                           |
| `pines.c`              | Reloj, GPIO y PPS                               |
| `interrupciones.c`     | La ISR única                                    |
| `temporizador.c`       | Timer0, periodo 6.667 ms                        |
| `encoder.c`            | Cuadratura x2 en RC3. RC6 solo dice el sentido  |
| `adc.c`                | Potenciómetro en RA2, referencia 2.048 V        |
| `drv8871_pwm.c`        | PWM a 31.25 kHz hacia el DRV8871                |
| `control_pid.c`        | PID en enteros                                  |
| `referencia.c`         | Consigna digital, analógica o búsqueda del cero |
| `protocolo.c`          | Comandos por UART                               |
| `telemetria.c`         | La línea CSV de cada muestra                    |
| `configuracion.h`      | Fs, cuentas por mm, ganancias, `CUENTAS_HOME`   |


Pines que sí se usan:


| Señal           | Pin            |
| --------------- | -------------- |
| IN1 del DRV8871 | RC2, PWM6      |
| IN2 del DRV8871 | RC1, PWM7      |
| Encoder A       | RC3            |
| Encoder B       | RC6            |
| Potenciómetro   | RA2            |
| TX / RX         | RC4 / RC5      |
| ICSP            | RA0, RA1, MCLR |


El PWM usa decaimiento lento: el tiempo en alto frena y el tiempo en bajo mueve. Con la salida del PID en 0 V las dos entradas quedan en alto y el carro se sostiene. `S` las deja en bajo y el motor queda suelto.

### Encoder y cero

600 PPR y solo los flancos de A dan 1200 cuentas por vuelta. Con 8 mm de avance son 150 cuentas por milímetro.

El conteo vive en RAM. Cada vez que el PIC se enciende o se graba, arranca en 0, en el sitio donde esté el carro. Por eso el 0 mm físico se fija así:

1. Con todo apagado, deja el carro en el 0 mm real.
2. Enciende o graba el PIC sin moverlo.
3. Deja `CUENTAS_HOME` en 0, en `include/configuracion.h`.
4. `C` tiene que responder `ENC:0`.
5. `Z` mueve el carro hasta ese conteo y, al llegar a 0.1 mm, lo toma como home.

Quitar solo los 24 V del motor no borra el conteo: el PIC sigue alimentado. Si `Z` se aleja del cero, cambia `INVERTIR_GIRO` a 1 y vuelve a grabar. `INVERTIR_ENCODER` invierte el signo de las cuentas.

La posición que viaja por la telemetría y entra al PID es el conteo actual menos `CUENTAS_HOME`.

### PID

Es el de la simulación, sin `float`. La parte integral va por Tustin y la derivada por Euler hacia atrás, sobre la medición. N queda fijo. La salida se satura en ±24 V y, si satura, el integrador no sigue creciendo.

Valores iniciales: Kp = 1.5 V/mm, Ki = 0, Kd = 0.07. Ki arranca en 0 porque el modelo no trae la fricción de la guía; se puede subir en caliente con `Ki:`.

## Comandos

UART a 115200 8N1. Cada comando termina en Enter. Al arrancar el PIC manda `RDY`.


| Comando   | Qué hace                                                    |
| --------- | ----------------------------------------------------------- |
| `C`       | Responde `ENC:` y el conteo crudo                           |
| `Z`       | Busca `CUENTAS_HOME`. Responde `OK` y, al llegar, `OK HOME` |
| `P50.0`   | Consigna digital, de −100.0 a 100.0 mm, resolución 0.1 mm   |
| `S`       | Suelta el motor y borra el estado del PID                   |
| `D`       | Vuelve a la última consigna digital                         |
| `A`       | La consigna pasa al potenciómetro. 1.024 V es 0 mm          |
| `Kp:1.5`  | Proporcional, de 0 a 20                                     |
| `Ki:0`    | Integral, de 0 a 20                                         |
| `Kd:0.07` | Derivativa, de 0 a 2                                        |


`P`, `D` y `A` responden `ERR HOME` hasta que un `Z` haya terminado bien. Si la búsqueda no llega en 8 s, también responde `ERR HOME` y suelta el motor. Un número mal escrito o fuera de rango responde `ERR VALOR` o `ERR RANGO`.

Cada muestra, si hay espacio en el buffer de TX, sale:

```text
consigna_mm,posicion_mm,error_mm,salida_V
```

Los milímetros van con un decimal y los voltios con dos. Los `OK` y los `ERR` no llevan números, para no meterse en la gráfica.

## GUI (`GUI/interfaz_guia_lineal.py`)

```text
pip install pyserial matplotlib pyvisa pyvisa-py
py -3 GUI\interfaz_guia_lineal.py
```

Arriba se elige el puerto COM y se conecta a 115200. A la izquierda están los comandos: leer encoder, home, parar, consigna digital o analógica, posición y las tres ganancias. También está el control de la fuente por VISA, sugerida en 24 V y 2 A.

A la derecha la gráfica muestra la referencia en naranja y la posición medida en cian. Al enviar `P` el tiempo vuelve a cero, así el escalón queda al inicio de una ventana de 10 s. Debajo hay una regla de −100 a 100 mm: el triángulo naranja es la consigna y el bloque cian es el carro. El terminal solo anota comandos, `OK`, errores y el `ENC:` de `C`. Las 150 muestras por segundo no se escriben ahí.

`pyvisa` solo hace falta para la fuente. Sin `pyserial` no hay puerto. Sin `matplotlib` no hay gráfica.