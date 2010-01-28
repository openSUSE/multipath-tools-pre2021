# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>
#
BUILD = glibc

include ../Makefile.inc

OBJS = libsg.o checkers.o readsector0.o tur.o directio.o emc_clariion.o hp_sw.o rdac.o cciss_tur.o

OBJS_DAEMON = $(OBJS:%.o=%-daemon.o)

all: $(BUILD) daemon

prepare:
	@file *-$(BUILD).a >/dev/null 2>&1 || rm -f core *.o *.gz

klibc: prepare $(OBJS)
	ar rs libcheckers-klibc.a *.o

glibc: prepare $(OBJS)
	ar rs libcheckers-glibc.a *.o

daemon: $(OBJS_DAEMON)
	ar rs libcheckers-daemon.a *-daemon.o

install:

uninstall:

clean:
	rm -f core *.a *.o *.gz

%-daemon.o : %.c
	$(CC) -c $(CFLAGS) -DDAEMON $< -o $@
