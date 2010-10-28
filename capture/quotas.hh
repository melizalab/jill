
#ifndef _QUOTAS_HH_
#define _QUOTAS_HH_

#include <list>
#include <string>

namespace capture {

class QuotaInterval {
public:
	QuotaInterval(int start, int end, int quota) : _start(start), _end(end), _quota(quota) {};

	int& start_time() { return _start; }
	int& end_time() { return _end; }
	int& quota() { return _quota; }
private:
	int _start, _end, _quota;
};

typedef std::list<QuotaInterval> QuotaInfo;

QuotaInfo load_quotas_from_file(const char* filename);

}

#endif

