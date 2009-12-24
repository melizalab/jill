PROGS = JILL_capture JILL_display_signal JILL_delay JILL_transport

all: run_tests ${PROGS}

JACK_LIB = -ljack -lpthread -lrt

LFLAGS = ${JACK_LIB} -lsndfile

CFLAGS = -Wall -c

SRCS = JILL.c gnuplot_i.c
OBJS = $(SRCS:.c=.o)
HDRS = JILL.h

.SUFFIXES:

.SUFFIXES: .c 

%.o : %.c
	$(CC) ${CFLAGS} $<

JILL_delay: ${OBJS} JILL_delay.o
	$(CC) -o $@ $^ ${LFLAGS}

JILL_capture: ${OBJS} JILL_capture.o
	$(CC) -o $@ $^ ${LFLAGS}

JILL_transport: ${OBJS} JILL_transport.o
	$(CC) -o $@ $^ ${LFLAGS}

JILL_display_signal: ${OBJS} JILL_display_signal.o 
	$(CC) -o $@ $^ ${LFLAGS}

# A rule that runs the unit tests
run_tests: runner
	./runner

# How to build the test runner
runner: runner.cpp ${OBJS}
	g++ -o $@ ${OBJS} ${LFLAGS} runner.cpp

# How to generate the test runner
runner.cpp: TestJILL.h ${SRCS}
	cxxtestgen -o $@ --error-printer TestJILL.h

clean:
	rm -f ${OBJS} $(PROGS:%=%.o)
