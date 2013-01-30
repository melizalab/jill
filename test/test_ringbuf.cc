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
test_period_ringbuf_popall(std::size_t nchannels)
{
        printf("Testing period ringbuffer (full period read): nchannels=%zu\n", nchannels);
        jill::sample_t buf[BUFSIZE];
        std::size_t i;
        for (i = 0; i < BUFSIZE; ++i) {
                buf[i] = nrand48(seed);
        }
        jill::dsp::period_ringbuffer::period_info_t const *info;
        jill::dsp::period_ringbuffer rb(BUFSIZE*nchannels*5);
        printf("created ringbuffer; size=%zu bytes\n", rb.size());

        assert(rb.reserve(25, BUFSIZE, nchannels) > 0);

        for (i = 0; i < nchannels; ++i) {
                assert(rb.chans_to_write() == nchannels - i);
                rb.push(buf);
                assert(rb.chans_to_write() == nchannels - i - 1);
        }

        info = rb.request();

        assert(info != 0);
        assert(info->nchannels == nchannels);
        assert(info->nbytes == BUFSIZE * sizeof(jill::sample_t));

        void * output = malloc(info->size());
        rb.pop_all(output);

        assert(memcmp(output, info, sizeof(jill::dsp::period_ringbuffer::period_info_t)) == 0);
}


void
test_period_ringbuf(std::size_t nchannels)
{
        printf("Testing period ringbuffer: nchannels=%zu\n", nchannels);
        bool flag = false;
        jill::sample_t buf[BUFSIZE], fbuf[BUFSIZE];
        std::size_t i;
        for (i = 0; i < BUFSIZE; ++i) {
                buf[i] = nrand48(seed);
        }
        jill::dsp::period_ringbuffer::period_info_t const *info;
        jill::dsp::period_ringbuffer rb(BUFSIZE*nchannels*5);
        printf("created ringbuffer; size=%zu bytes\n", rb.size());

        assert(rb.chans_to_write() == 0);
        assert(rb.chans_to_read() == 0);
        assert(rb.request() == 0); // can't see data yet

        // write N-1 periods
        for (int j = 0; j < 4; ++j) {

                try {
                        rb.push(buf);
                }
                catch (std::logic_error) {
                        flag = true;
                }
                assert(flag);
                flag = false;

                assert(rb.reserve(25, BUFSIZE, nchannels) > 0);

                for (i = 0; i < nchannels; ++i) {
                        assert(rb.chans_to_write() == nchannels - i);
                        rb.push(buf);
                        assert(rb.chans_to_write() == nchannels - i - 1);
                }

                try {
                        rb.push(buf);
                }
                catch (std::logic_error) {
                        flag = true;
                }
                assert(flag);
                flag = false;

        }
        // read chunks
        for (int j = 0; j < 4; ++j) {

                try {
                        rb.pop(buf);
                }
                catch (std::logic_error) {
                        flag = true;
                }
                assert(flag);
                flag = false;

                info = rb.request();

                assert(info != 0);
                assert(info->nchannels == nchannels);
                assert(info->time == 25);
                assert(info->nbytes == BUFSIZE * sizeof(jill::sample_t));

                for (i = 0; i < nchannels; ++i) {
                        assert (rb.chans_to_read() == nchannels - i);
                        rb.pop(fbuf);
                        assert(memcmp(fbuf, buf, info->nbytes) == 0);
                        assert (rb.chans_to_read() == nchannels - i - 1);
                }
                assert (rb.chans_to_read() == 0);

                try {
                        rb.pop(buf);
                }
                catch (std::logic_error) {
                        flag = true;
                }
                assert(flag);
                flag = false;
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
        test_period_ringbuf_popall(3);

        printf("passed tests\n");
        return 0;
}
