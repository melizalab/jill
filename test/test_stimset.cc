#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <cassert>

#include <iostream>
#include <vector>
#include <string>

#include <boost/program_options.hpp>

#include "jill/util/readahead_stimqueue.hh"
#include "jill/file/stimfile.hh"

size_t srates[] = {10000, 20000, 40000, 80000, 0};

namespace po = boost::program_options;
using namespace jill;
using namespace std;

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
load_stimset(util::stimqueue & q, int argc, char **argv)
{
        int count = 0;
        for (int i = 1; i < argc; ++i) {
                cout << "adding " << argv[i] << " to queue" << endl;
                int n = (i % 5) + 1;
                file::stimfile *f = new file::stimfile(argv[i]);
                // test_stimfile(*f);
                q.add(f, n);
                count += n;
        }
        return count;
}

void
test_stimqueue(util::stimqueue & q, int count)
{
        jill::stimulus_t const * ptr;
        q.shuffle();
        cout << "expecting " << count << "items in queue" << endl;
        while (count) {
                ptr = q.head();
                if (ptr == 0)
                        sleep(1);
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
        util::readahead_stimqueue queue(30000);
        assert(queue.head() == 0);
        int count = load_stimset(queue, argc, argv);

        test_stimqueue(queue, count);
}
