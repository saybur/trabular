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

#include <util/delay.h>
#include "adb.h"

// annoying delay declaration statements, TODO figure out a better
// way to handle these
#ifdef HALF_SPEED
	#define ADB_SIGDEL_ATTN_MIN 85
	#define ADB_SIGDEL_ATTN_MAX 116
	#define ADB_SIGDEL_SYNC_MIN 22
	#define ADB_SIGDEL_SYNC_MAX 77
	#define ADB_SIGDEL_SRQ_ASSERT 38
	#define ADB_SIGDEL_SRQ_MAX 41
	#define ADB_SIGDEL_TALK 24
	#define ADB_SIGDEL_LISTEN1 30
	#define ADB_SIGDEL_LISTEN2 5
	#define ADB_SIGDEL_LISTEN_SYNC 71
	#define ADB_SIGDEL_PULSE_SHORT 35
	#define ADB_SIGDEL_PULSE_LONG 75
	#define ADB_SIGDEL_BIT_SHORT 22
	#define ADB_SIGDEL_BIT_LONG 93
	#define ADB_SIGDEL_BIT_SPLIT 50
#else
	#define ADB_SIGDEL_ATTN_MIN 170
	#define ADB_SIGDEL_ATTN_MAX 233
	#define ADB_SIGDEL_SYNC_MIN 44
	#define ADB_SIGDEL_SYNC_MAX 154
	#define ADB_SIGDEL_SRQ_ASSERT 75
	#define ADB_SIGDEL_SRQ_MAX 83
	#define ADB_SIGDEL_TALK 47
	#define ADB_SIGDEL_LISTEN1 60
	#define ADB_SIGDEL_LISTEN2 10
	#define ADB_SIGDEL_LISTEN_SYNC 143
	#define ADB_SIGDEL_PULSE_SHORT 70
	#define ADB_SIGDEL_PULSE_LONG 130
	#define ADB_SIGDEL_BIT_SHORT 44
	#define ADB_SIGDEL_BIT_LONG 186
	#define ADB_SIGDEL_BIT_SPLIT 100
#endif

// a few pin check/change things we do frequently
#define ADB_IS_ASSERTED (!(ADB_PIN & ADB_DATA_MASK))
#define ADB_NOT_ASSERTED (ADB_PIN & ADB_DATA_MASK)
#define ADB_ASSERT() (ADB_DDR |= ADB_DATA_MASK)
#define ADB_RELEASE() (ADB_DDR &= ~ADB_DATA_MASK)

// branching instruction functions called from the adb handler
static uint8_t adb_srq(uint8_t);
static void adb_talk(uint8_t, uint8_t);
static void adb_listen(uint8_t, uint8_t);

// helper methods, see definition for details
static void adb_pulse_bit(uint8_t, uint8_t);
static uint8_t adb_read_byte();
static void adb_write_byte(uint8_t);
static uint8_t adb_wait_for_assertion(uint8_t);
static uint8_t adb_wait_for_line_free(uint8_t);
static uint8_t adb_resync(uint8_t);
// and aliases for simplicity
#define adb_pulse_bit_one() (adb_pulse_bit(ADB_SIGDEL_PULSE_SHORT, ADB_SIGDEL_PULSE_LONG))
#define adb_pulse_bit_zero() (adb_pulse_bit(ADB_SIGDEL_PULSE_LONG, ADB_SIGDEL_PULSE_SHORT))

// the "standard" way to turn the timers on and off.  note that both
// the start and stop clears the timer.  these use Timer0 and assume
// that nothing will change timer settings from the startup defaults.
// at 16MHz (hardcoded default) 8 is 0.5us/tick, 64 is 4us/tick
static inline void start_timer_8() __attribute__((always_inline));
static inline void start_timer_64() __attribute__((always_inline));
static inline uint8_t stop_timer() __attribute__((always_inline));

// some helpers for detecting problems
static uint8_t adb_address_collision = 0;
static uint8_t adb_protocol_error = 0;

// buffer for storing transmitted or received information 
static uint8_t xmit_buffer[8];
static uint8_t xmit_len;

/*
 * Overall note: the serial handler must be called about every 50-70us
 * or so to prevent data from being lost as new writes come in.
 * 
 * Also note that the timings are fairly sloppy, to account for the
 * AVR internal oscillator variance (+/-10%).
 */

/// --- LOGICAL FUNCTIONS ---

