
CC = /usr/bin/cc

LIB = libutil.a
SRCS = compat.c flopen.c pidfile.c
OBJS = ${SRCS:.c=.o}

CFLAGS = -I../.. -I../../include

all: ${OBJS}

clean:
	rm -f ${LIB} ${OBJS}


.SUFFIXES: .o .c

.c.o:
	$(CC) $(CFLAGS) $(DFLAGS) -c $<
	ar -rv $(LIB) $*.o

