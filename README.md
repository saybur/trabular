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
