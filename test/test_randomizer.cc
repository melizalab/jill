#include <cassert>
#include <iostream>
#include <boost/program_options.hpp>

#include "jill/util/event_randomizer.hh"

using namespace jill::util;
namespace po = boost::program_options;

int main(int argc, char ** argv)
{
        po::options_description desc("Allowed options");
        desc.add_options()
                ("help", "produce help message")
                ("prob,p", po::value<float>()->default_value(0.5), "set desired proportion of successes")
                ("control,c", po::value<float>()->default_value(8), "set control parameter")
                ("seed", po::value<int>()->default_value(1024), "set rng seed");
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        const int n_trials = 20;
        event_randomizer randomizer(vm["prob"].as<float>(), vm["control"].as<float>(), vm["seed"].as<int>());
        for (int i = 0; i < n_trials; ++i) {
                randomizer.present("A");
                randomizer.present("B");
                randomizer.present("C");
        }

        std::cout << "Results after " << n_trials << " trials:" << std::endl;
        for (const auto & item : randomizer.counts()) {
                std::cout << item.first << ": " << item.second.first << "/" << item.second.second << std::endl;
        }
}
