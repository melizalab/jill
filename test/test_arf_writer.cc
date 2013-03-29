#include <iostream>
#include <cassert>
#include <map>
#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/assign/list_of.hpp>

#include "jill/data_writer.hh"
// #include "jill/file/arf_writer.hh"
#include "jill/util/stream_logger.hh"

using namespace std;
using namespace jill;


boost::shared_ptr<event_logger> logger;
boost::shared_ptr<data_writer> writer;

int
main(int, char**)
{
        logger.reset(new util::stream_logger("test_arf_writer", cout));
        logger->log() << "a test log message";

        // map<string,string> attrs = boost::assign::map_list_of("experimenter","Dan Meliza")
        //         ("experiment","write stuff");


        // writer.reset(new file::arf_writer("test_arf_writer","test.arf", attrs, 0));

        // writer->log() << "a log message" << std::endl;
        // assert(!writer->ready());

        // void * buf = malloc(sizeof(period_info_t) + 1024 * sizeof(sample_t));
        // period_info_t * period = reinterpret_cast<period_info_t*>(buf);
        // period->time = 0;
        // period->nframes = 1024;
        // period->arg = 0;

        // writer->new_entry(0);
        // assert(writer->ready());

        // for (int i = 0; i < 10; ++i) {
        //         for (int j = 0; j < 2; ++j ) {
        //                 nframes_t n = writer->write(period);
        //                 assert(n == period->nframes);
        //         }
        //         if (i > 0)
        //                 assert(writer->aligned());
        //         period->time += period->nframes;
        // }

        // writer->write(period);
        // assert(!writer->aligned());

        // writer->close_entry();
        // assert(!writer->ready());

        // redirector r(*writer);
        // writer->log() << "another log message" << std::endl;
        // assert(r.was_redirected);
}
