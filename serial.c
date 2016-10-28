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

#ifdef USE_USART
	#define BAUD 38400
	#include <util/setbaud.h>
#endif

#ifdef USE_KEYBOARD
	static void handle_keyboard_data(uint8_t kc);
	static uint8_t kbd_temp = 0;
#endif

#ifdef USE_ARBITRARY
	static uint8_t arb_buf0_tmp = 0;
#endif


// --- methods ---

void init_data()
{
	#ifdef USE_USART
		// setup USART for the data connection
		UBRR0H = UBRRH_VALUE;
		UBRR0L = UBRRL_VALUE;
		#if USE_2X
			UCSR0A |= (1 << U2X0);
		#else
			UCSR0A &= ~(1 << U2X0);
		#endif
		// and enable
		UCSR0B = _BV(RXEN0) | _BV(TXEN0);
	
	#else /* ! USE_USART */
		// SPI enable instead
		USICR |= _BV(USIWM0) | _BV(USICS1);
	#endif /* USE_USART */
}

void handle_data()
{
	// --- use USI ---
	#ifndef USE_USART
		// is there a new byte? if not, nothing to do here
		if (!(USISR & _BV(USIOIF))) return;
		// read the buffer contents, clear the data flag, and process
		uint8_t serial = USIBR;
		USISR |= _BV(USIOIF);
		USIDR = handle_serial_data(serial);
		
	// --- use USART ---
	#else
		if (!(UCSR0A & _BV(RXC0))) return;
		uint8_t serial = UDR0;
		#ifndef DEBUG_MODE
			uint8_t response = handle_serial_data(serial);
			if (response > 0)
			{
				UDR0 = response;
			}
		#else
			handle_serial_data(serial);
		#endif /* ! DEBUG_MODE */
	
	#endif /* USE_USART */
}

uint8_t handle_serial_data(uint8_t spi)
{
	// the upper 4 bits of SPI byte control flow
	uint8_t cmd = spi >> 4;
	// and the lower 4 are the data bits
	uint8_t payload = spi & 0x0F;
	
	// status reporting variable, to be used later
	uint8_t status;
	
	// do keyboard upper bits first, as they have the largest
	// performance issues
	#ifdef USE_KEYBOARD
	if (cmd == 5)
	{
		// keyboard upper nibble, push
		uint8_t kc = (payload << 4) + kbd_temp;
		handle_keyboard_data(kc);
	}
	#endif /* USE_KEYBOARD */
	
	// route to the correct code
	if (cmd < 4)
	{
		if (cmd == 0)
		{
			// special commands, in the 0x0-0xF range
			switch(payload)
			{
			case 0x01: // TALK STATUS
				status = 0x80;
				#ifdef USE_ARBITRARY
				if (arb_buf2_set)
				{
					status |= _BV(3);
				}
				if (arb_buf0_set)
				{
					status |= _BV(2);
				}
				#endif /* USE_ARBITRARY */
				#ifdef USE_KEYBOARD
				if (ring_buffer_size(&kbd_buf) > RING_BUFFER_HALF_SIZE)
				{
					status |= _BV(0);
				}
				#endif /* USE_KEYBOARD */
				return status;
			#ifdef USE_ARBITRARY
			case 0x02: // ARBITRARY REGISTER 0 READY
				if (arb_buf0_len >= 2)
				{
					arb_buf0_set = 1;
				}
				break;
			case 0x03: // ARBITRARY CLEAR REGISTER 0
				arb_buf0_len = 0;
				arb_buf0_set = 0;
				break;
			case 0x04: // ARBITRARY REGISTER 2 CLEAR
				arb_buf2_high = 0;
				arb_buf2_low = 0;
				arb_buf2_set = 0;
				break;
			#endif /* USE_ARBITRARY */
			#ifdef USE_KEYBOARD
			case 0x05: // KEYBOARD CLEAR REGISTER 0
				ring_buffer_clear(&kbd_buf);
				break;
			#endif /* USE_KEYBOARD */
			#ifdef USE_MOUSE
			case 0x06: // MOUSE CLEAR BUTTONS
				mse_btn_data = 0x00;
				break;
			case 0x07: // MOUSE CLEAR X MOTION
				mse_x = 0;
				break;
			case 0x08: // MOUSE CLEAR Y MOTION
				mse_y = 0;
				break;
			#endif /* USE_MOUSE */
			#ifdef USE_ARBITRARY
			case 0x0C: // TALK ARBITRARY REGISTER 2 BYTE 0 LOWER NIBBLE
				return 0x40 + (arb_buf2_low & 0x0F);
			case 0x0D: // TALK ARBITRARY REGISTER 2 BYTE 0 UPPER NIBBLE
				return 0x50 + ((arb_buf2_low & 0xF0) >> 4);
			case 0x0E: // TALK ARBITRARY REGISTER 2 BYTE 1 LOWER NIBBLE
				return 0x60 + (arb_buf2_high & 0x0F);
			case 0x0F: // TALK ARBITRARY REGISTER 2 BYTE 1 UPPER NIBBLE
				return 0x70 + ((arb_buf2_high & 0xF0) >> 4);
			#endif /* USE_ARBITRARY */
			default: // all others reserved
				break;
			}
		}
		#ifdef USE_ARBITRARY
		else if (cmd == 2)
		{
			// arbitrary device register 0 lower nibble
			arb_buf0_tmp = payload;
		}
		else if (cmd == 3)
		{
			// and the upper nibble + update
			// drop if no space remaining
			if (arb_buf0_len < ARB_BUF0_SIZE)
			{
				arb_buf0[arb_buf0_len++] = (payload << 4) + arb_buf0_tmp;
			}
			// clear temp regardless
			arb_buf0_tmp = 0;
		}
		#endif /* USE_ARBITRARY */
	}
	else if (cmd < 8)
	{
		if (cmd < 6)
		{
			#ifdef USE_KEYBOARD
			// command 4 only, 5 was handled earlier
			// keyboard lower nibble, update lower nibble temp
			kbd_temp = payload;
			#endif /* USE_KEYBOARD */
		}
		else
		{
			#ifdef USE_MOUSE
			
			if (cmd == 6)
			{
				// set mouse buttons, lower nibble
				uint8_t new_mse_btn_data = payload;
				new_mse_btn_data |= mse_btn_data & 0xF0;
				mse_btn_data = new_mse_btn_data;
			}
			else
			{
				// set mouse buttons, upper nibble
				uint8_t new_mse_btn_data = payload << 4;
				new_mse_btn_data |= mse_btn_data & 0x0F;
				mse_btn_data = new_mse_btn_data;
			}
			
			#endif /* USE_MOUSE */
		}
	}
	else
	{
		#ifdef USE_MOUSE
		
		uint8_t high_bits = (cmd & 2) >> 1;
		uint8_t positive = ! (cmd & 1);
		
		if (cmd < 12)
		{
			// x motion
			if (positive)
			{
				if (high_bits)
				{
					mse_x += payload << 4;
				}
				else
				{
					mse_x += payload;
				}
			}
			else
			{
				if (high_bits)
				{
					mse_x -= payload << 4;
				}
				else
				{
					mse_x -= payload;
				}
			}
		}
		else
		{
			// y motion
			if (positive)
			{
				if (high_bits)
				{
					mse_y += payload << 4;
				}
				else
				{
					mse_y += payload;
				}
			}
			else
			{
				if (high_bits)
				{
					mse_y -= payload << 4;
				}
				else
				{
					mse_y -= payload;
				}
			}
		}
		
		#endif /* USE_MOUSE */
	}
	
	// catch-all return indicating no data
	return 0;
}

