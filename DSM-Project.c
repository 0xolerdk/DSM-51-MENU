#include <8051.h>
// Tablica zawierająca teksty wyświetlane na ekranie
                                    // Ekran główny
__code unsigned char MAIN[][33] = {">-1 MAIN SCREEN  -2 SETTINGS    ",//0 
                                   " -1 MAIN SCREEN >-2 SETTINGS    ",//1
                                   "OLEH FIHOL      PWM VALUE:   030",//2
                                   ">- 2.1 PWM       - 2.2 LED      ",//3
                                   " - 2.1 PWM      >- 2.2 LED      ",//4
                                   ">- 2.3 OTHER     - 2.4 LOAD DEF ",//5
                                   " - 2.3 OTHER    >- 2.4 LOAD DEF ",//6
                                   ">- STATE     OFF - VALUE     030",//7
                                   " - STATE     OFF>- VALUE     030",//8
                                   ">- BUZZ      OFF                ",//9
                                   ">- F1        OFF - F2        OFF",//10
                                   " - F1        OFF>- F2        OFF",//11
                                   ">- F3        OFF - F4        OFF",//12
                                   " - F3        OFF>- F4        OFF",//13
                                   ">- OK        OFF - ER        OFF",//14
                                   " - OK        OFF>- ER        OFF",//15
                                   };
// tablica zawierająca stan diód LED, PWM oraz buzzera
unsigned char LEDS_PWM_BUZZ[9] = {0,0,0,0,0,0,0,0,30};
// tablica zawierająca stan tymczasowy diód LED, PWM oraz buzzera
unsigned char LEDS_PWM_BUZZ_BUF[9] = {0,0,0,0,0,0,0,0,30};
// tablica zawierająca stan tymczasowy portów
unsigned char PORT_BUF[4];
// tablica zawierająca wzory cyfr do wyświetlenia na wyświetlaczu 7-segmentowym
__code unsigned char WZOR[10] = { 
    0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110, 
    0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111 };
// zmienna logiczna oznaczająca stan wysoki/niski
__bit t0_flag;
__bit timer_flag;
__bit __at (0x97) DLED;
__bit __at (0x95) BUZZ;
__bit __at (0xB5) keyB;


__xdata unsigned char * LCDWC = (__xdata unsigned char *) 0xFF80; //write command
__xdata unsigned char * LCDWD = (__xdata unsigned char *) 0xFF81; //write data
__xdata unsigned char * LCDRC = (__xdata unsigned char *) 0xFF82; //read command
__xdata unsigned char * CSDS = (__xdata unsigned char *) 0xFF30; //// CSDS, wybor jednego (lub wiecej) z 6 wyswietlaczy lub diody
__xdata unsigned char * CSDB = (__xdata unsigned char *) 0xFF38; // CSDB, wybor wlaczonych segmentow, lub wlaczonych diod z 6
__xdata unsigned char * KBD = (__xdata unsigned char *) 0xFF22; //klawiatura mult
__xdata unsigned char * CS55B = (__xdata unsigned char *) 0xFF29; //Wyjscie B
__xdata unsigned char * CS55D = (__xdata unsigned char *) 0xFF2B; //Zazadzanie portem B

// zmienne pomocnicze
unsigned char i, index = 0, key, is_pressed, point_diods=0b000000, wsk,pressed_key,first_key,iss_pressed = 0, counter, T_index = 0;
unsigned int TH0_LOW, TL0_LOW, TH0_HIGH, TL0_HIGH,ms = 0;
long tmp;

//Znalezenia pojemnosci ukladu c/l(Low/High)
void TH_APPLY(){
    tmp = (((1843*(LEDS_PWM_BUZZ[8]/10)))/10)+(18*(LEDS_PWM_BUZZ[8]%10)); 

    TH0_LOW = (47104+tmp)/256;//starszy bajt for low
    TL0_LOW = (47104+tmp)%256;//mlodszy bajt for low

    TH0_HIGH =(65536-tmp)/256;//starszy bajt for high
    TL0_HIGH =(65536-tmp)%256;//mlodszy bajt for high
}



