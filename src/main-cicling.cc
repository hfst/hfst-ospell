/*
  
  Copyright 2009 University of Helsinki
  
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
  This is a toy commandline utility for testing spellers on standard io.
 */

#include "ospell.h"
#include <getopt.h>
#include <cassert>
#include <math.h>

#define PACKAGE_NAME "hfst-ospell"
#define PACKAGE_STRING "hfst-ospell 0.1"
#define PACKAGE_BUGREPORT "hfst-bugs@ling.helsinki.fi"

bool print_usage(void)
{
    std::cerr <<
    "\n" <<
    "Usage: " << PACKAGE_NAME << " [OPTIONS] ERRORSOURCE LEXICON\n" <<
    "Run a composition of ERRORSOURCE and LEXICON on standard input and\n" <<
    "print corrected output\n" <<
    "\n" <<
    "  -h, --help                  Print this help message\n" <<
    "  -V, --version               Print version information\n" <<
    "  -v, --verbose               Be verbose\n" <<
    "  -q, --quiet                 Don't be verbose (default)\n" <<
    "  -s, --silent                Same as quiet\n" <<
    "\n" <<
    "\n" <<
    "Report bugs to " << PACKAGE_BUGREPORT << "\n" <<
    "\n";
    return true;
}

bool print_version(void)
{
    std::cerr <<
    "\n" <<
    PACKAGE_STRING << std::endl <<
    "copyright (C) 2009 University of Helsinki\n";
    return true;
}

bool print_short_help(void)
{
    print_usage();
    return true;
}

int main(int argc, char **argv)
{

    FILE * mutator_file = NULL;
    FILE * lexicon_file = NULL;
    
    int c;
    bool verbose = false;

    while (true) 
      {
        static struct option long_options[] =
        {
        // first the hfst-mandated options
        {"help",         no_argument,       0, 'h'},
        {"version",      no_argument,       0, 'V'},
        {"verbose",      no_argument,       0, 'v'},
        {"quiet",        no_argument,       0, 'q'},
        {"silent",       no_argument,       0, 's'},
        {0,              0,                 0,  0 }
        };
      
        int option_index = 0;
        c = getopt_long(argc, argv, "hVvqs", long_options, &option_index);

        if (c == -1) // no more options to look at
            break;

        switch (c) {
        case 'h':
            print_usage();
            return EXIT_SUCCESS;
            break;
          
        case 'V':
            print_version();
            return EXIT_SUCCESS;
            break;
          
        case 'v':
            verbose = true;
            break;
          
        case 'q': // fallthrough
        case 's':
            break;
          
        default:
            std::cerr << "Invalid option\n\n";
            print_short_help();
            return EXIT_FAILURE;
            break;
        }
    }
    // no more options, we should now be at the input filenames
    if ( (optind + 2) < argc) {
        std::cerr << "More than two input files given\n";
        return EXIT_FAILURE;
    } else if ( (optind + 2) > argc)
    {
        std::cerr << "Need two input files\n";
        return EXIT_FAILURE;
    } else {
        mutator_file = fopen(argv[(optind)], "r");
        if (mutator_file == NULL) {
            std::cerr << "Could not open file " << argv[(optind)]
                  << std::endl;
            return 1;
        }
        lexicon_file = fopen(argv[(optind + 1)], "r");
        if (lexicon_file == NULL) {
            std::cerr << "Could not open file " << argv[(optind + 1)]
                  << std::endl;
            return 1;
        }
    }
    hfst_ospell::Transducer * mutator;
    hfst_ospell::Transducer * lexicon;
    mutator = new hfst_ospell::Transducer(mutator_file);
    if (!mutator->is_weighted()) {
        std::cerr << "Error source was unweighted, exiting\n\n";
        return EXIT_FAILURE;
    }
    lexicon = new hfst_ospell::Transducer(lexicon_file);
    if (!lexicon->is_weighted()) {
        std::cerr << "Lexicon was unweighted, exiting\n\n";
        return EXIT_FAILURE;
    }
    
    hfst_ospell::Speller * speller;

    try {
        speller = new hfst_ospell::Speller(mutator, lexicon);
    } catch (hfst_ospell::AlphabetTranslationException& e) {
        std::cerr <<
        "Unable to build speller - symbol " << e.what() << " not "
        "present in lexicon's alphabet\n";
        return EXIT_FAILURE;
    }
    
    char * str = (char*) malloc(65535);
    while (!std::cin.eof()) {
        std::cin.getline(str, 65535);
        if (str[0] == '\0') {
            continue;
        }
            // n += 1
        char* p = strdup(str);
        char* tok = strtok(p, "\t");
        assert(tok != NULL);
        char* mispelt = strdup(tok);
        tok = strtok(NULL, "\t");
        assert(tok != NULL);
        char* corr = strdup(tok);
        tok = strtok(NULL, "\t");
        assert(tok != NULL);
        char* context = strdup(tok);
        // unknown += (corr in NWORDS)
        hfst_ospell::CorrectionQueue corrections = speller->correct(mispelt);
        if (corrections.size() == 0)
          {
            // correction too far
            fprintf(stdout, "%s\t%s\t%s[inf]\t%s\n",
                    mispelt, corr, mispelt, context);
          }
        else
          {
            fprintf(stdout, "%s\t%s", mispelt, corr);
            if (speller->check(mispelt))
              {
                fprintf(stdout, "\t%s[0]", mispelt);
              }
            while (corrections.size() > 0)
              {
                fprintf(stdout, "\t%s[%f]", corrections.top().first.c_str(),
                        corrections.top().second);
                corrections.pop();
              }
            fprintf(stdout, "\t%s\n", context);
          } // corrections size != 0
      }
    return EXIT_SUCCESS;
}
