BUILD = glibc
EXEC = multipathd
SEND = mpathsend

include ../Makefile.inc

#
# basic flags setting
#
CFLAGS += -pipe -Wall -Wunused -Wstrict-prototypes \
	 -DDAEMON -I$(multipathdir) -I$(checkersdir) -D_GNU_SOURCE
LDFLAGS = -lpthread -ldevmapper -lsysfs -lreadline

#
# debuging stuff
#
#CFLAGS += -DLCKDBG
#CFLAGS += -D_DEBUG_
#CFLAGS += -DLOGDBG

#
# object files
#
OBJS = main.o log.o log_pthread.o pidfile.o uxlsnr.o uxclnt.o cli.o cli_handlers.o \
       $(MULTIPATHLIB)-glibc.a $(CHECKERSLIB)-glibc.a \


#
# directives
#
all : $(BUILD)

glibc: $(EXEC) $(SEND)

klibc:
	$(MAKE) BUILD=glibc glibc

$(EXEC): clean $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) $(LDFLAGS)
#	$(STRIP) $(EXEC)
	$(GZIP) $(EXEC).8 > $(EXEC).8.gz

$(SEND): clean mpathsend.o
	$(CC) mpathsend.o -o $(SEND) $(LDFLAGS)

$(CHECKERSLIB)-glibc.a:
	$(MAKE) -C $(checkersdir) BUILD=glibc glibc

$(MULTIPATHLIB)-glibc.a:
	$(MAKE) -C $(multipathdir) DAEMON=1 BUILD=glibc glibc

install:
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(EXEC) $(DESTDIR)$(bindir)
	install -m 755 $(SEND) $(DESTDIR)$(bindir)
	install -d $(DESTDIR)$(rcdir)
	install -d $(DESTDIR)$(mandir)
	install -m 644 $(EXEC).8.gz $(DESTDIR)$(mandir)

uninstall:
	rm -f $(DESTDIR)$(bindir)/$(EXEC)
	rm -f $(DESTDIR)$(rcdir)/$(EXEC)
	rm -f $(DESTDIR)$(mandir)/$(EXEC).8.gz

clean:
	$(MAKE) -C $(multipathdir) prepare
	rm -f core *.o $(EXEC) $(SEND) *.gz