void handle_adb()
{
	uint8_t timing;
	uint8_t command;
	uint8_t target;
	
	// new call, clear exiting information
	adb_protocol_error = 0;
	xmit_len = 0;

	// wait until the line does something
	do
	{
		adb_wait_for_assertion(255);
	}
	while (ADB_NOT_ASSERTED);
	
	// we are now asserted, do a rough check to see if this is a
	// attention signal, a reset signal, or something else
	timing = adb_wait_for_line_free(ADB_SIGDEL_ATTN_MAX);
	
	// how long were we asserted?
	if(timing < ADB_SIGDEL_ATTN_MIN)
	{
		// not enough time to be asserted, not in the correct position
		// for start of comms.  this will happen frequently and is not
		// necessarily a problem
		return;
	}
	else if(timing >= ADB_SIGDEL_ATTN_MAX)
	{
		// reset and wait for new commands
		adb_reset();
		return;
	}
	
	// at the sync signal, so sync up with the bus to get data
	timing = adb_resync(ADB_SIGDEL_SYNC_MAX);
	if (timing < ADB_SIGDEL_SYNC_MIN || timing >= ADB_SIGDEL_SYNC_MAX)
	{
		// not in correct phase of bus
		#ifdef DEBUG_MODE
			UDR0 = 0xE0;
		#endif
		return;
	}
	
	// at the command byte, read it
	command = adb_read_byte();
	if (adb_protocol_error)
	{
		#ifdef DEBUG_MODE
			UDR0 = 0xE4;
		#endif
		return;
	}

	// at the stop byte now, we need to determine if we're 
	// being addressed and/or if we need to issue a SRQ
	target = adb_srq(command);
	if (adb_protocol_error)
	{
		#ifdef DEBUG_MODE
			UDR0 = 0xE8;
		#endif
		return;
	}
	if (! target)
	{
		// not being talked to, so ignore rest of transaction
		return;
	}
	
	/*
	 * At the (rough) start of Tlt now.  Remainder of behavior will
	 * depend on what the host wants.
	 * 
	 * This is weird, but the docs show that 0001 is the *whole*
	 * command used for flushing... but then say that it specifies a
	 * register.  we will only accept the whole command, and flush the
	 * entire referenced device.
	 */
	uint8_t lcmd = command & 15;
	if (lcmd == 1)
	{
		device_flush(target);
	}
	// not flush, so bits 3 and 2 are the only important ones
	// all other commands reserved and will be ignored
	lcmd >>= 2;
	if (lcmd == 3)
	{
		adb_talk(target, command & 3);
	}
	else if (lcmd == 2)
	{
		adb_listen(target, command & 3);
	}
}

/*
 * Called during the start of the stop bit after the command byte, and
 * handles whether or not a SRQ should be issued on the line.
 * 
 * Provides a packed byte back where the relevant bits indicate the
 * target being addressed by the command.  Will flag a protocol error
 * if something doesn't make sense during the SRQ issue.
 * 
 * This returns at the start of the Tlt condition, but may be up to
 * 20ms off from the actual start (worse case).
 */
static uint8_t adb_srq(uint8_t command)
{
	start_timer_64();
	handle_data();
	
	// are we being addressed?
	// also, guard here against internal address collisions: once we
	// have a target, others should be prevented from speaking
	uint8_t address = command >> 4;
	uint8_t target = 0;
	#ifdef USE_KEYBOARD
		if (address == kbd_addr)
		{
			target |= ADB_KBD_FLAG_MASK;
		}
	#endif
	#ifdef USE_MOUSE
		if (target == 0 && address == mse_addr)
		{
			target |= ADB_MSE_FLAG_MASK;
		}
	#endif
	#ifdef USE_ARBITRARY
		if (target == 0 && address == arb_addr)
		{
			target |= ADB_ARB_FLAG_MASK;
		}
	#endif
	
	// figure out what devices are asking for servicing
	uint8_t srq = devices_needing_srq();
	// we don't need to SRQ if we're being targeted
	srq &= ~target;

	// do we need to hold the line for SRQ?
	if (srq)
	{
		// yes.  first ensure we're still low
		if (ADB_NOT_ASSERTED)
		{
			stop_timer();
			adb_protocol_error = 1;
			return 0;
		}
		
		// then push low ourselves and hold for 300us total since
		// start of the stop bit
		ADB_ASSERT();
		do
		{
			handle_data();
		}
		while (TCNT0 < ADB_SIGDEL_SRQ_ASSERT);
		ADB_RELEASE();
	}
	
	// make sure the line has gone high before we return, so we're
	// in the same state regardless of SRQ issue from us
	while (ADB_IS_ASSERTED && TCNT0 < ADB_SIGDEL_SRQ_MAX)
	{
		handle_data();
	}
	uint8_t delay = stop_timer();
	if (delay >= ADB_SIGDEL_SRQ_MAX)
	{
		adb_protocol_error = 2;
		return 0;
	}
	
	stop_timer();
	return target;
}

