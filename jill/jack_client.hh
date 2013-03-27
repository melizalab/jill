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
#ifndef _JACK_CLIENT_HH
#define _JACK_CLIENT_HH

#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <string>
#include <list>
#include <jack/jack.h>
#include "data_source.hh"
#include "event_logger.hh"

/**
 * @defgroup clientgroup Creating and controlling JACK clients
 *
 */

namespace jill {

/**
 * @ingroup clientgroup
 * @brief Thin wrapper around JACK client
 *
 * This class handles the most basic aspects of JACK client manipulation,
 * including port creation and connection, and inspecting common attributes.
 *
 * It provides a boost::function based interface for many of the callbacks,
 * which is somewhat more convenient than a raw function wrapper. The callback
 * function has access to the object through a pointer
 *
 * The wrapper is thin, and the jack_client_t pointer is available in the _client
 * field.  Encapsulation will break if ports are registered or unregistered
 * using this pointer, or if the process callback is changed.
 *
 *
 */
class jack_client : public data_source {

public:

	/**
	 * Type of the process callback. Provides a pointer to a client object
         * and information about buffer size and the current time.
	 *
	 * @param client the current client object
	 * @param size the number of samples in the buffers
	 * @param time the time elapsed (in samples) since the client started
         * @return 0 if no errors, non-zero on error
	 */
	typedef boost::function<int (jack_client* client, nframes_t size, nframes_t time)> ProcessCallback;

        /**
         * Type of the port (un)registration callback. Provides information about
         * the port that was (un)registered. Only ports owned by the current
         * client result in this callback being called.
         *
         * @param client the current client object
         * @param port the id of the registered port
         * @param registered 0 for unregistered, nonzero if registered
         */
        typedef boost::function<void (jack_client* client, jack_port_t *port,
                                      int registered)> PortRegisterCallback;

        /**
         * Type of the port (dis)connection callback. Provides information about
         * the ports that were (dis)connected. Only ports owned by the current
         * client result in this callback being called.
         *
         * @param client the current client object
         * @param port1 the port owned by this client
         * @param port2 the port owned by another client (or the second port if self-connected)
         * @param connected 0 for unregistered, nonzero if registered
         */
        typedef boost::function<void (jack_client* client, jack_port_t *port1, jack_port_t *port2,
                                      int connected)> PortConnectCallback;

        typedef boost::function<int (jack_client* client, nframes_t srate)> SamplingRateCallback;
        typedef boost::function<int (jack_client* client, nframes_t nframes)> BufferSizeCallback;
        typedef boost::function<int (jack_client* client, float usec_delay)> XrunCallback;
        typedef boost::function<void (jack_status_t code, char const * reason)> ShutdownCallback;

        typedef std::list<jack_port_t*> port_list_type;

	/**
	 * Initialize a new JACK client. All clients are identified to the JACK
	 * server by an alphanumeric name, which is specified here. Creates the
	 * client and connects it to the server.
	 *
	 * @param name   the name of the client as represented to the server.
	 *               May be changed by the server if not unique.
         * @param logger reference to object used for logging messages
	 */
	jack_client(std::string const & name, event_logger & logger);
	~jack_client();

        /**
         * @brief Register a new port for the client.
         *
         * @param name  the (short) name of the port
         * @param type  the type of the port. common values include
         *              JACK_DEFAULT_AUDIO_TYPE and JACK_DEFAULT_MIDI_TYPE
         * @param flags flags for the port
         * @param buffer_size  the size of the buffer, or 0 for the default
         */
        jack_port_t* register_port(std::string const & name, std::string const & type,
                                   unsigned long flags, unsigned long buffer_size=0);

        void unregister_port(std::string const & name);
        void unregister_port(jack_port_t *port);

	/**
	 * Set the process callback. This can be a raw function
	 * pointer or any function object that matches the signature
	 * of @a ProcessCallback. The argument is copied; if this is
	 * undesirable use a boost::ref.
	 *
	 * @param cb The function object that will process the audio
	 * data [type @a ProcessCallback]
	 *
	 */
	void set_process_callback(ProcessCallback const & cb);

