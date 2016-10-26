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

#include "registers.h"

void reset_registers()
{
	#ifdef USE_KEYBOARD
		reset_kbd_data();
	#endif
	#ifdef USE_MOUSE
		reset_mse_data();
	#endif
	#ifdef USE_ARBITRARY
		reset_arb_data();
	#endif
}

uint8_t devices_needing_srq()
{
	uint8_t srq = 0;
	
	// keyboard needs servicing if any chars are in buffer
	#ifdef USE_KEYBOARD
	if (! ring_buffer_empty(&kbd_buf))
	{
		srq |= ADB_KBD_FLAG_MASK;
	}
	#endif
	
	// mouse needs servicing if it has button changes *or* movement
	#ifdef USE_MOUSE
	if (mse_btn_data != mse_btn_reported || mse_x != 0 || mse_y != 0)
	{
		srq |= ADB_MSE_FLAG_MASK;
	}
	#endif
	
	// arb needs servicing only if asked for it
	#ifdef USE_ARBITRARY
	if (arb_buf0_set)
	{
		srq |= ADB_ARB_FLAG_MASK;
	}
	#endif
	
	return srq;
}

void device_flush(uint8_t target)
{
	if (target & ADB_KBD_FLAG_MASK)
	{
		#ifdef USE_KEYBOARD
		reset_kbd_data();
		#endif
	}
	else if (target & ADB_MSE_FLAG_MASK)
	{
		#ifdef USE_MOUSE
		reset_mse_data();
		#endif
	}
	else if (target & ADB_ARB_FLAG_MASK)
	{
		#ifdef USE_ARBITRARY
		reset_arb_data();
		#endif
	}
}


// --- KEYBOARD ---
#ifdef USE_KEYBOARD

uint8_t kbd_addr;
uint8_t kbd_handler;
struct buffer kbd_buf;
uint8_t kbd_reg2_low;
uint8_t kbd_reg2_high;


static uint8_t kbd_talk_size = 0;

void reset_kbd_data()
{
	ring_buffer_clear(&kbd_buf);
	kbd_reg2_low = ~0;
	kbd_reg2_high = ~0;
	kbd_talk_size = 0;
}

uint8_t kbd_talk(uint8_t *xmit, uint8_t reg)
{
	if (reg == 0)
	{
		kbd_talk_size = ring_buffer_size(&kbd_buf);
		if (kbd_talk_size == 0)
		{
			return 0;
		}
		else
		{
			uint16_t kbd = ring_buffer_peek(&kbd_buf);
			xmit[0] = kbd >> 8;
			xmit[1] = kbd;
			return 2;
		}
	}
	else if (reg == 2)
	{
		xmit[0] = kbd_reg2_high;
		xmit[1] = kbd_reg2_low;
		return 2;
	}
	else if (reg == 3)
	{
		xmit[0] = 0x60; // per ADB protocol
		xmit[0] += kbd_addr;
		xmit[1] = kbd_handler;
		return 2;
	}
	else
	{
		return 0;
	}
}

void kbd_talk_drain(uint8_t reg)
{
	if (reg == 0)
	{
		ring_buffer_drain(&kbd_buf, kbd_talk_size);
	}
}

void kbd_listen(uint8_t reg, uint16_t data)
{
	if (reg == 2)
	{
		// set LEDs only
		kbd_reg2_low = (kbd_reg2_low & 0xF8)
				+ (data & 0x07);
	}
}

#endif /* USE_KEYBOARD */


// --- MOUSE ---
#ifdef USE_MOUSE

uint8_t mse_addr;
uint8_t mse_handler;
uint8_t mse_init_handler = 1;
uint8_t mse_btn_data;
uint8_t mse_btn_reported;
int16_t mse_x;
int16_t mse_y;
static int8_t mse_x_last = 0;
static int8_t mse_y_last = 0;

void reset_mse_data()
{
	mse_btn_data = 0;
	mse_btn_reported = 0;
	mse_x = 0;
	mse_y = 0;
}

uint8_t mse_talk(uint8_t *xmit, uint8_t reg)
{
	if (reg == 0
		&& (mse_x != 0
				|| mse_y != 0
				|| mse_btn_data != mse_btn_reported))
	{
		// gather x, y bits
		if (mse_x < 0)
		{
			if (mse_x < -64) mse_x_last = -64;
			else mse_x_last = (int8_t) mse_x;
		}
		else
		{
			if (mse_x > 63) mse_x_last = 63;
			else mse_x_last = (int8_t) mse_x;
		}
		if (mse_y < 0)
		{
			if (mse_y < -64) mse_y_last = -64;
			else mse_y_last = (int8_t) mse_y;
		}
		else
		{
			if (mse_y > 63) mse_y_last = 63;
			else mse_y_last = (int8_t) mse_y;
		}
		
		// then store in two's complement
		xmit[0] = mse_y_last & 0x7F;
		xmit[1] = mse_x_last & 0x7F;
		
		// do buttons
		xmit[0] |= ((~mse_btn_data) & 1) << 7;
		xmit[1] |= ((~mse_btn_data) & 2) << 6;
		
		return 2;
	}
	else if (reg == 3)
	{
		xmit[0] = 0x60;
		xmit[0] += mse_addr;
		xmit[1] = mse_handler;
		return 2;
	}
	else
	{
		return 0;
	}
}

void mse_talk_drain(uint8_t reg)
{
	if (reg == 0)
	{
		mse_btn_reported = mse_btn_data;
		mse_x -= mse_x_last;
		mse_y -= mse_y_last;
	}
}

#endif /* USE_MOUSE */


// --- ARBITRARY DEVICE ---
#ifdef USE_ARBITRARY

uint8_t arb_addr;
uint8_t arb_handler;
uint8_t arb_init_addr = 7;
uint8_t arb_init_handler = 0xFC;
uint8_t arb_buf0[ARB_BUF0_SIZE];
uint8_t arb_buf0_len;
uint8_t arb_buf0_set;
uint8_t arb_buf2_low;
uint8_t arb_buf2_high;
uint8_t arb_buf2_set;

void reset_arb_data()
{
	arb_buf0_len = 0;
	arb_buf0_set = 0;
	arb_buf2_low = 0;
	arb_buf2_high = 0;
	arb_buf2_set = 0;
}

uint8_t arb_talk(uint8_t *xmit, uint8_t reg)
{
	if (reg == 0 && arb_buf0_set)
	{
		uint8_t i;
		uint8_t xmit_len = arb_buf0_len;
		if (xmit_len > 8) xmit_len = 8;
		for (i = 0; i < xmit_len; i++)
		{
			xmit[i] = arb_buf0[i];
		}
		return xmit_len;
	}
	else if (reg == 2)
	{
		xmit[0] = arb_buf2_high;
		xmit[1] = arb_buf2_low;
		return 2;
	}
	else if (reg == 3)
	{
		xmit[0] = 0x60;
		xmit[0] += arb_addr;
		xmit[1] = arb_handler;
		return 2;
	}
	else
	{
		return 0;
	}
}

void arb_talk_drain(uint8_t reg)
{
	if (reg == 0)
	{
		// remove the data from arbitrary buffer 0
		arb_buf0_len = 0;
		arb_buf0_set = 0;
	}
}

void arb_listen(uint8_t reg, uint16_t data)
{
	if (reg == 2)
	{
		arb_buf2_high = (data & 0xFF00) >> 8;
		arb_buf2_low = data & 0xFF;
		arb_buf2_set = 1;
	}
}

#endif /* USE_ARBITRARY */
