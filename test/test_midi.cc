
#include "jill/ringbuffer.hh"
#include "jill/midi.hh"

int
main(int argc, char **argv)
{
        using namespace jill;

        event_t evt, evt1, evt2;

        evt.set(1,event_t::note_on,0);   // evt.buffer
        evt1 = evt;                 // should copy buffer data

        evt.set(2,event_t::note_off,0);  // evt.buffer now points elsewhere
        evt2 = evt;                 // should copy new data

        // evt objects in times should point to separate memory locations.

        Ringbuffer<event_t> times(64);

}
