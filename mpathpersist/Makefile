# Makefile
#
include ../Makefile.inc

OBJS = main.o 

CFLAGS += -I$(multipathdir) -I$(mpathpersistdir) 
LDFLAGS += -lpthread -ldevmapper -L$(mpathpersistdir) -lmpathpersist -L$(multipathdir) -lmultipath  -ludev

EXEC = mpathpersist

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) -g $(OBJS) -o $(EXEC) $(LDFLAGS) $(CFLAGS)

install:
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(EXEC) $(DESTDIR)$(bindir)/
	install -d $(DESTDIR)$(mandir)
	install -m 644 $(EXEC).8 $(DESTDIR)$(mandir)

clean:
	rm -f *.o $(EXEC)

uninstall:
	rm $(DESTDIR)$(bindir)/$(EXEC)
	rm $(DESTDIR)$(mandir)/$(EXEC).8
