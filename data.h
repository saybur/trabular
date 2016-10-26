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
#include "registers.h"

/*
 * Called during startup to initialize the data system to whatever
 * the device needs.  Note this occurs *after* the adb_reset()
 * takes care of initializing registers.
 */
void init_data();

/*
 * Called repeatedly by the ADB processing code in poll mode to handle
 * updating the registers with information from the system.  In the
 * default system, this is defined in data.c as a serial connection,
 * but if you are rolling your own system then this could do pretty
 * much anything, subject to some timing issues:
 * 
 * 1) This is called very frequently sometimes and not frequently at 
 * all other times: the maximum delay between invocations is typically
 * between 50-150us, depending on the activity on the ADB bus, but this
 * can be called very rapidly (<1us between calls).
 * 2) This cannot take very much time to execute.  Most of the ADB
 * handling code assumes a maximum execution time of about 20us.
 * If you're doing complicated stuff... uh, be careful.  The standard
 * system has test code and has been profiled to never take more than
 * about 10us or so. 
 */
void handle_data();
