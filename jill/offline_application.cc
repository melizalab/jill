
#include "offline_application.hh"
#include "util/logger.hh"
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
		
