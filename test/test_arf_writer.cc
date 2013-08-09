#include <iostream>
#include <cassert>
#include <map>
#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/assign/list_of.hpp>

#include "jill/data_writer.hh"
#include "jill/file/arf_writer.hh"

using namespace std;
using namespace jill;


boost::shared_ptr<data_writer> writer;

void
test_entry()
{
        int nperiods = 10;
        nframes_t start = -3000; // test overflow
        nframes_t nframes = 1024;
        char const * pattern = "pcm_%03d";

        void * buf = malloc(sizeof(data_block_t) + 7 + nframes * sizeof(sample_t));
        data_block_t * period = reinterpret_cast<data_block_t*>(buf);

        period->time = start;
        period->dtype = SAMPLED;
        period->sz_id = 7;
        period->sz_data = nframes * sizeof(sample_t);

        assert(!writer->ready());
        writer->new_entry(start);
        assert(writer->ready());

        for (int i = 0; i < nperiods; ++i) {
                for (int j = 0; j < 2; ++j ) {
                        // set name
                        sprintf((char *)(period + 1), pattern, j);
                        writer->write(period, 0, 0);
                }
                period->time += nframes;
        }

        writer->close_entry();
        assert(!writer->ready());
        free(buf);
}

int
main(int argc, char** argv)
{
        map<string,string> attrs = boost::assign::map_list_of("experimenter","Dan Meliza")
                ("experiment","write stuff");

        writer.reset(new file::arf_writer("test_arf_writer","test.arf", attrs, 0));
        // writer->log() << "a log message";
        test_entry();
}
