TOPDIR:=$(shell pwd)
export TOPDIR
export TARGETPATH=$(TOPDIR)/../../../device/rockchip/oem/oem_empty

CROSS_COMPILE=/home/percherry/rk356x/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin
LD=$(CROSS_COMPILE)/aarch64-linux-gnu-ld
CC=$(CROSS_COMPILE)/aarch64-linux-gnu-gcc
STRIP=$(CROSS_COMPILE)/aarch64-linux-gnu-strip

LFLAGS+=--sysroot=/home/percherry/rk356x/sysroot
CFLAGS+=-I/home/percherry/rk356x/sysroot/usr/include -I/home/percherry/rk356x/sysroot/usr/include/drm

.PHONY: clean
all: xs5013 softapDemo systest drmtest

xs5013: xs5013init.c
	$(CC) -o $@ $(CFLAGS) $(LFLAGS) $^
	@$(STRIP) $@
	cp xs5013 $(TARGETPATH)/bin/

systest: systest.c
	$(CC) -o systest -L./ -lsyswrapper -ldrm $(CFLAGS) $(LFLAGS) $^
	@$(STRIP) systest

libsyswrapper.so: kms.c pattern.c buffers.c format.c syswrapper.c
	$(CC) -fpic -shared $(CFLAGS) $(LFLAGS) $^ -o $@
	$(STRIP) $@
	cp $@ $(TARGETPATH)/pos/lib/

sendevent: sendevent.c
	$(CC) -o sendevent $(LFLAGS) $^
	@$(STRIP) sendevent

softapDemo: softapDemo.c
	$(CC) -o softapDemo $< $(LFLAGS)
	$(STRIP) softapDemo
#	cp softapDemo $(TARGETPATH)/bin/

drmtest: drmtest.c
	$(CC) -o drmtest -L./ -ldrm $(CFLAGS) $< $(LFLAGS)
	@$(STRIP) drmtest

drmtest1: drmtest.c
	$(CC) -DSYSWRAPPER -o drmtest -L./ -ldrm -lsyswrapper $(CFLAGS) $(LFLAGS) $^
	$(STRIP) drmtest

fbtest: fb_test.c
	$(CC) -o fbtest $(CFLAGS) $< $(LFLAGS)
	$(STRIP) fbtest

smartwlan: smartconfig.c
	$(CC) -o $@ $(CFLAGS) $< $(LFLAGS)
	$(STRIP) $@

testwifi: testwifi.cpp
	$(CC) -o $@ -L/home/percherry/rk356x/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/aarch64-linux-gnu/lib64/ -lstdc++ -lwpa_client -lpthread $(CFLAGS) $< $(LFLAGS)
	$(STRIP) $@

systest drmtest1: libsyswrapper.so

clean:
	rm -f *.o
	rm -f xs5013 drmtest sendevent libsyswrapper.so systest fbtest softapDemo
