
#ifndef __SAPSUCKER_MULTI_SNDFILE_H__
#define __SAPSUCKER_MULTI_SNDFILE_H__

#include <jill/util/multi_sndfile.hh>
#include <jill/util/logger.hh>

namespace capture {

class SapsuckerMultiSndfile : public jill::util::MultiSndfile {
public:
	SapsuckerMultiSndfile(const char* templ,
	                      size_type samplerate,
	                      const char* final_dest,
	                      jill::util::logstream *logv = NULL) :
		jill::util::MultiSndfile(templ, samplerate),
		_final_dest(final_dest),
		_logv(logv) {};
	SapsuckerMultiSndfile(const std::string &templ,
	                      size_type samplerate,
	                      const std::string &final_dest,
	                      jill::util::logstream *logv = NULL) :
		jill::util::MultiSndfile(templ, samplerate),
	        _final_dest(final_dest),
		_logv(logv) {};
protected:
	virtual void _close();
private:
	std::string _final_dest;
	jill::util::logstream *_logv;
}; //

}

#endif
