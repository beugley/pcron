
CC = /usr/bin/cc

LIB = libcron.a
SRCS = entry.c env.c misc.c
OBJS = ${SRCS:.c=.o}

CFLAGS = -I../.. -I../../include

all: ${OBJS}

clean:
	rm -f ${LIB} ${OBJS}


.SUFFIXES: .o .c

.c.o:
	$(CC) $(CFLAGS) $(DFLAGS) -c $<
	ar -rv $(LIB) $*.o

