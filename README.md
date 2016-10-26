# trabular

trabular is an [Apple Desktop 
Bus](https://en.wikipedia.org/wiki/Apple_Desktop_Bus) device 
implementation for AVR microcontrollers.

In the "standard" configuration, trabular is an independent transceiver
IC controlled over either
[SPI](https://en.wikipedia.org/wiki/Serial_Peripheral_Interface_Bus) or
[UART](https://en.wikipedia.org/wiki/Universal_asynchronous_receiver/transmitter).
This allows designers to create ADB-compatible devices without having
to handle the timing or signaling requirements of the bus.  Creative
types may also see fit to remove the SPI/UART interface and run their
implementation on the IC itself, with limitations.

The software includes support for simultaneously emulating an Extended 
Keyboard II, a standard mouse, and an arbitrary device for sending 
information back and forth from the bus master to the connected 
computer. Each of these follows the bus requirements for address 
reallocation, so the transceiver can be used alongside standard 
peripherals.

trabular uses a well-defined set of commands, and is inexpensive 
to implement: for simple configurations, all required parts are under 
$2.00 in single unit quantities.  Board space requirements are also
minimal in most cases.

The firmware is licensed under
[GPLv3](https://www.gnu.org/licenses/gpl-3.0.en.html).  See the COPYING
file.

## Current Development State

Support for keyboards and mice is beta-level: they work fine on my 
system, but I'm sure there are plenty of bugs still lurking in the code.  
The arbitrary device is untested.  I'm releasing this as-is to help get
additional eyes on the code, and to gauge the level of interest in
the project.

The only hardware tested so far is an Arduino on a 
[IIsi](https://en.wikipedia.org/wiki/Macintosh_IIsi), but the firmware 
should work across a range of AVR devices using several different clock 
sources.

Some plans for the immediate future:

* Finishing testing on different AVR hardware.
* Ensuring that the arbitrary device works.

Longer term features may include:

* Improving performance to allow 8MHz parts to work.
* Adding support for the extended mouse protocol.

## Hardware

The "standard" configuration planned is an ATtiny45 (85 would also 
work, but *not* the V suffix parts), a 100nF capacitor, and 4.7kOhm 
pull-up resistor on the AVR reset line. The device will almost 
certainly run at 5V, both to handle the 16MHz speed and to interface 
with the bus.  To work with 3.3V masters, a simple voltage divider is 
sufficient given the low transfer clock speeds: a 2.2kOhm series 
resistor and a 3.3kOhm resistor to ground on the SPI MISO line can be 
used to drop the output signal to 3V. This line can also be left 
disconnected if no TALK commands will be used.

The bus timings are loose, and the firmware was written with that in
mind to support internal oscillators, which are generally only accurate
to within +/-10% anyway.  The 8MHz standard oscillators will be too slow
to work with the current implementation, but the 16MHz PLL of the
ATtiny25/45/85 should be perfect.

### Device Setup for the ATtiny45

PB0-PB2 are used for SPI communication. Tie PB0 to the SPI master MOSI 
line, PB1 to the SPI master MISO line (using a voltage divider if 
needed), and PB2 to the SPI master clock line. Also note the usual way 
to connect an AVR to a programmer via the [ICSP 
header](https://www.arduino.cc/en/Tutorial/ArduinoISP) if needed.

PB3 is unused.

PB4 is the ADB data connection.  This is high-impedance when not 
transmitting, and low when asserting (transmitting). It can be directly 
connected to the ADB data line (pin 1).  Some schematics recommend a 
pull-up resistor connected between the data line and the 5V line, but 
that is up to the implementer (TODO check this).

PB5 is the reset line and should be attached high via the 4.7kOhm 
resistor. This can also be connected to the programming header if 
needed.

In most cases the device should be fused as follows:

* Low : 0xE1
* High : 0xD5
* Extended : 0xFF

## Serial Communication

Becuase of ADB bus timing needs, the serial data may not be checked
for up to ~100us. To prevent data from being lost, it is important that
the master only send information about every 200us or so. This 
corresponds to a maximum SPI speed of about 40kHz or a serial baudrate
of 38400bps. You might be able to stretch this a little, but it is not
reccomended.

### Serial Bytes to Transceiver

| Bits 7-4 | Bits 3-0 |
| -------- | -------- |
| 0000 | SPECIAL COMMANDS, SEE LOWER TABLE |
| 0001 | RESERVED |
| 0010 | ARBITRARY, PUSH REGISTER 0, LOWER NIBBLE |
| 0011 | ARBITRARY, PUSH REGISTER 0, UPPER NIBBLE |
| 0100 | KEYBOARD, PUSH KEY, LOWER NIBBLE |
| 0101 | KEYBOARD, PUSH KEY, UPPER NIBBLE |
| 0110 | MOUSE, SET BUTTONS, LOWER NIBBLE |
| 0111 | MOUSE, SET BUTTONS, UPPER NIBBLE |
| 1000 | MOUSE, ACCUMULATE X MOTION, LOWER NIBBLE, POSITIVE |
| 1001 | MOUSE, ACCUMULATE X MOTION, LOWER NIBBLE, NEGATIVE |
| 1010 | MOUSE, ACCUMULATE X MOTION, UPPER NIBBLE, POSITIVE |
| 1011 | MOUSE, ACCUMULATE X MOTION, UPPER NIBBLE, NEGATIVE |
| 1100 | MOUSE, ACCUMULATE Y MOTION, LOWER NIBBLE, POSITIVE |
| 1101 | MOUSE, ACCUMULATE Y MOTION, LOWER NIBBLE, NEGATIVE |
| 1110 | MOUSE, ACCUMULATE Y MOTION, UPPER NIBBLE, POSITIVE |
| 1111 | MOUSE, ACCUMULATE Y MOTION, UPPER NIBBLE, NEGATIVE |

The keyboard and arbitrary register PUSH commands must occur in lower,
then upper order. See the later sections for details.

### Special Commands

| SPI Data | Action | Notes |
| -------- | ------ | ----- |
| 0x00 | NO ACTION | | 
| 0x01 | TALK STATUS | 1 |
| 0x02 | ARBITRARY REGISTER 0 READY | 2 |
| 0x03 | ARBITRARY CLEAR REGISTER 0 | 3 |
| 0x04 | ARBITRARY CLEAR REGISTER 2 | |
| 0x05 | KEYBOARD CLEAR REGISTER 0 | |
| 0x06 | MOUSE CLEAR BUTTONS | |
| 0x07 | MOUSE CLEAR ACCUMULATED X MOTION | |
| 0x08 | MOUSE CLEAR ACCUMULATED Y MOTION | |
| 0x09 | RESERVED | |
| 0x0A | RESERVED | |
| 0x0B | RESERVED | |
| 0x0C | TALK ARBITRARY REGISTER 2 BYTE 0 LOWER NIBBLE | |
| 0x0D | TALK ARBITRARY REGISTER 2 BYTE 0 UPPER NIBBLE | |
| 0x0E | TALK ARBITRARY REGISTER 2 BYTE 1 LOWER NIBBLE | |
| 0x0F | TALK ARBITRARY REGISTER 2 BYTE 1 UPPER NIBBLE | |

1. See the next section for details about what is returned.
2. This flags arbitrary register 0 for a service request (if needed) 
and allows the data to be sent: without sending this command the 
contents of register 0 are not reported to the computer. This command
is ignored if there are less than 2 bytes in register 0.
3. This automatically clears the above flag as well.

### Talk Packet Format

These are the packets sent from the transceiver back to the master 
in response to the various TALK commands:

| Bits 7-4 | Bits 3-0 |
| -------- | -------- |
| 0000 | NO INFORMATION |
| 0001 | RESERVED |
| 0010 | RESERVED |
| 0011 | RESERVED |
| 0100 | ARBITRARY REGISTER 2 BYTE 0 LOWER NIBBLE |
| 0101 | ARBITRARY REGISTER 2 BYTE 0 UPPER NIBBLE |
| 0110 | ARBITRARY REGISTER 2 BYTE 1 LOWER NIBBLE |
| 0111 | ARBITRARY REGISTER 2 BYTE 1 UPPER NIBBLE |
| 1000 | TALK STATUS DATA |
| 1001 | RESERVED |
| 1010 | RESERVED |
| 1011 | RESERVED |
| 1100 | RESERVED |
| 1101 | RESERVED |
| 1110 | RESERVED |
| 1111 | NO INFORMATION |

The status data is organized as follows:

| Bit | Information |
| --- | ----------- |
| 3 | Arbitrary register 2 filled (1) since last clear |
| 2 | Arbitrary register 0 is set (1) or drained (0) |
| 1 | Reserved |
| 0 | Keyboard buffer less than (0) or more than (1) half full |

## Keyboard

The device accepts serial packets that encode the normal keycodes for 
the Extended Keyboard II.  Tables for the keycodes are widely available 
online. Translation between whatever you are using for your keyboard 
source and the ADB keycodes is left as an exercise to the implementor.

The lower nibble command will update a temporary variable. The upper 
nibble command will be added to the temporary lower nibble data and 
will be pushed into a ring buffer. Ensure that commands go in 
lower->upper order or you may get corrupted keyboard data.

As part of the keyboard push commands, register 2 will automatically be 
updated with the correct meta-key data.  The keyboard code will also 
automatically handle doubling-up the 0x7F and 0xFF reset key press, per 
technical note HW01. The internal buffer for the keyboard allows up to 
16 presses to be accumulated before overflow occurs, which will cause 
keycodes to be dropped.

Since most implementors will likely be powering the IC from the ADB 
power source, the power switch (reset) button keycode is reported on
the *data line* only. External hardware is needed for dropping
the PSW line to start soft-power systems (if the system supports it).

## Mouse

The mouse driver has a signed 16 bit accumulator for both the X and Y 
axis, which is adjusted based on the SPI packets sent. Mouse buttons 
are tracked, so that a quick press/depress SPI sequence is still sent 
to the attached computer.  Note that the SPI interface uses a bit of
0 to indicate button up, and 1 to indicate button down (the opposite
of the protocol).

Only the "classic" mouse protocol is currently supported. Bit 0 (LSB) 
of the "MOUSE, SET BUTTONS" command is the main button, bit 1 is the 
second button. As a result, only the lower nibble command is important. 
The CPI of the mouse is set in the relevant EEPROM flag.

## Arbitrary Device

This is a generic set of registers that allows custom code running on 
the system to communicate with the attached master, and vice versa. 
Register 0 is 8 bytes long and contains data sent from the master 
to the system. Register 2 is 2 bytes long and contains data from the
system to the master.

Note again that this feature has not been tested... like, at all.
Feedback on the following scheme would be appreciated.  Also note the 
recommendations from the mothership against using ADB as a generic data 
transfer channel, and the generally slow performance of the scheme
described in this section.

### Usage

To send data to the system, issue a series of commands pushing data 
into arbitrary register 0. When all data that needs to be sent is in 
the buffer, send a special command flagging the data as ready. To check 
if the system has pulled the data, send a periodic TALK STATUS command 
and check if the data is still set (from the earlier command) or 
drained (if the system has pulled information).

The lower/upper nibble update commands operate the same way as the
keyboard commands: ensure any lower updates occur before upper updates.

Since all responses to the "talk" command require at least 2 bytes, if 
you fill register 0 with less than 2 bytes of data the ARBITRARY 
REGISTER 0 READY command will be ignored.

To get data from the system, code on the system should send an ADB 
"listen register 0" command to register 2, which sets the relevant flag 
in the TALK STATUS message. The data can be retrieved using serial 
commands to pull the data out, then cleared with the special command. 
The code on the system should be careful to check if the registers are 
empty (0x00, 0x00) prior to refilling them with new information.
