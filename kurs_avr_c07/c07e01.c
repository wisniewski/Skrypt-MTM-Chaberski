/*
Debouncing przyciskow (4 najstarsze bity portu C) w przerwaniu + sprawdzanie na LEDach na porcie B.
Sa funkcje od obslugi wyswietlacza LCD oraz zarys menu opratego na strukturach.
*/

#include <avr/io.h> //I/O, rejestry itd.
#include <avr/pgmspace.h> //progmem
#include <stdlib.h> //malloc
#include <stdio.h> //sprintf
#include <avr/interrupt.h> //przerwania

struct lcd_field //definicja struktury
{
    volatile char rs : 1; //zmienna nie podlegajaca procesowi optymalizacji
    volatile char rw : 1;
    volatile char e : 1;
    volatile char : 1; //odstep bo trzeciego bitu nie uzywamy
    volatile char data : 4; //dostep do d4-d7 w LCD
};

//pole bitowe do przechowywania stanow przyciskow
struct button_field
{
	volatile char : 4; //puste bity, bo chce zajmowac PC4-7
    volatile char k : 4;
};

//definiowane nazw i dostep do poszczegolnych bitow w polu <-------- D E F I N I C J E
#define hardware_keys (((struct button_field *) &PINC) -> k)
#define hardware_keys_dir (((struct button_field *) &DDRC) -> k)
#define hardware_keys_port (((struct button_field *) &PORTC) -> k)

#define lcd_rs ((*((struct lcd_field *) &PORTB)).rs) //0 lub 1 na pinie
#define lcd_rs_dir (((struct lcd_field *) &DDRB) -> rs)
//operator wyluskania pola -> jesli mamy wskaznik do struktury, INICJALIZACJA

#define lcd_rw ((*((struct lcd_field *) &PORTB)).rw) //0 lub 1 na pinie
#define lcd_rw_dir (((struct lcd_field *) &DDRB) -> rw) //inicjalizacja

#define lcd_e ((*((struct lcd_field *) &PORTB)).e) //0 lub 1 na pinie
#define lcd_e_dir (((struct lcd_field *) &DDRB) -> e) //inicjalizacja

#define lcd_data ((*((struct lcd_field *) &PORTD)).data) //0 lub 1 na pinie
#define lcd_data_dir (((struct lcd_field *) &DDRD) -> data) //inicjalizacja

uint8_t volatile keys; //zmienna globalna, volatile <-> przerwania!

//deklaracje uzytych funkcji
void lcd_initiation (char data, char rs);
void lcd_write_data(char data);
void lcd_write_command(char data);
void buttons_debouncing(void);
void lcd_show(void);

//struktura menu // <-------- --------- ------- M E N U
struct menu
{
	struct menu *left;
	struct menu *right;
	struct menu *up;
	struct menu *down;
	char *str;
};

struct menu M1, M2, M3, M11, M12, M13, M131, M132, M21, M22, M31;

//definicja odwoluje sie do innych elementow {lewo, prawo, gora nic, dol, "string"}
struct menu M1 = {&M3, &M2, NULL, &M11, "M1"};
struct menu M2 = {&M1, &M3, NULL, &M21, "M2"};
struct menu M3 = {&M2, &M1, NULL, &M31, "M3"};

struct menu M11 = {&M13, &M12, &M1, NULL, "M11"};
struct menu M12 = {&M11, &M13, &M1, NULL, "M12"};
struct menu M13 = {&M12, &M11, &M1, &M131, "M13"};
struct menu M131 = {&M132, &M132, &M13, NULL, "M1311"};
struct menu M132 = {&M131, &M131, &M13, NULL, "M1312"};

struct menu M21 = {&M22, &M22, &M2, NULL, "M21"};
struct menu M22 = {&M21, &M21, &M2, NULL, "M22"};

struct menu M31 = {NULL, NULL, &M3, NULL, "M31"};

//wait \004\377
//komenda \001\x28
const char lcd_format[] PROGMEM =
"\004\377\001\x28\004\377\001\x28\004\377\001\x28\004\377\
\001\x0c\004\377\001\x06\004\377\001\x01\004\377\1\x28\001\x81\
pies i krowa\004\377\001\x28\004\377\001\xc3Kot z umk";

