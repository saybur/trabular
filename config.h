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

// define the ADB data port used by the controller
#ifdef ADB_PORTA
	#define ADB_PORT PORTA
	#define ADB_DDR DDRA
	#define ADB_PIN PINA
#elif ADB_PORTB
	#define ADB_PORT PORTB
	#define ADB_DDR DDRB
	#define ADB_PIN PINB
#elif ADB_PORTC
	#define ADB_PORT PORTC
	#define ADB_DDR DDRC
	#define ADB_PIN PINC
#elif ADB_PORTD
	#define ADB_PORT PORTD
	#define ADB_DDR DDRD
	#define ADB_PIN PIND
#else
	#error "You must define an ADB_PORT"
#endif

// and define the bit that will be used to read ADB data
#ifdef ADB_DATA_BIT
	#error "Please define ADB_DATA_PIN, not ADB_DATA_BIT"
#endif
#ifdef ADB_DATA_MASK
	#error "Please define ADB_DATA_PIN, not ADB_DATA_MASK"
#endif
#ifndef ADB_DATA_PIN
	#error "You must define an ADB_DATA_PIN"
#endif
#if ADB_DATA_PIN > 8
	#error "You must define ADB_DATA_PIN to a 8 bit port"
#endif

#define ADB_DATA_BIT ADB_DATA_PIN
#define ADB_DATA_MASK _BV(ADB_DATA_PIN)
