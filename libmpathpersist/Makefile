# Makefile
#
BUILD = glibc
include ../Makefile.inc

INSTALL_PROGRAM = install

SONAME=0
DEVLIB = libmpathpersist.so
LIBS = $(DEVLIB).$(SONAME)


CFLAGS += -I$(multipathdir) -I$(mpathpersistdir) 
LIBDEPS +=  -lpthread -ldevmapper -ldl -L$(multipathdir) -lmultipath

OBJS = mpath_persist.o mpath_updatepr.o mpath_pr_ioctl.o 

all: $(LIBS)


$(LIBS): $(OBJS)
	$(CC) $(LDFLAGS) $(SHARED_FLAGS) -Wl,-soname=$@ $(CFLAGS) -o $@ $(OBJS)
	ln -sf $@ $(DEVLIB)

install: $(LIBS)
	$(INSTALL_PROGRAM) -d $(DESTDIR)$(syslibdir)
	$(INSTALL_PROGRAM) -m 755 $(LIBS) $(DESTDIR)$(syslibdir)/$(LIBS)
	$(INSTALL_PROGRAM) -m 755 -d $(DESTDIR)$(man3dir)
	$(INSTALL_PROGRAM) -m 755 -d $(DESTDIR)/usr/include/
	$(INSTALL_PROGRAM) -m 755 -d $(DESTDIR)/usr/share/doc/mpathpersist/
	install -m 644 mpath_persistent_reserve_in.3 $(DESTDIR)$(man3dir)
	install -m 644 mpath_persistent_reserve_out.3 $(DESTDIR)$(man3dir)
	install -m 644 mpath_persist.h $(DESTDIR)/usr/include/

uninstall:
	rm -f $(DESTDIR)$(syslibdir)/$(LIBS)
	rm $(DESTDIR)$(mandir)/mpath_persistent_reserve_in.3
	rm $(DESTDIR)$(mandir)/mpath_persistent_reserve_out.3

clean:
	rm -f core *.a *.o 
	rm -f libmpathpersist.so.0
	rm -f libmpathpersist.so
