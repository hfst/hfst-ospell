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
#if HAVE_ERROR_H
#  include <error.h>
#else
#  define error(status, errnum, fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__); \
  if (status != 0) exit(status);
#endif

#include "ol-exceptions.h"
#include "ospell.h"
#include "ZHfstOspeller.h"

using hfst_ospell::ZHfstOspeller;
using hfst_ospell::Transducer;

//static bool quiet = false;
static bool verbose = false;

char*
find_dicts(const char* langcode)
  {
    FILE* testhandle = NULL;
    char* testname = (char*)malloc(sizeof(char) * 
                            (strlen(langcode) + strlen("speller-.zhfst") + 1));
    int rv = sprintf(testname, "speller-%s.zhfst", langcode);
    if (rv == 0)
      {
        perror("sprinting path");
      }
    testhandle = fopen(testname, "r");
    if (testhandle != NULL)
      {
        fclose(testhandle);
        return testname;
      }
    free(testname);
    testname = (char*)malloc(sizeof(char) * 
                            (strlen(langcode) + 
                             strlen("/usr/share/voikko/3/speller-.zhfst") + 1));
    rv = sprintf(testname, "/usr/share/voikko/3/speller-%s.zhfst",
                     langcode);
    if (rv == 0)
      {
        perror("sprinting path");
      }
    testhandle = fopen(testname, "r");
    if (testhandle != NULL)
      {
        fclose(testhandle);
        return testname;
      }
    free(testname);
    char* homepath = getenv("HOME");
    if (homepath == NULL)
      {
        return NULL;
      }
    testname = (char*)malloc(sizeof(char) * 
                      (strlen(homepath) + strlen("/.voikko/3/speller-.zhfst") +
                       strlen(langcode) + 1));
    rv = sprintf(testname, "%s/.voikko/3/speller-%s.zhfst", homepath,
                     langcode);
    if (rv == 0)
      {
        perror("sprinting path");
      }
    testhandle = fopen(testname, "r");
    if (testhandle != NULL)
      {
        fclose(testhandle);
        return testname;
      }
    free(testname);
    return NULL;
  }

bool print_usage(void)
  {
    fprintf(stdout, "Usage: %s [OPTION]... [FILE]...\n"
            "Check spelling of each FILE. Without FILE, check standard input."
            "\n\n", "hfst-ispell");
    fprintf(stdout,
            "  -1              check only first field in lines "
            "(delimiter = tabulator)\n"
            "  -a              Ispell's pipe interface\n"
            "  --check-url     Check URLs, email addresses and directory paths\n"
            "  -d d[,d2,...]   used d (d2 etc.) dictionaries\n"
            "  -D              show available dictionaries\n"
            "  -G              print only correct words or lines\n"
            "  -h, --help      display this help and exit\n"
            "  -l              print mispelled words\n"
            "  -L              print lines with mispelled words\n"
            "  -v, --version   print version number\n"
            "  -vv             print Ispell compatible version number\n"
            "  -w              print misspelled words (= lines) "
            "from one word/line input\n"
            "\n");
    fprintf(stdout, "Examples: %s -d fi file.txt\n"
                    "          %s -l file.txt\n\n", "hfst-ispell", "hfst-ispell");
    fprintf(stdout, "Report bugs to " PACKAGE_BUGREPORT "\n");
    return true;
  }

bool print_version(bool ispell_strict)
  {
    fprintf(stdout, "@(#) International Ispell Version 3.2.06 (but really "
            PACKAGE_STRING ")\n\n");
    if (!ispell_strict)
      {
        fprintf(stdout, "Copyright (C) 2013 University of Helsinki. APL\n");
        fprintf(stdout,
            "This is free software; see the source for copying conditions. "
           " There is NO\n"
           "warranty; not even for MERCHANTABILITY or FITNESS FOR A "
          " PARTICULAR PURPOSE,\n"
          "to the extent permitted by law.\n");
      }
    return true;
  }

bool print_short_help(void)
  {
    print_usage();
    return true;
  }

