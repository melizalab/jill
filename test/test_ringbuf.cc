#include <cstdlib>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cassert>

#include "jill/util/mirrored_memory.hh"
#include "jill/dsp/ringbuffer.hh"

#define BUFSIZE 4096
unsigned short seed[3] = { 0 };

void
test_mmemory()
{
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
        T buf[BUFSIZE], fbuf[BUFSIZE];
        std::size_t i;
        for (i = 0; i < BUFSIZE; ++i) {
                buf[i] = nrand48(seed);
        }

        jill::dsp::ringbuffer<T> rb(BUFSIZE);
        printf("created ringbuffer; size=%d bytes\n", rb.size());
        printf("read space = %d; read offset = %d; write space = %d write offset = %d\n",
               rb.read_space(), rb.read_offset(), rb.write_space(), rb.write_offset());

        for (i = 0; i < reps; ++i) {
                rb.push(buf,chunksize);
                printf("read space = %d; read offset = %d; write space = %d write offset = %d\n",
                       rb.read_space(), rb.read_offset(), rb.write_space(), rb.write_offset());
                assert (rb.read_space() == chunksize);
                assert (rb.write_space() == rb.size() - chunksize);

                rb.pop(fbuf,chunksize);
                printf("read space = %d; read offset = %d; write space = %d write offset = %d\n",
                       rb.read_space(), rb.read_offset(), rb.write_space(), rb.write_offset());
                assert (rb.read_space() == 0);
                assert (rb.write_space() == rb.size());
                assert(memcmp(fbuf, buf, chunksize) == 0);
        }
}


int
main(int argc, char **argv)
{
        test_mmemory();
        test_ringbuffer<char>(BUFSIZE/2,3);
        test_ringbuffer<char>(BUFSIZE/3+5,5);
        test_ringbuffer<float>(BUFSIZE/2,2);

        printf("passed tests\n");
        return 0;
}
