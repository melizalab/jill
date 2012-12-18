
#include "jill/ringbuffer.hh"
#include "jill/midi.hh"

int
main(int argc, char **argv)
{
        using namespace jill;

        event_t evt, evt1;

        evt.set(1,event_t::note_on,0);   // evt.buffer
        void * buf = evt.buffer;
        evt.set(1,event_t::note_off,0);

        assert(evt.buffer == buf);

        evt1 = evt;                 // should copy buffer data

        assert(evt.buffer != evt1.buffer);

        // also should work with ringbuffer
        Ringbuffer<event_t> times(64);
        times.push(evt);
        times.pop(&evt1, 1);

        assert(evt.buffer != evt1.buffer);
        std::cout << "passed all tests" << std::endl;

}
