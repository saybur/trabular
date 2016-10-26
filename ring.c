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

#include "ring.h"

#if USE_KEYBOARD

static inline void rb_add_raw(struct buffer *, uint8_t) __attribute__((always_inline));

/*
 * Adds a value into the buffer, if there is room to do so.  If the
 * buffer is full the value is dropped.
 */ 
void ring_buffer_add(struct buffer *buf, uint8_t v)
{
	if (buf->size < RING_BUFFER_SIZE)
	{
		rb_add_raw(buf, v);
	}
}

/*
 * Adds two values into the buffer, pre-padding the buffer make sure it
 * is even prior to adding the values.  This is a special construct for
 * handling the ADB communication of certain (0x7F, 0xFF) values.
 */ 
void ring_buffer_add_dual(struct buffer *buf, uint8_t v, uint8_t u)
{
	if (buf->size & 1)
	{
		// odd: only add if we have 3+ empty spaces to include a
		// 0xFF buffer to make sure that the values come out at the
		// same time
		if (buf->size + 2 < RING_BUFFER_SIZE)
		{
			rb_add_raw(buf, 0xFF);
			rb_add_raw(buf, v);
			rb_add_raw(buf, u);
		}
	}
	else
	{
		// even: only add if we have 2+ empty spaces
		if (buf->size + 1 < RING_BUFFER_SIZE)
		{
			rb_add_raw(buf, v);
			rb_add_raw(buf, u);
		}
	}
}

/*
 * Gets the first 2 bytes in ADB keyboard order out of the ring buffer.
 * The buffer is not affected by this operation.
 */ 
uint16_t ring_buffer_peek(struct buffer *buf)
{
	if (buf->size == 0)
	{
		return 0xFFFF;
	}
	else if (buf->size == 1)
	{
		uint16_t v = buf->data[buf->tail];
		v <<= 8;
		v += 0xFF;
		return v;
	}
	else
	{
		uint16_t v = buf->data[buf->tail];
		v <<= 8;
		v += buf->data[(buf->tail + 1) & RING_BUFFER_BITS];
		return v;
	}
}

/*
 * Removes the first X bytes from the ring buffer, where X is zero, 
 * one, or two.  Higher values of X are treated as 2.
 */
void ring_buffer_drain(struct buffer *buf, uint8_t cnt)
{
	if (buf->size >= 1 && cnt == 1)
	{
		buf->tail = (buf->tail + 1) & RING_BUFFER_BITS;
		buf->size = buf->size - 1;
	}
	else if (buf->size > 1 && cnt > 1)
	{
		buf->tail = (buf->tail + 2) & RING_BUFFER_BITS;
		buf->size = buf->size - 2;
	}
}

uint8_t ring_buffer_size(struct buffer *buf)
{
	return buf->size;
}

void ring_buffer_clear(struct buffer *buf)
{
	buf->size = 0;
}

uint8_t ring_buffer_empty(struct buffer *buf)
{
	return buf->size == 0;
}

/*
 * Adds a value to the buffer, not performing any checks before doing
 * so.
 */
static inline void rb_add_raw(struct buffer *buf, uint8_t v)
{
	buf->data[(buf->tail + buf->size++) & RING_BUFFER_BITS] = v;
}

#endif /* USE_KEYBOARD */
