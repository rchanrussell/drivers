obj-m += i2c.o

KERNELDIR=/lib/modules/${shell uname -r}/build

all:
	make -C ${KERNELDIR} M=${PWD} modules

clean:
	make -C ${KERNELDIR} M=${PWD} clean
