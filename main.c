#include <msp430.h> 
#include "grlib.h"
#include "uart_STDIO.h"
#include "Crystalfontz128x128_ST7735.h"
#include "HAL_MSP430G2_Crystalfontz128x128_ST7735.h"
#include <stdio.h>
#include "partituras.h"

void conf_reloj(char VEL){
    BCSCTL2 = SELM_0 | DIVM_0 | DIVS_0;
    switch(VEL){
    case 16:
        if (CALBC1_16MHZ != 0xFF) {
            __delay_cycles(100000);
            DCOCTL = 0x00;
            BCSCTL1 = CALBC1_16MHZ;     /* Set DCO to 16MHz */
            DCOCTL = CALDCO_16MHZ;
        }
        break;
    }
    BCSCTL1 |= XT2OFF | DIVA_0;
    BCSCTL3 = XT2S_0 | LFXT1S_2 | XCAP_1;

}

void inicia_ADC(char canales){
    ADC10CTL0 &= ~ENC;//deshabilita ADC
    ADC10CTL0 = ADC10ON | ADC10SHT_3 | SREF_0|ADC10IE;
    //enciende ADC, S/H 64clk’s, REF:VCC-GND, con INT
    ADC10CTL1 = CONSEQ_0 | ADC10SSEL_0 | ADC10DIV_0 | SHS_0 | INCH_0;
    //Modo simple, reloj ADC, sin subdivision, Disparo soft, Canal 0
    ADC10AE0 = canales; //habilita los canales indicados
    ADC10CTL0 |= ENC; //Habilita el ADC
}

int lee_ch(char canal){
    ADC10CTL0 &= ~ENC;//deshabilita el ADC
    ADC10CTL1&=(0x0fff);//Borra canal anterior
    ADC10CTL1|=canal<<12;//selecciona nuevo canal
    ADC10CTL0|= ENC;//Habilita el ADC
    ADC10CTL0|=ADC10SC;//Empieza la conversión
    LPM0;//Espera fin en modo LPM0
    return(ADC10MEM);//Devuelve valor leido
}


// DECLARACIONES

Graphics_Context g_sContext;



char *Puntero3= (char *)(0x1000); // Puntero que apunta a la direccion de memoria donde guardamos el highscore
char cadena[20];
int avionx=26,posx1=26,avionxant=26,posbarra=0,posbarra2=-10,ampliacion=0,nivel2=0,segunda=0;// Posicion del avion y de las barras, y parámetros de los niveles
char mute=0; // Variable para mutear el juego. De serie comienza con sonido
int puntuacion=0,highscore=0; // Puntuaciones
volatile char caracter=0;  // EL caracter que recibimos por puerto serie
int ejex,t=0,tms=0,t2=0,tmus=0; // Posicion del joystick y diferentes temporizadores
char limizq,limder,limizq2,limder2; // Límites de las barras
long int coloravion=GRAPHICS_COLOR_GRAY,colormuro,colorfondo,colorventana=0x0093D6FA,colorarmas=GRAPHICS_COLOR_BLACK; // Colores que modificaremos en funcion del mapa
unsigned char estado=0,i; // Maquina de estados y la i para los bucles for
enum pantallas{inicio,opciones,premapas,mapas,juego,premuerte,muerte,preprejuego,prejuego,prejuego2,prejuego3}; // Estados de la maquina de estados




