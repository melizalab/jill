
#include "quotas.hh"

#include <iostream>
#include <fstream>
#include <stdio.h>

namespace capture {

QuotaInfo load_quotas_from_file(const char* filename) {
	QuotaInfo quotas;

	std::ifstream f(filename);
	int start_hour, start_min, end_hour, end_min, count;
	int start_time, end_time;
		
	while(!f.eof()) {
		std::string str;

		std::getline(f, str);
		if (str[0] == '#')
			continue;
		// scan for whitespace
		if (std::string::npos == str.find_first_not_of(" \n\r\t")) {
			// skip!
			continue;
		} // if
		// parse out quota info
		int parsed = sscanf(str.c_str(), "%d:%d %d:%d %d", &start_hour, &start_min, &end_hour, &end_min, &count);
		if (parsed != 5) {
			std::cerr << "Unable to parse line from interval file, skipping:" << std::endl;
			std::cerr << "--->" << str << "<---" << std::endl;
			continue;
		} // if
		start_time = start_hour * 60 * 60 + start_min * 60;
		end_time   = end_hour   * 60 * 60 + end_min * 60;
		QuotaInterval q(start_time, end_time, count);
		quotas.push_back(q);
	} // while
	return quotas;
} // load_from_file()

}

