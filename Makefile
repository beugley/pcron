
CC = /usr/bin/cc

EXE1 = pcrontab
SRCS1 = pcrontab.c
OBJS1 = ${SRCS1:.c=.o}
EXE2 = pcron
SRCS2 = pcron.c database.c job.c do_command.c popen.c user.c
OBJS2 = ${SRCS2:.c=.o}

#DFLAGS = -DDEBUGGING
CFLAGS = -I. -Iinclude -Ilib/libutil
LDFLAGS = -Llib/libutil -Llib/libcron
LIBS = -lutil -lcron

all: libs ${EXE1} ${EXE2}

libs: 
	cd lib; make all DFLAGS=$(DFLAGS)

clean:
	rm -f ${EXE1} ${EXE2} ${OBJS1} ${OBJS2}
	cd lib; make clean

${EXE1}: ${OBJS1}
	${CC} -o $@ ${OBJS1} ${LDFLAGS} ${LIBS}

${EXE2}: ${OBJS2}
	${CC} -o $@ ${OBJS2} ${LDFLAGS} ${LIBS}


.SUFFIXES: .o .c

.c.o:
	$(CC) $(CFLAGS) $(DFLAGS) -c $<