void T_start(){
    // Ustawienie trybu komunikacji
    /*W rejestrze TCON zgromadzono bity flagowe stanu przepełnienia licznika,
TF0 i TF1 oraz bity sterowania pracą liczników, TR0 i TR1. Pozostałe bity, IE1,
IT1, IE0 oraz IT0 są zarezerwowane do obsługi przerwań i*/
    SCON = 0b01010000;
    /*Rejestr TMOD jest przeznaczony do określenia trybu i sposobu pracy liczni-
ków T0 i T1. Licznikowi T0 przypisano 4 młodsze bity a licznikowi T1 - cztery
starsze bity rejestru. Nazwy odpowiadających sobie bitów z kaŜdej podgrupy są
identyczne.*/
    TMOD = 0b00100001;
    // Ustawienie wartości dla młodszego rejestru timera
    TL1 = 0xFD;
    // Ustawienie wartości dla starszego rejestru timera
    TH1 = 0xFD;
    // Zmiana stanu biegunowego
    PCON = 0x40;
    // Zerowanie flagi timera 1
    TF1 = 0;
    // Włączenie timera 1
    TR1 = 1;
    // Włączenie przerwań timera 1
    ES = 1;

    // Włączenie przerwań timera 0
    ET0=1;
    // Włączenie ogólnej obsługi przerwań
    EA=1;

    // Zerowanie flagi timera 0
    TF0=0;
    // Włączenie timera 0
    TR0=1;

    // Ustawienie trybu działania modułu cs55d
    *(CS55D) = 0x80;
}


void lcd_wait_while_busy(){
	// Pętla, która czeka na zakończenie operacji na wyświetlaczu
	while(*LCDRC & 0b10000000);
}




void DATA_BUFFERING(unsigned char direc){
    // Sprawdzenie, czy wybrano kierunek kopiowania danych w kierunku do bufora
    if (direc == 1)
    {
        // Przeniesienie danych z tablicy LEDS_PWM_BUZZ_BUF do tablicy LEDS_PWM_BUZZ
        for (i = 0; i < 9; i++)
        {
            LEDS_PWM_BUZZ[i] = LEDS_PWM_BUZZ_BUF[i];
        }
    }
    // Sprawdzenie, czy wybrano kierunek kopiowania danych z bufora
    else if(direc == 0){
        // Przeniesienie danych z tablicy LEDS_PWM_BUZZ do tablicy LEDS_PWM_BUZZ_BUF
        for (i = 0; i < 9; i++)
        {
            LEDS_PWM_BUZZ_BUF[i] = LEDS_PWM_BUZZ[i];
        }
        // Zmiana stanu wyjścia dla buzzera
        BUZZ = !LEDS_PWM_BUZZ[6];
    }
    // Przypadek, gdy wartość direc jest inna niż 0 lub 1
    else{
        // Zerowanie obu tablic LEDS_PWM_BUZZ i LEDS_PWM_BUZZ_BUF
        for (i = 0; i < 9; i++)
        {
            LEDS_PWM_BUZZ_BUF[i] = 0;
            LEDS_PWM_BUZZ[i] = 0;
        }
        // Ustawienie wartości 30 w tablicy LEDS_PWM_BUZZ_BUF
        LEDS_PWM_BUZZ_BUF[8] = 30;
        LEDS_PWM_BUZZ[8] = 30;
        // Ustawienie stanu wyjścia buzzera na 1
        BUZZ = 1;
    }
    
}


void lcd_init(){
	lcd_wait_while_busy();
    *LCDWC = 0b00111000;
    lcd_wait_while_busy();
    *LCDWC = 0b00001111;
    lcd_wait_while_busy();
    *LCDWC = 0b00000110;
    lcd_wait_while_busy();
    *LCDWC = 0b00000001;
}

void delay(){            //dodano
     unsigned char k = 255;

     while(k>0){
         --k;
     }
}

void PWM_SUM(){
    if(timer_flag){
			// Wysyłanie kolejnych cyfr liczby do wysłania
            SBUF = ((LEDS_PWM_BUZZ[8]/100)%10) + 48;
            delay();
            SBUF = ((LEDS_PWM_BUZZ[8]/10)%10) + 48;
            delay();
            SBUF = (LEDS_PWM_BUZZ[8]%10) + 48;
            delay();
            // Wysłanie separatora
            SBUF = '_';
            // Zerowanie licznika milisekund
            timer_flag = 0;
    }

}

