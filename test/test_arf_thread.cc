/*
 * A really stripped down jrecord for testing arf_thread.
 *
 */
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <boost/scoped_ptr.hpp>
#include <map>
#include <string>

#include "jill/file/multichannel_writer.hh"

#define CLIENT_NAME "test_arf_thread"
#define PERIOD_SIZE 1024
#define NPERIODS 1024

using namespace jill;

boost::scoped_ptr<data_thread> writer;
unsigned short seed[3] = { 0 };
sample_t test_data[PERIOD_SIZE];

void setup()
{
        for (size_t idx = 0; idx < PERIOD_SIZE; ++idx) {
                test_data[idx] = nrand48(seed);
        }
}


void
signal_handler(int sig)
{
        printf("got interrupt; trying to stop thread\n");
        writer->stop();
        writer->join();
        printf("passed tests\n");
        exit(sig);
}

void
test_write_log()
{
        writer->log("[" CLIENT_NAME "] a random log message");
}

void
start_dummy_writer(nframes_t buffer_size)
{
        writer.reset(new file::multichannel_writer(buffer_size));
        writer->start();
}

/**
 * Test compliance and performance of writer thread types. Throttles the rate of
 * writing data until no xruns are emitted for some duration.
 *
 * @param max_time    the amount of time without xruns needed to exit
 * @return final rate, in bytes/sec
 */
long
test_write_data(boost::posix_time::time_duration const & max_time)
{
	using namespace boost::posix_time;
        const size_t max_periods = 1 << 20; // sanity
        // long sleep_us = 0;
        timespec sleep_time = { 0, 0 };

        period_info_t info = {0, PERIOD_SIZE, 0};
        ptime last_xrun_t(microsec_clock::local_time());
        size_t last_xrun_i = 0;
        for (size_t i = 0; i < max_periods; ++i) {
                info.time = i;
                nframes_t r = writer->push(test_data, info);
                if (r < PERIOD_SIZE) {
                        last_xrun_i = i;
                        last_xrun_t = microsec_clock::local_time();
                        sleep_time.tv_nsec += 1;
                        // sleep_us += 1;
                }
                else {
                        time_duration dur = microsec_clock::local_time() - last_xrun_t;
                        if (dur > max_time) {
                                break;
                        }
                }
                nanosleep(&sleep_time, 0);
        }
        return sleep_time.tv_nsec;
}


int
main(int argc, char **argv)
{
        setup();

        signal(SIGINT,  signal_handler);
        signal(SIGTERM, signal_handler);
        signal(SIGHUP,  signal_handler);
        printf("Hit Ctrl-C to stop test\n");

        start_dummy_writer(PERIOD_SIZE);
        test_write_log();
        // test stopping writer without storing any data
        writer->stop();
        writer->join();

        printf("Testing dummy writer\n");
        nframes_t buffer_sizes[] = {PERIOD_SIZE * 3, PERIOD_SIZE * 5,
                                    PERIOD_SIZE * 10, PERIOD_SIZE * 20, 0};
        for (nframes_t *buffer_size = buffer_sizes; *buffer_size; ++buffer_size) {
                start_dummy_writer(*buffer_size);
                long rate = test_write_data(boost::posix_time::seconds(3));
                writer->stop();
                writer->join();
                printf("buffer size %d, period size %d :: sleep was %ld ns\n", *buffer_size, PERIOD_SIZE, rate);
        }

        return 0;
}
