#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <cassert>
#include <math.h>

#include <iostream>
#include <boost/ptr_container/ptr_vector.hpp>
#include <vector>
#include <string>

#include "jill/util/readahead_stimqueue.hh"
#include "jill/file/stimfile.hh"

size_t srates[] = {10000, 20000, 40000, 80000, 0};

using namespace jill;
using namespace std;

boost::ptr_vector<stimulus_t> _stimuli;
std::vector<stimulus_t *> _stimlist;

/* test loading and resampling */
void
test_stimfile(file::stimfile & f)
{
        // assumes samples haven't been loaded
        size_t samplerate = f.samplerate();
        size_t nframes = f.nframes();
        float duration = f.duration();

        assert(f.buffer() == 0);
        f.load_samples();
        assert(f.buffer() != 0);
        assert(f.nframes() == nframes);
        assert(f.samplerate() == samplerate);

        for (size_t *sr = srates; *sr; ++sr) {
                f.load_samples(*sr);
                assert(f.buffer() != 0);
                assert(fabs(f.duration() - duration) < (1.0 / (float)fmin(*sr,samplerate)));
                assert(f.samplerate() == *sr );
        }
}

int
load_stimset(int argc, char **argv)
{
        int count = 0;
        for (int i = 1; i < argc; ++i) {
                int n = (i % 5) + 1;
                file::stimfile *f = new file::stimfile(argv[i]);
                _stimuli.push_back(f);
                for (int j = 0; j < n; ++j)
                        _stimlist.push_back(f);
                count += n;
        }
        return count;
}

void
test_stimqueue(util::stimqueue & q, int count)
{
        jill::stimulus_t const * ptr;
        cout << "expecting " << count << " items in queue" << endl;
        while (count) {
                ptr = q.head();
                if (ptr == 0)
                        usleep(1000);
                else {
                        cout << ptr->name() << ": " << ptr->nframes() << " @" << ptr->samplerate() << endl;
                        q.release();
                        count -= 1;
                }
        }
        assert(q.head() == 0);
}

int main(int argc, char **argv)
{
        int count = load_stimset(argc, argv);
        std::random_shuffle(_stimlist.begin(), _stimlist.end());
        util::readahead_stimqueue queue(_stimlist.begin(), _stimlist.end(), 30000);

        test_stimqueue(queue, count);
        queue.join();
}
