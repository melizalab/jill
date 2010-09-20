
#include "jill_client.hh"
#include <jack/jack.h>
#include <jack/statistics.h>

using namespace jill;


JillClient::JillClient(const std::string & name)
	: _quit(false)
{
	if ((_client = jack_client_open(name.c_str(), JackNullOption, NULL)) == 0)
		throw AudioError("can't connect to jack server");

	jack_set_xrun_callback(_client, &xrun_callback_, static_cast<void*>(this));
	jack_on_shutdown(_client, &shutdown_callback_, static_cast<void*>(this));
}

JillClient::~JillClient()
{
	jack_client_close(_client);
}


void 
JillClient::set_timebase_callback(const TimebaseCallback &cb)
{
	if (cb) {
		if (jack_set_timebase_callback(_client, 0,
					       &timebase_callback_, static_cast<void*>(this)) != 0) {
			throw AudioError("failed to become jack transport master");
		}
	}
	else {
		if (_timebase_cb) {
			jack_release_timebase(_client);
		}
	}
	_timebase_cb = cb;
}

std::string 
JillClient::client_name() const
{
	return std::string(jack_get_client_name(_client));
}


pthread_t 
JillClient::client_thread() const
{
	return jack_client_thread_id(_client);
}

nframes_t 
JillClient::samplerate() const
{
	return jack_get_sample_rate(_client);
}


bool 
JillClient::transport_rolling() const
{
	return (jack_transport_query(_client, NULL) == JackTransportRolling);
}


position_t 
JillClient::position() const
{
	position_t pos;
	jack_transport_query(_client, &pos);
	return pos;
}


nframes_t 
JillClient::frame() const
{
	return position().frame;
}


bool 
JillClient::set_position(position_t const & pos)
{
	// jack doesn't modify pos, should have been const anyway, i guess...
	return (jack_transport_reposition(_client, const_cast<position_t*>(&pos)) == 0);
}


bool 
JillClient::set_frame(nframes_t frame)
{
	return (jack_transport_locate(_client, frame) == 0);
}


void 
JillClient::timebase_callback_(jack_transport_state_t /*state*/, nframes_t /*nframes*/,
			       position_t *pos, int /*new_pos*/, void *arg)
{
	JillClient *this_ = static_cast<JillClient*>(arg);

	if (this_->_timebase_cb) {
		this_->_timebase_cb(pos);
	}
}


void 
JillClient::shutdown_callback_(void *arg)
{
	JillClient *this_ = static_cast<JillClient*>(arg);
	// this needs to be done async-safe, so I just set a flag and let _run handle it
	this_->_err_msg = "shut down by server";
	this_->_quit = true;
}


int 
JillClient::xrun_callback_(void *arg)
{
	JillClient *this_ = static_cast<JillClient*>(arg);
	if (this_->_xrun_cb)
		return this_->_xrun_cb(jack_get_xrun_delayed_usecs(this_->_client));
	return 0;
}


int
JillClient::_run(unsigned int usec_delay)
{
	for (;;) {
		::usleep(usec_delay);
		if (_quit) {
			if (!_err_msg.empty())
				throw std::runtime_error(_err_msg);
			else
				return 0;
		}
		else if (_mainloop_cb) {
			int ret = _mainloop_cb();
			if(ret!=0) {
				_err_msg = "Main loop terminated";
				return ret;
			}
		}
	}
}