void show_diods(){
    // Pętla po każdej z diod LED
    for (i = 0; i <= 5; i++)
    {
        // Jeśli wartość odpowiadająca danej diodzie jest ustawiona na 1
        if (LEDS_PWM_BUZZ_BUF[i])
        {
            // Dodanie bieżącej diody do zmiennej point_diods
            point_diods |= (1<<i);
        }
    }
    // Ustawienie wysokiego stanu na linii P1_6
    P1_6 = 1;
    // Wysłanie stanu diod do rejestru CSDB
    *CSDB = point_diods;
    // Ustawienie linii CSDS na wysoki stan
    *CSDS = 0b1000000;
    // Ustawienie niskiego stanu na linii P1_6
    P1_6 = 0;
    // Wyzerowanie zmiennej point_diods
    point_diods=0b000000; 
}


void show_on_led(){
    

    wsk = 0b000001;
    i = LEDS_PWM_BUZZ[8];
    counter=0;

    while (i>0)
    {
        P1_6 = 1;
        *CSDB = WZOR[i%10];
        *CSDS = wsk;
        P1_6 = 0;
        delay();
        counter++;

        i /= 10;
        wsk <<= 1;
    }
    if (counter==2)
    {
        P1_6 = 1;
        *CSDB = WZOR[i%10];
        *CSDS = wsk;
        P1_6 = 0;
        delay();
    }

    show_diods();
    
}
//Wuszwietlenia "N " na LCD
void LCD_PUT_ON(){
    lcd_wait_while_busy();
    *LCDWD = 'N';
    lcd_wait_while_busy();
    *LCDWD = ' ';
    i++;
}
//Odsziezanie LCD z nowymi zmennymi(PWM od 30 - 120, ON\OFF), zmiena wyszwietlanego podmeniu
void lcd_refresh(){

    delay();
    for (i = 0; i < sizeof(MAIN[0])/sizeof(MAIN[0][0])-1; i++)
        {
            //F1 - ER ustawienia ON jezeli Led aktywowany
            if (index > 9 && index < 16)
            {
                
                if (index<12)
                {
                    if ((LEDS_PWM_BUZZ[0] && i==14))
                {
                    LCD_PUT_ON();
                    continue;
                    
                }

                else if ((LEDS_PWM_BUZZ[1] && i==30))
                {
                    LCD_PUT_ON();
                    continue;
                }
                }
                
                else if (index<14)
                {
                    if ((LEDS_PWM_BUZZ[2] && i==14))
                {
                    LCD_PUT_ON();
                    continue;
                    
                }

                if ((LEDS_PWM_BUZZ[3] && i==30))
                {
                    LCD_PUT_ON();
                    continue;
                }
                }
                
                else if (index < 16)
                {
                    if ((LEDS_PWM_BUZZ[4] && i==14))
                {
                    LCD_PUT_ON();
                    continue;
                    
                }

                if ((LEDS_PWM_BUZZ[5] && i==30))
                {
                   LCD_PUT_ON();
                    continue;
                }
                }
                
            }

            //BUZ ON
            if (index == 9 && (LEDS_PWM_BUZZ[6] && i==14) )
                {
                    LCD_PUT_ON();
                    continue;
                }

            //STATE ON
            if ((LEDS_PWM_BUZZ[7] && i==14) && (index == 7 || index == 8))
            {
                LCD_PUT_ON();
                continue;
            }
            //Wyswietlinie wartosci PWM
            if (i==29 && (index == 8 ||index == 2 || index == 7))
            {
                //setki
                *LCDWD = (((LEDS_PWM_BUZZ[8]/10)/10)%10)+48;
                lcd_wait_while_busy();
                //dziesieki
                *LCDWD = ((LEDS_PWM_BUZZ[8]/10)%10)+48;
                lcd_wait_while_busy();
                //jednostki
                *LCDWD = (LEDS_PWM_BUZZ[8]%10)+48;
                i+=2;
                continue;

            }
            if (i==16)
            {
                lcd_wait_while_busy();
               *LCDWC = 0b11000000;
            }
            //wyswietlienie podmeniu(index) z listy po czaram
            lcd_wait_while_busy();
            *LCDWD = MAIN[index][i];
        }
}

