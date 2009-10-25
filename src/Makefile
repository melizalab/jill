all: run_tests JILL_capture JILL_display_signal

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

JILL_capture: ${OBJS} JILL_capture.o
	$(CC) -o $@ ${OBJS} JILL_capture.o ${LFLAGS}

JILL_display_signal: ${OBJS} JILL_display_signal.o 
	$(CC) -o $@ ${OBJS} JILL_display_signal.o ${LFLAGS}

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
	rm -f ${OBJS} JILL_capture.o JILL_display_signal.o
