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

/*
	Tests up to 8 variations of each input token:
	- Verbatim
	- With leading non-alphanumerics removed
	- With trailing non-alphanumerics removed
	- With leading and trailing non-alphanumerics removed
	- First-lower of all the above
*/

#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_set>
#include <map>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <cerrno>
#include <cctype>
#include <getopt.h>

#define U_CHARSET_IS_UTF8 1
#include <unicode/uclean.h>
#include <unicode/ucnv.h>
#include <unicode/uloc.h>
#include <unicode/uchar.h>
#include <unicode/unistr.h>
using namespace icu;

#include "ZHfstOspeller.h"

using hfst_ospell::ZHfstOspeller;
using hfst_ospell::Transducer;

typedef std::map<UnicodeString,bool> valid_words_t;
valid_words_t valid_words;

struct word_t {
	size_t start, count;
	UnicodeString buffer;
};
std::vector<word_t> words(16);
std::string buffer, wbuf;
using Alt = std::pair<double,std::string>;
std::vector<Alt> alts;
std::unordered_set<std::string> outputs;
UnicodeString ubuffer, uc_buffer;
size_t cw;

bool verbatim = false;
bool debug = false;
hfst_ospell::Weight max_weight = -1.0;
hfst_ospell::Weight beam = -1.0;
float time_cutoff = 6.0;
bool uc_first = false;
bool uc_all = true;

bool find_alternatives(ZHfstOspeller& speller, size_t suggs) {
	outputs.clear();
	alts.clear();

	// Gather corrections from all the tried variants, starting with verbatim and increasing mangling from there
	for (size_t k=0 ; k < cw && alts.size()<suggs ; ++k) {
		buffer.clear();
		words[k].buffer.toUTF8String(buffer);
		auto corrections = speller.suggest(buffer);

		if (corrections.size() == 0) {
			continue;
		}

		for (size_t i=0, e=corrections.size() ; i<e ; ++i) {
			// Work around https://github.com/hfst/hfst-ospell/issues/54
			if (max_weight > 0.0 && corrections.top().second > max_weight) {
				break;
			}
			auto w = corrections.top().second * (1.0 + k/10.0);

			buffer.clear();
			if (k != 0) {
				words[0].buffer.tempSubString(0, words[k].start).toUTF8String(buffer);
			}
			if (uc_all) {
				UnicodeString::fromUTF8(corrections.top().first).toUpper().toUTF8String(buffer);
			}
			else if (uc_first) {
				uc_buffer.setTo(UnicodeString::fromUTF8(corrections.top().first));
				ubuffer.setTo(uc_buffer, 0, 1);
				ubuffer.toUpper();
				ubuffer.append(uc_buffer, 1, uc_buffer.length()-1);
				ubuffer.toUTF8String(buffer);
			}
			else {
				buffer.append(corrections.top().first);
			}
			if (k != 0) {
				words[0].buffer.tempSubString(words[k].start + words[k].count).toUTF8String(buffer);
			}

			if (debug) {
				wbuf.resize(64);
				wbuf.resize(sprintf(&wbuf[0], " (%.2f;%zu)", corrections.top().second, k));
				buffer += wbuf;
			}

			if (outputs.count(buffer) == 0) {
				alts.push_back({w, buffer});
				std::sort(alts.begin(), alts.end());
				while (alts.size() > suggs) {
					alts.pop_back();
				}
			}
			outputs.insert(buffer);
			corrections.pop();
		}
	}

	if (!alts.empty()) {
		std::cout << "&";
		for (auto& alt : alts) {
			std::cout << "\t" << alt.second;
		}
		std::cout << std::endl;
		return true;
	}

	return false;
}

