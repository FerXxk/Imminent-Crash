/*
 * lcd_uart.h
 *
 *  Created on: 15/12/2014
 *      Author: Manolo
 */



/* Funciones para el manejo de la consola a través de la UART*/

void UARTinit(char vel);
void UARTprintc(char c);
void UARTprint(const char * frase);
void UARTprintCR(const char *frase);
void UARTgets( char *BuffRx, int TMAX);
int  UARTgetint(void);