#ifdef USE_KEYBOARD
/*
 * Takes a given keycode and applies it to both the keyboard buffer and
 * the relevant bits of register 2.
 */
static void handle_keyboard_data(uint8_t kc)
{
	uint8_t up = kc >> 7; // flag for up or down
	uint8_t key = kc & 0x7F; // and the key to test
	
	// there is special handling for the power key
	if (key == 0x7F)
	{
		// add command and set the relevant flag
		ring_buffer_add_dual(&kbd_buf, kc, kc);
		if(up)
		{
			kbd_reg2_high &= ~_BV(KBD_REG2_RST_BIT);
		}
		else
		{
			kbd_reg2_high |= _BV(KBD_REG2_RST_BIT);
		}
		
		// then we're done
		return;
	}
	
	// keyboard upper nibble, push into buffer with previous 
	// low nibble and reset the lower nibble temporary value
	ring_buffer_add(&kbd_buf, kc);
	kbd_temp = 0;
				
	// update register 2 flags information with keys
	switch (key)
	{
	case 0x71: // scroll lock
		if (up)
		{
			kbd_reg2_low &= ~_BV(KBD_REG2_SCRL_BIT);
		}
		else
		{
			kbd_reg2_low |= _BV(KBD_REG2_SCRL_BIT);
		}
		break;
	case 0x47: // num lock
		if (up)
		{
			kbd_reg2_low &= ~_BV(KBD_REG2_NUML_BIT);
		}
		else
		{
			kbd_reg2_low |= _BV(KBD_REG2_NUML_BIT);
		}
		break;
	case 0x37: // command
		if (up)
		{
			kbd_reg2_high &= ~_BV(KBD_REG2_CMD_BIT);
		}
		else
		{
			kbd_reg2_high |= _BV(KBD_REG2_CMD_BIT);
		}
		break;
	case 0x3A: // left option
	case 0x7C: // right option
		if (up)
		{
			kbd_reg2_high &= ~_BV(KBD_REG2_OPT_BIT);
		}
		else
		{
			kbd_reg2_high |= _BV(KBD_REG2_OPT_BIT);
		}
		break;
	case 0x38: // left shift
	case 0x7B: // right shift
		if (up)
		{
			kbd_reg2_high &= ~_BV(KBD_REG2_SHFT_BIT);
		}
		else
		{
			kbd_reg2_high |= _BV(KBD_REG2_SHFT_BIT);
		}
		break;
	case 0x36: // left control
	case 0x7D: // right control
		if (up)
		{
			kbd_reg2_high &= ~_BV(KBD_REG2_CNTL_BIT);
		}
		else
		{
			kbd_reg2_high |= _BV(KBD_REG2_CNTL_BIT);
		}
		break;
	case 0x39: // caps lock
		if (up)
		{
			kbd_reg2_high &= ~_BV(KBD_REG2_CPSL_BIT);
		}
		else
		{
			kbd_reg2_high |= _BV(KBD_REG2_CPSL_BIT);
		}
		break;
	case 0x33: // delete
		if (up)
		{
			kbd_reg2_high &= ~_BV(KBD_REG2_DEL_BIT);
		}
		else
		{
			kbd_reg2_high |= _BV(KBD_REG2_DEL_BIT);
		}
		break;
	}
}
#endif /* USE_KEYBOARD */
