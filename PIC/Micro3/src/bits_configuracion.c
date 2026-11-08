#include <xc.h>

/* 32 MHz internos, sin cristal. MCLR para el SNAP.
   Watchdog apagado en las pruebas. */

#pragma config FEXTOSC = OFF
#pragma config RSTOSC = HFINT32
#pragma config CLKOUTEN = OFF
#pragma config CSWEN = ON
#pragma config FCMEN = OFF

#pragma config MCLRE = ON
#pragma config PWRTS = PWRT_64
#pragma config LPBOREN = OFF
#pragma config BOREN = ON
#pragma config BORV = HI
#pragma config ZCD = OFF
#pragma config PPS1WAY = OFF
#pragma config STVREN = ON
#pragma config DEBUG = OFF

#pragma config WDTCPS = WDTCPS_31
#pragma config WDTE = OFF
#pragma config WDTCWS = WDTCWS_7
#pragma config WDTCCS = SC

#pragma config BBSIZE = BB512
#pragma config BBEN = OFF
#pragma config SAFEN = OFF
#pragma config WRTAPP = OFF
#pragma config WRTB = OFF
#pragma config WRTC = OFF
#pragma config WRTD = OFF
#pragma config WRTSAF = OFF
#pragma config LVP = ON

#pragma config CP = OFF
