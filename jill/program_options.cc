/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2013 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <iostream>
#include <map>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "logging.hh"
#include "logger.hh"
#include "program_options.hh"

using namespace jill;
using std::string;
using std::vector;
using std::map;

program_options::program_options(std::string program_name, bool remote_log_default)
        :  _program_name(std::move(program_name))
{
        po::options_description generic("General options");
        generic.add_options()
                ("version,v", "print version string")
                ("help,h",    "print help message")
                ("no-remote-log,L",
                 po::bool_switch()->default_value(!remote_log_default),
                 "disable logging to jrecord (will still log to console)")
                ("config,C",  po::value<string>(), "load options from a ini file (overruled by command-line)");
        cmd_opts.add(generic);
        visible_opts.add(generic);
}

void
program_options::print_version()
{
        std::cout << _program_name << " " JILL_VERSION << std::endl;
}


void
program_options::print_usage()
{
        std::cout << "Usage: " << _program_name << " [options] "
                  << visible_opts;
}


void
program_options::parse(int argc, char **argv)
{

        // need two passes, one to find out if we need to read a config file
        po::store(po::command_line_parser(argc, argv).options(cmd_opts).
                  positional(pos_opts).run(), vmap);
        if (vmap.count("help")) {
                print_usage();
                throw Exit(EXIT_SUCCESS);
        }
        else if (vmap.count("version")) {
                print_version();
                throw Exit(EXIT_SUCCESS);
        }
        // set source for logging
        logger::instance().set_sourcename(get<string>("name", _program_name));
        auto server_name = get<string>("server", "default");
        if (!get<bool>("no-remote-log", false)) {
                logger::instance().connect(server_name);
        }
        else
                LOG << "remote logging is disabled";
        LOG << _program_name << ", version " JILL_VERSION;
        LOG << "jackd server: " << server_name;

        boost::filesystem::path configfile = get<string>("config", "");
        if (boost::filesystem::is_regular_file(configfile)) {
                boost::filesystem::ifstream ff(configfile);
                LOG << "[Parsing " << configfile.string() << ']';
                po::parsed_options parsed = po::parse_config_file(ff, cmd_opts, true);
                po::store(parsed, vmap);
        }
        else if (!configfile.empty()) {
                LOG << "ERROR: configuration file " << configfile << " doesn't exist";
                throw Exit(EXIT_FAILURE);
        }

        po::notify(vmap);
        process_options();
}


int
program_options::parse_keyvals(map<string, string> & dict, string const & name)
{
        int i = 0;
        if (vmap.count(name)==0) return i;
        auto const & kv = vmap[name].as<vector<string> >();
        for (auto it = kv.begin(); it != kv.end(); ++it, ++i) {
                string::size_type eq = it->find_last_of("=");
                if (eq == string::npos)
                        throw po::invalid_option_value(" additional option syntax: key=value");
                dict[it->substr(0,eq)] = it->substr(eq+1);
        }
        return i;
}
