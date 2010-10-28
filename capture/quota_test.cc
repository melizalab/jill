
#include "quotas.hh"

#include <iostream>

using namespace std;
using namespace capture;

int main(int argc, char** argv)
{
	// get filename
	const char* filename = argv[1];

	QuotaInfo q_list = load_quotas_from_file(filename);
	for (QuotaInfo::iterator q_iter = q_list.begin(); q_iter != q_list.end(); q_iter++) {
		QuotaInterval q = *q_iter;
		int start_time = q.start_time();
		cout << "Start " << (start_time / (60*60)) << ':' << ((start_time % (60*60)) / 60);
		cout << '\t';
		int end_time = q.end_time();
		cout << "End " << (end_time / (60*60)) << ':' << ((end_time % (60*60)) / 60);
		cout << '\t';
		cout << "Count " << q.quota();
		cout << endl;
	} // for
}

