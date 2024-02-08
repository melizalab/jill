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
#ifndef _OPTIONS_HH
#define _OPTIONS_HH

#include <string>
#include <stdexcept>
#include <vector>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>
#include "version.hh"

/**
 * @defgroup optionsgroup Parse command-line options
 *
 * JILL attempts to provide users with some means of regularizing the
 * somewhat onerous task of processing command-line options.  It uses
 * the boost::program_options library to allow applications to
 * automatically parse JILL-related options while easily adding
 * options of their own.
 *
 * Note that the boost::program_options library is not header-only.
 *
 * @see http://www.boost.org/doc/libs/1_43_0/doc/html/program_options.html
 */

namespace jill {

namespace po = boost::program_options;

/**
 * @ingroup optionsgroup
 * @brief base class for parsing options
 *
 * The program_options class is an ABC for parsing options. It handles the most
 * basic options common to all programs.
 *
 * This class is a thin wrapper around boost/program_options. The
 * options_descriptions members and parsed value map are exposed to allow
 * additional options without a need for subclassing. For example, to add a
 * single option:
 *
 * options.cmd_opts.add_options()("user-name", po:value<string>(), "the user's name");
 * options.parse(argc, argv);
 * string uname = options.get("user_name","");
 *
 * Alternatively, a options can be specified in the constructor of a subclass.
 * Deriving classes may override process_options(), print_version(), and
 * print_usage(). It may be necessary to may override parse() for especially
 * complicated cases.
 */
class program_options {
public:
        /**
         * Initialize the options parser with the program's name.
         *
         * @param program_name        the name of the program
         * @param remote_log_default  whether to turn on remote logging by default
         */
        program_options(std::string program_name, bool remote_log_default=true);
        virtual ~program_options() = default;

        /** Options parsed from the commandline and configfile */
        po::options_description cmd_opts;
        /** Description of options visible in the help (may be a subset of cmd_opts) */
        po::options_description visible_opts;
        /** Options which are processed positionally */
        po::positional_options_description pos_opts;
        /** Values for options are stored here after parsing */
        po::variables_map vmap;

        /**
         * Parse the command line arguments and store options.
         *
         * @param argc Number of arguments
         * @param argv Array of command-line arguments
         */
        virtual void parse(int argc, char **argv);

        /**
         * Retrieve the value of an option from storage. It's
         * recommended to explicitly specify the type, e.g. get<float>("blah")
         *
         * @param name  The name of the option
         */
	template <typename T>
	boost::optional<T> get(std::string const &name) const {
                return (vmap.count(name) == 0) ? boost::none : boost::optional<T>(vmap[name].as<T>());
	}

        template <typename T>
        T get(std::string const &name, T const & default_value) const {
		return get<T>(name).value_or(default_value);
        }

        int count(std::string const & name) const {
                return vmap.count(name);
        }

        /**
         * Assign a parsed value to a variable, if the value is defined.
         *
         * @param ref      reference to variable to recieve new value
         * @param name     the name of the option
         * @return         true if the value was defined
         * @throws boost::bad_any_cast if the value could not be cast appropriately
         */
        template <typename T>
        bool assign(T &ref, std::string const &name) {
                if (vmap.count(name)) {
                        ref = vmap[name].as<T>();
                        return true;
                }
                return false;
        }

        /**
         * Assign key-value pairs to a map. Use a vector of strings, e.g.
         * ("attr,a",     po::value<vector<string> >(), "set additional attribute (key=value)");
         *
         * @param dict    the map to store values in
         * @param name   the name of the option
         * @throws boost::bad_any_cast if values could not be cast to strings
         */
        int parse_keyvals(std::map<std::string, std::string> & dict, std::string const & name);

protected:
        std::string _program_name;

        /**
         * This function is called once the options are parsed; its
         * job is to load data into the data members of the
         * object. Typically this consists of a series of calls to
         * @ref assign.
         */
        virtual void process_options() {};

        /**
         * Print the name and version of the program. Called by
         * parse() when the user specifies the '-v' flag
         */
        virtual void print_version();

        /**
         * Print the usage information. Called by parse() when the
         * user specifies the '-h' flag
         */
        virtual void print_usage();

};


template <> inline
bool program_options::assign<bool>(bool &ref, std::string const &name)
{
        ref = (vmap.count(name) > 0);
        return ref;
}


/**
 * @ingroup miscgroup
 * @brief an exception intended to trigger program termination
 */
class Exit : public std::exception {
public:
        /**
         * Trigger program termination with a specific exit value.
         *
         * @param status    the value to return to the OS
         */
        Exit(int status) : _status(status) { }
        ~Exit() noexcept override = default;

        int status() const noexcept { return _status; }

protected:
        int _status;
};


} // namespace jill

#endif // _OPTIONS_HH
