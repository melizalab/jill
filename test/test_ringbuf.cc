#include <cstdlib>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cassert>

#include "jill/util/mirrored_memory.hh"
#include "jill/dsp/ringbuffer.hh"
#include "jill/dsp/period_ringbuffer.hh"

#define BUFSIZE 4096
unsigned short seed[3] = { 0 };

void
test_mmemory()
{
        printf("Testing mirrored memory\n");
        char buf[BUFSIZE];
        std::size_t i;
        for (i = 0; i < BUFSIZE; ++i) {
                buf[i] = nrand48(seed);
        }

        jill::util::mirrored_memory m(BUFSIZE);
        assert( m.size() == BUFSIZE);
        memcpy(m.buffer(), buf, BUFSIZE);
        assert(memcmp(m.buffer(), m.buffer() + m.size(), m.size()) == 0);
}

template <typename T>
void
test_ringbuffer(std::size_t chunksize, std::size_t reps)
{
        printf("Testing ringbuffer: chunksize=%zu, reps=%zu\n", chunksize, reps);
        T buf[BUFSIZE], fbuf[BUFSIZE];
        std::size_t i;
        for (i = 0; i < BUFSIZE; ++i) {
                buf[i] = nrand48(seed);
        }

        jill::dsp::ringbuffer<T> rb(BUFSIZE);
        //printf("created ringbuffer; size=%zu bytes\n", rb.size());
        //printf("read space = %zu; read offset = %zu; write space = %zu write offset = %zu\n",
        //       rb.read_space(), rb.read_offset(), rb.write_space(), rb.write_offset());

        for (i = 0; i < reps; ++i) {
                rb.push(buf,chunksize);
                //printf("read space = %zu; read offset = %zu; write space = %zu write offset = %zu\n",
                //       rb.read_space(), rb.read_offset(), rb.write_space(), rb.write_offset());
                assert (rb.read_space() == chunksize);
                assert (rb.write_space() == rb.size() - chunksize);

                rb.pop(fbuf,chunksize);
                //printf("read space = %zu; read offset = %zu; write space = %zu write offset = %zu\n",
                //       rb.read_space(), rb.read_offset(), rb.write_space(), rb.write_offset());
                assert (rb.read_space() == 0);
                assert (rb.write_space() == rb.size());
                assert(memcmp(fbuf, buf, chunksize) == 0);
        }
}

void
test_period_ringbuf(std::size_t nchannels)
{
        printf("Testing period ringbuffer nchannels=%zu\n", nchannels);
        jill::dsp::period_ringbuffer rb(BUFSIZE*nchannels*5);
        printf("created ringbuffer; size=%zu bytes\n", rb.size());

        jill::sample_t buf[BUFSIZE];
        std::size_t channels[nchannels];
        std::size_t idx, chan, write_space;

        for (idx = 0; idx < BUFSIZE; ++idx) {
                buf[idx] = nrand48(seed);
        }

        // test initialized state
        assert(rb.peek() == 0);
        assert(rb.peek_ahead() == 0);
        write_space = rb.write_space(BUFSIZE);

        for (chan = 0; chan < nchannels; ++chan) {
                jill::period_info_t info;
                info.time = 0;
                info.nframes = BUFSIZE;
                channels[chan] = chan; // has to be stable
                info.arg = channels+chan;

                assert (rb.write_space(BUFSIZE) == write_space - chan);
                std::size_t stored = rb.push(buf, info);
                assert (stored == BUFSIZE);
        }

        // test read-ahead
        for (chan = 0; chan < nchannels; ++chan) {
                jill::period_info_t const *info;
                info = rb.peek_ahead();

                assert(info != 0);
                assert(info->time == 0);
                assert(info->nframes == BUFSIZE);
                assert(*(std::size_t*)(info->arg) == chan);

                assert(memcmp(buf, info+1, info->bytes()) == 0);
        }

        assert(rb.peek_ahead() == 0);

        for (chan = 0; chan < nchannels; ++chan) {
                jill::period_info_t const *info;
                info = rb.peek();

                assert(info != 0);
                assert(info->time == 0);
                assert(info->nframes == BUFSIZE);
                assert(*(std::size_t*)(info->arg) == chan);
                assert(rb.peek_ahead() == 0);

                assert(memcmp(buf, info+1, info->bytes()) == 0);

                // check that repeated calls to peek return same data
                info = rb.peek();
                assert(info != 0);
                assert(*(std::size_t*)(info->arg) == chan);

                rb.release();
        }
}

int
main(int argc, char **argv)
{
        test_mmemory();
        test_ringbuffer<char>(BUFSIZE/2,3);
        test_ringbuffer<char>(BUFSIZE/3+5,5);
        test_ringbuffer<float>(BUFSIZE/2,2);

        test_period_ringbuf(1);
        test_period_ringbuf(3);

        printf("passed tests\n");
        return 0;
}