static
void
print_correct(const char* /*s*/)
  {
    fprintf(stdout, "*\n");
  }

static
void
print_corrections(const char* s, hfst_ospell::CorrectionQueue& c)
  {
    fprintf(stdout, "& %s %zu %d: ", s, c.size(), 0);
    bool comma = false;
    while (c.size() > 0)
      {
        if (comma)
          {
            fprintf(stdout, ", ");
          }
        fprintf(stdout, "%s", c.top().first.c_str());
        comma = true;
        c.pop();
      }
    fprintf(stdout, "\n");
  }

static
void
print_no_corrects(const char* s)
  {
    fprintf(stdout, "# %s %d\n", s, 0);
  }

int
legacy_spell(const char* errmodel_filename, const char* acceptor_filename)
  {
    FILE* mutator_file = fopen(errmodel_filename, "r");
    if (mutator_file == NULL) 
      {
        std::cerr << "Could not open file " << errmodel_filename
              << std::endl;
        return EXIT_FAILURE;
      }
    FILE* lexicon_file = fopen(acceptor_filename, "r");
    if (lexicon_file == NULL) 
      {
        std::cerr << "Could not open file " << acceptor_filename
              << std::endl;
        return EXIT_FAILURE;
      }
    hfst_ospell::Transducer * mutator;
    hfst_ospell::Transducer * lexicon;
    mutator = new hfst_ospell::Transducer(mutator_file);
    if (!mutator->is_weighted()) 
      {
        std::cerr << "Error source was unweighted, exiting\n\n";
        return EXIT_FAILURE;
      }
    lexicon = new hfst_ospell::Transducer(lexicon_file);
    if (!lexicon->is_weighted()) 
      {
        std::cerr << "Lexicon was unweighted, exiting\n\n";
        return EXIT_FAILURE;
      }
    hfst_ospell::Speller * speller;
    try 
      {
        speller = new hfst_ospell::Speller(mutator, lexicon);
      }
    catch (hfst_ospell::AlphabetTranslationException& e) 
      {
        std::cerr <<
        "Unable to build speller - symbol " << e.what() << " not "
        "present in lexicon's alphabet\n";
        return EXIT_FAILURE;
      }
    char * str = (char*) malloc(2000);
    while (!std::cin.eof()) 
      {
        std::cin.getline(str, 2000);
        if (str[0] == '\0') 
          {
            fprintf(stdout, "\n");
            continue;
          }
        if (str[strlen(str) - 1] == '\r')
          {
            fprintf(stderr, "\\r is not allowed\n");
            exit(1);
          }
        if (speller->check(str)) 
          {
            print_correct(str);
          }
        else
          {
            hfst_ospell::CorrectionQueue corrections = speller->correct(str, 5);
            if (corrections.size() > 0) 
              {
                print_corrections(str, corrections);
              }
            else
              {
                print_no_corrects(str);
              }
          }
      }
    return EXIT_SUCCESS;
  }