int main(void) {

    WDTCTL = WDTPW | WDTHOLD;   // Stop watchdog timer
    conf_reloj(16); // Configuración del reloj

    FCTL2 = FWKEY + FSSEL_2 + 34; // Habilitamos el guardado en la flash

    // Timer 100 ms
    TA1CTL=TASSEL_2|ID_3| MC_1;         //SMCLK, DIV=8 (2MHz), UP
    TA1CCR0=19999;       //periodo=20000: 10ms
    TA1CCTL0=CCIE;      //CCIE=1


    TA0CTL=TASSEL_2| MC_1;  //SMCLK, DIV=1, UP
    TA0CCTL1=OUTMOD_7;      //OUTMOD=7

    inicia_ADC(BIT0); // Inicializamos el joystick solamente en el eje x



    //Pines del buzzer
    P2DIR|=BIT6;            //P2.6 salida
    P2SEL|= BIT6;           //P2.6 pwm
    P2SEL2&=~(BIT6+BIT7);   //P2.7 gpio
    P2SEL&=~(BIT7);         //P2.7 gpio

    // Pines y configuración para el puerto serie
    P1SEL2 = BIT1 | BIT2; //P1.1 RX, P1.2: TX
    P1SEL = BIT1 | BIT2;
    P1DIR = BIT0+ BIT2;

    /*------ Configuración de la USCI-A para modo UART:----------*/
    UCA0CTL1 |= UCSWRST; // Reset
    UCA0CTL1 = UCSSEL_2 | UCSWRST; //UCSSEL_2: SMCLK (16MHz) | Reset on
    UCA0CTL0 = 0; // 8bit, 1stop, sin paridad. NO NECESARIO
    UCA0BR0 = 139; // 16MHz/139=115108...
    UCA0CTL1 &= ~UCSWRST; //Quita reset
    IFG2 &= ~(UCA0RXIFG); //Quita flag
    IE2 |= UCA0RXIE; // y habilita int. recepcion

    // Pines del led RGB
    P2DIR|=BIT1|BIT4|BIT2; //Añadimos las direcciones del led RGB que vamos a usar
    P2OUT&=~(BIT1|BIT4|BIT2); // Inicializamos el led RGB apagado

    __bis_SR_register(GIE); // Habilita las interrupciones globalmente


    // Inicializamos Graphics Context
    Crystalfontz128x128_Init();
    Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_UP); // Dejamos la orientacion por defecto
    Graphics_initContext(&g_sContext, &g_sCrystalfontz128x128);

    Graphics_clearDisplay(&g_sContext); // Limpiamos la pantalla por lo que pudiera haber de antes
    Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);// Ponemos el fondo en negro

    Graphics_setFont(&g_sContext, &g_sFontCm16b); // Usamos este tamaño de fuente

    // Dibujamos la imagen de la portada

    for(i=0;i<6;i++){ // Bucle for para dibujar un marco rectangular de ancho 6
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_GRAY); // El marco será de color gris
        Graphics_Rectangle rectangulo1 = {1+i,1+i,127-i,127-i}; // Rectángulo que abarcará toda la pantalla e ira disminuyendo su tamaño con el bucle for
        Graphics_drawRectangle(&g_sContext,&rectangulo1); // Dibujamos solo los bordes del rectángulo, ya que si lo llenaramos taparía toda la pantalla
    }
    // Letras del título del juego
    Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_LIME_GREEN); // Color para "Imminent"
    Graphics_drawString(&g_sContext,"Imminent", 12, 27, 50, TRANSPARENT_TEXT);
    Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_RED); // Color para "CRASH"
    Graphics_drawString(&g_sContext,"CRASH", 5, 45, 65, TRANSPARENT_TEXT);
    Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_YELLOW); // Color para "!"
    Graphics_drawString(&g_sContext,"!", 1, 97, 65, TRANSPARENT_TEXT);


    // Mensaje que se mandará por el puerto serie al inicio del juego
    UARTprintCR("WELCOME TO IMMINENT CRASH! Try to dodge the walls moving your plane left and right with the joystick.");
    UARTprintCR("Press 'enter' to continue");


    while(1){
        LPM0; // Entramos en modo de bajo consumo


        switch(estado)
        {
        case inicio: // Estado inicial del juego

            P2OUT|=BIT4; // Encendemos el led de color azul

            // Toca melodía de inicio del juego
            if(tmus<20)
                toca_nota(SI,mute); // Función para tocar una nota, si mute vale 1 no se tocará.
            else if(tmus<30)
                toca_nota(1,mute);
            else if(tmus<50)
                toca_nota(SOL,mute);
            else if(tmus<60)
                toca_nota(1,mute);
            else if(tmus<90)
                toca_nota(RE,mute);
            else if(tmus<120)
                toca_nota(1,mute);
            else if(tmus<140)
                toca_nota(SI,mute);
            else if(tmus<150)
                toca_nota(1,mute);
            else if(tmus<170)
                toca_nota(SOL,mute);
            else if(tmus<180)
                toca_nota(1,mute);
            else if(tmus<200)
                toca_nota(RE,mute);
            else {
                toca_nota(1);

                if(caracter==13) // Si el caracter recibido por puerto serie es el enter
                {
                    tmus=0; // Ponemos el temporizador de la musica a 0

                    UARTprintCR("MENU");
                    UARTprintCR("  - Press 'm' for MUTE");
                    UARTprintCR("  - Press 's' to turn on SOUND ");
                    UARTprintCR("  - Press 'r' to RESET highscore ");
                    UARTprintCR("  - Press 'h' to see your HIGHSCORE ");
                    UARTprintCR("  - Press 'space' to open the MAP MENU ");
                    estado=opciones;

                }

            }
            break;

        case opciones: // Menu del juego, configuraciones...


            if(caracter=='m'){ // Si el caracter recibido es "m":
                mute=1; // Ponemos mute a 1, con lo que se silenciará toda la música y sonidos del juego

                UARTprintCR("Now the game is muted"); // Mensaje por puerto serie para saber que se ha silenciado

                caracter=0; // Asignamos el valor 0 a caracter para no entrar en un bucle


            }
            if(caracter=='s'){ // Si el caracter recibido es "s":
                mute=0; // Ponemos mute a 0, con lo que toda la musica y sonidos del juego sonarán de forma normal

                UARTprintCR("Now the game is unmuted"); // Mensaje por puerto serie para saber que se ha activado el sonido

                caracter=0; // Asignamos el valor 0 a caracter para no entrar en un bucle

            }

            if(caracter=='r'){ // Si el caracter recibido es "r":
                highscore=0;  // Ponemos la variable highscore a 0

                guarda_flash_seg(highscore, 0); // La guardamos en nuestra dirección de memoria asignada (0x1000)

                UARTprintCR("Now the highscore is set to 0"); // Mensaje por puerto serie para saber que se ha reseteado el highscore

                caracter=0;  // Asignamos el valor 0 a caracter para no entrar en un bucle

            }
            if(caracter=='h'){ // Si el caracter recibido es "h":
                highscore=*Puntero3; // Asignamos a la variable highscore el valor almacenado en memoria

                sprintf(cadena,"Your highscore is: %d",highscore); // Escribimos la frase con nuestro highscore en el vector cadena

                UARTprintCR(cadena); // Mensaje por puerto serie que nos indica nuestro highscore actual

                caracter=0;  // Asignamos el valor 0 a caracter para no entrar en un bucle


            }
            if(caracter==' '){ // Si el caracter recibido es el espacio:

                estado=premapas; // Pasamos al  estado previo al menú de selección de mapas
            }
            break;
        case premapas: // Estado previo al menú de selección de mapas

            UARTprintCR("CHOOSE YOUR MAP:"); // Se manda por puerto serie las opciones de mapas existentes
            UARTprintCR("  - Press '1' for OCEAN");
            UARTprintCR("  - Press '2' for SNOW ");
            UARTprintCR("  - Press '3' for FOREST");
            UARTprintCR("  - Press '4' for DESERT");
            UARTprintCR("  - Press '5' for SPACE");

            estado=mapas; // Pasamos a la selección de mapas
            break;

        case mapas: // Estado de selección de mapas

            if(caracter=='1') // Si el mapa seleccionado es el océano
            {
                colorfondo=0x00277CFF; // Se asigna el color del fondo, azul
                colormuro=GRAPHICS_COLOR_WHITE; // Se asigna el color de los muros, blanco
                ampliacion=20; // Se asigna el ancho del hueco, en este caso 20 ya que es el nivel más fácil
                nivel2=0; // En este nivel sólo hay 1 barra por pantalla

                estado=preprejuego; // Pasamos al siguiente estado para comenzar la partida
            }

            if(caracter=='2') // Si el mapa seleccionado es la nieve
            {
                colorfondo=GRAPHICS_COLOR_SNOW; // Se asigna el color del fondo, blanco nieve
                colormuro=GRAPHICS_COLOR_DARK_GRAY; // Se asigna el color de los muros, gris
                ampliacion=10; // Se asigna el ancho del hueco, en este caso 10 ya que el nivel es fácil
                nivel2=0; // En este nivel sólo hay 1 barra por pantalla

                estado=preprejuego; // Pasamos al siguiente estado para comenzar la partida
            }

            if(caracter=='3') // Si el mapa seleccionado es el bosque
            {
                colorfondo=GRAPHICS_COLOR_LIME_GREEN; // Se asigna el color del fondo, verde claro
                colormuro=GRAPHICS_COLOR_FOREST_GREEN; // Se asigna el color de los muros, verde oscuro
                ampliacion=14; // Se asigna el ancho del hueco, en este caso 14 ya que es el nivel más fácil con 2 muros
                nivel2=1;  // En este nivel hay 2 barras por pantalla

                estado=preprejuego; // Pasamos al siguiente estado para comenzar la partida
            }

            if(caracter=='4') // Si el mapa seleccionado es el desierto
            {
                colorfondo=GRAPHICS_COLOR_GOLD; // Se asigna el color del fondo, dorado claro
                colormuro=GRAPHICS_COLOR_GOLDENRON; // Se asigna el color de los muros, dorado oscuro
                ampliacion=10; // Se asigna el ancho del hueco, en este caso 10 ya que incrementa la dificultad habiendo 2 muros
                nivel2=1;  // En este nivel hay 2 barras por pantalla

                estado=preprejuego; // Pasamos al siguiente estado para comenzar la partida
            }

            if(caracter=='5') // Si el mapa seleccionado es el espacio
            {
                colorfondo=GRAPHICS_COLOR_BLACK; // Se asigna el color del fondo, negro
                colormuro=0x00EA94FF; // Se asigna el color de los muros, morado
                ampliacion=4; // Se asigna el ancho del hueco, en este caso 4 ya que es el nivel más dificil
                nivel2=1;  // En este nivel hay 2 barras por pantalla

                estado=preprejuego; // Pasamos al siguiente estado para comenzar la partida

            }

            break;
        case preprejuego: // Estado realizado para optimizar código
            tmus=0; // Ponemos el temporizador de la musica a 0
            UARTprintCR("3..."); // Mensaje mostrado por puerto serie para saber que inicia la cuenta atrás
            estado=prejuego; // Pasamos al siguiente estado
            break;

        case prejuego:

            P2OUT&=~(BIT4|BIT2); // Se apagan los leds azul y verde
            P2OUT|=(BIT1); // Se enciende el led en rojo, para empezar la cuenta atrás

            toca_nota(MI>>1,mute); // Primera nota de la cuenta atrás

            if(tmus>=100){
                toca_nota(1,mute); // Añadimos un silencio
                UARTprintCR("2..."); // Mensaje mostrado por puerto serie de la cuenta atrás
                tmus=0; // Ponemos el temporizador de la musica a 0
                // Inicializamos el avión en el centro de la pantalla
                Graphics_setBackgroundColor(&g_sContext, colorfondo); // Ponemos el color de fondo correspondiente
                Graphics_clearDisplay(&g_sContext); // Borramos lo que hubiera antes
                dibuja_avion(avionx,54,coloravion,colorventana,colorarmas); // Dibujamos el avión
                // Inicializamos los limites de los muros
                limizq = t;
                limder = t+40+ampliacion;
                limizq2 = t2;
                limder2 = t2+40+ampliacion;
                estado=prejuego2; // Pasamos al siguiente estado
            }
            break;

        case prejuego2:


            toca_nota(1,mute); // Seguimos en silencio

            if(tmus>=100){
                toca_nota(MI>>1,mute); // Segunda nota de la cuenta atrás
                P2OUT|=(BIT1|BIT2); // Se enciende el led en amarillo, continuando la cuenta atrás
                UARTprintCR("1..."); // Mensaje mostrado por puerto serie de la cuenta atrás
                tmus=0; // Ponemos el temporizador de la musica a 0
                estado=prejuego3; // Pasamos al siguiente estado
            }
            break;

        case prejuego3:

            if(tmus>88)
                toca_nota(1,mute); // Tocamos un silencio

            if(tmus>=190){
                toca_nota(MI>>2,mute); // Última nota de la cuenta atrás
                P2OUT&=~(BIT4|BIT1); // Se apagan los leds azul y rojo
                P2OUT|=(BIT2); // Se enciende el led en verde, finalizando la cuenta atrás
                tmus=0;  // Ponemos el temporizador de la musica a 0
                UARTprintCR("GO!"); // Mensaje mostrado por puerto serie indicando que se ha finalizado la cuenta atrás

                estado=juego; // Pasamos al estado principal del juego
            }
            break;


        case juego: // Estado donde se desarrolla el juego
            if(tmus>50){ // Despues de poco tiempo

                P2OUT&=~(BIT4|BIT2|BIT1); // Apagamos el led
            }
            if(tmus>20){
                toca_nota(1,mute); // Deja de sonar música

            }


            ejex=lee_ch(0); // Asignamos a la variable ejex el valor del eje x del joystick, que ira de 0 a 1024

            posbarra+=2; // Hacemos que el muro avance

            for(i=0;i<10;i++){ // Bucle for para dibujar el muro
                Graphics_setForegroundColor(&g_sContext, colormuro); // Lo pintamos del color correspondiente
                Graphics_drawLine(&g_sContext,0,0+i+posbarra,limizq,0+i+posbarra);
                // Línea horizontal que va desde el inicio de la pantalla hasta el limite izquierdo del hueco
                Graphics_drawLine(&g_sContext,limder,0+i+posbarra,128,0+i+posbarra);
                // Línea horizontal que va desde el limite derecho del hueco hasta el final de la pantalla
            }

            Graphics_setForegroundColor(&g_sContext, colorfondo); // Borramos los dos pixeles anteriores del muro
            Graphics_drawLine(&g_sContext,0,posbarra-1,limizq,posbarra-1);
            Graphics_drawLine(&g_sContext,limder,posbarra-1,128,posbarra-1);
            Graphics_drawLine(&g_sContext,0,posbarra-2,limizq,posbarra-2);
            Graphics_drawLine(&g_sContext,limder,posbarra-2,128,posbarra-2);


            if(posbarra>128){ // Cuando la barra llegue al final de la pantalla
                posbarra=-10; // Se resetea la posición para que aparezca otra vez desde el inicio de la pantalla
                // Se toman nuevos límites para el muro

                limizq=t;
                limder=t+40+ampliacion;
                t=t-26;
                puntuacion+=1; // Se incrementa en 1 la puntuación
            }

            if(posbarra>54 && nivel2==1){ // Si la primera barra pas ala mitad de la pantalla, y es un nivel con dos barras:
                segunda=1; // Enable para que la segunda barra funcione
            }
            if(segunda==1){ // Si esta variable vale 1, se permite el movimiento de la segunda barra
                posbarra2+=2; // Avanza la segunda barra dos pixeles
                for(i=0;i<10;i++){ // Se dibuja la segunda barra de la misma forma que la primera
                    Graphics_setForegroundColor(&g_sContext, colormuro);
                    Graphics_drawLine(&g_sContext,0,0+i+posbarra2,limizq2,0+i+posbarra2);
                    Graphics_drawLine(&g_sContext,limder2,0+i+posbarra2,128,0+i+posbarra2);
                }

                Graphics_setForegroundColor(&g_sContext, colorfondo);
                // Se borran los dos píxeles anteriores
                Graphics_drawLine(&g_sContext,0,posbarra2-1,limizq2,posbarra2-1);
                Graphics_drawLine(&g_sContext,limder2,posbarra2-1,128,posbarra2-1);
                Graphics_drawLine(&g_sContext,0,posbarra2-2,limizq2,posbarra2-2);
                Graphics_drawLine(&g_sContext,limder2,posbarra2-2,128,posbarra2-2);

                // Cuando la segunda barra ha recorrido toda la pantalla, aparece nuevamente arriba, de la misma forma que la primera
                if(posbarra2>128){
                    posbarra2=-10;
                    limizq2=t2;
                    limder2=t2+40+ampliacion;
                    t2=t2-13;
                    puntuacion+=1;

                }
            }

            // Movimiento del avión con el joystick
            if(ejex>800){
                // Si el joystick se inclina a la derecha más de 800
                posx1=posx1+3; // el avión se desplaza 3 píxeles a la derecha
            }
            if(ejex<=200){
                // Si el joystick se inclina a la izquierda menos de 200
                posx1=posx1-3;  // el avión se desplaza 3 píxeles a la izquierda
            }
            if(posx1<-25){
                // Si la posición del avión es más pequeña que -25, se saldría de la pantalla
                posx1=-25;
            }
            if(posx1>75){
                // Si la posición del avión es mayor que 75, se saldría de la pantalla
                posx1=75;
            }

            avionx=posx1; // Actualizamos la posición del avión

            if(avionx==avionxant){ } // Si el avión no se ha movido, no se redibuja
            else{ // Si el avión se ha movido
                dibuja_avion(avionxant,54,colorfondo,colorfondo,colorfondo); // Pintamos del color del fondo la posición anterior del avión
                dibuja_avion(avionx,54,coloravion,colorventana,colorarmas); // Dibujamos el avión en su nueva posición
                avionxant=avionx;
            }
            // Colisiones
            if(posbarra>81 && posbarra<111){ // Si la barra está a la altura del avión
                if((avionx+28)<limizq || (avionx+48)>limder){ // Y el avión se sale de los límites del hueco
                    tmus=0; // Reiniciamos el tiempo de la música
                    estado=premuerte; // Se pasa al siguiente estado, ya que el jugador ha perdido la partida

                }
            }
            if(posbarra2>81 && posbarra2<111){ // Si la barra 2 está a la altura del avión
                if((avionx+26)<limizq2 || (avionx+50)>limder2){ // Y el avión se sale de los límites del hueco 2
                    tmus=0; // Reiniciamos el tiempo de la música
                    estado=premuerte; // Se pasa al siguiente estado, ya que el jugador ha perdido la partida

                }
            }
            break;

        case premuerte: // Estado anterior a la muerte

            P2OUT&=~(BIT4|BIT2); // Apagamos los leds verde y azul
            P2OUT|=(BIT1); // Encendemos el led rojo indicando la muerte
            // Melodía de game over
            if(tmus<20)
                toca_nota(MI,mute);
            else if(tmus<30)
                toca_nota(1,mute);
            else if(tmus<50)
                toca_nota(RE,mute);
            else if(tmus<60)
                toca_nota(1,mute);
            else if(tmus<90)
                toca_nota(DO,mute);
            else if(tmus<100)
                toca_nota(1,mute);
            else{
                pantallamuerte(); // Sale "Game over" en color rojo por pantalla

                sprintf(cadena,"SCORE: %d",puntuacion); // Se muestra por puerto serie la puntuación obtenida en la partida jugada
                UARTprintCR(cadena);

                highscore=*Puntero3; // Se lee la dirección de memoria donde está almacenado el highscore

                if(puntuacion>highscore){ // Si la puntuación obtenida es mayor que el highscore
                    highscore=puntuacion; // Actualizamos el highscore

                    guarda_flash_seg(highscore,0); // Lo guardamos en el espacio de memoria asignado

                    UARTprintCR("Congratulations! You have a new highscore."); // Mensaje por puerto serie de actualización del highscore
                }
                else{}
                // Reset de todas las variables necesarias para iniciar una nueva partida
                segunda=0;
                avionx=26;
                avionxant=26;
                posx1=26;
                posbarra=-10;
                posbarra2=-10;
                limizq=t;
                limder=t+40+ampliacion;
                limizq2=t2;
                limder2=t2+40+ampliacion;
                caracter=0;
                // Menú final
                UARTprintCR("  - Press 'space' to restart");
                UARTprintCR("  - Press 'm' to open the map menu");
                UARTprintCR("  - Press 'o' to open options");
                puntuacion=0;


                estado=muerte; // Pasamos al último estado
            }
            break;

        case muerte:

            if(caracter==' ') // Si se pulsa el espacio se reiniciará la partida
            {
                P2OUT&=~BIT1; // Se apaga el led rojo
                caracter=0;  // Asignamos el valor 0 a caracter para no entrar en un bucle
                tmus=0;
                UARTprintCR("3...");
                estado=prejuego; // Pasamos al estado de prejuego e inicia la cuenta atrás
            }

            if(caracter=='m') // Si se pulsa la letra m volvemos al menú de mapas
            {
                P2OUT&=~BIT1; // Se apaga el led rojo
                caracter=0; // Asignamos el valor 0 a caracter para no entrar en un bucle
                nivel2=0; // Reseteamos el nivel
                UARTprintCR("");
                estado=premapas; // Pasamos al menú de mapas
            }
            if(caracter=='o') // Si se pulsa la tecla o volvemos al menú de opciones
            {
                P2OUT&=~BIT1; // Se apaga el led rojo
                nivel2=0; // Reseteamos el nivel
                UARTprintCR("");
                tmus=200; // Cuando vuelve al estado inicio no toca la melodía, directamente pasa al estado opciones
                caracter=13; // Pasa directamente sin darle al enter
                estado=inicio; // Volvemos al inicio
            }
            break;

        }
    }
}
// Interrupciones
#pragma vector=ADC10_VECTOR // Interrupción generada por el convertidor ADC que usamos para leer el joystick
__interrupt void ConvertidorAD(void)
{
    LPM0_EXIT; //Despierta al micro al final de la conversión
}
#pragma vector=TIMER1_A0_VECTOR // Interrupción generada por el timer 1
__interrupt void Interrupcion_T1(void)
{
    tms++;
    t++; // Incremento de la variable para los límites aleatorios en la barra 1
    t2++; // Incremento de la variable para los límites aleatorios en la barra 2
    if(t>88){
        t=0;}
    if(t2>78){
        t2=0;
    }
    if(tms>=1)
    {
        tms=0;
        tmus++; // Incremento de la variable que indica la duración de una nota musical
        LPM0_EXIT;
    }
}


