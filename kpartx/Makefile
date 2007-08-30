# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>
#
BUILD=glibc

include ../Makefile.inc

CFLAGS += -I. -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64

ifeq ($(strip $(BUILD)),klibc)
	OBJS = bsd.o dos.o kpartx.o solaris.o unixware.o gpt.o crc32.o \
	       lopart.o xstrncpy.o devmapper.o dasd.o mac.o sun.o \
	       $(MULTIPATHLIB)-$(BUILD).a $(libdm)
else
	LDFLAGS = -ldevmapper
	OBJS = bsd.o dos.o kpartx.o solaris.o unixware.o dasd.o sun.o \
	       gpt.o mac.o crc32.o lopart.o xstrncpy.o devmapper.o
endif

EXEC = kpartx

all: $(BUILD)

prepare:
	rm -f core *.o

glibc: prepare $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) $(LDFLAGS)

klibc: prepare $(OBJS)
	$(CC) -static -o $(EXEC) $(CRT0) $(OBJS) $(KLIBC) $(LIBGCC)

$(MULTIPATHLIB)-$(BUILD).a:
	make -C $(multipathdir) BUILD=$(BUILD)

install: $(EXEC) kpartx_id $(EXEC).8
	install -d $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM) -m 755 $(EXEC) $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM) -m 755 activate_dm_linear $(DESTDIR)$(bindir)
	install -d $(DESTDIR)$(libudevdir)
	install -m 755 kpartx_id $(DESTDIR)$(libudevdir)
	install -m 755 dm_linear $(DESTDIR)$(libudevdir)
	install -d $(DESTDIR)/etc/udev/rules.d
	install -m 644 kpartx.rules $(DESTDIR)/etc/udev/rules.d/70-kpartx.rules
	install -d $(DESTDIR)$(mandir)
	install -m 644 $(EXEC).8 $(DESTDIR)$(mandir)

uninstall:
	rm -f $(DESTDIR)$(bindir)/$(EXEC)
	rm -f $(DESTDIR)$(bindir)/activate_dm_linear
	rm -f $(DESTDIR)$(libudevdir)/kpartx_id
	rm -f $(DESTDIR)$(libudevdir)/dm_linear
	rm -f $(DESTDIR)/etc/udev/rules.d/70-kpartx.rules
	rm -f $(DESTDIR)$(mandir)/$(EXEC).8

clean:
	rm -f core *.o $(EXEC)
