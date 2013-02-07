
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

void
port_registry::add(jack_client * client, string const & name, string const & src, DataType storage)
{
        port_info_t pt = {name, src, storage};
        jack_port_t *p = client->get_port(src);
        if (p==0) {
                client->log(false) << "error registering port " << name
                                   << ": source port " << src << " does not exist" << std::endl;
                return;
        }
        if (!(jack_port_flags(p) & JackPortIsOutput)) {
                client->log(false) << "error registering port " << name
                                   << ": source port " << src << " is not an output port" << std::endl;
                return;
        }

        client->register_port(name,
                              jack_port_type(p),
                              JackPortIsInput | JackPortIsTerminal, 0);
        // try to guess storage type
        if (pt.storage == UNDEFINED && strcmp(jack_port_type(p),JACK_DEFAULT_MIDI_TYPE)==0) {
                pt.storage = EVENT;
        }
        else if (pt.storage >= EVENT && strcmp(jack_port_type(p),JACK_DEFAULT_AUDIO_TYPE)==0) {
                pt.storage = UNDEFINED;
        }
        _ports.push_back(pt);
}

void
port_registry::add(jack_port_t const * p, DataType storage)
{
        port_info_t pt = {jack_port_name(p), "", storage};
        _ports.push_back(pt);
}


void
port_registry::add(jack_client * client, string const & src, DataType storage)
{
        char buf[16];
        sprintf(buf,"in_%03d",_name_index);
        add(client,
            buf,
            src,
            storage);
        _name_index += 1;
}

/** case-insensitive comparison */
struct ci_less
{
        bool operator() (const string & s1, const string & s2) const
                {
                        return strcasecmp(s1.c_str(), s2.c_str()) < 0;
                }
};

/** map strings to data type enum */
static
std::map<string,DataType,ci_less> datatypes = boost::assign::map_list_of("UNDEFINED",UNDEFINED)
	("ACOUSTIC",ACOUSTIC)
	("EXTRAC_HP",EXTRAC_HP)
	("EXTRAC_LF",EXTRAC_LF)
	("EXTRAC_EEG",EXTRAC_EEG)
	("INTRAC_CC",INTRAC_CC)
	("INTRAC_VC",INTRAC_VC)
        ("EVENT",EVENT)
	("SPIKET",SPIKET)
	("BEHAVET",BEHAVET)
        ("INTERVAL",INTERVAL)
	("STIMI",STIMI)
	("COMPONENTL",COMPONENTL);


void
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

        while (getline(is, line)) {
                tokenizer<boost::char_separator<char> > tok(line, sep);
                vector<string> fields(tok.begin(), tok.end());
                if (fields.size() < 3) continue;
                DataType storage = (datatypes.count(fields[2])) ?
                        datatypes[fields[2]] : UNDEFINED;
                add(client, fields[0], fields[1], storage);
        }

}

void
port_registry::connect_all(jack_client * client)
{
        for (std::vector<port_info_t>::const_iterator it = _ports.begin();
             it != _ports.end();
             ++it) {
                // fails silently if the ports are already connected
                if (!it->source.empty()) client->connect_port(it->source, it->name);
        }
}
