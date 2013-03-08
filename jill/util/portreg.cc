
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
port_registry::add(jack_client * client, string const & src, string const & name)
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

        client->register_port(name,
                              jack_port_type(p),
                              JackPortIsInput | JackPortIsTerminal, 0);
        _ports[name] = src;
        return 1;
}


int
port_registry::add(jack_client * client, string const & src)
{
        char buf[16];
        sprintf(buf,"in_%03d",_name_index);
        int rv = add(client, src, buf);
        _name_index += rv;
        return rv;
}


int
port_registry::add_from_file(jack_client * client, std::istream & is)
{
        using namespace std;
        using namespace boost;
        boost::char_separator<char> sep(" \t\n");
        // using namespace boost::filesystem;
        // path portfile(path);
	// if (!is_regular_file(configfile)) return;
        // ifstream ff(portfile);
        string line;
        // ifstream ff(path.c_str(), ifstream::in);

        int i = 0;
        while (getline(is, line)) {
                tokenizer<boost::char_separator<char> > tok(line, sep);
                vector<string> fields(tok.begin(), tok.end());
                if (fields.size() < 2) continue;
                i += add(client, fields[0], fields[1]);
        }
        return i;
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
