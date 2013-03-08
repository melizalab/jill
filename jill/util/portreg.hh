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
#include <map>
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
         * Register a port based on a source port. The created port matches the
         * type of the source port.
         *
         * @param client   the client to register the port on
         * @param name     the name of the target port. If undefined, named sequentially.
         * @param src      the name of the source port. if invalid, a warning is issued
         */
        int add(jack_client * client, std::string const & src, std::string const & name);
        int add(jack_client * client, std::string const & src);

        /** Add a sequence of ports to the list */
        template <typename Iterator>
        int add(jack_client * client, Iterator const & begin, Iterator const & end) {
                int i = 0;
                for (Iterator it = begin; it != end; ++it) {
                        i += add(client, *it);
                }
                return i;
        }

        /**
         * Register ports listed in a file. The file should be
         * whitespace-delimited, with two columns corresponding to the name of
         * name of the source port and the name of the target port.
         */
        int add_from_file(jack_client * client, std::istream & is);

        /**
         * Connect registered ports to their sources. If the port is already
         * connected, it is ignored.
         */
        void connect_all(jack_client * client);

private:
        // target, source to allow multiple sources connected to different ports
        std::map<std::string,std::string> _ports;
        int _name_index;

};


}} //namespace jill

#endif
