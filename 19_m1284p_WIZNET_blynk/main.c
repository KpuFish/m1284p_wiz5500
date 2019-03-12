/*
 * main.c
 *
 *  Created on: 22 нояб. 2018 г.
 *      Author: maxx
 */
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <compat/deprecated.h>  //sbi, cbi etc..
#include "avr/wdt.h" // WatchDog
#include <stdio.h>  // printf etc..
#include "uart_extd.h"
#include "spi.h"

#include "globals.h" //Global definitions for project

#include "stdbool.h"
#include "Ethernet/socket.h"
#include "Ethernet/wizchip_conf.h"
#include "Application/Blynk/blynk.h"
#include "Internet/DNS/dns.h"

#define _MAIN_DEBUG_

//***********BLYNK related: BEGIN
#define SOCK_BLYNK_CLIENT		6
//My auth token for my android test application MEGA+USB:
uint8_t auth[] = "c113f724351444fc872ae586d70b18cd";	// You should get Auth Token in the Blynk App
// IP: 139.59.206.133 for <blynk-cloud.com> via WIN7 nslookup - actually need to use DNS resolving
//Resolve here via DNS query see below Domain_IP[4]
//uint8_t blynk_server_ip[4] = {139, 59, 206, 133};		// Blynk cloud server IP (cloud.blynk.cc, 8422)
//uint8_t BLYNK_RX_BUF[DATA_BUF_SIZE];
uint8_t BLYNK_TX_BUF[BLYNK_DATA_BUF_SIZE];

//***********BLYNK related: END

//***************** DNS: BEGIN
//////////////////////////////////////////////////
// Socket & Port number definition for Examples //
//////////////////////////////////////////////////
#define SOCK_DNS       5

unsigned char gDATABUF_DNS[512];
//#define IP_WORK

////////////////
// DNS client //
////////////////
uint8_t Domain_name[] = BLYNK_DEFAULT_DOMAIN;    		// Public russian ntp server - works good via GSM Modem
uint8_t Domain_IP[4]  = {0, };               		// Translated IP address by DNS Server
//***************** DNS: END

/*
 * (19)OK (v1.0) Trying port to WIZNET5500 BLYNK IOT app (look: https://blynk.io/)
 * TODO:
 * OK (v1.2) Add DNS resolve before BLYNK app running to <blynk-cloud.com>
 * OK (v1.1) Add LED_ON/LED_OFF handle on LED D13 BLYNK Android application
 *  GPIO OUT  - works OK (look ./Application/Blynk/blynkDependency.c digitalWrite(..) && pinMode(..))!
 * OK(v1.2) Add printout <blynk> server metrics on start-up
 * Need to try next:
 * OK (v1.3)GPIO IN - fixed bug (remove redundant space symbol in <dw xx xx >)
 * Virtual IN/OUT
 * Analog Read/Write
 * Restore pins state on board reboot
 * OK ??3.Try fix frequent reconnection with blynk server - every ~22sec may be this OK.
 * Need compare local blynk.c code with modern <blynk> library - (Too old version here - 0.2.1 (On git blynk March 2019 - 0.6.x) )
 *
 * (3) Trying WIZNET5500 init with using official Wiznet ioLibrary_Driver
 * working ping, assign static IP
 * LED1 = ON when phy_link detected
 * and loopback test on TCP-IP:5000 and UDP:3000 ports.
 * use Hercules terminal utility to check network connection see:
 *
 * https://wizwiki.net/wiki/doku.php?id=osh:cookie:loopback_test
 * https://www.hw-group.com/software/hercules-setup-utility
 *
 */

//***********Prologue for fast WDT disable & and save reason of reset/power-up: END
uint8_t mcucsr_mirror __attribute__ ((section (".noinit")));

// This is for fast WDT disable & and save reason of reset/power-up
void get_mcusr(void) \
  __attribute__((naked)) \
  __attribute__((section(".init3")));
void get_mcusr(void)
{
  mcucsr_mirror = MCUSR;
  MCUSR = 0;
  wdt_disable();
}
//***********Prologue for fast WDT disable & and save reason of reset/power-up: END

//*********Global vars
#define TICK_PER_SEC 1000UL
volatile unsigned long _millis; // for millis tick !! Overflow every ~49.7 days

