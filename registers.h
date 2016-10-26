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

#pragma once

#include <avr/io.h>

/*
 * This file has all the register information that can be manipulated
 * by the data devices and the ADB bus.
 * 
 * Note: the system will automatically handle communication on the bus.
 * It will prevent calls to _drain() if the data couldn't be sent,
 * prevent calls to _listen() on register 3 completely, and performs
 * similar "odd" behavior.  If you are going to make changes, inspect
 * the code carefully.
 */

// bit masks for extracting individual devices from status vars
#define ADB_KBD_FLAG_MASK 1
#define ADB_MSE_FLAG_MASK 2
#define ADB_ARB_FLAG_MASK 4

/*
 * Resets the contents of all registers to the initial defaults.  This
 * is invoked during startup from the configuration routine and whenever
 * the ADB bus resets from adb_reset().
 */
void reset_registers();
/*
 * Provides back a SRQ indicator, for showing which devices require
 * servicing at the time the call is made.  The reply byte matches
 * the flag masks defined earlier.  This must return promptly.
 */
uint8_t devices_needing_srq();
/*
 * Called whenever the ADB bus asks for a device to flush.  Given
 * the mask of the target being flushed.
 */
void device_flush(uint8_t);


// and then the per-device registers


// --- KEYBOARD ---
#ifdef USE_KEYBOARD

#include "ring.h"
#define KBD_REG2_SCRL_BIT 6
#define KBD_REG2_NUML_BIT 7
#define KBD_REG2_CMD_BIT 0
#define KBD_REG2_OPT_BIT 1
#define KBD_REG2_SHFT_BIT 2
#define KBD_REG2_CNTL_BIT 3
#define KBD_REG2_RST_BIT 4
#define KBD_REG2_CPSL_BIT 5
#define KBD_REG2_DEL_BIT 6

// basic address/handlers
extern uint8_t kbd_addr;
extern uint8_t kbd_handler;
// and the keyboard register stuff
extern struct buffer kbd_buf;
extern uint8_t kbd_reg2_low;
extern uint8_t kbd_reg2_high;
void reset_kbd_data();
uint8_t kbd_talk(uint8_t *, uint8_t);
void kbd_talk_drain(uint8_t);
void kbd_listen(uint8_t, uint16_t);

#endif /* USE_KEYBOARD */


// --- MOUSE ---
#ifdef USE_MOUSE

// basic address/handlers
extern uint8_t mse_addr;
extern uint8_t mse_handler;
extern uint8_t mse_init_handler;
// and the mouse register stuff
extern uint8_t mse_btn_data;
extern uint8_t mse_btn_reported;
extern int16_t mse_x;
extern int16_t mse_y;
void reset_mse_data();
uint8_t mse_talk(uint8_t *, uint8_t);
void mse_talk_drain(uint8_t);
void mse_listen(uint8_t, uint16_t);

#endif /* USE_MOUSE */


// --- ARBITRARY DEVICE ---
#ifdef USE_ARBITRARY

// basic address/handlers
#define ARB_BUF0_SIZE 8
extern uint8_t arb_addr;
extern uint8_t arb_handler;
extern uint8_t arb_init_addr;
extern uint8_t arb_init_handler;
// and the arbitrary registers
extern uint8_t arb_buf0[ARB_BUF0_SIZE];
extern uint8_t arb_buf0_len;
extern uint8_t arb_buf0_set;
extern uint8_t arb_buf2_low;
extern uint8_t arb_buf2_high;
extern uint8_t arb_buf2_set;
void reset_arb_data();
uint8_t arb_talk(uint8_t *, uint8_t);
void arb_talk_drain(uint8_t);
void arb_listen(uint8_t, uint16_t);

#endif /* USE_ARBITRARY */
