#include <cstdlib>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cassert>

#include "jill/types.hh"
#include "jill/util/stimset.hh"

using namespace jill;

#define GAP 48000U

void
test_stimqueue()
{
        util::stimqueue sq = {};

        // test starting conditions - last start and stop at 0
        assert(sq.offset(15000, 0, GAP) == (GAP - 15000));
        assert(sq.offset(15000, GAP, GAP - 1000) == (GAP - 15000));
        assert(sq.offset(3951616U, GAP, 0) == 0);
        assert(sq.offset(3951616U, 0, GAP) == 0);
        assert(sq.offset(4294967295U, 0, GAP) == 0);

        sq.last_start = 3951616U;
        sq.last_stop =  4951616U;
        assert(sq.offset(4951616U, 0, GAP) == GAP);
        assert(sq.offset(4951616U, GAP, 0) == 0);
        assert(sq.offset(4951616U + 10000U, GAP, 15000U) == 5000);

        // time has overflowed
        sq.last_start = -1;     // 4294967295U;
        sq.last_stop  = sq.last_start - 1000U;
        assert(sq.last_start == 4294967295U);

        assert(sq.offset(0, 0, GAP) == (GAP - 1001));
        assert(sq.offset(1000, GAP, 0) == (GAP - 1001));
}

int
main(int argc, char **argv)
{
        test_stimqueue();

        printf("passed all tests\n");
}
