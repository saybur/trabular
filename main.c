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

#include <avr/io.h>
#include "adb.h"

// reminder for F_CPU
#ifndef F_CPU
	#error "F_CPU is not defined"
#else
	#ifdef HALF_SPEED
		#if F_CPU != 80000000
			#error "F_CPU set incorrectly (HALF_SPEED)"
		#endif
	#else
		#if F_CPU != 16000000
			#error "F_CPU set incorrectly"
		#endif
	#endif
#endif

int main()
{
	// before any ADB communication, ensure that the ADB pin will go
	// low when the direction is changed
	ADB_PORT &= ~_BV(ADB_DATA_BIT);
	
	// perform an initial reset of the ADB system
	adb_reset();
	
	// setup any data system requirements
	init_data();
	
	// then pass off to the main handler
	while (1)
	{
		handle_adb();
	}
	
	// should never reach this
	return 0;
}
