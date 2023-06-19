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


#if HAVE_CONFIG_H
#  include <config.h>
#endif
#if HAVE_GETOPT_H
#  include <getopt.h>
#endif

#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>

#include "ol-exceptions.h"
#include "ospell.h"
#include "ZHfstOspeller.h"


using hfst_ospell::ZHfstOspeller;
using hfst_ospell::Transducer;

static bool quiet = false;
static bool verbose = false;
static FILE* profile_file;
clock_t profile_start, profile_end;

bool print_usage(void)
{
    std::cerr <<
    "\n" <<
    "Usage: " << PACKAGE_NAME << " [OPTIONS] ERRORSOURCE LEXICON\n" <<
    "       " << PACKAGE_NAME << " [OPTIONS] ZHFST-ARCHIVE\n" <<
    "Run a composition of ERRORSOURCE and LEXICON on standard input and\n" <<
    "print corrected output\n" <<
    "Second form seeks error sources and lexicons from the ZHFST-ARCHIVE\n"
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
    "copyright (C) 2009 - 2011 University of Helsinki\n";
    return true;
}

bool print_short_help(void)
{
    print_usage();
    return true;
}

int
legacy_spell(const char* errmodel_filename, const char* acceptor_filename)
{
    FILE* mutator_file = fopen(errmodel_filename, "r");
    if (mutator_file == NULL) {
        std::cerr << "Could not open file " << errmodel_filename
              << std::endl;
        return EXIT_FAILURE;
    }
    FILE* lexicon_file = fopen(acceptor_filename, "r");
    if (lexicon_file == NULL) {
        std::cerr << "Could not open file " << acceptor_filename
              << std::endl;
        return EXIT_FAILURE;
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
    char * str = (char*) malloc(2000);
    
    while (!std::cin.eof()) {
        std::cin.getline(str, 2000);
        if (speller->check(str)) {
        std::cout << "\"" << str << "\" is in the lexicon\n\n";
        } else {
        hfst_ospell::CorrectionQueue corrections = speller->correct(str);
        if (corrections.size() > 0) {
            std::cout << "Corrections for \"" << str << "\":\n";
            while (corrections.size() > 0)
            {
            std::cout << corrections.top().first << "    " << corrections.top().second << std::endl;
            corrections.pop();
            }
            std::cout << std::endl;
        } else {
            std::cout << "Unable to correct \"" << str << "\"!\n\n";
        }
        }
    }
    return EXIT_SUCCESS;

}

int
fallback_spell(const char* errmodel_filename1, const char* errmodel_filename2,
	     const char* acceptor_filename)
{
    FILE* mutator_file1 = fopen(errmodel_filename1, "r");
    if (mutator_file1 == NULL) {
        std::cerr << "Could not open file " << errmodel_filename1
              << std::endl;
        return EXIT_FAILURE;
    }
    FILE* mutator_file2 = fopen(errmodel_filename2, "r");
    if (mutator_file2 == NULL) {
        std::cerr << "Could not open file " << errmodel_filename2
              << std::endl;
        return EXIT_FAILURE;
    }
    FILE* lexicon_file = fopen(acceptor_filename, "r");
    if (lexicon_file == NULL) {
        std::cerr << "Could not open file " << acceptor_filename
              << std::endl;
        return EXIT_FAILURE;
    }
    hfst_ospell::Transducer * mutator1;
    hfst_ospell::Transducer * mutator2;
    hfst_ospell::Transducer * lexicon;
    mutator1= new hfst_ospell::Transducer(mutator_file1);
    if (!mutator1->is_weighted()) {
        std::cerr << "Error source was unweighted, exiting\n\n";
        return EXIT_FAILURE;
    }
    mutator2= new hfst_ospell::Transducer(mutator_file2);
    if (!mutator2->is_weighted()) {
        std::cerr << "Error source was unweighted, exiting\n\n";
        return EXIT_FAILURE;
    }
    lexicon = new hfst_ospell::Transducer(lexicon_file);
    if (!lexicon->is_weighted()) {
        std::cerr << "Lexicon was unweighted, exiting\n\n";
        return EXIT_FAILURE;
    }
    
    hfst_ospell::Speller * speller1;
    hfst_ospell::Speller * speller2;

    try {
        speller1 = new hfst_ospell::Speller(mutator1, lexicon);
    } catch (hfst_ospell::AlphabetTranslationException& e) {
        std::cerr <<
        "Unable to build speller - symbol " << e.what() << " not "
        "present in lexicon's alphabet\n";
        return EXIT_FAILURE;
    }
    try {
        speller2 = new hfst_ospell::Speller(mutator2, lexicon);
    } catch (hfst_ospell::AlphabetTranslationException& e) {
        std::cerr <<
        "Unable to build speller - symbol " << e.what() << " not "
        "present in lexicon's alphabet\n";
        return EXIT_FAILURE;
    }
    char * str = (char*) malloc(2000);
    
    while (!std::cin.eof()) {
        std::cin.getline(str, 2000);
        if (speller1->check(str)) {
        std::cout << "\"" << str << "\" is in the lexicon 1\n\n";
        } else {
        hfst_ospell::CorrectionQueue corrections1 = speller1->correct(str);
        if (corrections1.size() > 0) {
            std::cout << "Corrections for \"" << str << "\" w/ source 1:\n";
            while (corrections1.size() > 0)
            {
            std::cout << corrections1.top().first << "    " << corrections1.top().second << std::endl;
            corrections1.pop();
            }
            std::cout << std::endl;
        } else {
	    hfst_ospell::CorrectionQueue corrections2 = speller2->correct(str);
	    if (corrections2.size() > 0) {
		std::cout << "Corrections for \"" << str << "\" w/ source 2:\n";
		while (corrections2.size() > 0)
		{
		    std::cout << corrections2.top().first << "    " << corrections2.top().second << std::endl;
		    corrections2.pop();
		}
		std::cout << std::endl;
	    } else {
		std::cout << "Unable to correct \"" << str << "\"!\n\n";
	    }
        }
	}
    }
    return EXIT_SUCCESS;

}

int
zhfst_spell(char* zhfst_filename)
{
  ZHfstOspeller speller;
  try
    {
      speller.read_zhfst(zhfst_filename);
    }
  catch (hfst_ospell::ZHfstZipReadingError zhzre)
    {
      std::cerr << "cannot read zhfst archive " << zhfst_filename << ":" 
          << std::endl
          << zhzre.what() << "." << std::endl
          << "trying to read as legacy automata directory" << std::endl;
      speller.read_legacy(zhfst_filename);
    }
  catch (hfst_ospell::ZHfstLegacyReadingError zhlre)
    {
      std::cerr << "cannot read legacy hfst speller dir " << zhfst_filename 
          << ":" << std::endl
          << zhlre.what() << "." << std::endl;
      return EXIT_FAILURE;
    }

  if (verbose)
    {
      std::cout << "Following metadata was read from ZHFST archive:" << std::endl
                << speller.metadata_dump() << std::endl;
    }
  char * str = (char*) malloc(2000);
    
    while (!std::cin.eof()) {
        std::cin.getline(str, 2000);
        if (str[0] == '\0') {
        break;
        }
        if (speller.spell(str)) {
        std::cout << "\"" << str << "\" is in the lexicon\n\n";
        } else {
        hfst_ospell::CorrectionQueue corrections = speller.suggest(str);
        if (corrections.size() > 0) {
            std::cout << "Corrections for \"" << str << "\":\n";
            while (corrections.size() > 0)
            {
            std::cout << corrections.top().first << "    " << corrections.top().second << std::endl;
            corrections.pop();
            }
            std::cout << std::endl;
        } else {
            std::cout << "Unable to correct \"" << str << "\"!\n\n";
        }
        }
    }
    return EXIT_SUCCESS;
  return EXIT_SUCCESS;
}

void
hfst_print_profile_line()
  {
    if (profile_file == 0)
      {
        return;
      }
    fprintf(profile_file, "ospell");
    clock_t profile_end = clock();
    fprintf(profile_file, "\t%f", ((float)(profile_end - profile_start))
                                               / CLOCKS_PER_SEC);
    struct rusage* usage = static_cast<struct rusage*>
        (malloc(sizeof(struct rusage)));
    errno = 0;
    int rv = getrusage(RUSAGE_SELF, usage);
    if (rv != -1)
      {
        fprintf(profile_file, "\t%lu.%lu\t%lu.%lu"
                "\t%ld\t%ld\t%ld"
                "\t%ld"
                "\t%ld\t%ld\t%ld"
                "\t%ld\t%ld"
                "\t%ld\t%ld"
                "\t%ld"
                "\t%ld\t%ld",
                usage->ru_utime.tv_sec, usage->ru_utime.tv_usec,
                usage->ru_stime.tv_sec, usage->ru_stime.tv_usec,
                usage->ru_maxrss, usage->ru_ixrss, usage->ru_idrss,
                usage->ru_isrss,
                usage->ru_minflt, usage->ru_majflt, usage->ru_nswap,
                usage->ru_inblock, usage->ru_oublock, 
                usage->ru_msgsnd, usage->ru_msgrcv,
                usage->ru_nsignals,
                usage->ru_nvcsw, usage->ru_nivcsw);
      }
    else
      {
        fprintf(profile_file, "\tgetrusage: %s", strerror(errno));
      }
    fprintf(profile_file, "\n");
  }


int main(int argc, char **argv)
{
    
    int c;
  
#if HAVE_GETOPT_H
    while (true) {
        static struct option long_options[] =
            {
            // first the hfst-mandated options
            {"help",         no_argument,       0, 'h'},
            {"version",      no_argument,       0, 'V'},
            {"verbose",      no_argument,       0, 'v'},
            {"quiet",        no_argument,       0, 'q'},
            {"silent",       no_argument,       0, 's'},
            {"profile",      required_argument, 0, 'p'},
            {0,              0,                 0,  0 }
            };
          
        int option_index = 0;
        c = getopt_long(argc, argv, "hVvqsp:", long_options, &option_index);

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
            quiet = false;
            break;
          
        case 'p':
            profile_file = fopen(optarg, "a");
            if (NULL == profile_file)
              {
                perror("Couldn't open profiling file for appending");
              }
            profile_start = clock();
            break;
        case 'q': // fallthrough
        case 's':
            quiet = true;
            verbose = false;
            break;
          
        default:
            std::cerr << "Invalid option\n\n";
            print_short_help();
            return EXIT_FAILURE;
            break;
        }
    }
#else
    int optind = 1;
#endif
    // no more options, we should now be at the input filenames
    int rv = EXIT_SUCCESS;
    if (optind == (argc - 3))
      {
	  rv = fallback_spell(argv[optind], argv[optind+1], argv[optind+2]);
      }
    else if (optind == (argc - 2))
      {
        rv = legacy_spell(argv[optind], argv[optind+1]);
      }
    else if (optind == (argc - 1))
      {
        rv = zhfst_spell(argv[optind]);
      }
    else if (optind < (argc - 3))
      {
        std::cerr << "No more than three free parameters allowed" << std::endl;
        print_short_help();
        return EXIT_FAILURE;
      }
    else if (optind >= argc)
      {
        std::cerr << "Give full path to zhfst spellers or two automata"
            << std::endl;
        print_short_help();
        return EXIT_FAILURE;
      }
    hfst_print_profile_line();
    return rv;
  }