volatile  uint8_t lcd_buff_full;
char *lcd_buff;

ISR(TIMER0_COMP_vect) //10 k Hz = 100 us <-------- P R Z E R W A N I E
{
	buttons_debouncing(); //obsluga przyciskow w przerwaniu
	lcd_show(); //obsluga LCD w przerwaniu
}

int main (void) // <-------- -------- -------- -------- ----- M A I N
{
	//deklaracja przyciskow i polaryzacja
	hardware_keys_port = 0xf; //f lub 15 - bo tylko robie pull up na 4 bitach!

	DDRB = 0xff;
	PORTB = 0xff;

    //inicjalizacja LCD
    lcd_rs_dir = 1;
    lcd_rw_dir = 1;
    lcd_e_dir = 1;
    lcd_data_dir = 0xf;

	//wyswietlanie napisu na lcd
	lcd_buff_full = 1;
	lcd_buff = malloc(32);
    sprintf_P(lcd_buff, lcd_format);

	//obsluga przerwan i timera0
    TCCR0 |= _BV(CS01) | (1 << WGM01);
    TIMSK |= (1 << OCIE0);
    OCR0 = 99;
    sei();

    while(1)
	{
		if(keys != 0)
		{
			switch(keys)
			{
				case 1: PORTB++; break;
				case 2: PORTB--; break;
				case 4: PORTB = 0x00; break;
				case 8: PORTB = 0xff; break;
			}
			keys = 0;
		}
	}

    return 0;
}

/* - - - - - - - - - - - - - funkcje uzyte w mainie - - - - - - - - - - */

void lcd_initiation (char data, char rs)
{
    lcd_rw = 0;
    lcd_rs = rs;
    lcd_e = 1;
    lcd_data = data >> 4; //przesuniecie na 4 najmlodsze bity
    lcd_e = 0;
    lcd_e = 1;
    lcd_data = data & 0xF;
    lcd_e = 0;
    //gdyby nie pola bitowe... (((((PORTD >> 4)++) & 0xF) << 2) | PORTD & 0xC3)
}

void lcd_write_data(char data)
{
    lcd_initiation(data, 1); //1 bo RS 1 to data
}

void lcd_write_command(char data)
{
    lcd_initiation(data, 0); //0 to command
}

void buttons_debouncing(void)
{
	static uint8_t tmp_key, key_cnt, t;
	if(key_cnt == 0)
	{
		switch(t)
		{
			case 0:
			{
			tmp_key = hardware_keys;
			if(tmp_key != 15) //15 bo zadeklarowalem CZTERY bity, a 15 = 0b1111, sprawdzam zadeklarowane bity
				{
				t = 1;
				key_cnt = 200;
				}
			}
			break;

			case 1:
			{
			if(hardware_keys == tmp_key)
				{
				keys = (~tmp_key)&0xf; //wyzerowanie 4 starszych bitow
				t = 2;
				}
			else
				t = 0;
				}
			break;

			case 2:
			{
			if(hardware_keys == 15)
				t = 0;
			}
			break;
		}
	}
	else
		key_cnt--;
}

void lcd_show(void)
{
	static uint8_t lcd_cnt, lcd_read;
	if((lcd_buff_full) && (lcd_cnt == 0))
    {
        switch(lcd_buff[(int)lcd_read])
        {
            case 0: //koniec lancucha
            {
                lcd_buff_full = 0;
                lcd_read = 0;
            }
            break;
            case 1: //wyslanie komendy do wyswietlacza
            {
                lcd_read++;
                lcd_write_command(lcd_buff[lcd_read++]); //wysylam komende
                //i znowu inkrementuje, aby wskazywac na kolejny element
            }
            break;
            case 4: //czekanie
            {
                lcd_read++;
                lcd_cnt = lcd_buff[lcd_read++];
            }
            break;
            //lcd_read mowi ktory znak odczytac oraz inkrementacja do nastepnego znaku
            default: lcd_write_data(lcd_buff[lcd_read++]);
        }
    }
    else if(lcd_cnt > 0)
    {
        lcd_cnt--;
    }
}

