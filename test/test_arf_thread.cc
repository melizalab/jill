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
// #include "jill/file/continuous_arf_thread.hh"
// #include "jill/file/triggered_arf_thread.hh"
// #include "jill/util/string.hh"

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
boost::shared_ptr<dsp::buffered_data_writer> thread;

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
        printf("\nrate: %ld periods in %ld ms\n", i, ms);
}


// size_t
// write_data_simple(nframes_t periods, nframes_t channels, period_info_t * info)
// {

//         size_t ret = 0;
//         for (size_t i = 0; i < periods; ++i) {
//                 for (size_t chan = 0; chan < channels; ++chan) {
//                         writer->push(test_data, *info);
//                         ret += 1;
//                 }
//                 info->time += PERIOD_SIZE;
//         }
//         return ret;
// }


// void
// test_write_continuous()
// {
//         map<string,string> attrs = boost::assign::map_list_of("experimenter","Dan Meliza")
//                 ("experiment","write continuous");
//         dsp::multichannel_data_thread *p = new file::continuous_arf_thread("test.arf", attrs, 0, COMPRESSION);
//         nframes_t buffer_size = p->resize_buffer(PERIOD_SIZE, NCHANNELS * 16);
//         writer.reset(p);
//         printf("\nTesting arf writer, buffer size=%d, compression=%d\n", buffer_size, COMPRESSION);
//         writer->start();

//         test_write_log();
//         write_data_rate(boost::posix_time::seconds(5));
//         writer->stop();
//         writer->join();
//         writer.reset();
// }


// void
// test_write_triggered(nframes_t start_time)
// {
//         nframes_t trig_on_time, trig_off_time;
//         size_t periods;
//         period_info_t info = {start_time, PERIOD_SIZE, 0};

//         file::triggered_arf_thread *p;
//         map<string,string> attrs = boost::assign::map_list_of("experimenter","Dan Meliza")
//                 ("experiment","write triggered");
//         p = new file::triggered_arf_thread("test.arf", 0, PREBUFFER, POSTBUFFER, attrs, 0, COMPRESSION);
//         p->resize_buffer(PERIOD_SIZE, NCHANNELS * 16);
//         writer.reset(p);

//         nframes_t write_space = p->write_space(PERIOD_SIZE);
//         p->start();

//         // write enough data to underfill prebuffer
//         periods = write_data_simple(PREBUFFER / PERIOD_SIZE, NCHANNELS, &info);
//         // make sure the thread has not pulled any data off the buffer
//         assert(p->write_space(PERIOD_SIZE) == write_space - periods);

//         // now overfill the prebuffer
//         periods += write_data_simple(PREBUFFER / PERIOD_SIZE, NCHANNELS, &info);
//         sleep(1);
//         // check that old frames are being dropped
//         nframes_t expected = (PREBUFFER / PERIOD_SIZE) + ((PREBUFFER % PERIOD_SIZE) ? 1 : 0);
//         assert(p->write_space(PERIOD_SIZE) == write_space - expected * NCHANNELS);

//         p->start_recording(info.time);
//         trig_on_time = info.time;
//         // at this point only the last period should be in the buffer
//         assert(p->write_space(PERIOD_SIZE) == write_space - NCHANNELS);

//         // now write some additional periods
//         write_data_simple(PREBUFFER / PERIOD_SIZE, NCHANNELS, &info);
//         sleep(1);
//         assert(p->write_space(PERIOD_SIZE) == write_space);

//         // stop the recorder (give it a chance to catch up)
//         p->stop_recording(info.time + 100);
//         trig_off_time = info.time + 100;
//         expected = (POSTBUFFER / PERIOD_SIZE) + ((POSTBUFFER % PERIOD_SIZE) ? 1 : 0);
//         write_data_simple(expected, NCHANNELS, &info);
//         sleep(1);
//         assert(p->write_space(PERIOD_SIZE) == write_space);

//         periods = write_data_simple(PREBUFFER / PERIOD_SIZE, NCHANNELS, &info);

//         p->stop();
//         p->join();
//         printf("\npretrig=%d; posttrig=%d; trig_on=%u; trig_off=%u; expected=%d\n",
//                PREBUFFER, POSTBUFFER, trig_on_time, trig_off_time,
//                trig_off_time - trig_on_time + PREBUFFER + POSTBUFFER);
// }

int
main(int argc, char **argv)
{
        nframes_t buffer_size = 20480;
        if (argc > 1) {
                buffer_size = atoi(argv[1]);
        }
        setup();
        printf("Buffer size is %u\n", buffer_size);

        printf("Testing dummy writer\n");
        writer.reset(new file::null_writer);
        writer->log() << "a random log message" << std::endl;

        printf("Testing continuous writer thread\n");
        thread.reset(new dsp::buffered_data_writer(writer, buffer_size));
        write_data_rate(boost::posix_time::seconds(5));

        printf("Testing arf writer, no compression\n");
        map<string,string> attrs = boost::assign::map_list_of("experimenter","Dan Meliza")
                ("experiment","write compressed");
        writer.reset(new file::arf_writer(CLIENT_NAME, "test.arf", attrs, client, 0));
        writer->log() << "a random log message" << std::endl;

        thread.reset(new dsp::buffered_data_writer(writer, buffer_size));
        write_data_rate(boost::posix_time::seconds(5));

        // test_write_continuous();

        // test_write_triggered(-2 * PREBUFFER);

        printf("passed tests\n");

        return 0;
}