bool is_valid_word(ZHfstOspeller& speller, const std::string& word, size_t suggs) {
	ubuffer.setTo(UnicodeString::fromUTF8(word));

	if (word.size() == 13 && word[5] == 'D' && word == "nuvviDspeller") {
		uc_first = false;
		uc_all = false;
		words[0].start = 0;
		words[0].count = ubuffer.length();
		words[0].buffer = ubuffer;
		cw = 1;
		return false;
	}

	uc_first = false;
	uc_all = true;
	bool has_letters = false;
	for (int32_t i=0 ; i<ubuffer.length() ; ++i) {
		if (u_isalpha(ubuffer[i])) {
			has_letters = true;
			if (u_isupper(ubuffer[i]) && uc_all) {
				uc_first = true;
			}
			else if (u_islower(ubuffer[i])) {
				uc_all = false;
				break;
			}
		}
	}

	// If there are no letters in this token, just ignore it
	if (has_letters == false) {
		return true;
	}

	size_t ichStart = 0, cchUse = ubuffer.length();
	auto pwsz = ubuffer.getTerminatedBuffer();

	// Always test the full given input
	words[0].buffer.remove();
	words[0].start = ichStart;
	words[0].count = cchUse;
	words[0].buffer = ubuffer;
	cw = 1;

	if (cchUse > 1 && !verbatim) {
		size_t count = cchUse;
		while (count && !u_isalnum(pwsz[ichStart+count-1])) {
			--count;
		}
		if (count != cchUse) {
			// If the input ended with non-alphanumerics, test input with non-alphanumerics trimmed from the end
			words[cw].buffer.remove();
			words[cw].start = ichStart;
			words[cw].count = count;
			words[cw].buffer.append(pwsz, words[cw].start, words[cw].count);
			++cw;
		}

		size_t start = ichStart, count2 = cchUse;
		while (start < ichStart+cchUse && !u_isalnum(pwsz[start])) {
			++start;
			--count2;
		}
		if (start != ichStart) {
			// If the input started with non-alphanumerics, test input with non-alphanumerics trimmed from the start
			words[cw].buffer.remove();
			words[cw].start = start;
			words[cw].count = count2;
			words[cw].buffer.append(pwsz, words[cw].start, words[cw].count);
			++cw;
		}

		if (start != ichStart && count != cchUse) {
			// If the input both started and ended with non-alphanumerics, test input with non-alphanumerics trimmed from both sides
			words[cw].buffer.remove();
			words[cw].start = start;
			words[cw].count = count - (cchUse - count2);
			words[cw].buffer.append(pwsz, words[cw].start, words[cw].count);
			++cw;
		}
	}

	for (size_t i=0, e=cw ; i<e ; ++i) {
		// If we are looking for suggestions, don't use the cache
		auto it = suggs ? valid_words.end() : valid_words.find(words[i].buffer);

		if (it == valid_words.end()) {
			buffer.clear();
			words[i].buffer.toUTF8String(buffer);
			bool valid = speller.spell(buffer);
			it = valid_words.insert(std::make_pair(words[i].buffer,valid)).first;

			if (!valid && !verbatim && uc_first) {
				// If the word was not valid, try a first-lower variant
				buffer.clear();
				ubuffer.setTo(words[i].buffer, 0, 1);
				ubuffer.toLower();
				ubuffer.append(words[i].buffer, 1, words[i].buffer.length() - 1);
				ubuffer.toUTF8String(buffer);

				// Add the first-lower case variant to the list so that we get suggestions using that, if need be
				words[cw].start = words[i].start;
				words[cw].count = words[i].count;
				words[cw].buffer = ubuffer;
				++cw;

				// Don't try again if the first-lower variant has already been tried
				valid_words_t::iterator itl = suggs ? valid_words.end() : valid_words.find(ubuffer);
				if (itl != valid_words.end()) {
					it->second = itl->second;
					it = itl;
				}
				else {
					valid = speller.spell(buffer);
					it->second = valid; // Also mark the original mixed case variant as whatever the first-lower one was
					it = valid_words.insert(std::make_pair(words[i].buffer,valid)).first;
				}
			}
		}

		if (it->second == true) {
			return true;
		}
	}

	return false;
}

