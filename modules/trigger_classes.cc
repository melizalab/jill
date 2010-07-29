
#include "trigger_classes.hh"

TriggerOptions::TriggerOptions(const char *program_name, const char *program_version)
	: Options(program_name, program_version) // this calls the superclass constructor
{
	cmd_opts.add_options()
		("output_file", po::value<std::string>(), "set output file");
	pos_opts.add("output_file", -1);
} 

void 
TriggerOptions::process_options() 
{
	if (vmap.count("output_file"))
		output_file = get<std::string>("output_file");
	else {
		std::cerr << "Error: missing required output file name " << std::endl;
		throw Exit(EXIT_FAILURE);
	}
}

void 
TriggerOptions::print_usage() 
{
	std::cout << "Usage: " << _program_name << " [options] output_file\n\n"
		  << "output_file can be any file format supported by libsndfile\n"
		  << visible_opts;
}



