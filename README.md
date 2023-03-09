# Hfst-ospell library and toy commandline tester

This is a minimal hfst optimized lookup format based spell checker library and
a demonstrational implementation of command line based spell checker. The
library is licenced under Apache licence version 2, other licences can be
obtained from University of Helsinki.

[![Build Status](https://travis-ci.org/hfst/hfst-ospell.svg?branch=master)](https://travis-ci.org/hfst/hfst-ospell)

## Dependencies

- libxml++2
- libarchive

## Debian packages for dependencies

- libxml++2-dev
- libarchive-dev

## Usage

Usage in external programs:

    #include <ospell.h>

and compile your project with:

    $(pkg-config --cflags hfstospell)

and link with:

    $(pkg-config --libs hfstospell)

## Programming examples

The library lives in a namespace called hfst_ospell. Pass (weighted!) Transducer
pointers to the Speller constructor, eg.:

    FILE * error_source = fopen(error_filename, "r");
    FILE * lexicon_file = fopen(lexicon_filename, "r");
    hfst_ospell::Transducer * error;
    hfst_ospell::Transducer * lexicon;
    try {
        error = new hfst_ospell::Transducer(error_source);
        lexicon = new hfst_ospell::Transducer(lexicon_file);
    } catch (hfst_ospell::TransducerParsingException& e) {
            /* problem with transducer file, usually completely
            different type of file - there's no magic number
            in the header to check for this */
        }
    hfst_ospell::Speller * speller;
    try {
        speller = new hfst_ospell::Speller(error, lexicon);
    } catch (hfst_ospell::AlphabetTranslationException& e) {
        /* problem with translating between the two alphabets */
    }


And use the functions:

    // returns true if line is found in lexicon
    bool hfst_ospell::Speller::check(char * line);

    // CorrectionQueue is a priority queue, sorted by weight
    hfst_ospell::CorrectionQueue hfst_ospell::Speller::correct(char * line);


to communicate with it. See main.cc for a concrete usage example. 

## Command-line tool

Main.cc provides a demo utility with the following help message:

    Usage: hfst-ospell [OPTIONS] ERRORSOURCE LEXICON
    Run a composition of ERRORSOURCE and LEXICON on standard input and
    print corrected output

      -h, --help                  Print this help message
      -V, --version               Print version information
      -v, --verbose               Be verbose
      -q, --quiet                 Don't be verbose (default)
      -s, --silent                Same as quiet


    Report bugs to hfst-bugs@ling.helsinki.fi

# Use in real-world applications

The HFST based spellers can be used in real applications with help of
[voikko](http://voikko.sf.net). Voikko in turn can be used with enchant,
libreoffice, and firefox.

