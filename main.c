
/* Name: main.c
 * Project: midicom (based on V-USB MIDI device on Low-Speed USB)
 * Author: Anton Eliasson
 * Creation Date: 2008-03-11
 * Copyright: (c) 2008 by Martin Homuth-Rosemann, 2011 by Anton Eliasson.
 * License: GNU General Public License version 2.
 *
 */

//---------------------------------------------------------------------------
// Includes

#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>

#include "usbdrv.h"
#include "oddebug.h"

#include "usbdescriptor.h"

//---------------------------------------------------------------------------
// Pin definitions

#define BTN_PORT PORTB
#define BTN0_PIN PB0
#define LED_PORT PORTC
#define LED0_PIN PC0
#define LED1_PIN PC1
#define LED2_PIN PC2
#define LED3_PIN PC3
#define LED4_PIN PC4
#define LED5_PIN PC5 // unused

//---------------------------------------------------------------------------
// Macros

#define sbi(port, bit) (port) |= (1 << (bit))
#define cbi(port, bit) (port) &= ~(1 << (bit))

//---------------------------------------------------------------------------
// Variable declarations

#define FOSC 12000000 // Clock Speed
#define BAUD 31250 // MIDI @ 31,25 kbaud
#define MYUBRR FOSC/16/BAUD-1

//---------------------------------------------------------------------------

// USART functions
void USART_Init( unsigned int ubrr)
{
    /*Set baud rate */
    UBRR0H = (unsigned char)(ubrr>>8);
    UBRR0L = (unsigned char)ubrr;
    /* Enable receiver and transmitter */
    UCSR0B = (1<<RXEN0)|(1<<TXEN0);
    /* Set frame format: 8data, 2stop bit */
    UCSR0C = (1<<USBS0)|(3<<UCSZ00);
}

void USART_Transmit( unsigned char data )
{
    /* Wait for empty transmit buffer */
    while ( !( UCSR0A & (1<<UDRE0)) )
    ;
    /* Put data into buffer, sends the data */
    UDR0 = data;
}

unsigned char USART_Receive( void )
{
    /* Wait for data to be received */
    while ( !(UCSR0A & (1<<RXC0)) )
    ;
    /* Get and return received data from buffer */
    return UDR0;
}

uchar usbFunctionDescriptor(usbRequest_t * rq)
{

	if (rq->wValue.bytes[1] == USBDESCR_DEVICE) {
		usbMsgPtr = (uchar *) deviceDescrMIDI;
		return sizeof(deviceDescrMIDI);
	} else {		/* must be config descriptor */
		usbMsgPtr = (uchar *) configDescrMIDI;
		return sizeof(configDescrMIDI);
	}
}


static uchar sendEmptyFrame;


/* ------------------------------------------------------------------------- */
/* ----------------------------- USB interface ----------------------------- */
/* ------------------------------------------------------------------------- */

