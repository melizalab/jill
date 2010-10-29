
#include "simple_template.hh"

#include <iostream>
#include <sstream>

namespace jill {
namespace util {

const char *TEMPLATE_MONTH          = "month";
const char *TEMPLATE_DAY            = "day";
const char *TEMPLATE_YEAR           = "year";
const char *TEMPLATE_HOUR           = "hour";
const char *TEMPLATE_MIN            = "min";
const char *TEMPLATE_SEC            = "sec";


void SimpleTemplate::add_variable(const char* key, const char* value)
{
	_value_map.insert(std::make_pair(key, value));
}

void SimpleTemplate::add_variable(const char* key, int value)
{
        std::ostringstream stream;
        stream << value;
        std::string str = stream.str();
        this->add_variable(key, str.c_str());
}

void SimpleTemplate::process(const std::string& tmpl_str)
{
	const char* tmpl = tmpl_str.c_str();
	_template_string = tmpl;
	_template.clear();
	int template_len = strlen(tmpl);
	int start = 0;
	bool in_template = false;
	for (int n=0; n < template_len; n++) {
		if (tmpl[n] == TEMPLATE_CHAR) {
			int count = n-start + (in_template ? 1 : 0);
			std::string str(tmpl, start, count);
			_template.push_back(str);
			start = n + (in_template ? 1 : 0);
			in_template = ! in_template;
		} // if
	} // for
	std::string str(tmpl, start, template_len-start);
	_template.push_back(str);

	std::list<std::string>::iterator iter;
	for (iter = _template.begin(); iter != _template.end(); iter++) {
	} // for
} 

int SimpleTemplate::_calc_size() 
{
	int size = 0;
	std::list<std::string>::iterator iter;
	for (iter = _template.begin(); iter != _template.end(); iter++) {
		std::string s = *iter;
		if (s[0] == TEMPLATE_CHAR) {
			int start = 1;
			int end = s.length();
			if (s[s.length()-1] == TEMPLATE_CHAR) {
				end -= 1;
			} // if
			std::string key(s, start, end);
			// lookup template param
			std::map<std::string, std::string>::iterator map_iter;
			map_iter = _value_map.find(key);
			if (map_iter == _value_map.end()) {
				size += s.length();
			} else {
				size += (*map_iter).second.length();
			} // if
		} else {
			size += s.length();
		} // if	
	} // for
	return size;
}

std::string SimpleTemplate::str()
{
	int s = this->_calc_size(); 

	// build string
	std::string str;
	str.reserve(s);

	std::list<std::string>::iterator iter;
	for (iter = _template.begin(); iter != _template.end(); iter++) {
		std::string s = *iter;
		if (s[0] == TEMPLATE_CHAR) {
			int start = 1;
			int end = s.length()-1;
			if (s[s.length()-1] == TEMPLATE_CHAR) {
				end -= 1;
			} // if
			std::string key(s, start, end);
			// lookup template param
			std::map<std::string, std::string>::iterator map_iter;
			map_iter = _value_map.find(key);
			if (map_iter == _value_map.end()) {
				str.append("_");
			} else {
				str.append((*map_iter).second);
			} // if
		} else {
			str.append(s);
		} // if	
	} // for

	return str;
}

void add_time_values(SimpleTemplate& templ) {
        time_t t = time(NULL);
        struct tm tm;
        localtime_r(&t, &tm);
        templ.add_variable(TEMPLATE_YEAR, tm.tm_year+1900);
        templ.add_variable(TEMPLATE_DAY, tm.tm_mday);
        templ.add_variable(TEMPLATE_HOUR, tm.tm_hour);
        templ.add_variable(TEMPLATE_MIN, tm.tm_min);
        templ.add_variable(TEMPLATE_SEC, tm.tm_sec);

        char buf[64];
        strftime(buf, 64, "%B", &tm);
        templ.add_variable(TEMPLATE_MONTH, buf);
} // add_time_values()

}
}


