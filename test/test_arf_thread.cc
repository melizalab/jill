/*
 * A really stripped down jrecord for testing arf_thread.
 *
 */
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <signal.h>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <boost/scoped_ptr.hpp>
#include <boost/assign/list_of.hpp>
#include <map>
#include <string>

#include "jill/file/multichannel_writer.hh"
#include "jill/file/arf_writer.hh"
#include "jill/util/string.hh"

#define CLIENT_NAME "test_arf_thread"
#define PERIOD_SIZE 1024
#define NCHANNELS 2
#define NPERIODS 1024

using namespace std;
using namespace jill;

boost::scoped_ptr<data_thread> writer;
unsigned short seed[3] = { 0 };
sample_t test_data[PERIOD_SIZE];

map<string,string> attrs = boost::assign::map_list_of("experimenter","Dan Meliza")
        ("experiment","testing");

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
        writer->log("[] a random log message");
}

void
test_to_hex()
{
        int vals[] = {0xf0, 0xab, 0x90, 0x00};
        std::string s = jill::util::to_hex(vals, 4);
        assert(s=="f0ab9000");
}


void
start_dummy_writer(nframes_t buffer_size)
{
        fprintf(stderr, "Testing dummy writer, buffer size = %d\n", buffer_size);
        writer.reset(new file::multichannel_writer(buffer_size));
        writer->start();
}

void
start_arf_writer(nframes_t buffer_size, int compression=0)
{
        file::arf_writer *f = new file::arf_writer("test.arf", attrs, 0, compression);
        buffer_size = f->resize_buffer(buffer_size);
        writer.reset(f);
        fprintf(stderr, "Testing arf writer, buffer size=%d, compression=%d\n", buffer_size, compression);
        writer->start();
}

void
test_write_data_rate(boost::posix_time::time_duration const & max_time)
{
	using namespace boost::posix_time;
        size_t i = 0;
        period_info_t info = {0, PERIOD_SIZE, 0};
        ptime start(microsec_clock::local_time());
        time_duration dur(seconds(0));
        while (dur < max_time) {
                info.time = (i / NCHANNELS) * PERIOD_SIZE;
                nframes_t r = writer->push(test_data, info);
                if (r == PERIOD_SIZE)
                        ++i;
                dur = microsec_clock::local_time() - start;
        }
        long ms = dur.total_milliseconds();
        fprintf(stderr, "\nrate: %ld periods in %ld ms\n", i * NCHANNELS, ms);
}



/**
 * Test compliance and performance of writer thread types. Throttles the rate of
 * writing data until no xruns are emitted for some duration.
 *
 * @param max_time    the amount of time without xruns needed to exit
 * @return final rate, in bytes/sec
 */
long
test_write_data_speed(boost::posix_time::time_duration const & max_time)
{
	using namespace boost::posix_time;
        const size_t max_periods = 1 << 20; // sanity
        timespec sleep_time = { 0, 0 };
        size_t i;

        period_info_t info = {0, PERIOD_SIZE, 0};
        ptime last_xrun_t(microsec_clock::local_time());
        for (i = 0; i < max_periods; ++i) {
                info.time = (i / NCHANNELS) * PERIOD_SIZE;
                nframes_t r = writer->push(test_data, info);
                if (r < PERIOD_SIZE) {
                        last_xrun_t = microsec_clock::local_time();
                        sleep_time.tv_nsec += 1;
                }
                else {
                        time_duration dur = microsec_clock::local_time() - last_xrun_t;
                        if (dur > max_time) {
                                break;
                        }
                }
                nanosleep(&sleep_time, 0);
        }
        if (i >= max_periods-1)
                fprintf(stderr, "frame counter overrun; max rate may not be correct");
        return sleep_time.tv_nsec;
}


int
main(int argc, char **argv)
{
        nframes_t buffer_size = 20480;
        if (argc > 1) {
                buffer_size = atoi(argv[1]);
        }
        setup();
        test_to_hex();

        signal(SIGINT,  signal_handler);
        signal(SIGTERM, signal_handler);
        signal(SIGHUP,  signal_handler);
        fprintf(stderr, "Hit Ctrl-C to stop test\n");

        start_dummy_writer(PERIOD_SIZE);
        test_write_log();
        // test stopping writer without storing any data
        writer->stop();

        start_dummy_writer(buffer_size);
        test_write_data_rate(boost::posix_time::seconds(5));
        writer->stop();
        writer->join();

        start_arf_writer(buffer_size);
        test_write_log();
        test_write_data_rate(boost::posix_time::seconds(5));
        writer->stop();
        writer->join();
        writer.reset();

        return 0;
}