/*
 * Called at the approximate (~20us) start of Tlt for a talk command.
 * Given a packed target byte per defines and the register to talk.
 */
static void adb_talk(uint8_t target, uint8_t reg)
{
	start_timer_64();
	uint8_t i;
	
	#ifdef DEBUG_MODE
		if (reg == 3)
		{
			UDR0 = 0xD0 + target;
		}
	#endif
	
	// construct our response
	#ifdef USE_KEYBOARD
		if (target & ADB_KBD_FLAG_MASK)
		{
			xmit_len = kbd_talk(xmit_buffer, reg);
		}
	#endif
	#ifdef USE_MOUSE
		if (target & ADB_MSE_FLAG_MASK)
		{
			xmit_len = mse_talk(xmit_buffer, reg);
		}
	#endif
	#ifdef USE_ARBITRARY
		if (target & ADB_ARB_FLAG_MASK)
		{
			xmit_len = arb_talk(xmit_buffer, reg);
		}
	#endif

	// if no data will be sent, just end
	if (xmit_len < 1)
	{
		stop_timer();
		return;
	}
	// otherwise our response handling is generic.
	// hold until timer hits ~190us with a random variance
	// ------------ TODO ADD A RANDOM DELAY -----------
	uint8_t wait_ticks = ADB_SIGDEL_TALK;
	while (ADB_NOT_ASSERTED && TCNT0 < wait_ticks)
	{
		handle_data();
	}
	uint8_t delay = stop_timer();
	if (delay < wait_ticks)
	{
		// someone started transmitting before we could
		#ifdef DEBUG_MODE
			UDR0 = 0xEC;
		#endif
		if (reg == 3)
		{
			// ah, someone lives at our address
			// set the collision flag for appropriate handling per the
			// ADB spec
			adb_address_collision |= target;
		}
		return;
	}
	
	// then transmit the data
	adb_pulse_bit_one();
	if (adb_protocol_error)
	{
		#ifdef DEBUG_MODE
			UDR0 = (adb_protocol_error == 0xFF ? 0xED : 0xEE);
		#endif
		if (adb_protocol_error == 0xFF && reg == 3)
		{
			// as above, another collision issue
			adb_address_collision |= target;
		}
		return;
	}
	for (i = 0; i < xmit_len; i++)
	{
		adb_write_byte(xmit_buffer[i]);
		if (adb_protocol_error)
		{
			#ifdef DEBUG_MODE
				UDR0 = 0xEF;
			#endif
			if (adb_protocol_error == 0xFF && reg == 3)
			{
				adb_address_collision |= target;
			}
			return;
		}
	}
	adb_pulse_bit_zero();
	
	// transmission successful, drain data that we consumed, if needed
	#ifdef USE_KEYBOARD
		if (target & ADB_KBD_FLAG_MASK)
		{
			kbd_talk_drain(reg);
		}
	#endif
	#ifdef USE_MOUSE
		if (target & ADB_MSE_FLAG_MASK)
		{
			mse_talk_drain(reg);
		}
	#endif
	#ifdef USE_ARBITRARY
		if (target & ADB_ARB_FLAG_MASK)
		{
			arb_talk_drain(reg);
		}
	#endif
}

/*
 * Called at the approximate (~20us) start of Tlt for a listen command.
 * Given a packed target byte per defines and the register to listen.
 */