#pragma vector=USCIAB0RX_VECTOR // Interrupción generada por el puerto serie al recibir un caracter
__interrupt void USCI0RX_ISR_HOOK(void)
{
    caracter=UCA0RXBUF; // Leo dato recibido
    LPM3_EXIT;
}

// Funciones usadas
char guarda_flash_seg(char dato, char direc) // Función para guardar en memoria flash
{
    char *  Puntero = (char *) (0x1000); // Apunta a la direccion
    char buffer_ram[64];
    char i;
    if(direc<64)
    { //Comprueba area permitida
        if(Puntero[direc]!=0xFF)
        {
            //Guarda el bloque en RAM
            for (i=0;i<64;i++) buffer_ram[i]= Puntero[i];
            //Borra el bloque
            FCTL1 = FWKEY + ERASE;       // activa  Erase
            FCTL3 = FWKEY;               // Borra Lock (pone a 0)
            Puntero[0] = 0;             // Escribe algo para borrar el segmento
            //Sobreescribe posicion en RAM
            buffer_ram[direc]=dato;
            //Reescribe la flash
            FCTL1 = FWKEY + WRT;         // Activa WRT y borra ERASE
            for (i=0;i<64;i++) Puntero[i]= buffer_ram[i];
            FCTL1 = FWKEY;               // Borra bit WRT
            FCTL3 = FWKEY + LOCK;        //activa LOCK
            return 1;
        }
        else
        {
            //Escribe la flash (solo ese dato)
            FCTL3 = FWKEY;              // Borra Lock (pone a 0)
            FCTL1 = FWKEY + WRT;        // Activa WRT
            Puntero[direc]=dato;        //Escribo el dato
            FCTL1 = FWKEY;              // Borra bit WRT
            FCTL3 = FWKEY + LOCK;       //activa LOCK
            return 2;
        }
    }
    return 0;
}

