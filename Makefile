obj-m += sp_virt_sensor.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

.PHONY: all clean test

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

test: test_sensor

test_sensor: test_sensor.c sp_virt_sensor_uapi.h
	$(CC) -Wall -Wextra -O2 -o $@ test_sensor.c

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f test_sensor