int zhfst_spell(const char* zhfst_filename) {
	ZHfstOspeller speller;
	try {
		if (debug) {
			std::cout << "@@ Loading " << zhfst_filename << " with args max-weight=" << max_weight << ", beam=" << beam << ", time-cutoff=" << time_cutoff << std::endl;
		}
		speller.read_zhfst(zhfst_filename);
		speller.set_weight_limit(max_weight);
		speller.set_beam(beam);
		speller.set_time_cutoff(time_cutoff);
	}
	catch (hfst_ospell::ZHfstMetaDataParsingError zhmdpe) {
		fprintf(stderr, "cannot finish reading zhfst archive %s:\n%s.\n", zhfst_filename, zhmdpe.what());
		return EXIT_FAILURE;
	}
	catch (hfst_ospell::ZHfstZipReadingError zhzre) {
		fprintf(stderr, "cannot read zhfst archive %s:\n%s.\n", zhfst_filename, zhzre.what());
		return EXIT_FAILURE;
	}
	catch (hfst_ospell::ZHfstXmlParsingError zhxpe) {
		fprintf(stderr, "Cannot finish reading index.xml from %s:\n%s.\n", zhfst_filename, zhxpe.what());
		return EXIT_FAILURE;
	}

	std::cout << "@@ hfst-ospell-office is alive" << std::endl;

	std::string line;
	std::string word;
	std::istringstream ss;
	while (std::getline(std::cin, line)) {
		while (!line.empty() && std::isspace(line[line.size()-1])) {
			line.resize(line.size()-1);
		}
		if (line.empty()) {
			continue;
		}

		if (line.size() >= 5 && line[0] == '$' && line[1] == '$' && line[3] == ' ') {
			if (line[2] == 'd' && isdigit(line[4]) && line.size() == 5) {
				debug = (line[4] != '0');
				std::cout << "@@ Option debug changed to " << debug << std::endl;
				continue;
			}
			if (line[2] == 'T' && isdigit(line[4]) && line.size() == 5) {
				verbatim = (line[4] != '0');
				std::cout << "@@ Option verbatim changed to " << verbatim << std::endl;
				continue;
			}
			if (line[2] == 'w' && isdigit(line[4])) {
				max_weight = std::stof(&line[4]);
				speller.set_weight_limit(max_weight);
				std::cout << "@@ Option max-weight changed to " << max_weight << std::endl;
				continue;
			}
			if (line[2] == 'b' && isdigit(line[4])) {
				beam = std::stof(&line[4]);
				speller.set_beam(beam);
				std::cout << "@@ Option beam changed to " << beam << std::endl;
				continue;
			}
			if (line[2] == 't' && isdigit(line[4])) {
				time_cutoff = std::stof(&line[4]);
				speller.set_time_cutoff(time_cutoff);
				std::cout << "@@ Option time-cutoff changed to " << time_cutoff << std::endl;
				continue;
			}
		}

		// Just in case anyone decides to use the speller for a minor eternity
		if (valid_words.size() > 20480) {
			valid_words.clear();
		}

		ss.clear();
		ss.str(line);
		size_t suggs = 0;
		char c = 0;
		if (!(ss >> suggs) || !ss.get(c) || !std::getline(ss, line)) {
			std::cout << "!" << std::endl;
			continue;
		}

		if (is_valid_word(speller, line, suggs)) {
			std::cout << "*" << std::endl;
			continue;
		}

		if (!suggs || !find_alternatives(speller, suggs)) {
			std::cout << "#" << std::endl;
		}
	}
    return EXIT_SUCCESS;
}

void print_help() {
	std::cout
		<< "Usage: hfst-ospell [options] zhfst-archive\n"
		<< "\n"
		<< " -h, --help            Shows this help\n"
		<< " -d, --debug           Debug output with weights attached to results\n"
		<< " -T, --verbatim        Disables case-folding and non-alphanumeric trimming\n"
		<< " -w, --max-weight=W    Suppress corrections with weights above W\n"
		<< " -b, --beam=W          Suppress corrections worse than best candidate by more than W\n"
		<< " -t, --time-cutoff=T   Stop trying to find better corrections after T seconds; defaults to 6.0\n"
		<< std::flush;
}

int main(int argc, char **argv) {
	UErrorCode status = U_ZERO_ERROR;
	u_init(&status);
	if (U_FAILURE(status) && status != U_FILE_ACCESS_ERROR) {
		std::cerr << "Error: Cannot initialize ICU. Status = " << u_errorName(status) << std::endl;
		return -1;
	}

	ucnv_setDefaultName("UTF-8");
	uloc_setDefault("en_US_POSIX", &status);

	struct option long_options[] =
		{
		{"help",         no_argument,       0, 'h'},
		{"debug",        no_argument,       0, 'd'},
		{"verbatim",     no_argument,       0, 'T'},
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
			debug = true;
			break;

		case 'T':
			verbatim = true;
			break;

		case 'w':
			max_weight = std::stof(optarg);
			break;

		case 'b':
			beam = std::stof(optarg);
			break;

		case 't':
			time_cutoff = std::stof(optarg);
			break;
		}
	}

	if (optind >= argc) {
		throw std::invalid_argument("Must pass a zhfst as argument");
	}

	std::cerr << std::fixed << std::setprecision(2);
	std::cout << std::fixed << std::setprecision(2);
	int rv = zhfst_spell(argv[optind]);

	u_cleanup();
	return rv;
}
