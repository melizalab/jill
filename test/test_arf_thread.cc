#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <signal.h>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/assign/list_of.hpp>
#include <map>
#include <string>

#include "jill/data_source.hh"
#include "jill/file/null_writer.hh"
#include "jill/file/arf_writer.hh"
#include "jill/dsp/buffered_data_writer.hh"
#include "jill/dsp/triggered_data_writer.hh"
#include "jill/dsp/period_ringbuffer.hh"

#define CLIENT_NAME "test_arf_thread"
#define COMPRESSION 0
#define PERIOD_SIZE 1024
#define NCHANNELS 2
#define PREBUFFER 5000
#define POSTBUFFER 2000

using namespace std;
using namespace jill;

boost::shared_ptr<data_source> client;
boost::shared_ptr<data_writer> writer;
boost::shared_ptr<data_thread> thread;

nframes_t buffer_size = 20480;
unsigned short seed[3] = { 0 };
sample_t test_data[PERIOD_SIZE];

void setup()
{
        for (size_t idx = 0; idx < PERIOD_SIZE; ++idx) {
                test_data[idx] = nrand48(seed);
        }
}

void
write_data_rate(boost::posix_time::time_duration const & max_time)
{
	using namespace boost::posix_time;
        thread->start();
        long i = 0;
        period_info_t info = {0, PERIOD_SIZE, 0};
        ptime start(microsec_clock::local_time());
        time_duration dur(seconds(0));
        while (dur < max_time) {
                info.time = (i / NCHANNELS) * PERIOD_SIZE;
                nframes_t r = thread->push(test_data, info);
                if (r == PERIOD_SIZE)
                        ++i;
                dur = microsec_clock::local_time() - start;
        }
        long ms = dur.total_milliseconds();
        thread->stop();
        thread->join();
        printf("\nrate: %ld periods in %ld ms (%.2f MB/s)\n", i, ms, float(i) * PERIOD_SIZE / (1e6 * ms / 1000));
}


size_t
write_data_simple(nframes_t periods, nframes_t channels, period_info_t * info)
{

        size_t ret = 0;
        for (size_t i = 0; i < periods; ++i) {
                for (size_t chan = 0; chan < channels; ++chan) {
                        thread->push(test_data, *info);
                        ret += 1;
                }
                info->time += PERIOD_SIZE;
        }
        return ret;
}

namespace jill { namespace dsp {

/* need access to some private members of triggered_data_writer */
struct triggered_data_writer_test {
        triggered_data_writer_test(triggered_data_writer * w) : p(w) {
                p->resize_buffer(buffer_size, NCHANNELS);
                write_space = p->write_space(PERIOD_SIZE);
                thread.reset(p);
                thread->start();
        }
        ~triggered_data_writer_test() {
                thread->stop();
                thread->join();
        }
        void start_recording(nframes_t);
        void test(nframes_t);
        triggered_data_writer *p;
        nframes_t write_space;

};

void
triggered_data_writer_test::start_recording(nframes_t time)
{
        p->start_recording(time);
        // at this point only the last period should be in the buffer
        assert(p->write_space(PERIOD_SIZE) == write_space - NCHANNELS);

        // and flush it so that the next call to write doesn't fail
        period_info_t const * tail = p->_buffer->peek();
        while (tail != 0) {
                p->_writer->write(tail);
                p->_buffer->release();
                tail = p->_buffer->peek();
        }
}

void
triggered_data_writer_test::test(nframes_t start_time)
{
        size_t periods;
        period_info_t info = {start_time, PERIOD_SIZE, 0};
        nframes_t trig_on_time, trig_off_time;

        // write enough data to underfill prebuffer
        periods = write_data_simple(PREBUFFER / PERIOD_SIZE, NCHANNELS, &info);
        // make sure the thread has not pulled any data off the buffer
        assert(p->write_space(PERIOD_SIZE) == write_space - periods);

        // now overfill the prebuffer
        periods += write_data_simple(PREBUFFER / PERIOD_SIZE, NCHANNELS, &info);
        sleep(1);
        // check that old frames are being dropped
        nframes_t expected = (PREBUFFER / PERIOD_SIZE) + ((PREBUFFER % PERIOD_SIZE) ? 1 : 0);
        assert(p->write_space(PERIOD_SIZE) == write_space - expected * NCHANNELS);

        start_recording(info.time);
        trig_on_time = info.time;

        // now write some additional periods
        write_data_simple(PREBUFFER / PERIOD_SIZE, NCHANNELS, &info);
        sleep(1);
        assert(p->write_space(PERIOD_SIZE) == write_space);

        // stop the recorder (give it a chance to catch up)
        p->close_entry(info.time + 100);
        trig_off_time = info.time + 100;
        expected = (POSTBUFFER / PERIOD_SIZE) + ((POSTBUFFER % PERIOD_SIZE) ? 1 : 0);
        write_data_simple(expected, NCHANNELS, &info);
        sleep(1);
        assert(p->write_space(PERIOD_SIZE) == write_space);

        p->_writer->close_entry();

        printf("\npretrig=%d; posttrig=%d; trig_on=%u; trig_off=%u; expected=%d\n",
               PREBUFFER, POSTBUFFER, trig_on_time, trig_off_time,
               trig_off_time - trig_on_time + PREBUFFER + POSTBUFFER);
}

}}

int
main(int argc, char **argv)
{
        if (argc > 1) {
                buffer_size = atoi(argv[1]);
        }
        setup();
        printf("Buffer size is %u\n", buffer_size);

        printf("Testing dummy writer\n");
        writer.reset(new file::null_writer);
        writer->log() << "a random log message";

        printf("Testing continuous writer thread\n");
        thread.reset(new dsp::buffered_data_writer(writer, buffer_size));
        write_data_rate(boost::posix_time::seconds(5));

        printf("Testing arf writer, no compression\n");
        map<string,string> attrs = boost::assign::map_list_of("experimenter","Dan Meliza")
                ("experiment","write uncompressed");
        writer.reset(new file::arf_writer(CLIENT_NAME, "test.arf", attrs, 0));
        writer->log() << "a random log message";

        thread.reset(new dsp::buffered_data_writer(writer, buffer_size));
        write_data_rate(boost::posix_time::seconds(5));

        dsp::triggered_data_writer_test t(new dsp::triggered_data_writer(writer, 0, PREBUFFER, POSTBUFFER));
        t.test(0);
        t.test(-2 * PREBUFFER);

        printf("Testing arf writer, compression=1\n");
        attrs = boost::assign::map_list_of("experimenter","Dan Meliza")
                ("experiment","write compressed");
        writer.reset(new file::arf_writer(CLIENT_NAME, "test.arf", attrs, 1));
        writer->log() << "a random log message";

        thread.reset(new dsp::buffered_data_writer(writer, buffer_size));
        write_data_rate(boost::posix_time::seconds(5));

        printf("passed tests\n");

        return 0;
}
