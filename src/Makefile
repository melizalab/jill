all: run_tests JILL JILL_capture JILL_display_signal

JACK_LIB = -ljack -lpthread -lrt

LFLAGS = ${JACK_LIB} -lsndfile

CFLAGS = -Wall -c 

SRCS = JILL_capture.c
OBJS = JILL.o
HDRS = JILL.h

#.SUFFIXES:

.SUFFIXES: .c 

%.o : %.c
	$(CC) $(CFLAGS) $<

JILL: JILL.c JILL.h  
	$(CC) ${CFLAGS} JILL.c

JILL_capture: JILL JILL_capture.o 
	$(CC) -o $@ JILL.o JILL_capture.o ${LFLAGS}

JILL_display_signal: JILL JILL.o JILL_display_signal.o 
	$(CC) -o $@ JILL.o gnuplot_i.o JILL_display_signal.o ${LFLAGS}

# A rule that runs the unit tests
run_tests: runner
	./runner

# How to build the test runner
runner: runner.cpp JILL
	g++ -o $@ JILL.o ${LFLAGS} runner.cpp

# How to generate the test runner
runner.cpp: TestJILL.h ${SRCS}
	cxxtestgen -o $@ --error-printer TestJILL.h

clean:
	rm -f ${OBJS}