//laczenie warsci PWM z otrzmanych cyfr z transmisji(3 cyfry)
void PWM_COMBINE(){
    /*w liscie PORT_BUF znajduje sie 3 cyfry( po indeksem 0 - setki, 1-dziesientki, 
    2-jednostki), za pomoca aryfmetycznych operacij mozemy ich zlaczycz w jedna wartosc PWM*/
    LEDS_PWM_BUZZ[8] = (PORT_BUF[0])*100+(PORT_BUF[1])*10+(PORT_BUF[2]);

    if (LEDS_PWM_BUZZ[8]>120)
    {
        LEDS_PWM_BUZZ[8] = 120;
    }
    else if (LEDS_PWM_BUZZ[8]<30)
    {
        LEDS_PWM_BUZZ[8] = 30;
    }
    lcd_refresh();
    T_index = 0;
}

void main(){
    T_start();
    TH_APPLY();
    lcd_init();
    lcd_refresh();

    while (1)
    {

        while (is_pressed){

            TH_APPLY();

            key = *KBD;
            
            if (key == 0b11111111)
            {
                is_pressed = 0;
            }
            show_on_led();
			PWM_SUM();
        }
        
        while (!is_pressed)
        {
            PWM_SUM();
            TH_APPLY();
            show_on_led();
            TH_APPLY();
        P1_6 = 1;
        pressed_key = 0b000000;
        first_key = 0b000001;

        for (i = 0; i < 6; i++)
            {
                *CSDS = first_key;
                
                if (keyB!=0)
                {
                    pressed_key = first_key;
                    break;
                }
                first_key <<= 1;
            }

            if (iss_pressed != 0 && pressed_key == 0b000000 )
            {
                iss_pressed = 0;
            }
            else if (iss_pressed == 0)
            {
                if (pressed_key == 0b001000)//up
            {
                LEDS_PWM_BUZZ[8]++;  
                if (LEDS_PWM_BUZZ[8]>120)
                {
                    LEDS_PWM_BUZZ[8]=120;
                }  
                iss_pressed = 1;
                lcd_refresh();
                
            }

            if (pressed_key == 0b010000)//down
            {
                LEDS_PWM_BUZZ[8]--;
                if (LEDS_PWM_BUZZ[8]<30)
                {
                    LEDS_PWM_BUZZ[8]=30;
                }
                
                iss_pressed = 1;
                lcd_refresh();

            }

            if (pressed_key == 0b100000) //left
            {
                LEDS_PWM_BUZZ[8]-=10;
                if (LEDS_PWM_BUZZ[8]<30)
                {
                    LEDS_PWM_BUZZ[8]=30;
                }
                iss_pressed = 1;
                lcd_refresh();
            }

            if (pressed_key == 0b000100)//right
            {
                LEDS_PWM_BUZZ[8]+=10;  
                if (LEDS_PWM_BUZZ[8]>120)
                {
                    LEDS_PWM_BUZZ[8]=120;
                }      
                iss_pressed = 1;
                lcd_refresh();
            }
            }
            P1_6 = 1;
            key = *KBD;
            if (key == 0b11101111)//up/
            {
                if (index > 9 && index < 16)
                {
                    index--;
                    if (index == 9)
                    {
                        index = 10;
                    }
                }

                if (index == 7)
                {
                    index = 8;
                }
                else if (index == 8)
                {
                    index = 7;
                }

                if (index<2)
                {
                    index = !index;
                }
                else if (index != 2 && index <7)
                {
                    index--;
                    if (index == 2)
                    {
                        index = 6;
                    }
                }

                *LCDWC = 0b00000001;
                is_pressed = 1;
                lcd_refresh();
            }

            if (key == 0b11011111)//down
            {
                if (index > 9)
                {
                    index++;
                    if (index == 16)
                    {
                        index = 15;
                    }
                    
                }

                if (index == 7)
                {
                    index = 8;
                }
                else if (index == 8)
                {
                    index = 7;
                }

                if (index<2)
                {
                    index = !index;
                }
                else if (index != 2 && index <7)
                {
                    index++;
                    if (index == 7)
                    {
                        index = 3;
                    }
                    
                }

                *LCDWC = 0b00000001;
                is_pressed = 1;
                lcd_refresh();
            }

            if (key == 0b01111111)//enter
            {
                is_pressed = 1;
                *LCDWC = 0b00000001;
                if (index>6)
                {
                    DATA_BUFFERING(0);
                    index = 3;
                }
                else if (index == 3)
                {
                    index = 7;
                }
        
                
                
                if (index == 5)
                {
                    index = 9;
                }
                

                if(index == 6){
                    DATA_BUFFERING(2);
                    index = 3;
                }

                
                if (index == 4)
                {
                   index = 10;
                }
                if (index == 0)
                {
                    index = 2;
                }
                if (index == 1)
                {
                    index = 3;
                }

                lcd_refresh();
            }
            
            if (key == 0b10111111 && index>1)//esc
            {
                is_pressed = 1;
                *LCDWC = 0b00000001;
                if (index>6)
                {
                    DATA_BUFFERING(1);
                }
                if (index >6)
                {
                    index = 3;
                }
                else{
                    index = 0;
                }
                
                lcd_refresh();
            }

            if (key == 0b11110111 )//right
            {
                if (index == 8)
                {
                    LEDS_PWM_BUZZ[8]+=10;
                    if (LEDS_PWM_BUZZ[8]>120)
                    {
                        LEDS_PWM_BUZZ[8]=120;
                    }  
                    
                }
                
                if (index == 7)
                {
                    LEDS_PWM_BUZZ[7] = 1;
                }
                

                if(index == 9){
                	LEDS_PWM_BUZZ[6] = 1;

                }

                if (index>9 && index < 16)
                {
                    LEDS_PWM_BUZZ[index-10] = 1;
                }

                *LCDWC = 0b00000001;
                is_pressed = 1;
                lcd_refresh();
            }

            if (key == 0b11111011)//left
            {
                if (index == 8)
                {
                    LEDS_PWM_BUZZ[8]-=10;
                    if (LEDS_PWM_BUZZ[8]<30)
                {
                    LEDS_PWM_BUZZ[8]=30;
                }  
                }

                if (index == 7)
                {
                    LEDS_PWM_BUZZ[7] = 0;
                }

                if(index == 9){
				   LEDS_PWM_BUZZ[6] = 0;
                }
                if (index>9 && index < 16)
                {
                    LEDS_PWM_BUZZ[index-10] = 0;
                }

                *LCDWC = 0b00000001;
                is_pressed = 1;
                lcd_refresh();
            }

            show_on_led();
            
            
        }
    }
}