static void adb_listen(uint8_t target, uint8_t reg)
{
	// wait out the Tlt time.  we get a "1" start data bit, so we can
	// use rough timing to skip into that:)
	uint8_t delay = adb_wait_for_assertion(ADB_SIGDEL_LISTEN1);
	if (delay >= ADB_SIGDEL_LISTEN1)
	{
		// timeout waiting for data
		#ifdef DEBUG_MODE
			UDR0 = 0xDA;
		#endif
		return;
	}
	
	// rest of start bit
	delay = adb_wait_for_line_free(ADB_SIGDEL_LISTEN2);
	if (delay >= ADB_SIGDEL_LISTEN2)
	{
		#ifdef DEBUG_MODE
			UDR0 = 0xDB;
		#endif
		return;
	}
	delay = adb_resync(ADB_SIGDEL_LISTEN_SYNC);
	if (delay >= ADB_SIGDEL_LISTEN_SYNC)
	{
		#ifdef DEBUG_MODE
			UDR0 = 0xDC;
		#endif
		return;
	}
	
	// listen to data
	uint8_t v = adb_read_byte();
	while ( (! adb_protocol_error) && xmit_len < 8)
	{
		xmit_buffer[xmit_len++] = v;
		v = adb_read_byte();
	}
	if (xmit_len < 2)
	{
		// not enough data
		#ifdef DEBUG_MODE
			UDR0 = 0xDD;
		#endif
		return;
	}
	
	// if register 3, stuff gets weird, so handle that case within
	// the actual ADB handler
	if (reg == 3)
	{
		#ifdef DEBUG_MODE
			UDR0 = 0xDE;
		#endif
		
		uint8_t naddr = xmit_buffer[0] & 15;
		uint8_t nhandler = xmit_buffer[1];
		
		// we don't support self-testing
		if (nhandler == 0xFF) return;
		// if we collided last time, we must ignore this update
		// we'll try again the next time the host talks to our addr
		if (nhandler == 0xFE && adb_address_collision & target)
		{
			adb_address_collision &= ~target;
			return;
		}
		
		// if new address is nonsense, don't change anything beyond
		// this point
		if (naddr < 0x01 || naddr > 0x0F) return;
		
		// regardless of remaining codes, update
		#ifdef USE_KEYBOARD
			if (target & ADB_KBD_FLAG_MASK)
			{
				kbd_addr = naddr;
				// support changing to handlers 2 or 3 only
				if (nhandler == 2 || nhandler == 3)
				{
					kbd_handler = nhandler;
				}
			}
		#endif
		#ifdef USE_MOUSE
			else if (target & ADB_MSE_FLAG_MASK)
			{
				mse_addr = naddr;
			}
		#endif
		#ifdef USE_ARBITRARY
			else if (target & ADB_ARB_FLAG_MASK)
			{
				arb_addr = naddr;
			}
		#endif
	}
	else
	{
		#ifdef DEBUG_MODE
			UDR0 = 0xDF;
		#endif
		
		uint16_t data = (xmit_buffer[0] << 8) + xmit_buffer[1];
		#ifdef USE_KEYBOARD
			if (target & ADB_KBD_FLAG_MASK)
			{
				kbd_listen(reg, data);
			}
		#endif
		#ifdef USE_MOUSE
			// does not listen to anything at this point
			//if (target & ADB_MSE_FLAG_MASK)
			//{
			//	mse_listen(reg, data);
			//}
		#endif
		#ifdef USE_ARBITRARY
			if (target & ADB_ARB_FLAG_MASK)
			{
				arb_listen(reg, data);
			}
		#endif
	}
}

/*
 * Called immediately after the ADB bus reset condition, and upon MCU
 * startup.  This will set all address and handler values back to their
 * defaults, and will empty all buffers.
 */
void adb_reset()
{
	// note reception of reset signal
	#ifdef DEBUG_MODE
		UDR0 = 0xFF;
	#endif
	
	// ---reset addresses---
	#ifdef USE_KEYBOARD
		kbd_addr = 2;
		kbd_handler = 2;
	#endif
	
	#ifdef USE_MOUSE
		mse_addr = 3;
		mse_handler = mse_init_handler;
	#endif
	
	#ifdef USE_ARBITRARY
		arb_addr = arb_init_addr;
		arb_handler = arb_init_handler;
	#endif
	
	// ---reset registers---
	reset_registers();
}


/// --- UTILITY METHODS ---

/*
 * Reads a byte from the line.  This aborts if it detects a timing flaw
 * and sets the protocol error flag appropriately.  If there is no
 * error, this returns after the line goes low again after the 8th bit
 * is received.
 * 
 * Return value is the read byte.  Ignore the return value if errored.
 */
