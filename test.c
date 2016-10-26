/*
 * Copyright 2016 saybur
 * 
 * This file is part of trabular.
 * 
 * trabular is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * trabular is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with trabular.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "serial.h"

#define BAUD 38400
#include <util/setbaud.h>

static void test_ring_buffer();
static void test_sequential();
static void test_keyboard();

static void report(uint8_t, uint8_t, uint16_t);
static uint8_t nibble_to_ascii(uint8_t);
static void uart_send(uint8_t);

/*
 * Test code for use on a Arduino.  Flash the unit with the code,
 * attach via the USB serial, adjust settings, then reset to see the
 * results in ASCII text.
 */
int main()
{
	// setup UART for sending results
	UBRR0H = UBRRH_VALUE;
	UBRR0L = UBRRL_VALUE;
	#if USE_2X
		UCSR0A |= _BV(U2X0);
	#else
		UCSR0A &= ~(_BV(U2X0));
	#endif
	UCSR0B |= _BV(TXEN0);
	
	test_ring_buffer();
	test_sequential();
	test_keyboard();
	
	// go into busy loop until reset
	while (1);
	return 0;
}

/*
 * Test to ensure the ring buffer construct is consistent in both
 * single and dual entry mode, and returns correct results.
 */
static void test_ring_buffer()
{
	uart_send(nibble_to_ascii((RING_BUFFER_BITS & 0xF0) >> 4));
	uart_send(nibble_to_ascii((RING_BUFFER_BITS & 0x0F)));
	uart_send('\n');
	
	struct buffer buf = {};
	ring_buffer_add_dual(&buf, 0x01, 0x02);
	ring_buffer_add_dual(&buf, 0x03, 0x04);
	report(1, 2, ring_buffer_peek(&buf));
	ring_buffer_add(&buf, 0x05);
	ring_buffer_add_dual(&buf, 0x06, 0x07);
	ring_buffer_add_dual(&buf, 0x08, 0x09);
	ring_buffer_add_dual(&buf, 0x0A, 0x0B);
	ring_buffer_add_dual(&buf, 0x0C, 0x0D);
	ring_buffer_add_dual(&buf, 0x0E, 0x0F);
	ring_buffer_add_dual(&buf, 0x10, 0x11);
	report(0x01, 0x02, ring_buffer_peek(&buf));
	ring_buffer_drain(&buf, 2);
	report(0x03, 0x04, ring_buffer_peek(&buf));
	ring_buffer_add_dual(&buf, 0x12, 0x13);
	ring_buffer_add_dual(&buf, 0x14, 0x15);
	report(0x03, 0x04, ring_buffer_peek(&buf));
	ring_buffer_drain(&buf, 2);
	report(0x05, 0x00, ring_buffer_peek(&buf));
	ring_buffer_drain(&buf, 2);
	report(0x06, 0x07, ring_buffer_peek(&buf));
	ring_buffer_drain(&buf, 2);
	report(0x08, 0x09, ring_buffer_peek(&buf));
	ring_buffer_drain(&buf, 2);
	report(0x0A, 0x0B, ring_buffer_peek(&buf));
	ring_buffer_drain(&buf, 2);
	report(0x0C, 0x0D, ring_buffer_peek(&buf));
	ring_buffer_drain(&buf, 2);
	report(0x0E, 0x0F, ring_buffer_peek(&buf));
	ring_buffer_drain(&buf, 2);
	report(0x12, 0x13, ring_buffer_peek(&buf));
	ring_buffer_drain(&buf, 2);
	report(0x00, 0x00, ring_buffer_peek(&buf));
	report(0, 2, ring_buffer_size(&buf));
	report(0, 0, ring_buffer_empty(&buf));
	ring_buffer_drain(&buf, 2);
	report(0, 0, ring_buffer_size(&buf));
	report(0, 1, ring_buffer_empty(&buf));
	
	ring_buffer_add_dual(&buf, 20, 21);
	report(20, 21, ring_buffer_peek(&buf));
	report(0, 2, ring_buffer_size(&buf));
	report(0, 0, ring_buffer_empty(&buf));
	ring_buffer_clear(&buf);
	report(0, 0, ring_buffer_size(&buf));
	report(0, 1, ring_buffer_empty(&buf));
}

/*
 * Profile all input values into to the SPI system.
 */
static void test_sequential()
{
	uart_send(nibble_to_ascii(0x00));
	uart_send(nibble_to_ascii(0x0A));
	uart_send('\n');
	
	uint16_t i;
	for (i = 0; i <= 0xFF; i++)
	{
		// start timer @ prescale /1 and measure an operation
		TCCR1B = 0x01;
		handle_serial_data(i);
		TCCR1B = 0x00;

		// print the timing data and reset for the next run
		report(0, i, TCNT1);
		TCNT1 = 0;
	}
}

/*
 * Profile all keyboard codes.  This sends the relevant low bit, then
 * times the upper bit's performance (low bit should be very fast,
 * check other test methods).
 */
static void test_keyboard()
{
	uint16_t i;
	uint8_t nib;
	
	for (i = 0; i <= 0xFF; i++)
	{
		// send low bits, which are known to be fast
		handle_serial_data((i & 0x0F) + 0x40);
		
		// then send high bits and check performance
		nib = ((i & 0xF0) >> 4) + 0x50;
		TCCR1B = 0x01;
		handle_serial_data(nib);
		TCCR1B = 0x00;

		// print the timing data and reset for the next run
		report(1, i, TCNT1);
		TCNT1 = 0;
		ring_buffer_clear(&kbd_buf);
		ring_buffer_add(&kbd_buf, 0); // garbage to slow 7F/FF down
	}
}

static void report(uint8_t p, uint8_t i, uint16_t v)
{
	uart_send(nibble_to_ascii((p & 0xF0) >> 4));
	uart_send(nibble_to_ascii((p & 0x0F)));
	uart_send(' ');
	uart_send(nibble_to_ascii((i & 0xF0) >> 4));
	uart_send(nibble_to_ascii((i & 0x0F)));
	uart_send(' ');
	uart_send(nibble_to_ascii((v & 0xF000) >> 12));
	uart_send(nibble_to_ascii((v & 0x0F00) >> 8));
	uart_send(nibble_to_ascii((v & 0x00F0) >> 4));
	uart_send(nibble_to_ascii(v & 0x000F));
	uart_send('\n');
}

static void uart_send(uint8_t c)
{
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = c;
}

static uint8_t nibble_to_ascii(uint8_t v)
{
	if(v < 0x0A)
	{
		return v + 0x30;
	}
	else if(v < 0x0F)
	{
		return v + 0x37;
	}
	else
	{
		return 0x46;
	}
}