void sio_int(void) __interrupt(4){
// Funkcja przerwania dla połączenia szeregowego
// Jeśli zmienna TI jest ustawiona na 1
if (TI){
	// Wyzeruj zmienną TI
	TI = 0;
}
else {
    // Zapisz wartość przysłaną z połączenia szeregowego
    // jako element tablicy PORT_BUF
    PORT_BUF[T_index] = SBUF-48;
    // Inkrementuj indeks tablicy
    T_index++;
    // Jeśli indeks tablicy osiągnie wartość 3
    if (T_index == 3)
    {
        // Wywołaj funkcję łączenia wartosci PWM z otrzymanych cyfer z transmisji(od 030 do 120)
        PWM_COMBINE();
    }

	// Wyzeruj zmienną RI
	RI = 0;
}
}





// Funkcja obsługująca przerwanie Timer0
void t0_int( void ) __interrupt(1)
{
    // Zmienna pomocnicza - Stan STATE
    if(LEDS_PWM_BUZZ_BUF[7]){

        ms++;
        // Jeśli 100ms zostało odmierzone
        if (ms == 100)
        {
            timer_flag = 1;
            ms = 0;
        }
        // Zmienna pomocnicza informująca o stanie flagi
        t0_flag = !t0_flag;
        
        TF0=0;
        // Jeśli flaga jest ustawiona na 1
        if(t0_flag)
        {
            // Ustawienie portu CS55B na 1
            *(CS55B) = 0b11111111;
            DLED = 0;
            
            TL0 = TL0_HIGH; //mlodszy bajt pojemnosci ukladu dlia stanu wysokiego od 3% - 12% wszystkiego czasu dzialania
            TH0 = TH0_HIGH; //starszy bajt pojemnosci ukladu dlia stanu wysokiego od 3% - 12% wszystkiego czasu dzialania
        }
        // Jeśli flaga jest ustawiona na 0
        else
        {
            // Ustawienie portu CS55B na 0
            *(CS55B) = 0b00000000;
            DLED = 1;
            
            TL0 = TL0_LOW; //mlodszy bajt pojemnosci ukladu dlia stanu niskiego od 88% - 97% wszystkiego czasu dzialania
            TH0 = TH0_LOW; //starszy bajt pojemnosci ukladu dlia stanu niskiego od 88% - 97% wszystkiego czasu dzialania
        }
    }
}
