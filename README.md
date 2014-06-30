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

I2C device, 24AA1025 from Microchip, digikey PN: 24AA1025-I/P-ND, IC EEPROM 1MBIT 400KHZ 8DIP
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

// Usage (as root, or prefix with "sudo" ):
make
insmod i2c.ko
tail -n 2 /var/log/messages   ** or /var/log/kern.log or /var/log/syslog depending on your distro - look for major number, system will assign it for you
mknod /dev/i2c c 243 0    ** /dev/xxx the name you want the device to be, 243 is the major number, 0 is the minor number
chmod 666 /dev/i2c    ** ensure regular users can read/write to the "file", which the driver will handle as reading/writing

When writing, say with fwrite, the first byte is a control byte:
0x00 - nothing
0x01 - send i2c start
0x02 - send i2c stop
0x03 - send i2c start then at the end send i2c stop

After the control byte you typically send the device address then the data.  For the 24AA1025, you send 0xA0 for a write, then two addresses, first is the high address byte, then the low address byte, then the data to be written, upt to 128 chars (the size of one page, any more data written will simply be overwritten by the device as it implements a circular buffer).

For a read, 0xA1 (bit 0 is 1 for read, 0 for write, bits7:4 are 1010 for a control byte for the device), and the data will be read.  Send another ACK if you want more data.  Address read will be the last address written to.  Ideally, you would write your data, then send a write config and two address bytes, but no data, then send a new start and read config and the device will return your data, up to 128B.

After 128 bytes you have to change the address with the write-config and data address bytes, then the read config.

The device you're using should specify the protocol details for you.  The LCD I tested this with was different from the SEEPROM I tested it with.
