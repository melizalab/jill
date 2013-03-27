#ifndef _LOGGER_PROXY_HH
#define _LOGGER_PROXY_HH

#include <iosfwd>                          // streamsize
#include <boost/iostreams/categories.hpp>  // sink_tag
#include "event_logger.hh"

namespace jill {

        struct logger_proxy : public boost::iostreams::sink {
                logger_proxy(event_logger &w) : _w(w) {}
                std::streamsize write(char const * s, std::streamsize n) {
                        return _w.log(s, n);
                }
        private:
                event_logger &_w;
        };
}


#endif
