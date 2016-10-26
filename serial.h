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

#include "data.h"

/*
 * Handles the poll-based reading of the serial register.  This must be
 * called about every 50us to ensure no data in the system is lost.
 * 
 * As of the last time this was updated (2016-10-21), execution time
 * was ~10us in the worst case, using avr-gcc 4.9.2 in -Os.  This
 * should be safe on most systems.  To re-profile, take a look at the
 * test.c code.
 * 
 * Return data should be inserted into the serial return buffer for the
 * next transaction.
 */
uint8_t handle_serial_data(uint8_t);
