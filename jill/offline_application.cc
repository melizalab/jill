
#include "offline_application.hh"
using namespace jill;

/* ----------------------- AudioInterfaceOffline ----------------------- */
AudioInterfaceOffline::AudioInterfaceOffline(nframes_t blocksize, nframes_t samplerate)
	: _blocksize(blocksize), _samplerate(samplerate),
	  _bufin(new sample_t[blocksize]), _bufout(new sample_t[blocksize])
{}


AudioInterfaceOffline::~AudioInterfaceOffline() {}


void 
AudioInterfaceOffline::set_input(const std::string &sndfile, nframes_t startframe)
{
	_sfin.open(sndfile.c_str());
	_sfin.seek(startframe,0);
	_samplerate = _sfin.samplerate();
}

void 
AudioInterfaceOffline::set_output(const std::string &sndfile)
{
	_sfout.open(sndfile.c_str(), _samplerate);
}


void 
AudioInterfaceOffline::process()
{
	nframes_t nf = _blocksize, totframes = 0;
	if (_sfin) {
		nf = _sfin.read(_bufin.get(), _blocksize);
		if (nf == 0) {
			_quit = true;
			return;
		}
	}
	totframes += nf;
	_process_cb(_bufin.get(), _bufout.get(), nf, totframes);
	if (_sfout)
		_sfout.write(_bufout.get(), nf);
}

/* ----------------------- OfflineApplication ----------------------- */
OfflineApplication::OfflineApplication(AudioInterfaceOffline &client, util::logstream &logv)
	: _logv(logv),_client(client) {}

void
OfflineApplication::run(unsigned int nblocks)
{
	_logv << _logv.allfields << "Starting test run; process runs " << nblocks << "x faster" << std::endl;
	unsigned int it = 0;
	for (;;++it) {
		_client.process();
		if ((it % nblocks)==0 && _mainloop_cb)
			if (_mainloop_cb()!=0) {
				_logv << _logv.allfields << "Main loop terminated" << std::endl;
				return;
			}
		if (_client.is_shutdown()) {
			_logv << _logv.allfields << "Client loop terminated" << std::endl;
			return;
		}
	}
}
		

/* ----------------------- OfflineOptions ----------------------- */

OfflineOptions::OfflineOptions(const char *program_name, const char *program_version)
	: Options(program_name, program_version)
{
	po::options_description testopts("Offline module options");
	testopts.add_options()
		("log,l",     po::value<std::string>(), "set logfile (default stdout)")
		("in,i",      po::value<std::string>(), "set input file")
		("out,o",     po::value<std::string>(), "set output file")
		("blocksize,b", po::value<int>(), "set number of samples processed in each loop")
		("samplerate,s", po::value<int>(), "set sampling rate (if no input file)");
	cmd_opts.add(testopts);
	visible_opts.add(testopts);
}


void
OfflineOptions::process_options()
{
	if (vmap.count("out"))
		output_file = get<std::string>("out");
	if (vmap.count("in"))
		input_file = get<std::string>("in");
	if (vmap.count("log"))
		logfile = get<std::string>("log");
	if (vmap.count("blocksize"))
		blocksize = get<int>("blocksize");
	if (vmap.count("samplerate"))
		samplerate = get<int>("samplerate");
	    
}