//*********Program metrics
const char compile_date[] PROGMEM    = __DATE__;     // Mmm dd yyyy - Дата компиляции
const char compile_time[] PROGMEM    = __TIME__;     // hh:mm:ss - Время компиляции
const char str_prog_name[] PROGMEM   = "\r\nAtMega1284p v1.3 Static IP BLYNK WIZNET_5500 ETHERNET 12/03/2019\r\n"; // Program name

#if defined(__AVR_ATmega128__)
const char PROGMEM str_mcu[] = "ATmega128"; //CPU is m128
#elif defined (__AVR_ATmega2560__)
const char PROGMEM str_mcu[] = "ATmega2560"; //CPU is m2560
#elif defined (__AVR_ATmega2561__)
const char PROGMEM str_mcu[] = "ATmega2561"; //CPU is m2561
#elif defined (__AVR_ATmega328P__)
const char PROGMEM str_mcu[] = "ATmega328P"; //CPU is m328p
#elif defined (__AVR_ATmega32U4__)
const char PROGMEM str_mcu[] = "ATmega32u4"; //CPU is m32u4
#elif defined (__AVR_ATmega644P__)
const char PROGMEM str_mcu[] = "ATmega644p"; //CPU is m644p
#elif defined (__AVR_ATmega1284P__)
const char PROGMEM str_mcu[] = "ATmega1284p"; //CPU is m1284p
#else
const char PROGMEM str_mcu[] = "Unknown CPU"; //CPU is unknown
#endif


//FUNC headers
static void avr_init(void);
void timer0_init(void);


//Wiznet FUNC headers
void print_network_information(void);

