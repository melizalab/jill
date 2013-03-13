#include <cstdlib>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cassert>

#include <vector>
#include <string>

#include <boost/program_options.hpp>

#include "jill/util/stimset.hh"
#include "jill/util/stimqueue.hh"
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
load_stimset(util::stimset & sset, int argc, char **argv)
{
        int count = 0;
        for (int i = 1; i < argc; ++i) {
                printf("opening %s\n", argv[i]);
                int n = (i % 5) + 1;
                file::stimfile *f = new file::stimfile(argv[i]);
                test_stimfile(*f);
                sset.add(f, n);
                count += n;
        }
        return count;
}

void
test_stimset(util::stimset & sset, int expected_count)
{
        printf("stimset has %d entries\n", expected_count);
        sset.shuffle();
        stimulus_t const *fp = sset.next();
        while (fp->buffer()) {
                expected_count -= 1;
                fp = sset.next();
        }
        assert (expected_count == 0);
}


void
test_stimqueue(util::stimset & sset)
{
        util::stimqueue queue;
        assert (!queue.ready());
        assert (!queue.playing());

        sset.shuffle();
        queue.enqueue(sset.next());
        assert(queue.ready());

        // how to test mutex?

        queue.enqueue(sset.next());
        while (queue.ready()) {

                assert(!queue.playing());
                assert(strlen(queue.name()) > 0);
                printf("%s\n", queue.name());
                assert(queue.nsamples() > 0);

                sample_t const * buf = queue.buffer();
                nframes_t n = queue.nsamples();
                assert(buf);
                queue.advance(10);
                assert(queue.playing());
                assert(queue.nsamples() == n - 10);
                assert(queue.buffer() == buf + 10);

                queue.advance(queue.nsamples());
                assert(queue.nsamples() == 0);

                queue.release();
                queue.enqueue(sset.next());
        }
}

int main(int argc, char **argv)
{
        util::stimset sset(30000);
        int count = load_stimset(sset, argc, argv);
        test_stimset(sset, count);
        test_stimqueue(sset);
}
