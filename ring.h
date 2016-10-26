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

/*
 * Defines a simple 8 byte ring buffer for the keyboard.
 * 
 * This is not a generic ring buffer: it has some idiosyncracies due to
 * ADB keyboard handling, like ensuring that a keycode of 0x7F and 0xFF
 * are correctly followed by a duplicate for the reset button, making
 * sure that dual-entered keys will come out during the same
 * transaction, and other such things.
 */

#pragma once

#include <avr/io.h>

#ifdef USE_KEYBOARD

#define RING_BUFFER_SIZE 16 // must be power of 2
#define RING_BUFFER_HALF_SIZE (RING_BUFFER_SIZE >> 1)
#define RING_BUFFER_BITS (RING_BUFFER_SIZE - 1)

struct buffer
{
	uint8_t tail;
	uint8_t size;
	uint8_t data[RING_BUFFER_SIZE];
};

void ring_buffer_add(struct buffer *, uint8_t);
void ring_buffer_add_dual(struct buffer *, uint8_t, uint8_t);
void ring_buffer_clear(struct buffer *);
void ring_buffer_drain(struct buffer *, uint8_t);
uint8_t ring_buffer_empty(struct buffer *);
uint16_t ring_buffer_peek(struct buffer *);
uint8_t ring_buffer_size(struct buffer *);

#endif /* USE_KEYBOARD */
