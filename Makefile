# $Id$


# Set the following variables to the location of the libsmbclient and
# libreadline header and library files.

LIBREADLINE_INCLUDE=$(HOME)/local/include
LIBREADLINE_LIBRARY=$(HOME)/local/lib
LIBSMBCLIENT_INCLUDE=$(HOME)/local/include
LIBSMBCLIENT_LIBRARY=$(HOME)/local/lib


# From the following source/object files, the samblah binary is
# built.  This does not include the files for libegetopt.a and
# libsmbwrap.a.
SRCS=alias.c cmdls.c cmds.c complete.c init.c interface.c main.c misc.c parsecl.c pcols.c smbglob.c smbhlp.c transfer.c vars.c
OBJS=alias.o cmdls.o cmds.o complete.o init.o interface.o main.o misc.o parsecl.o pcols.o smbglob.o smbhlp.o transfer.o vars.o


CC=cc
CFLAGS=-Wall -g
LD=cc
LDFLAGS=
NROFF=nroff -mdoc
AR=ar -rc
RANLIB=ranlib


all: samblah samblah.0

.SUFFIXES: .c.o
.c.o:
	$(CC) $(CFLAGS) -I. -I$(LIBREADLINE_INCLUDE) -c -o $@ $?

samblah: $(OBJS) libegetopt.a libsmbwrap.a
	$(LD) $(LDFLAGS) -L. -L$(LIBREADLINE_LIBRARY) -L$(LIBSMBCLIENT_LIBRARY) -o samblah $(OBJS) libegetopt.a libsmbwrap.a -lncurses -lreadline -lsmbclient

libegetopt.a: egetopt.c egetopt.h
	$(CC) $(CFLAGS) -c -o egetopt.o egetopt.c
	$(AR) libegetopt.a egetopt.o
	$(RANLIB) libegetopt.a

libsmbwrap.a: smbwrap.c smbwrap.h
	$(CC) $(CFLAGS) -I$(LIBSMBCLIENT_INCLUDE) -c -o smbwrap.o smbwrap.c
	$(AR) libsmbwrap.a smbwrap.o
	$(RANLIB) libsmbwrap.a

samblah.0: samblah.1
	$(NROFF) samblah.1 > samblah.0

clean:
	-rm samblah samblah.0 $(OBJS) libegetopt.a egetopt.o libsmbwrap.a smbwrap.o 2> /dev/null

lint:
	lint -I. -I$(LIBREADLINE_INCLUDE) -I$(LIBSMBCLIENT_INCLUDE) -aabchruH -lposix $(SRCS) egetopt.c smbwrap.c | grep -v "warning: ANSI C does not support 'long long'"
