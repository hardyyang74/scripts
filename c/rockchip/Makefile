TOPDIR:=$(shell pwd)
export TOPDIR

CROSS_COMPILE=/home/percherry/rk356x/gcc-buildroot-9.3.0-2020.03-x86_64_aarch64-rockchip-linux-gnu/bin/
LD=$(CROSS_COMPILE)aarch64-rockchip-linux-gnu-ld
CC=$(CROSS_COMPILE)aarch64-rockchip-linux-gnu-cc
STRIP=$(CROSS_COMPILE)aarch64-rockchip-linux-gnu-strip

all: xs5013 sendevent

xs5013: xs5013init.c
	$(CC) -o xs5013 $< -Iinclude $(LFLAGS)
	$(STRIP) xs5013

sendevent: sendevent.c
	$(CC) -o sendevent  $< -Iinclude $(LFLAGS)
	$(STRIP) sendevent

drmtest: drmtest.c
	$(CC) -o drmtest -L./ -ldrm $(CFLAGS) $< -Iinclude $(LFLAGS)
	$(STRIP) drmtest
