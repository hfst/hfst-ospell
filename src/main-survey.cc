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

static FILE* profile_file = 0;
static FILE* histogram_file = 0;
static FILE* statistics_file = 0;
static clock_t profile_start, profile_end;

static long check_results = 65536;

static long correct_not_in_lm = 0;
static long lines = 0;
static long fixable_lines = 0;
static long some_results = 0;
static long* results = 0;
static long* corrects_at = 0;
static long results_beyond = 0;
static long corrects_beyond = 0;
static long no_corrects = 0;
static long in_language = 0;

static long max_results = 1024;

bool
print_usage(void)
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
    "  -P, --profile=PFILE         Save profiling data to PFILE\n" <<
    "  -X, --statistics=SFILE      Save statistsics to SFILE\n" <<
    "  -H, --histogram=HFILE       Save match numbes to HFILE\n" <<
    "  -n, --n-best=NBEST          Collect and provide only NBEST suggestions\n"
    <<
    "\n" <<
    "\n" <<
    "Report bugs to " << PACKAGE_BUGREPORT << "\n" <<
    "\n";
    return true;
}

bool
print_version(void)
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
   
    char* always_incorrect = strdup("\001\002@ALWAYS INCORRECT@"); 
    bool correcting = false;
    while (!std::cin.eof()) {
        std::cin.getline(str, 2000);
        if (str[0] == '\0') 
          {
            std::cerr << "Skipping empty lines" << std::endl;
            continue;
          }
        if (str[strlen(str) - 1] == '\r')
          {
            std::cerr << "There is a WINDOWS linebreak in this file" <<
                std::endl <<
                "Please convert with dos2unix or fromdos" << std::endl;
            exit(1);
          }
        char* tab = strchr(str, '\t');
        char* correct = 0;
        if (tab != NULL)
          {
            *tab = '\0';
            correct = strdup(tab + 1);
            char* p = correct;
            while (*p != '\0')
              {
                p++;
                if (*p == '\n')
                  {
                    *p = '\0';
                  }
              }
            correcting = true;
          }
        else
          {
            correct = always_incorrect;
            correcting = false;
          }
        if (verbose)
          {
            fprintf(stdout, "Checking if %s == %s\n", str, correct);
          }
        else
          {
            fprintf(stdout, "%s", str);
          }
        lines++;
        int i = 0;
        bool any_corrects = false;
        if (speller->check(str)) 
          {
            // spelling correct string is as if one suggestion was
            // made at edit 0 for means of this article;
            i++;
            if (strcmp(str, correct) == 0)
              {
                corrects_at[i]++;
                any_corrects = true;
              }
            results[i]++;
            in_language++;
            if (verbose)
              {
                fprintf(stdout, "%s was in the lexicon\n", str);
              }
            else
              {
                fprintf(stdout, "\t%s\n", str);
              }
          }
        hfst_ospell::CorrectionQueue corrections = speller->correct(str /*,
                                                                max_results */);
        while (corrections.size() > 0)
          {
            i++;
            if (i >= check_results)
              {
                break;
              }
            if (verbose)
              {
                fprintf(stdout, "Trying %s\n", 
                        corrections.top().first.c_str());
              }
            else
              {
                fprintf(stdout, "\t%s",
                    corrections.top().first.c_str());
              }
            if (strcmp(corrections.top().first.c_str(), correct) == 0)
              {
                if (i >= max_results)
                  {
                    if (verbose)
                      {
                        fprintf(stdout, "%d was correct beyond threshold\n", i);
                      }
                    corrects_beyond++;
                  }
                else
                  {
                    if (verbose)
                      {
                        fprintf(stdout, "%d was correct\n", i);
                      }
                    corrects_at[i]++;
                  }
                any_corrects = true;
              }
            corrections.pop();
          }
        if (!any_corrects)
          {
            no_corrects++;
            if (verbose)
              {
                fprintf(stdout, "no corrects for %s ?= %s\n", str, correct);
              }
          }
        fprintf(stdout, "\n");
        if (i >= max_results)
          {
            results_beyond++;
          }
        else
          {
            results[i]++;
          }
        if (!speller->check(correct))
          {
            if (verbose)
              {
                fprintf(stdout, "could not have been corrected, missing %s\n",
                        correct);
              }
            correct_not_in_lm++;
          }
        if (correcting)
          {
            free(correct);
          }
      } 
    return EXIT_SUCCESS;

}

