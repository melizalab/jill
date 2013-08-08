#include <cstdlib>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cassert>

#include "jill/util/mirrored_memory.hh"
#include "jill/dsp/ringbuffer.hh"
#include "jill/dsp/block_ringbuffer.hh"

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
        using namespace jill::dsp;
        jill::sample_t buf[BUFSIZE];
        char chan_name[32];
        std::size_t idx, chan, write_space, data_bytes;
        data_bytes = BUFSIZE * sizeof(jill::sample_t);

        printf("Testing period ringbuffer nchannels=%zu\n", nchannels);
        block_ringbuffer rb(data_bytes * nchannels * 5);
        printf("created ringbuffer; size=%zu bytes\n", rb.size());


        for (idx = 0; idx < BUFSIZE; ++idx) {
                buf[idx] = nrand48(seed);
        }

        // test initialized state
        assert(rb.peek() == 0);
        assert(rb.peek_ahead() == 0);
        write_space = rb.write_space();

        for (chan = 0; chan < nchannels; ++chan) {
                sprintf(chan_name, "chan_%03d", chan);
                std::size_t bytes = rb.push(0, jill::SAMPLED, chan_name, data_bytes, buf);
                write_space -= bytes;
                assert (rb.write_space() == write_space);
        }

        // test read-ahead
        for (chan = 0; chan < nchannels; ++chan) {
                sprintf(chan_name, "chan_%03d", chan);
                jill::data_block_t const *info;
                info = rb.peek_ahead();

                assert(info != 0);
                assert(info->time == 0);
                assert(info->sz_data == data_bytes);
                assert(info->id() == chan_name);
                assert(memcmp(buf, info->data(), info->sz_data) == 0);
        }
        assert(rb.peek_ahead() == 0);

        for (chan = 0; chan < nchannels; ++chan) {
                sprintf(chan_name, "chan_%03d", chan);
                jill::data_block_t const *info;
                info = rb.peek();

                assert(info != 0);
                assert(info->time == 0);
                assert(info->sz_data == data_bytes);
                assert(info->id() == chan_name);
                assert(memcmp(buf, info->data(), info->sz_data) == 0);
                assert(rb.peek_ahead() == 0);

                // check that repeated calls to peek return same data
                info = rb.peek();
                assert(info != 0);
                assert(info->id() == chan_name);

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
