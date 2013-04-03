#include <iostream>
#include <cassert>


#include "jill/dsp/crossing_trigger.hh"

using namespace std;
using namespace jill;

void test_counter(size_t size)
{
        int count = 0;
        dsp::running_counter<int> counter(size);
        for (size_t i = 0; i < size; ++i) {
                assert(!counter.full());
                count += i;
                counter.push(i);
                assert(counter.running_count() == count);
        }
        // now the counter is full
        for (size_t i = 0; i < size; ++i) {
                assert(counter.full());
                counter.push(i);
                assert(counter.running_count() == count);
        }
        counter.reset();
        assert(!counter.full());
}

void test_crossing_counter(size_t thresh, size_t period_size, size_t period_count, size_t nblocks)
{
        dsp::crossing_counter<float> counter(thresh, period_size, period_count);
        assert(counter.count() == 0);
        assert(counter.thresh() == thresh);

        for (size_t block = 0; block < nblocks; ++block) {
                // generate a chunk of data
                // push it to counter
                // verify return value
        }
}

int main(int, char**)
{

        test_counter(10);

}


