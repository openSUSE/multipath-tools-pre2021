BUILD = glibc
EXEC = multipathd

include ../Makefile.inc

#
# basic flags setting
#
CFLAGS += -DDAEMON -I$(multipathdir) -I$(checkersdir)
LDFLAGS = -lpthread -ldevmapper -lreadline -lncurses

#
# debuging stuff
#
#CFLAGS += -DLCKDBG
#CFLAGS += -D_DEBUG_
#CFLAGS += -DLOGDBG

#
# object files
#
OBJS = main.o pidfile.o uxlsnr.o uxclnt.o cli.o cli_handlers.o \
       $(MULTIPATHLIB)-glibc.a $(CHECKERSLIB)-glibc.a \


#
# directives
#
all : $(BUILD)

glibc: $(EXEC)

klibc:
	$(MAKE) BUILD=glibc glibc

$(EXEC): clean $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) $(LDFLAGS)

$(CHECKERSLIB)-glibc.a:
	$(MAKE) -C $(checkersdir) BUILD=glibc glibc

$(MULTIPATHLIB)-glibc.a:
	$(MAKE) -C $(multipathdir) DAEMON=1 BUILD=glibc glibc

install:
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(EXEC) $(DESTDIR)$(bindir)
	install -d $(DESTDIR)$(rcdir)
	install -d $(DESTDIR)$(mandir)
	install -m 644 $(EXEC).8 $(DESTDIR)$(mandir)

uninstall:
	rm -f $(DESTDIR)$(bindir)/$(EXEC)
	rm -f $(DESTDIR)$(rcdir)/$(EXEC)

clean:
	$(MAKE) -C $(multipathdir) prepare DAEMON=1
	rm -f core *.o $(EXEC)

