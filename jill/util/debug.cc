
#ifndef NDEBUG

#include "../types.hh"
#include <iostream>

namespace jill {

std::ostream & operator<<(std::ostream & os, period_info_t const & p) {
        os << "time=" << p.time << "; frames=" << p.nframes << "; arg=" << p.arg;
        return os;
}

}

#endif