// RAM Memory usage test
int freeRam (void)
{
	extern int __heap_start, *__brkval;
	int v;
	int _res = (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
	return _res;
}


//******************* MILLIS ENGINE: BEGIN
//ISR (TIMER0_COMP_vect )
ISR (TIMER0_COMPA_vect)
{
	// Compare match Timer0
	// Here every 1ms
	_millis++; // INC millis tick
	// Тест мигаем при в ходе в прерывание
	// 500Hz FREQ OUT
	// LED_TGL;
}

unsigned long millis(void)
{
	unsigned long i;
	cli();
	// Atomic tick reading
	i = _millis;
	sei();
	return i;
}
//******************* MILLIS ENGINE: END

//***************** UART0: BEGIN
// Assign I/O stream to UART
/* define CPU frequency in Mhz here if not defined in Makefile */
//#ifndef F_CPU
//#define F_CPU 16000000UL
//#endif

/* 19200 baud */
//#define UART_BAUD_RATE      19200
//#define UART_BAUD_RATE      38400
#define UART_BAUD_RATE      115200

static int uart0_putchar(char ch,FILE *stream);
static void uart0_rx_flash(void);

static FILE uart0_stdout = FDEV_SETUP_STREAM(uart0_putchar, NULL, _FDEV_SETUP_WRITE);
//PS. stdin не переназначаю, т.к. удобнее с ним работать через uart.h - api:

/*
 * Т.е. например так
        c = uart1_getc();
        if (( c & UART_NO_DATA ) == 0)
        {
           uart1_putc( (unsigned char)c );
        }
 При этом чекаем что буфер приема не пуст и опрос идет неблокирующий (+ работаем через UART RX RINGBUFFER),
 а если работаем в стиле stdin->getchar() там опрос блокируется пока символ не будет принят (поллинг)
 через UART1_RX, т.е. неудобно.
 */

// STDOUT UART0 TX handler
static int uart0_putchar(char ch,FILE *stream)
{
	uart_putc(ch);
	return 0;
}

// Очищаем буфер приема UART1 RX (иногда нужно)
static void uart0_rx_flash(void)
{
	// Считываем все из ring-buffer UART1 RX
	unsigned int c;
	do
	{
		c = uart_getc();
	} while (( c & UART_NO_DATA ) == 0); // Check RX1 none-empty

}
//***************** UART0: END

//***************** ADC: BEGIN

#ifndef ADC_DIV
//12.5MHz or over use this ADC reference clock
#define ADC_DIV (1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0) //:128 ADC Prescaler
#endif

#ifndef ADC_REF
// vcc voltage ref default
#define ADC_REF (1<<REFS0)
#endif

void adc_init(void)
{
	ADCSRA = 0;
	ADCSRA |= (ADC_DIV);    // ADC reference clock
	ADMUX |= (ADC_REF);     // Voltage reference
	ADCSRA |= (1<<ADEN);    // Turn on ADC
	ADCSRA |= (1<<ADSC);    // Do an initial conversion because this one is the
	// slowest and to ensure that everything is up
	// and running
}

uint16_t adc_read(uint8_t channel)
{
	ADMUX &= 0b11100000;                    //Clear the older channel that was read
	ADMUX |= channel;                //Defines the new ADC channel to be read
	ADCSRA |= (1<<ADSC);                //Starts a new conversion
	while(ADCSRA & (1<<ADSC));            //Wait until the conversion is done

	return ADCW;                    //Returns the ADC value of the chosen channel
}
//***************** ADC: END


//***************** WIZCHIP INIT: BEGIN
//Shouldn't used here
/*
#define SOCK_TCPS       0
#define SOCK_UDPS       1
#define PORT_TCPS		5000
#define PORT_UDPS       3000

#define ETH_MAX_BUF_SIZE	512

//unsigned char ethBuf0[ETH_MAX_BUF_SIZE];
//unsigned char ethBuf1[ETH_MAX_BUF_SIZE];
*/
void cs_sel() {
	SPI_WIZNET_ENABLE();
}

void cs_desel() {
	SPI_WIZNET_DISABLE();
}

uint8_t spi_rb(void) {
	uint8_t rbuf;
	//HAL_SPI_Receive(&hspi1, &rbuf, 1, HAL_MAX_DELAY);
	SPI_READ(rbuf);
	return rbuf;
}

void spi_wb(uint8_t b) {
	//HAL_SPI_Transmit(&hspi1, &b, 1, HAL_MAX_DELAY);
	SPI_WRITE(b);
}

void spi_rb_burst(uint8_t *buf, uint16_t len) {
	//HAL_SPI_Receive_DMA(&hspi1, buf, len);
	//while(HAL_SPI_GetState(&hspi1) == HAL_SPI_STATE_BUSY_RX);
	for (uint16_t var = 0; var < len; var++) {
		SPI_READ(*buf++);
	}
}

void spi_wb_burst(uint8_t *buf, uint16_t len) {
	//HAL_SPI_Transmit_DMA(&hspi1, buf, len);
	//while(HAL_SPI_GetState(&hspi1) == HAL_SPI_STATE_BUSY_TX);
	for (uint16_t var = 0; var < len; var++) {
		SPI_WRITE(*buf++);
	}
}

void IO_LIBRARY_Init(void) {
	uint8_t bufSize[] = {2, 2, 2, 2, 2, 2, 2, 2};

	reg_wizchip_cs_cbfunc(cs_sel, cs_desel);
	reg_wizchip_spi_cbfunc(spi_rb, spi_wb);
	reg_wizchip_spiburst_cbfunc(spi_rb_burst, spi_wb_burst);

	wizchip_init(bufSize, bufSize);
	wizchip_setnetinfo(&netInfo);
	//wizchip_setinterruptmask(IK_SOCK_0);
}
//***************** WIZCHIP INIT: END

/*
void spi_speed_tst(void)
{
	// Here on SPI pins: MOSI 400Khz freq out, on SCLK 3.2MhzOUT
	while(1)
	{
		SPI_WRITE(0xF0);
		SPI_WRITE(0xF0);
		SPI_WRITE(0xF0);
		SPI_WRITE(0xF0);
		SPI_WRITE(0xF0);
		SPI_WRITE(0xF0);
		SPI_WRITE(0xF0);
		SPI_WRITE(0xF0);
		SPI_WRITE(0xF0);
		SPI_WRITE(0xF0);
		SPI_WRITE(0xF0);
		SPI_WRITE(0xF0);
		SPI_WRITE(0xF0);
		SPI_WRITE(0xF0);
		SPI_WRITE(0xF0);
		SPI_WRITE(0xF0);
		SPI_WRITE(0xF0);
		SPI_WRITE(0xF0);
		SPI_WRITE(0xF0);
	}
}
*/
int main()
{
	//uint8_t prev_sw1 = 1; // VAR for sw1 pressing detect

	// INIT MCU
	avr_init();
	spi_init(); //SPI Master, MODE0, 4Mhz(DIV4), CS_PB.3=HIGH - suitable for WIZNET 5x00(1/2/5)
	//spi_speed_tst(); / Here on SPI pins: MOSI 400Khz freq out, on SCLK 3.2MhzOUT (Witk SPI CLK 4Mhz)


	// Print program metrics
	PRINTF("%S", str_prog_name);// Название программы
	PRINTF("Compiled at: %S %S\r\n", compile_time, compile_date);// Время Дата компиляции
	PRINTF(">> MCU is: %S; CLK is: %luHz\r\n", str_mcu, F_CPU);// MCU Name && FREQ
	PRINTF(">> Free RAM is: %d bytes\r\n", freeRam());


	//Wizchip WIZ5500 Ethernet initialize
	IO_LIBRARY_Init(); //After that ping must working
	print_network_information();


	/* DNS client Initialization */
	PRINTF("> [BLYNK] Target Domain Name : %s\r\n", Domain_name);
    DNS_init(SOCK_DNS, gDATABUF_DNS);

    /* DNS processing */
    int32_t ret;
    if ((ret = DNS_run(netInfo.dns, Domain_name, Domain_IP)) > 0) // try to 1st DNS
    {
#ifdef _MAIN_DEBUG_
       PRINTF("> 1st DNS Respond\r\n");
#endif
    }
    else if ((ret != -1) && ((ret = DNS_run(DNS_2nd, Domain_name, Domain_IP))>0))     // retry to 2nd DNS
    {
#ifdef _MAIN_DEBUG_
    	PRINTF("> 2nd DNS Respond\r\n");
#endif
    }
    else if(ret == -1)
    {
#ifdef _MAIN_DEBUG_
    	PRINTF("> MAX_DOMAIN_NAME is too small. Should be redefined it.\r\n");
#endif
       ;
    }
    else
    {
#ifdef _MAIN_DEBUG_
    	PRINTF("> DNS Failed\r\n");
#endif
       ;
    }

    if(ret > 0)
    {
#ifdef _MAIN_DEBUG_
    	printf("> Translated %s to [%d.%d.%d.%d]\r\n\r\n",Domain_name,Domain_IP[0],Domain_IP[1],Domain_IP[2],Domain_IP[3]);
#endif
    	//IOT BLYK app init:
    	/* Blynk client Initialization  */
    	PRINTF("Try connect to BLYNK SERVER [%s]: %d.%d.%d.%d:%d..\n\r",Domain_name,Domain_IP[0],Domain_IP[1],Domain_IP[2],Domain_IP[3],BLYNK_DEFAULT_PORT);
    	blynk_begin(auth, Domain_IP, BLYNK_DEFAULT_PORT, BLYNK_TX_BUF, SOCK_BLYNK_CLIENT);
    }
    else
    {
    	PRINTF("> [BLYNK] Target Domain Name : %s resolve ERROR\r\nReboot board..\r\n", Domain_name);
    	while(1);
    }



	//Short Blink LED 3 times on startup
	unsigned char i = 3;
	while(i--)
	{
		led1_high();
		_delay_ms(100);
		led1_low();
		_delay_ms(400);
		wdt_reset();
	}


	/* Loopback Test: TCP Server and UDP */
	// Test for Ethernet data transfer validation
	uint32_t timer_link_1sec = millis();
	while(1)
	{
		//Here at least every 1sec
		wdt_reset(); // WDT reset at least every sec
		// Blynk process handler
		blynk_run();

		//Use Hercules Terminal to check loopback tcp:5000 and udp:3000
		/*
		 * https://www.hw-group.com/software/hercules-setup-utility
		 * */
		//loopback_tcps(SOCK_TCPS,ethBuf0,PORT_TCPS);
		//loopback_udps(SOCK_UDPS,ethBuf0,PORT_UDPS);

		//loopback_ret = loopback_tcpc(SOCK_TCPS, gDATABUF, destip, destport);
		//if(loopback_ret < 0) printf("loopback ret: %ld\r\n", loopback_ret); // TCP Socket Error code

		//Shouldn't use it here
		/*
		if((millis()-timer_link_1sec)> 1000)
		{
			//here every 1 sec
			timer_link_1sec = millis();
			if(wizphy_getphylink() == PHY_LINK_ON)
			{
				led1_high();
			}
			else
			{
				led1_low();
			}
		}
		*/
	}
	return 0;
}

// Timer0
// 1ms IRQ
// Used for millis() timing
void timer0_init(void)
{
	/*
	 *
	 * For M128
	TCCR0 = (1<<CS02)|(1<<WGM01); //TIMER0 SET-UP: CTC MODE & PS 1:64
	OCR0 = 249; // 1ms reach for clear (16mz:64=>250kHz:250-=>1kHz)
	TIMSK |= 1<<OCIE0;	 //IRQ on TIMER0 output compare
	 */
	//For M664p
	TCCR0A = (1<<WGM01); //TIMER0 SET-UP: CTC MODE
	TCCR0B = (1<<CS01)|(1<<CS00); // PS 1:64
	OCR0A = 249; // 1ms reach for clear (16mz:64=>250kHz:250-=>1kHz)
	TIMSK0 |= 1<<OCIE0A;	 //IRQ on TIMER0 output compareA
}

static void avr_init(void)
{
	// Initialize device here.
	// WatchDog INIT
	wdt_enable(WDTO_8S);  // set up wdt reset interval 2 second
	wdt_reset(); // wdt reset ~ every <2000ms

	timer0_init();// Timer0 millis engine init

	// Initial UART Peripheral
	/*
	 *  Initialize uart11 library, pass baudrate and AVR cpu clock
	 *  with the macro
	 *  uart1_BAUD_SELECT() (normal speed mode )
	 *  or
	 *  uart1_BAUD_SELECT_DOUBLE_SPEED() ( double speed mode)
	 */
#if	(UART_BAUD_RATE == 115200)
	uart_init( UART_BAUD_SELECT_DOUBLE_SPEED(UART_BAUD_RATE,F_CPU) ); // To works without error on 115200 bps/F_CPU=16Mhz
#else
	uart_init( UART_BAUD_SELECT(UART_BAUD_RATE,F_CPU) );
#endif
	// Define Output/Input Stream
	stdout = &uart0_stdout;

	//ADC init
	adc_init();
	adc_read(0); //Dummy read


	led1_conf();
	led1_low();// LED1 is OFF


	sw1_conf();//SW1 internal pull-up

	sei(); //re-enable global interrupts

	return;
}

void print_network_information(void)
{

	uint8_t tmpstr[6] = {0,};
	ctlwizchip(CW_GET_ID,(void*)tmpstr); // Get WIZCHIP name
    PRINTF("\r\n=======================================\r\n");
    PRINTF(" WIZnet chip:  %s \r\n", tmpstr);
    PRINTF("=======================================\r\n");

	wiz_NetInfo gWIZNETINFO;
	wizchip_getnetinfo(&gWIZNETINFO);
	if (gWIZNETINFO.dhcp == NETINFO_STATIC)
		PRINTF("STATIC IP\r\n");
	else
		PRINTF("DHCP IP\r\n");
	PRINTF("Mac address: %02x:%02x:%02x:%02x:%02x:%02x\n\r",gWIZNETINFO.mac[0],gWIZNETINFO.mac[1],gWIZNETINFO.mac[2],gWIZNETINFO.mac[3],gWIZNETINFO.mac[4],gWIZNETINFO.mac[5]);
	PRINTF("IP address : %d.%d.%d.%d\n\r",gWIZNETINFO.ip[0],gWIZNETINFO.ip[1],gWIZNETINFO.ip[2],gWIZNETINFO.ip[3]);
	PRINTF("SM Mask	   : %d.%d.%d.%d\n\r",gWIZNETINFO.sn[0],gWIZNETINFO.sn[1],gWIZNETINFO.sn[2],gWIZNETINFO.sn[3]);
	PRINTF("Gate way   : %d.%d.%d.%d\n\r",gWIZNETINFO.gw[0],gWIZNETINFO.gw[1],gWIZNETINFO.gw[2],gWIZNETINFO.gw[3]);
	PRINTF("DNS Server : %d.%d.%d.%d\n\r",gWIZNETINFO.dns[0],gWIZNETINFO.dns[1],gWIZNETINFO.dns[2],gWIZNETINFO.dns[3]);
}

