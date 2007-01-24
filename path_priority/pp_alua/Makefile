#==============================================================================
# (C) Copyright IBM Corp. 2004, 2005   All Rights Reserved.
#
# Makefile
#
# Tool to make use of a SCSI-feature called Asymmetric Logical Unit Access.
# It determines the ALUA state of a device and prints a priority value to
# stdout.
#
# Author(s): Jan Kunigk
#            S. Bader <shbader@de.ibm.com>
#
# This file is released under the GPL.
#==============================================================================
EXEC		= mpath_prio_alua
BUILD		= glibc
DEBUG		= 0
DEBUG_DUMPHEX	= 0
OBJS		= main.o rtpg.o
INSTALL		= install -D

TOPDIR	= ../..

ifneq ($(shell ls $(TOPDIR)/Makefile.inc 2>/dev/null),)
include $(TOPDIR)/Makefile.inc
endif

CFLAGS += -DDEBUG=$(DEBUG)

all: $(BUILD)

glibc:	$(OBJS)
	$(CC) -o $(EXEC) $(OBJS) $(LDFLAGS)

klibc:	$(OBJS)
	$(CC) -static -o $(EXEC) $(OBJS)

install: $(EXEC) $(EXEC).8
	$(INSTALL) -s -m 755 $(EXEC) $(DESTDIR)$(bindir)/$(EXEC)
	$(INSTALL) -m 644 $(EXEC).8 $(DESTDIR)$(mandir)/$(EXEC).8

uninstall:
	rm $(DESTDIR)$(bindir)/$(EXEC)
	rm $(DESTDIR)$(mandir)/$(EXEC).8

clean:	
	rm -f *.o *.gz $(EXEC)

main.o:	main.c rtpg.h spc3.h

rtpg.o:	rtpg.c rtpg.h spc3.h
