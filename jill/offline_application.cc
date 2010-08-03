
#include "offline_application.hh"
using namespace jill;

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
		("blocksize,b", po::value<int>()->default_value(1024), 
		 "set number of samples processed in each loop")
		("samplerate,s", po::value<int>()->default_value(20000), 
		 "set sampling rate (if no input file)");
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