void
hfst_print_profile_line()
  {
    if (profile_file == 0)
      {
        return;
      }
    if (ftell(profile_file) == 0L)
      {
        fprintf(profile_file, "name\tclock\t"
                "utime\tstime\tmaxrss\tixrss\tidrss\tisrss\t"
                "minflt\tmajflt\tnswap\tinblock\toublock\t"
                "msgsnd\tmsgrcv\tnsignals\tnvcsw\tnivcsw\n");
      }

    fprintf(profile_file, "ospell");
    profile_end = clock();
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

void
print_statistics()
  {
    if (NULL == statistics_file)
      {
        return;
      }
    if (lines == 0)
      {
        fprintf(stderr, "DATOISSA VIRHE. END.\n");
        exit(1);
      }
    // calculate stuff
    unsigned long corrects_rest = 0;
    unsigned long total_results = 0;
    for (int i = 6; i < max_results; i++)
      {
        corrects_rest += corrects_at[i];
      }
    for (int i = 0; i < max_results; i++)
      {
        total_results += results[i] * i;
      }
    some_results = lines - results[0];
    fixable_lines = lines - correct_not_in_lm;
    // print
    fprintf(statistics_file, "All\tIn LM\tLM+EM\t0s\tImpossible\n");
    fprintf(statistics_file, "%lu\t%lu\t%lu\t%lu\t%lu\n",
            lines, in_language, some_results, results[0], correct_not_in_lm);
    fprintf(statistics_file, "%.2f %%\t%.2f %%\t%.2f %%\t%.2f %%\n",
            static_cast<float>(lines) / static_cast<float>(lines) * 100.0f,
            static_cast<float>(in_language) / static_cast<float>(lines) * 100.0f,
            static_cast<float>(some_results) / static_cast<float>(lines) * 100.0f,
            static_cast<float>(results[0]) / static_cast<float>(lines) * 100.0f);
    fprintf(statistics_file, "All\t1sts\t2nds\t3rds\t4ths\t5ths\t"
            "Rests\tNo corrects\n");
    fprintf(statistics_file, "%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t"
            "%lu\t%lu\n",
            fixable_lines,
            corrects_at[1], corrects_at[2], corrects_at[3], corrects_at[4],
            corrects_at[5], corrects_rest, no_corrects - correct_not_in_lm);
    fprintf(statistics_file, "%.2f %%\t"
            "%.2f %%\t%.2f %%\t%.2f %%\t%.2f %%\t"
            "%.2f %%\t"
            "%.2f %%\t"
            "%.2f %%\n",
            static_cast<float>(fixable_lines) / static_cast<float>(fixable_lines) * 100.0f,
            static_cast<float>(corrects_at[1]) / static_cast<float>(fixable_lines) * 100.0f,
            static_cast<float>(corrects_at[2]) / static_cast<float>(fixable_lines) * 100.0f,
            static_cast<float>(corrects_at[3]) / static_cast<float>(fixable_lines) * 100.0f,
            static_cast<float>(corrects_at[4]) / static_cast<float>(fixable_lines) * 100.0f,
            static_cast<float>(corrects_at[5]) / static_cast<float>(fixable_lines) * 100.0f,
            static_cast<float>(corrects_rest) / static_cast<float>(fixable_lines) * 100.0f,
            static_cast<float>(no_corrects - correct_not_in_lm) / static_cast<float>(fixable_lines) * 100.0f);
    if (histogram_file == NULL)
      {
        return;
      }
    fprintf(histogram_file, "Result count\tfrequency\n");
    for (int i = 0; i < max_results; i++)
      {
        fprintf(histogram_file, "%u\t%lu\n", i, results[i]);
      }
  }

int main(int argc, char **argv)
  {
    results = new long[max_results];
    corrects_at = new long[max_results];
    
    int c;
    //std::locale::global(std::locale(""));
  
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
            {"profile",      required_argument, 0, 'P'},
            {"statistics",   required_argument, 0, 'X'},
            {"histogram",    required_argument, 0, 'H'},
            {"n-best",       required_argument, 0, 'n'},
            {0,              0,                 0,  0 }
            };
          
        int option_index = 0;
        c = getopt_long(argc, argv, "hVvqsP:X:H:n:", long_options, &option_index);

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
          
        case 'P':
            profile_file = fopen(optarg, "a");
            if (NULL == profile_file)
              {
                perror("Couldn't open profiling file for append");
              }
            profile_start = clock();
            break;
        case 'X':
            statistics_file = fopen(optarg, "w");
            if (NULL == statistics_file)
              {
                perror("Couldn't open statistic file for writing");
              }
            break;
        case 'H':
            histogram_file = fopen(optarg, "w");
            if (NULL == histogram_file)
              {
                perror("Couldn't open histogram file for wriitng");
              }
            break;
        case 'q': // fallthrough
        case 's':
            quiet = true;
            verbose = false;
            break;
        case 'n':
            char* endptr;
            max_results = strtoul(optarg, &endptr, 10L);
            if (*endptr != '\0')
              {
                perror("argument not valid for n-best");
              }
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
    if (optind == (argc - 2))
      {
        legacy_spell(argv[optind], argv[optind+1]);
      }
    else if (optind < (argc - 2))
      {
        std::cerr << "No more than two free parameters allowed" << std::endl;
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
    print_statistics();
    return EXIT_SUCCESS;
  }