void toca_nota(unsigned int nota,char mute) // Función para tocar una nota musical
{
    if(mute==1){ // Si mute es 1, toca un silencio
        TA0CCR0=1;
        TA0CCR1=1>>1;
    }
    else{ // De otra forma, toca la nota asignada
        TA0CCR0=nota;
        TA0CCR1=nota>>1;
    }
}



int pantallamuerte(){ // Función para la pantalla del game over
    Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_RED);
    Graphics_drawString(&g_sContext,"GAME OVER", 9, 17, 55, TRANSPARENT_TEXT);
    return 0;

}

int dibuja_avion(int posx,int posy,long int coloravion,long int colorventana,long int colorarmas){
    // Función para dibujar y borrar el avión línea a línea
    int i;


    for(i=0;i<2;i++){ // Armas
        Graphics_setForegroundColor(&g_sContext, colorarmas);
        Graphics_drawLine(&g_sContext,31-i+posx,50+posy,31-i+posx,44+posy);
        Graphics_drawLine(&g_sContext,45+i+posx,50+posy,45+i+posx,44+posy);
    }


    Graphics_setForegroundColor(&g_sContext, coloravion);
    for(i=0;i<2;i++){  // Cuerpo del avión (se dibuja línea a línea simétricamente en base a un centro)
        Graphics_drawLine(&g_sContext,26+posx,50+posy,26+posx,49+posy);
        Graphics_drawLine(&g_sContext,26+posx,50+posy,26+posx,49+posy);
        Graphics_drawLine(&g_sContext,27+posx,51+posy,27+posx,48+posy);
        Graphics_drawLine(&g_sContext,28+posx,50+posy,28+posx,48+posy);
        Graphics_drawLine(&g_sContext,29+posx,49+posy,29+posx,47+posy);
        Graphics_drawLine(&g_sContext,31-i+posx,49+posy,31-i+posx,46+posy);
        Graphics_drawLine(&g_sContext,33-i+posx,49+posy,33-i+posx,45+posy);
        Graphics_drawLine(&g_sContext,34+posx,49+posy,34+posx,44+posy);
        Graphics_drawLine(&g_sContext,35+posx,50+posy,35+posx,43+posy);
        Graphics_drawLine(&g_sContext,36+posx,56+posy,36+posx,39+posy);
        Graphics_drawLine(&g_sContext,37+posx,56+posy,37+posx,37+posy);
        Graphics_drawLine(&g_sContext,38+posx,56+posy,38+posx,35+posy);
        Graphics_drawLine(&g_sContext,39+posx,56+posy,39+posx,37+posy);
        Graphics_drawLine(&g_sContext,40+posx,56+posy,40+posx,39+posy);
        Graphics_drawLine(&g_sContext,41+posx,50+posy,41+posx,43+posy);
        Graphics_drawLine(&g_sContext,42+posx,49+posy,42+posx,44+posy);
        Graphics_drawLine(&g_sContext,43+posx+i,49+posy,43+i+posx,45+posy);
        Graphics_drawLine(&g_sContext,45+i+posx,49+posy,45+i+posx,46+posy);
        Graphics_drawLine(&g_sContext,47+posx,49+posy,47+posx,47+posy);
        Graphics_drawLine(&g_sContext,48+posx,50+posy,48+posx,48+posy);
        Graphics_drawLine(&g_sContext,49+posx,51+posy,49+posx,48+posy);
        Graphics_drawLine(&g_sContext,50+posx,50+posy,50+posx,49+posy);
    }
    for(i=0;i<3;i++){ // Cola del avión
        Graphics_drawLine(&g_sContext,35-i+posx,56+posy,35-i+posx,54+i+posy);
        Graphics_drawLine(&g_sContext,41+i+posx,56+posy,41+i+posx,54+i+posy);
    }
    Graphics_setForegroundColor(&g_sContext, colorventana); // Ventana del avión
    Graphics_drawLine(&g_sContext,37+posx,49+posy,37+posx,45+posy);
    Graphics_drawLine(&g_sContext,38+posx,49+posy,38+posx,44+posy);
    Graphics_drawLine(&g_sContext,39+posx,49+posy,39+posx,45+posy);

    return 0;

    // fin :)

}

