#CROSS_COMPILE=/data/toolchain/gcc-arm-8.2-2018.08-x86_64-arm-linux-gnueabihf/bin/arm-linux-gnueabihf
CROSS_COMPILE=/home/yangjz/git/r818/prebuilt/gcc/linux-x86/aarch64/toolchain-sunxi-glibc/toolchain/bin/aarch64-openwrt-linux
LD=$(CROSS_COMPILE)-ld
CC=$(CROSS_COMPILE)-gcc
STRIP=$(CROSS_COMPILE)-strip

# LFLAGS+=--sysroot=/home/percherry/rk356x/sysroot
CFLAGS+=-I./

.PHONY: clean
all: fbtest uartest

fbtest: fb-test.c
	$(CC) -o $@ -L./ $(CFLAGS) $(LFLAGS) $^
	# $(CC) -o $@ -L./ -lmi_sys $(CFLAGS) $(LFLAGS) $^

uartest: uart-test.c
	$(CC) -o $@ -L./ $(CFLAGS) $(LFLAGS) $^

clean:
	rm -f *.o
