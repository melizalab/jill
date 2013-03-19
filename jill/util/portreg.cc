
#include "portreg.hh"
#include <strings.h>
#include <cstdio>
#include <iostream>
#include <map>
#include <boost/tokenizer.hpp>
#include <boost/assign/list_of.hpp>
#include "../jack_client.hh"

using namespace jill;
using namespace arf;
using jill::util::port_registry;
using std::string;

port_registry::port_registry()
        : _name_index(0)
{}

int
port_registry::add(jack_client * client, string const & src, char const * name)
{
        jack_port_t *p = client->get_port(src);
        if (p==0) {
                client->log(false) << "error registering port " << name
                                   << ": source port " << src << " does not exist" << std::endl;
                return 0;
        }
        if (!(jack_port_flags(p) & JackPortIsOutput)) {
                client->log(false) << "error registering port " << name
                                   << ": source port " << src << " is not an output port" << std::endl;
                return 0;
        }

        if (!name) {
                char buf[16];
                if (strcmp(jack_port_type(p),JACK_DEFAULT_AUDIO_TYPE)==0)
                        sprintf(buf,"pcm_%03d",_name_index);
                else
                        sprintf(buf,"evt_%03d",_name_index);
                _name_index++;
                name = buf;
        }

        client->register_port(name,
                              jack_port_type(p),
                              JackPortIsInput | JackPortIsTerminal, 0);
        _ports[name] = src;
        return 1;
}

void
port_registry::connect_all(jack_client * client)
{
        for (std::map<string,string>::const_iterator it = _ports.begin();
             it != _ports.end();
             ++it) {
                // fails silently if the ports are already connected
                if (!it->second.empty()) client->connect_port(it->second, it->first);
        }
}