int
zhfst_spell(char* zhfst_filename, FILE* input)
  {
    ZHfstOspeller speller;
    try
      {
        speller.read_zhfst(zhfst_filename);
      }
    catch (hfst_ospell::ZHfstMetaDataParsingError zhmdpe)
      {
        std::cerr << "cannot finish reading zhfst archive " << zhfst_filename <<
                   ":" << zhmdpe.what() << "." << std::endl;
        return EXIT_FAILURE;
      }
    catch (hfst_ospell::ZHfstZipReadingError zhzre)
      {
        std::cerr << "cannot read zhfst archive " << zhfst_filename << ":" 
          << zhzre.what() << "." << std::endl
          << "trying to read as legacy automata directory" << std::endl;
        try 
          {
            speller.read_legacy(zhfst_filename);
          }
        catch (hfst_ospell::ZHfstLegacyReadingError zhlre)
          {
            std::cerr << "cannot fallback to read legacy hfst speller dir " 
              << zhfst_filename 
              << ":" << std::endl
              << zhlre.what() << "." << std::endl;
            return EXIT_FAILURE;
          }
      }
    if (verbose)
      {
        std::cout << "Following metadata was read from ZHFST archive:" << std::endl
                << speller.metadata_dump() << std::endl;
      }
    char* str = NULL;
    size_t len = 0;
    while (getline(&str, &len, input) != -1) 
      {
        if (str[0] == '\0') 
          {
            break;
          }
        if (str[strlen(str) - 1] == '\r')
          {
            fprintf(stderr, "\\r is not allowed\n");
            exit(1);
          }
        else if (str[strlen(str) - 1] == '\n')
          {
            str[strlen(str) - 1] = '\0';
          }
        if (speller.spell(str)) 
          {
            print_correct(str);
          } 
        else 
          {
            hfst_ospell::CorrectionQueue corrections = speller.suggest(str);
            if (corrections.size() > 0)
              {
                print_corrections(str, corrections);
              }
            else
              {
                print_no_corrects(str);
              }
          }
      }
    free(str);
    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    
    int c;
    char* langcode = 0;
    //std::locale::global(std::locale(""));
    int version = 0;
#if HAVE_GETOPT_H
    while (true) {
        static struct option long_options[] =
            {
            // first the hfst-mandated options
            {"help",         no_argument,       0, 'h'},
            {"version",      no_argument,       0, 'v'},
            {"one",          no_argument,       0, '1'},
            {"ispell",       no_argument,       0, 'a'},
            {"check-url",    no_argument,       0, 'X'},
            {"dictionary",   required_argument, 0, 'd'},
            {"list",         no_argument,       0, 'D'},
            {"mispelt",      no_argument,       0, 'l'},
            {"misslines",    no_argument,       0, 'L'},
            {"wordperline",  no_argument,       0, 'w'},
            {0,              0,                 0,  0 }
            };
          
        int option_index = 0;
        c = getopt_long(argc, argv, "1ad:DGhvlLw", long_options, &option_index);

        if (c == -1) // no more options to look at
            break;

        switch (c) 
          {
        case 'h':
            print_usage();
            return EXIT_SUCCESS;
            break;
        case 'V':
            version += 1;
            break;
        case 'v':
            version += 1;
            break;
        case 'd':
            langcode = optarg;
            break;
        default:
            std::cerr << "Invalid option\n\n";
            print_short_help();
            return EXIT_FAILURE;
            break;
        }
    }
    if (version == 1) 
      {
        print_version(false);
        return EXIT_SUCCESS;
      }
    else if (version == 2)
      {
        print_version(true);
        return EXIT_SUCCESS;
      }
    else if (version >= 3)
      {
        fprintf(stdout, "Come on, really?\n");
        exit(version);
      }
#else
    int optind = 1;
#endif
    // find the dicts
    char* zhfst_file = 0;
    if (NULL == langcode)
      {
        fprintf(stderr, "Currently -d is required since I'm too lazy to check "
                "locale\n");
        exit(1);
      }
    else
      {
        zhfst_file = find_dicts(langcode);
        if (NULL == zhfst_file)
          {
            fprintf(stderr, "Could not find dictionary %s in standard "
                    "locations\n"
                    "Please install one of:\n"
                    "  /usr/share/voikko/3/speller-%s.zhfst\n"
                    "  $HOME/.voikko/3/speller-%s.zhfst\n"
                    "  ./speller-%s.zhfst\n", 
                    langcode, langcode, langcode, langcode);
            exit(1);
          }
      }
    // no more options, we should now be at the input filenames
    if (optind == argc)
      {
        return zhfst_spell(zhfst_file, stdin);
      }
    else if (optind < argc)
      {
        while (optind < argc)
          {
            FILE* infile = fopen(argv[optind], "r");
            if (NULL == infile)
              {
                fprintf(stderr, "Could not open %s for reading",
                        argv[optind]);
                exit(1);
              }
            zhfst_spell(zhfst_file, infile);
            optind++;
          }
      }
    return EXIT_SUCCESS;
  }
