#include <iostream>
#include <cassert>
#include <map>
#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "jill/data_writer.hh"
#include "jill/data_source.hh"
#include "jill/file/arf_writer.hh"

using namespace std;
using namespace jill;
using namespace boost::posix_time;

boost::shared_ptr<data_writer> writer;

class null_source : public data_source {

public:
        null_source(std::string const & name, nframes_t sampling_rate)
                : _name(name), _sampling_rate(sampling_rate), _base_time(microsec_clock::universal_time())
                {}

        /** the name of the data source. */
        char const * name() const {
                return _name.c_str();
        }

	/** The sample rate of the data */
	nframes_t sampling_rate() const {
                return _sampling_rate;
        }

	/** The current frame in the data stream (since client start) */
	nframes_t frame() const {
                return frame(time());
        }

        /** Convert microsecond time to frame count */
        nframes_t frame(utime_t t) const {
                return t / (1000000 / _sampling_rate);
        }

        /** Convert frame count to microseconds */
        utime_t time(nframes_t t) const {
                return t * (1000000 / _sampling_rate);
        }

        /** Get current time in microseconds */
        utime_t time() const {
                time_duration ts = microsec_clock::universal_time() - _base_time;
                return ts.total_microseconds();
        }

private:
        std::string _name;
        nframes_t _sampling_rate;
        ptime _base_time;

};

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
        *(sample_t *)(buf + sizeof(data_block_t) + period->sz_id) = 134.;

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

        null_source source("test", 20000);
        writer.reset(new file::arf_writer("test.arf", source, attrs, 0));
        writer->log(microsec_clock::universal_time(), "test", "a log message");
        test_entry();
}
