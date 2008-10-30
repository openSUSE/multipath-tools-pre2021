# Makefile
#
# Copyright (C) 2003 Christophe Varoqui, <christophe.varoqui@free.fr>
#
include ../Makefile.inc

OBJS = main.o

CFLAGS += -I$(multipathdir) -Wl,-rpath,$(libdir)
LDFLAGS += -lpthread -ldevmapper -ldl \
	   -lmultipath -L$(multipathdir)

EXEC = multipath

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(EXEC) $(LDFLAGS)

install:
	$(INSTALL_PROGRAM) -d $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM) -m 755 $(EXEC) $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM) -d $(DESTDIR)$(rcdir)
	$(INSTALL_PROGRAM) -m 755 multipath.init.suse $(DESTDIR)$(rcdir)/boot.multipath
	$(INSTALL_PROGRAM) -d $(DESTDIR)/etc/udev/rules.d
	$(INSTALL_PROGRAM) -m 644 multipath.rules $(DESTDIR)/etc/udev/rules.d/71-multipath.rules
	$(INSTALL_PROGRAM) -d $(DESTDIR)$(mandir)
	$(INSTALL_PROGRAM) -m 644 $(EXEC).8 $(DESTDIR)$(mandir)
	$(INSTALL_PROGRAM) -d $(DESTDIR)$(man5dir)
	$(INSTALL_PROGRAM) -m 644 $(EXEC).conf.5 $(DESTDIR)$(man5dir)
	$(INSTALL_PROGRAM) -d $(DESTDIR)/lib/mkinitrd/scripts
	$(INSTALL_PROGRAM) -m 755 boot-multipath.sh $(DESTDIR)/lib/mkinitrd/scripts
	$(INSTALL_PROGRAM) -m 755 setup-multipath.sh $(DESTDIR)/lib/mkinitrd/scripts

uninstall:
	rm $(DESTDIR)/etc/udev/rules.d/multipath.rules
	rm $(DESTDIR)$(bindir)/$(EXEC)
	rm -f $(DESTDIR)/lib/mkinitrd/scripts/boot-multipath.sh
	rm -f $(DESTDIR)/lib/mkinitrd/scripts/setup-multipath.sh

clean:
	rm -f core *.o $(EXEC)