uint8_t adb_read_byte()
{
	uint8_t delay = 0;
	uint8_t value = 0;
	uint8_t i = 0;

	for (i = 0; i < 8; i++)
	{
		start_timer_8();
		handle_data();
		
		// wait while the line is low, with timeout
		while (ADB_IS_ASSERTED && TCNT0 < ADB_SIGDEL_BIT_LONG);
		
		// get the time value we waited, halting the timer, with
		// a check that the transmission was within specs
		delay = stop_timer();
		if (delay < ADB_SIGDEL_BIT_SHORT
			|| delay >= ADB_SIGDEL_BIT_LONG)
		{
			// bit was outside expected time frame, error on line
			adb_protocol_error = 1;
			return 0;
		}
		
		// restart the timer for the line high condition
		start_timer_8();
		
		// shift existing values, and set the current bit
		// (ADB talks in MSB->LSB order)
		value <<= 1;
		if (delay < ADB_SIGDEL_BIT_SPLIT) value |= 1;
		
		// wait for the line to assert again for the
		// next bit in line (with generic timeout)
		while (ADB_NOT_ASSERTED && TCNT0 < ADB_SIGDEL_BIT_LONG);
		
		// verify that the delay wasn't wild
		delay = stop_timer();
		if (delay >= ADB_SIGDEL_BIT_LONG)
		{
			adb_protocol_error = 2;
			return 0;
		}
	}

	return value;
}

/*
 * Writes a byte to the line.  Aborts if it detects a timing flaw or a
 * collision and sets the error flag appropriately.  Normally returns
 * after the 8th bit is written to the line.
 */
void adb_write_byte(uint8_t v)
{
	uint8_t i;
	
	for (i = 0; i < 8; i++)
	{
		// write the MSB
		if (v & _BV(7))
		{
			adb_pulse_bit_one();
		}
		else
		{
			adb_pulse_bit_zero();
		}
		
		// if an error occurs, stop writing
		if (adb_protocol_error) return;
		
		// then move the next bit up to MSB
		v <<= 1;
	}
}

/*
 * Talks on the line, and includes checks for serial and collisions. The
 * low time is the time the line is asserted, and the high time is the
 * time that the line is left high after assertion (including collision
 * checks).
 * 
 * Both the time values are for the timer in /8 prescale.
 */
static void adb_pulse_bit(uint8_t low, uint8_t high)
{
	uint8_t delay;
	
	// check that the line isn't being used by someone else
	if (ADB_IS_ASSERTED)
	{
		adb_protocol_error = 1;
		return;
	}
	
	// assert for the required time period
	ADB_ASSERT();
	start_timer_8();
	handle_data(); // usual check, done in <20us
	while (TCNT0 < low);
	ADB_RELEASE();
	stop_timer();
	
	// then leave line high for the required time period
	start_timer_8();
	handle_data();  // usual check, done in <20us
	
	// watch that line remains unasserted during up time
	while (ADB_NOT_ASSERTED && TCNT0 < high);
	delay = stop_timer();
	if (delay < high)
	{
		// collision
		adb_protocol_error = 0xFF;
	}
}

/*
 * Waits for line assertion, continuously monitoring the serial
 * data flow.  When the line goes low, this tries to return as
 * quickly as possible.
 * 
 * The given value is a Timer0 timeout at prescale /64, after which
 * this function will return.
 * 
 * This method is not very granular.  SPI updates may alter the result
 * up to 20us, which at the used prescale of /64 may be late by a value
 * of 5 or 6.  This is hard to avoid, so be careful in judging 
 * timing to be out of spec.
 */
uint8_t adb_wait_for_assertion(uint8_t timeout)
{
	start_timer_64();
	while (ADB_NOT_ASSERTED && TCNT0 < timeout)
	{
		handle_data();
	}
	return stop_timer();
}

/*
 * Same as the above function, but for the "line free' condition.
 */
uint8_t adb_wait_for_line_free(uint8_t timeout)
{
	start_timer_64();
	while (ADB_IS_ASSERTED && TCNT0 < timeout)
	{
		handle_data();
	}
	return stop_timer();
}

/*
 * Called to spin the CPU until the ADB line is asserted or the given
 * timeout is reached.  This uses a /8 prescale and only checks serial
 * during the *initial* call to the function.  Do not send high 
 * timeouts or SPI data may be lost.  Returns the timer value when
 * either timeout or assertion occurs.
 */
uint8_t adb_resync(uint8_t timeout)
{
	start_timer_8();
	handle_data();
	while (ADB_NOT_ASSERTED && TCNT0 < timeout);
	return stop_timer();
}

// --- timer helper methods ---

static inline void start_timer_8()
{
	TCNT0 = 0x00;
	TCCR0B = 0x02;
}

static inline void start_timer_64()
{
	TCNT0 = 0x00;
	TCCR0B = 0x03;
}

static inline uint8_t stop_timer()
{
	uint8_t delay = TCNT0;
	TCCR0B = 0x00;
	TCNT0 = 0x00;
	return delay;
}
