TOPDIR:=$(shell pwd)
export TOPDIR

CROSS_COMPILE=/home/percherry/rk356x/gcc-buildroot-9.3.0-2020.03-x86_64_aarch64-rockchip-linux-gnu/bin/
LD=$(CROSS_COMPILE)aarch64-rockchip-linux-gnu-ld
CC=$(CROSS_COMPILE)aarch64-rockchip-linux-gnu-cc

all: xs5013

xs5013: xs5013init.c
	$(CC) -o xs5013 $< -Iinclude $(LFLAGS)

