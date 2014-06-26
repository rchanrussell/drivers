drivers
=======
/* 
 * I2C using Parallel Port and custom driver project
 * Path: i2cPpLKM
 */

Used parallel port cable split open
PP Pin2 is SCL
PP Pin3 is SDA out
PP Pin12 is SDA in
SDA and SCL are buffered using open drain inverter (7406N)

I2C device, 24AA1025 from Microchip, digikey PN: 24AA1024-I/P-ND, IC EEPROM 1MBIT 400KHZ 8DIP
Pin 1, A0, VSS
Pin 2, A1, VSS
Pin 3, A2, VCC
Pin 4, VSS, VSS
Pin 5, SDA, SDA
Pin 6, SCL, SCL
Pin 7, WP, VSS
Pin 8, VCC, VCC

VCC is 5V.

Connections:
Pin2 of PP to Pin1 of 7406N, 10k pull up resistor (to VCC)
Pin2 of 7406N to device, 10k pull up resistor
Pin3 of PP to Pin3 of 7406N, 10k pull up resistor
Pin4 of 7406N to device, 10k pull up resistor
Pin4 of 7406N to Pin5 of 7406N (handle device SDA signals)
Pin12 of PP to Pin6 of 7406N, 10k pull up resistor

I was using the Arduino Uno to provide the 5V VCC but I recieved my USB breakout cable and used the +5V and Common from that

Debugging:
After ironing out the algorithm on the Arduino Uno, I chose to use two 8-bit serial-in-parallel-out shift registers
Outputs sink current lighting the LED's when 0 is entered for that register
Shift registers used: STP08DP05
VCC 5V
RExt 10k for Red LED and 2k for Yellow LED
Without sending start/stop conditions, you can validate the data being sent to the device (notice the inversion on i2c_setSCL
and i2c_setSDA)
For driver debugging I used printk's all over and gradually removed them
I also grabbed the Arduino Uno to verify the writes were taking place

There are far better drivers out there, with far greater complexity, using IOCTL for example.  Here I used a simple
character driver and modified it to perform inb/outb against the parallel port addresses (0x378/379)

This is something I have wanted to do for a very long time and though it took a lot longer than I had hoped, at least it is
working to the point where I can release it for others to build upon/correct/learn from.
