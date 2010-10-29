/**
 * @file   simple_template.hh
 * @author Mike Lusignan <mlusigna@uchicago.edu>
 * @date   Fri Oct 29 11:35 2010
 *
 * @brief  A simple templating class
 *
 * Copyright C Daniel Meliza, 2010.  Licensed for use under Creative
 * Commons Attribution-Noncommercial-Share Alike 3.0 United States
 * License (http://creativecommons.org/licenses/by-nc-sa/3.0/us/).
 */

#ifndef __SIMPLE_TEMPLATE__H__
#define __SIMPLE_TEMPLATE__H__

#include <string>
#include <list>
#include <map>

namespace jill {
namespace util {

const char TEMPLATE_CHAR = '%';

/*
const char *TEMPLATE_MONTH          = "%month%";
const char *TEMPLATE_DAY            = "%day%";
const char *TEMPLATE_YEAR           = "%year%";
const char *TEMPLATE_HOUR           = "%hour%";
const char *TEMPLATE_MIN            = "%min%";
const char *TEMPLATE_SEC            = "%sec%"; 
*/

extern const char *TEMPLATE_MONTH;
extern const char *TEMPLATE_DAY;
extern const char *TEMPLATE_YEAR;
extern const char *TEMPLATE_HOUR;
extern const char *TEMPLATE_MIN;
extern const char *TEMPLATE_SEC; 

/**
 * @ingroup miscgroup
 * @brief simple substitution-based templating on string keys
 *
 * Class to evaluate templates using string based keys.
 */
class SimpleTemplate {
public:
       /**
         * Bind a template variable to a string value.
         *
         * @param key     the variable name
         * @param value   the variable value
         */
	void add_variable(const char* key, const char* value);

       /**
         * Bind a template variable to an integer value.
         *
         * @param key     the variable name
         * @param value   the variable value
         */
	void add_variable(const char* key, int value);

       /**
         * Process a template, determining the included variables.
         *
         * @param tmpl     the template string
         */
	void process(const std::string& tmpl);

       /**
         * Evaluate a template, returning a string with bound
	 * variables substituted in.  Unbound variables are replaced
	 * with '_'.
         *
         * @return         the string with variables substituted in
         */
	virtual std::string str();
private:
	int _calc_size();

	std::string _template_string;
	std::list<std::string> _template;
	std::map<std::string, std::string> _value_map;
}; // filename_template

void add_time_values(SimpleTemplate&);

} // namespace util
} // namespace jlll

#endif

