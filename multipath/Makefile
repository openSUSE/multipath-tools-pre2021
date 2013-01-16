# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@opensvc.com>
#
include ../Makefile.inc

OBJS = main.o

CFLAGS += -I$(multipathdir)
LDFLAGS += -lpthread -ldevmapper -ldl -L$(multipathdir) -lmultipath

EXEC = multipath

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(EXEC) $(LDFLAGS)

install:
	$(INSTALL_PROGRAM) -d $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM) -m 755 $(EXEC) $(DESTDIR)$(bindir)/
	$(INSTALL_PROGRAM) -d $(DESTDIR)$(mandir)
	$(INSTALL_PROGRAM) -m 644 $(EXEC).8 $(DESTDIR)$(mandir)
	$(INSTALL_PROGRAM) -d $(DESTDIR)$(man5dir)
	$(INSTALL_PROGRAM) -m 644 $(EXEC).conf.5 $(DESTDIR)$(man5dir)
	$(INSTALL_PROGRAM) -d $(DESTDIR)/lib/mkinitrd/scripts
	$(SED) 's/@LIB@/$(LIB)/g' boot-multipath.sh > $(DESTDIR)/lib/mkinitrd/scripts/boot-multipath.sh
	chmod 755 $(DESTDIR)/lib/mkinitrd/scripts/boot-multipath.sh
	$(SED) 's/@LIB@/$(LIB)/g' setup-multipath.sh > $(DESTDIR)/lib/mkinitrd/scripts/setup-multipath.sh
	chmod 755 $(DESTDIR)/lib/mkinitrd/scripts/setup-multipath.sh

uninstall:
	rm $(DESTDIR)$(bindir)/$(EXEC)
	rm $(DESTDIR)$(mandir)/$(EXEC).8
	rm $(DESTDIR)$(man5dir)/$(EXEC).conf.5
	rm -f $(DESTDIR)/lib/mkinitrd/scripts/boot-multipath.sh
	rm -f $(DESTDIR)/lib/mkinitrd/scripts/setup-multipath.sh

clean:
	rm -f core *.o $(EXEC)
