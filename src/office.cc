/*

  Copyright 2015 University of Helsinki

  Bug reports for this file should go to:
  Tino Didriksen <mail@tinodidriksen.com>
  Code adapted from https://github.com/TinoDidriksen/trie-tools

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

*/

#include "liboffice.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <getopt.h>
using namespace hfst_ospell;

void print_help() {
	std::cout
		<< "Usage: hfst-ospell [options] zhfst-archive\n"
		<< "\n"
		<< " -h, --help            Shows this help\n"
		<< " -d, --debug           Debug output with weights attached to results\n"
		<< " -T, --verbatim        Disables case-folding and non-alphanumeric trimming\n"
		<< " -s, --stream          Stream of <s></s> blocks instead of ispell\n"
		<< " -w, --max-weight=W    Suppress corrections with weights above W\n"
		<< " -b, --beam=W          Suppress corrections worse than best candidate by more than W\n"
		<< " -t, --time-cutoff=T   Stop trying to find better corrections after T seconds; defaults to 6.0\n"
		<< std::flush;
}

int main(int argc, char **argv) {
	OfficeSpellerOptions opts;
	bool stream = false;

	struct option long_options[] =
		{
		{"help",         no_argument,       0, 'h'},
		{"debug",        no_argument,       0, 'd'},
		{"verbatim",     no_argument,       0, 'T'},
		{"stream",       no_argument,       0, 's'},
		{"max-weight",   required_argument, 0, 'w'},
		{"beam",         required_argument, 0, 'b'},
		{"time-cutoff",  required_argument, 0, 't'},
		{0,              0,                 0,  0 }
		};

	int c = 0;
	while (true) {
		int option_index = 0;
		c = getopt_long(argc, argv, "hdTw:b:t:", long_options, &option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
		case 'h':
			print_help();
			return EXIT_SUCCESS;

		case 'd':
			opts.debug = true;
			break;

		case 'T':
			opts.verbatim = true;
			break;

		case 's':
			stream = true;
			break;

		case 'w':
			opts.max_weight = std::stof(optarg);
			break;

		case 'b':
			opts.beam = std::stof(optarg);
			break;

		case 't':
			opts.time_cutoff = std::stof(optarg);
			break;
		}
	}

	if (optind >= argc) {
		throw std::invalid_argument("Must pass a zhfst as argument");
	}

	std::cerr << std::fixed << std::setprecision(2);
	std::cout << std::fixed << std::setprecision(2);

	OfficeSpeller speller(argv[optind], opts);
	if (stream) {
		speller.stream(std::cin, std::cout);
	}
	else {
		speller.ispell(std::cin, std::cout);
	}
}
