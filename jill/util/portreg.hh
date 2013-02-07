/*
 * JILL - C++ framework for JACK
 *
 * includes code from klick, Copyright (C) 2007-2009  Dominic Sacre  <dominic.sacre@gmx.de>
 * additions Copyright (C) 2010 C Daniel Meliza <dmeliza@uchicago.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _PORTREG_HH
#define _PORTREG_HH

#include <boost/noncopyable.hpp>
#include <string>
#include <vector>
#include "../types.hh"

namespace jill {

namespace util {

/**
 * @brief Utility class for registering and maintaining ports
 *
 */
class port_registry : boost::noncopyable {

public:
        port_registry();

        /**
         * Add an already registered port to the list.
         *
         * @param p         port object
         * @param storage   arf data type; used to determine how the data are
         *                  stored/processed. If UNDEFINED and the port is MIDI,
         *                  actual type will be EVENT. If an event type, and the
         *                  port type is AUDIO, will be set to UNDEFINED
         */
        void add(jack_port_t const * p, arf::DataType storage=arf::UNDEFINED);

        /**
         * Register a port based on a source port. The created port matches the
         * type of the source port.
         *
         * @param client   the client to register the port on
         * @param name     the name of the target port. If undefined, named sequentially.
         * @param src      the name of the source port. if invalid, a warning is issued
         * @param storage  the data type of the port.
         */
        void add(jack_client * client,
                 std::string const & name, std::string const & src, arf::DataType storage=arf::UNDEFINED);
        void add(jack_client * client,
                 std::string const & src, arf::DataType storage=arf::UNDEFINED);

        /** Add a sequence of ports to the list */
        template <typename Iterator>
        void add(jack_client * client,
                 Iterator const & begin, Iterator const & end, arf::DataType storage=arf::UNDEFINED) {
                for (Iterator it = begin; it != end; ++it) {
                        add(client, *it, storage);
                }
        }

        /**
         * Register ports listed in a file. The file should be
         * whitespace-delimited, with three columns corresponding to the name of
         * the name of the port, the name of the source port, and the datatype
         * of the source port
         */
        void add_from_file(jack_client * client, std::istream & is);

        /**
         * Connect registered ports to their sources. If the port is already
         * connected, it is ignored.
         */
        void connect_all(jack_client * client);

        std::size_t size() const { return _ports.size(); }
        std::vector<port_info_t> const * ports() const { return &_ports; }

protected:
        std::vector<port_info_t> _ports;

private:
        int _name_index;

};


}} //namespace jill

#endif