        /**
         * Set the callback for when the samplerate changes. Currently, this
         * will be called only when the callback is set or changed.
         */
	void set_sample_rate_callback(SamplingRateCallback const & cb);

	void set_port_registration_callback(PortRegisterCallback const & cb);
	void set_port_connect_callback(PortConnectCallback const & cb);
	void set_buffer_size_callback(BufferSizeCallback const & cb);
	void set_xrun_callback(XrunCallback const & cb);
        void set_shutdown_callback(ShutdownCallback const & cb);

        /** Activate the client. Do this before attempting to connect ports */
        void activate();

        /** Deactivate the client. Disconnects all ports */
        void deactivate();


	/**
	 * Connect one the client's ports to another port. The long name (e.g.
	 * client:in) is looked up first, and if this fails then it attempts to
	 * find it on the local client. Fails silently if the ports are already
	 * connected.
	 *
	 * @param src   The name of the source port (client:port or port).
	 * @param dest  The name of the destination port.
	 *
	 */
	void connect_port(std::string const & src, std::string const & dest);

        /** Connect a sequence of ports to a destination */
        template <typename It>
        void connect_ports(It const & begin, It const & end, std::string const & dest) {
                for (It it = begin; it != end; ++it)
                        connect_port(*it, dest);
        }

        /** Connect a sequence of ports to a source */
        template <typename It>
        void connect_ports(std::string const & src, It const & begin, It const & end) {
                for (It it = begin; it != end; ++it)
                        connect_port(src, *it);
        }

	/**
	 * Disconnect the client from all its ports.
	 */
	void disconnect_all();

        /** Get sample or event buffer for ports */
        sample_t * samples(std::string const & name, nframes_t nframes);
        sample_t * samples(jack_port_t *port, nframes_t nframes);
        void * events(jack_port_t *port, nframes_t);

	/* -- Inspect state of the client or server -- */

        /** The JACK client object */
        jack_client_t * client() { return _client;}

        /** List of ports registered through this object. Realtime safe. */
        port_list_type const & ports() const { return _ports;}
        std::size_t nports() const { return _nports; }

        /**
         * Look up a jack port by name. The port doesn't have to be owned by the
         * client. May not be RT safe.
         *
         * Returns 0 if the port doesn't exist
         */
        jack_port_t* get_port(std::string const & name) const;

        /** The size of the client's buffer */
        nframes_t buffer_size() const;

	/**  JACK client name */
	std::string name() const;

        /* implementations of data_source functions */
	nframes_t sampling_rate() const;
	nframes_t frame() const;
        nframes_t frame(utime_t) const;
        utime_t time(nframes_t) const;
        utime_t time() const;

protected:
        /** Ports owned by this client */
        port_list_type _ports;
        std::size_t _nports;

        /** log a message */
        std::ostream & log() { return _log.log(); }

private:
	jack_client_t * _client; // pointer to jack client
        event_logger & _log;     // logging facility

	ProcessCallback _process_cb;
        PortRegisterCallback _portreg_cb;
        PortConnectCallback _portconn_cb;
        SamplingRateCallback _sampling_rate_cb;
        BufferSizeCallback _buffer_size_cb;
        XrunCallback _xrun_cb;
        ShutdownCallback _shutdown_cb;


	/*
	 * These are the callback functions that are actually
	 * registered with the JACK server. The registration functions
	 * take function pointers, not member function pointers, so
	 * these functions have to be static.  A pointer to the object
	 * is passed as a void pointer, and used to access the
	 * callback functions registered by the user.
	 */
	static int process_callback_(nframes_t, void *);
	static void portreg_callback_(jack_port_id_t, int, void *);
	static void portconn_callback_(jack_port_id_t, jack_port_id_t, int, void *);
	static int sampling_rate_callback_(nframes_t, void *);
	static int buffer_size_callback_(nframes_t, void *);
	static int xrun_callback_(void *);
        static void shutdown_callback_(jack_status_t, char const *, void *);

};


} //namespace jill

#endif
