#include "audio_interface_offline.hh"
using namespace jill;

/* ----------------------- AudioInterfaceOffline ----------------------- */
AudioInterfaceOffline::AudioInterfaceOffline(nframes_t blocksize, nframes_t samplerate)
	: _blocksize(blocksize), _samplerate(samplerate), _quit(false),
	  _bufin(new sample_t[blocksize]), _bufout(new sample_t[blocksize])
{}


AudioInterfaceOffline::~AudioInterfaceOffline() {}


void 
AudioInterfaceOffline::set_input(const std::string &sndfile, nframes_t startframe)
{
	_sfin.open(sndfile.c_str());
	_sfin.seek(startframe,0);
	_samplerate = _sfin.samplerate();
	_framecount = 0;
}

void 
AudioInterfaceOffline::set_output(const std::string &sndfile)
{
	_sfout.open(sndfile.c_str(), _samplerate);
}


void 
AudioInterfaceOffline::process()
{
	nframes_t nf = _blocksize;
	if (_sfin) {
		nf = _sfin.read(_bufin.get(), _blocksize);
		if (nf == 0) {
			_quit = true;
			return;
		}
	}
	_framecount += nf;
	_process_cb(_bufin.get(), _bufout.get(), nf, _framecount);
	if (_sfout)
		_sfout.write(_bufout.get(), nf);
}
