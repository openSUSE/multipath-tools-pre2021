EXEC = multipathd

include ../Makefile.inc

#
# basic flags setting
#
CFLAGS += -I$(multipathdir) -I$(mpathpersistdir)
LDFLAGS += -lpthread -ldevmapper -lreadline -ludev -ldl \
	   -L$(multipathdir) -lmultipath -L$(mpathpersistdir) -lmpathpersist

#
# debuging stuff
#
#CFLAGS += -DLCKDBG
#CFLAGS += -D_DEBUG_
#CFLAGS += -DLOGDBG

#
# object files
#
OBJS = main.o pidfile.o uxlsnr.o uxclnt.o cli.o cli_handlers.o


#
# directives
#
all : $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(EXEC)

install:
	$(INSTALL_PROGRAM) -d $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM) -m 755 $(EXEC) $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM) -d $(DESTDIR)$(rcdir)
	$(INSTALL_PROGRAM) -m 755 multipathd.init.suse $(DESTDIR)$(rcdir)/$(EXEC)
	$(INSTALL_PROGRAM) -d $(DESTDIR)$(unitdir)
	$(INSTALL_PROGRAM) -m 644 $(EXEC).service $(DESTDIR)$(unitdir)
	$(INSTALL_PROGRAM) -d $(DESTDIR)$(mandir)
	$(INSTALL_PROGRAM) -m 644 $(EXEC).8 $(DESTDIR)$(mandir)
	$(INSTALL_PROGRAM) -d $(DESTDIR)/lib/mkinitrd/scripts
	$(SED) 's/@LIB@/$(LIB)/g' boot-multipathd.sh > $(DESTDIR)/lib/mkinitrd/scripts/boot-multipathd.sh
	chmod 755 $(DESTDIR)/lib/mkinitrd/scripts/boot-multipathd.sh
	$(SED) 's/@LIB@/$(LIB)/g' boot-killmultipathd.sh > $(DESTDIR)/lib/mkinitrd/scripts/boot-killmultipathd.sh
	chmod 755 $(DESTDIR)/lib/mkinitrd/scripts/boot-killmultipathd.sh

uninstall:
	rm -f $(DESTDIR)$(bindir)/$(EXEC)
	rm -f $(DESTDIR)$(rcdir)/$(EXEC)
	rm -f $(DESTDIR)$(mandir)/$(EXEC).8
	rm -f $(DESTDIR)$(unitdir)/$(EXEC).service
	rm -f $(DESTDIR)/lib/mkinitrd/scripts/boot-multipathd.sh
	rm -f $(DESTDIR)/lib/mkinitrd/scripts/boot-killmultipathd.sh

clean:
	rm -f core *.o $(EXEC)