uchar usbFunctionSetup(uchar data[8])
{
	usbRequest_t *rq = (void *) data;

	// DEBUG LED
	LED_PORT ^= (1<<LED0_PIN); // never used?

	if ((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS) {	/* class request type */

		/*  Prepare bulk-in endpoint to respond to early termination   */
		if ((rq->bmRequestType & USBRQ_DIR_MASK) ==
		    USBRQ_DIR_HOST_TO_DEVICE)
			sendEmptyFrame = 1;
	}

	return 0xff;
}


/*---------------------------------------------------------------------------*/
/* usbFunctionRead                                                           */
/*---------------------------------------------------------------------------*/

uchar usbFunctionRead(uchar * data, uchar len)
{
	// DEBUG LED
	LED_PORT ^= (1<<LED1_PIN); // never used?

	data[0] = 0;
	data[1] = 0;
	data[2] = 0;
	data[3] = 0;
	data[4] = 0;
	data[5] = 0;
	data[6] = 0;

	return 7;
}


/*---------------------------------------------------------------------------*/
/* usbFunctionWrite                                                          */
/*---------------------------------------------------------------------------*/

uchar usbFunctionWrite(uchar * data, uchar len)
{
	// DEBUG LED
	LED_PORT ^= (1<<LED2_PIN); // never used?
	return 1;
}


/*---------------------------------------------------------------------------*/
/* usbFunctionWriteOut                                                       */
/*                                                                           */
/* this Function is called if a MIDI Out message (from PC) arrives.          */
/*                                                                           */
/*---------------------------------------------------------------------------*/

void usbFunctionWriteOut(uchar * data, uchar len)
{
	// DEBUG LED
	LED_PORT ^= (1<<LED3_PIN);
}



/*---------------------------------------------------------------------------*/
/* hardwareInit                                                              */
/*---------------------------------------------------------------------------*/

static void hardwareInit(void)
{
	uchar i, j;

	/* activate pull-ups except on USB lines */
	USB_CFG_IOPORT =
	    (uchar) ~ ((1 << USB_CFG_DMINUS_BIT) |
		       (1 << USB_CFG_DPLUS_BIT));
	/* all pins input except USB (-> USB reset) */
#ifdef USB_CFG_PULLUP_IOPORT	/* use usbDeviceConnect()/usbDeviceDisconnect() if available */
	USBDDR = 0;		/* we do RESET by deactivating pullup */
	usbDeviceDisconnect();
#else
	USBDDR = (1 << USB_CFG_DMINUS_BIT) | (1 << USB_CFG_DPLUS_BIT);
#endif

	j = 0;
	while (--j) {		/* USB Reset by device only required on Watchdog Reset */
		i = 0;
		while (--i);	/* delay >10ms for USB reset */
	}
#ifdef USB_CFG_PULLUP_IOPORT
	usbDeviceConnect();
#else
	USBDDR = 0;		/*  remove USB reset condition */
#endif

    USART_Init(MYUBRR); // init midi connection

// keys/switches setup
// PORTB has up to six keys (active low).
	PORTB = 0xff;		/* activate all pull-ups */
	DDRB = 0;		/* all pins input */
// PORTC has up to six debug LEDs (active low).
	PORTC = 0xff;		/* all LEDs off, pullups on the rest of the pins */
	DDRC = 0x3f;		/* pins PC0-PC5 output */
}



/* Simple monophonic keyboard
   The following function returns a midi note value for the first key pressed. 
   Key 0 -> 60 (middle C),
   Key 1 -> 62 (D)
   Key 2 -> 64 (E)
   Key 3 -> 65 (F)
   Key 4 -> 67 (G)
   Key 5 -> 69 (A)
   Key 6 -> 71 (B)
   Key 7 -> 72 (C)
 * returns 0 if no key is pressed.
 */
static uchar keyPressed(void)
{
	uchar i, mask, x;

	x = PINB;
	mask = 1;
	for (i = 0; i <= 10; i += 2) {
		if (6 == i)
			i--;
		if ((x & mask) == 0)
			return i + 60;
		mask <<= 1;
	}
	return 0;
}



int main(void)
{
	uchar key, lastKey = 0;
	uchar keyDidChange = 0;
	uchar midiMsg[8];
	uchar iii;

	wdt_enable(WDTO_1S);
	hardwareInit();
	odDebugInit();
	usbInit();

	sendEmptyFrame = 0;

	sei();
	
	for (;;) {		/* main event loop */
		wdt_reset();
		usbPoll();

		key = keyPressed();
		if (lastKey != key)
			keyDidChange = 1;

		if (usbInterruptIsReady()) {
			if (keyDidChange) {
				LED_PORT ^= (1<<LED4_PIN); // blinkar när en knapp trycks in?
				/* use last key and not current key status in order to avoid lost
				   changes in key status. */
				// up to two midi events in one midi msg.
				// For description of USB MIDI msg see:
				// http://www.usb.org/developers/devclass_docs/midi10.pdf
				// 4. USB MIDI Event Packets
				iii = 0;
				if (lastKey) {	/* release */
					midiMsg[iii++] = 0x08; // USB: CN=Cable 0, CIN=Note-off event
					midiMsg[iii++] = 0x80; // MIDI: Channel 0, Note-off
					midiMsg[iii++] = lastKey; // MIDI: Key number
					midiMsg[iii++] = 0x00; // MIDI: velocity (0=min)
				}
				if (key) {	/* press */
					midiMsg[iii++] = 0x09; // USB: CN=Cable 0, CIN=Note-on event
					midiMsg[iii++] = 0x90; // MIDI: Channel 0, Note-on
					midiMsg[iii++] = key; // MIDI: Key number
					midiMsg[iii++] = 0x7f; // MIDI: velocity (0x7f=max)
				}
				if (8 == iii)
					sendEmptyFrame = 1;
				else
					sendEmptyFrame = 0;
				usbSetInterrupt(midiMsg, iii);
				keyDidChange = 0;
				lastKey = key;
			}
		}		// usbInterruptIsReady()
	}
	return 0;
}
