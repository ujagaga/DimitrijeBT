/*
 * main.c
 *
 *  Created on: Jul 5, 2016
 *      Author: ujagaga
 */

#include <util/delay.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdbool.h>


#define USART_BAUDRATE 	(9600)
#define BAUD_PRESCALE 	(((( F_CPU / 16) + ( USART_BAUDRATE / 2) ) / ( USART_BAUDRATE ) ) - 1)
#define waitTxReady()	while (( UCSRA & (1 << UDRE ) ) == 0)
#define QueueSize		(32)
#define QueueMask		(QueueSize - 1)
#define QueueEmpty()	(((WrIdx - RdIdx) & QueueMask) == 0)
#define QueueFull()		(((WrIdx - RdIdx) & QueueMask) > (QueueSize - 2))


typedef enum{
	msgAck = 0,
	msgFail = 1,
	msgFull = 2,
	msgStart = ':',
	msgStopCurrent = 0xFE,
	msgStopAll = 0xFF
}msg_t;

typedef struct{
	uint8_t state;
	uint8_t delay;
}state_t;


volatile state_t ActionQueue[QueueSize];
volatile uint8_t RdIdx = 0;
volatile uint8_t WrIdx = 0;
volatile bool stopCurrent_flag = false;
volatile bool stopAll_flag = false;
volatile uint8_t rx_index = 0;
volatile uint8_t rcvPinState;
volatile uint8_t rcvDuration;

bool addToActionQueue(uint8_t value, uint8_t duration);


void sendMsg(uint8_t oneByte)
{
	UDR = oneByte;
	waitTxReady();
	UDR = '\n';
	waitTxReady();
}

uint8_t charToInt(uint8_t data){

	if((data > 47) && (data < 58)){
		return (data - '0');
	}

	if((data > 64) && (data < 71)){
		return (data - 'A' + 10);
	}

	if((data > 96) && (data < 103)){
		return (data - 'a' + 10);
	}

	return 0;
}


ISR(USART_RX_vect) {

	uint8_t received = UDR;

	if(received == msgStart){
		rx_index = 0;
	}


//	if(received == ':'){
//		PORTB = 0xF0;
//	}else{
//		PORTB = (charToInt(received) << 1);
//	}
//
//	_delay_us(30);
//	PORTB = 0;


	switch(rx_index){
		case 1:
			rcvPinState = charToInt(received) << 4;
			break;
		case 2:
			rcvPinState += charToInt(received);
			break;
		case 3:
			rcvDuration = charToInt(received) << 4;
			break;
		case 4:
		{
			rcvDuration += charToInt(received);
			/* All received */
			if(rcvPinState == msgStopCurrent){
				stopCurrent_flag = true;
			}else if(rcvPinState == msgStopAll){
				stopAll_flag = true;
			}else{
				addToActionQueue(rcvPinState, rcvDuration);
			}
			break;
		}
		default:
			rcvPinState = 0;
			rcvDuration = 0;
			break;
	}

	/* Increment but prevent wrap around */
	if(rx_index < 0xFE){
		rx_index++;
	}
}


void uart_init(void)
{

	DDRD = 2; // set TX pin as output

	// lets set the baud rate
	UBRRL = (unsigned char)BAUD_PRESCALE;
	UBRRH = (BAUD_PRESCALE >> 8);

	// enable tx and rx
	UCSRB = (1<<TXEN) | (1<<RXEN);

	//  enable RX interrupt
	UCSRB |= (1 << RXCIE);

	rx_index = 0;
}


void custom_delay(uint8_t interval){
	while((interval > 0) && !stopCurrent_flag ){
		_delay_ms(100);
		interval--;
	}

	stopCurrent_flag = false;
}


bool addToActionQueue(uint8_t value, uint8_t duration){
	if(QueueFull()){
		return false;
	}

	ActionQueue[WrIdx & QueueMask].state = value;
	ActionQueue[WrIdx & QueueMask].delay = duration;
	WrIdx++;
	return true;
}


bool getFromActionQueue(uint8_t* state, uint8_t* duration){
	if(QueueEmpty()){
		return false;
	}

	*state = ActionQueue[RdIdx & QueueMask].state;
	*duration = ActionQueue[RdIdx & QueueMask].delay;

	RdIdx++;
	return true;
}


int main( void ){

	uint8_t state, duration;

	ACSR |= 1 << ACD; /* Disable analog comparer to reduce power consumption */

	DDRB = 0xFF; /* Configure output port B*/

	uart_init();

	sei();

	while(1){
		if(getFromActionQueue(&state, &duration)){

			PORTB = (state << 1);  /* Only pins 1 through 7 are connected  */
			custom_delay(duration);
		}

		if(stopAll_flag){
			PORTB = 0;
			RdIdx = WrIdx;
			stopAll_flag = false;
		}

		asm("nop");

	}
	return 0;
}



