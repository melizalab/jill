#include <cstdlib>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cassert>

#include <vector>
#include <string>

#include <boost/program_options.hpp>

#include "jill/util/stimset.hh"
#include "jill/file/stimfile.hh"

size_t srates[] = {10000, 20000, 40000, 80000, 0};

namespace po = boost::program_options;
using namespace jill::file;
using namespace std;

/* test loading and resampling */
void
test_stimfile(stimfile & f)
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

void
test_stimset(int argc, char **argv)
{
        jill::util::stimset sset(48000);
        int count = 0;
        for (int i = 1; i < argc; ++i) {
                int n = (i % 5) + 1;
                sset.add(new stimfile(argv[i]), n);
                count += n;
        }
        std::vector<jill::stimulus_t *>::const_iterator it = sset.begin(); // shuffles
        for (;it != sset.end(); ++it) {
                jill::stimulus_t const *fp = *it;
                printf("%s\n", fp->path().c_str());
                // assert(fp->buffer() != 0);
                count -= 1;
        }
        assert (count == 0);
}


int main(int argc, char **argv)
{
        for (int i = 1; i < argc; ++i) {
                printf("Opening %s\n", argv[i]);
                stimfile sf(argv[i]);
                test_stimfile(sf);
        }
        test_stimset(argc, argv);
}
