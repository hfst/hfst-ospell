/* -*- Mode: C++ -*- */
// Copyright 2010 University of Helsinki
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#if HAVE_CONFIG_H
#  include <config.h>
#endif

// C
#if HAVE_LIBARCHIVE
#  include <archive.h>
#  include <archive_entry.h>
#endif
// C++
#if HAVE_LIBXML
#  include <libxml++/libxml++.h>
#endif
#include <string>
#include <map>

using std::string;
using std::map;

// local
#include "ospell.h"
#include "hfst-ol.h"
#include "ZHfstOspeller.h"

namespace hfst_ol
{

#if HAVE_LIBARCHIVE
#if ZHFST_EXTRACT_TO_MEM
static
char*
extract_to_mem(archive* ar, archive_entry* entry, size_t* n)
{
    size_t full_length = 0;
    const struct stat* st = archive_entry_stat(entry);
    size_t buffsize = st->st_size;
    char* buff = new char[buffsize];
    for (;;) {
        ssize_t curr = archive_read_data(ar, buff + full_length, buffsize - full_length);
        if (0 == curr) {
            break;
        } else if (ARCHIVE_RETRY == curr) {
            continue;
        } else if (ARCHIVE_FAILED == curr) {
            throw ZHfstZipReadingError("Archive broken (ARCHIVE_FAILED)");
        } else if (curr < 0) {
            throw ZHfstZipReadingError("Archive broken...");
        } else {
            full_length += curr;
        }
    }
    *n = full_length;
    return buff;
}
#endif // if ZHFST_EXTRACT_TO_MEM

#if ZHFST_EXTRACT_TO_TMPDIR
static
char*
extract_to_tmp_dir(archive* ar)
{
    char* rv = strdup("/tmp/zhfstospellXXXXXXXX");
    int temp_fd = mkstemp(rv);
    int rr = archive_read_data_into_fd(ar, temp_fd);
    if ((rr != ARCHIVE_EOF) && (rr != ARCHIVE_OK)) {
        throw ZHfstZipReadingError("Archive not EOF'd or OK'd");
    }
    close(temp_fd);
    return rv;
}
#endif

#endif // HAVE_LIBARCHIVE

ZHfstOspeller::ZHfstOspeller() :
    suggestions_maximum_(0),
    maximum_weight_(-1.0),
    beam_(-1.0),
    time_cutoff_(0.0),
    can_spell_(false),
    can_correct_(false),
    can_analyse_(true),
    current_speller_(0),
    current_sugger_(0)
{
}

ZHfstOspeller::~ZHfstOspeller()
{
    if ((current_speller_ != NULL) && (current_sugger_ != NULL)) {
        if (current_speller_ != current_sugger_) {
            delete current_speller_;
            delete current_sugger_;
        } else {
            delete current_speller_;
        }
        current_sugger_ = 0;
        current_speller_ = 0;
    }
    for (map<string, Transducer*>::iterator acceptor = acceptors_.begin();
         acceptor != acceptors_.end();
         ++acceptor) {
        delete acceptor->second;
    }
    for (map<string, Transducer*>::iterator errmodel = errmodels_.begin();
         errmodel != errmodels_.end();
         ++errmodel) {
        delete errmodel->second;
    }
    can_spell_ = false;
    can_correct_ = false;
}

void
ZHfstOspeller::inject_speller(Speller* s)
{
    current_speller_ = s;
    current_sugger_ = s;
    can_spell_ = true;
    can_correct_ = true;
}

void
ZHfstOspeller::set_queue_limit(unsigned long limit)
{
    suggestions_maximum_ = limit;
}

void
ZHfstOspeller::set_weight_limit(Weight limit)
{
    maximum_weight_ = limit;
}

void
ZHfstOspeller::set_beam(Weight beam)
{
    beam_ = beam;
}

void
ZHfstOspeller::set_time_cutoff(float time_cutoff)
{
    time_cutoff_ = time_cutoff;
}

bool
ZHfstOspeller::spell(const string& wordform)
{
    if (can_spell_ && (current_speller_ != 0)) {
        char* wf = strdup(wordform.c_str());
        bool rv = current_speller_->check(wf);
        free(wf);
        return rv;
    }
    return false;
}

CorrectionQueue
ZHfstOspeller::suggest(const string& wordform)
{
    CorrectionQueue rv;
    if ((can_correct_) && (current_sugger_ != 0)) {
        char* wf = strdup(wordform.c_str());
        rv = current_sugger_->correct(wf,
                                      suggestions_maximum_,
                                      maximum_weight_,
                                      beam_,
                                      time_cutoff_);
        free(wf);
        return rv;
    }
    return rv;
}

AnalysisQueue
ZHfstOspeller::analyse(const string& wordform, bool ask_sugger)
{
    AnalysisQueue rv;
    char* wf = strdup(wordform.c_str());
    if ((can_analyse_) && (!ask_sugger) && (current_speller_ != 0)) {
        rv = current_speller_->analyse(wf);
    } else if ((can_analyse_) && (ask_sugger) && (current_sugger_ != 0)) {
        rv = current_sugger_->analyse(wf);
    }
    free(wf);
    return rv;
}

AnalysisCorrectionQueue
ZHfstOspeller::suggest_analyses(const string& wordform)
{
    AnalysisCorrectionQueue rv;
    // FIXME: should be atomic
    CorrectionQueue cq = suggest(wordform);
    while (cq.size() > 0) {
        AnalysisQueue aq = analyse(cq.top().first, true);
        while (aq.size() > 0) {
            StringPair sp(cq.top().first, aq.top().first);
            StringPairWeightPair spwp(sp, aq.top().second);
            rv.push(spwp);
            aq.pop();
        }
        cq.pop();
    }
    return rv;
}

void
ZHfstOspeller::read_zhfst(const string& filename)
{
#if HAVE_LIBARCHIVE
    struct archive* ar = archive_read_new();
    struct archive_entry* entry = 0;

#if USE_LIBARCHIVE_2
    archive_read_support_compression_all(ar);
#else
    archive_read_support_filter_all(ar);
#endif // USE_LIBARCHIVE_2

    archive_read_support_format_all(ar);
    int rr = archive_read_open_filename(ar, filename.c_str(), 10240);
    if (rr != ARCHIVE_OK) {
        throw ZHfstZipReadingError("Archive not OK");
    }
    for (int rr = archive_read_next_header(ar, &entry);
         rr != ARCHIVE_EOF;
         rr = archive_read_next_header(ar, &entry)) {
        if (rr != ARCHIVE_OK) {
            throw ZHfstZipReadingError("Archive not OK");
        }
        char* filename = strdup(archive_entry_pathname(entry));
        if (strncmp(filename, "acceptor.", strlen("acceptor.")) == 0) {
#if ZHFST_EXTRACT_TO_TMPDIR
            char* temporary = extract_to_tmp_dir(ar);
#elif ZHFST_EXTRACT_TO_MEM
            size_t total_length = 0;
            char* full_data = extract_to_mem(ar, entry, &total_length);
#endif
            char* p = filename;
            p += strlen("acceptor.");
            size_t descr_len = 0;
            for (const char* q = p; *q != '\0'; q++) {
                if (*q == '.') {
                    break;
                } else {
                    descr_len++;
                }
            }
            char* descr = hfst_strndup(p, descr_len);
#if ZHFST_EXTRACT_TO_TMPDIR
            FILE* f = fopen(temporary, "r");
            if (f == NULL) {
                throw ZHfstTemporaryWritingError("reading acceptor back "
                                                 "from temp file");
            }
            Transducer* trans = new Transducer(f);
#elif ZHFST_EXTRACT_TO_MEM
            Transducer* trans = new Transducer(full_data);
            delete[] full_data;
#endif
            acceptors_[descr] = trans;
            free(descr);
        } else if (strncmp(filename, "errmodel.", strlen("errmodel.")) == 0) {
#if ZHFST_EXTRACT_TO_TMPDIR
            char* temporary = extract_to_tmp_dir(ar);
#elif ZHFST_EXTRACT_TO_MEM
            size_t total_length = 0;
            char* full_data = extract_to_mem(ar, entry, &total_length);
#endif
            const char* p = filename;
            p += strlen("errmodel.");
            size_t descr_len = 0;
            for (const char* q = p; *q != '\0'; q++) {
                if (*q == '.') {
                    break;
                } else {
                    descr_len++;
                }
            }
            char* descr = hfst_strndup(p, descr_len);
#if ZHFST_EXTRACT_TO_TMPDIR
            FILE* f = fopen(temporary, "r");
            if (NULL == f) {
                throw ZHfstTemporaryWritingError("reading errmodel back "
                                                 "from temp file");
            }
            Transducer* trans = new Transducer(f);
#elif ZHFST_EXTRACT_TO_MEM
            Transducer* trans = new Transducer(full_data);
            delete[] full_data;
#endif
            errmodels_[descr] = trans;
            free(descr);
        } // if acceptor or errmodel
        else if (strcmp(filename, "index.xml") == 0) {
#if ZHFST_EXTRACT_TO_TMPDIR
            char* temporary = extract_to_tmp_dir(ar);
            metadata_.read_xml(temporary);
#elif ZHFST_EXTRACT_TO_MEM
            size_t xml_len = 0;
            char* full_data = extract_to_mem(ar, entry, &xml_len);
            metadata_.read_xml(full_data, xml_len);
            delete[] full_data;
#endif

        } else {
            fprintf(stderr, "Unknown file in archive %s\n", filename);
        }
        free(filename);
    } // while r != ARCHIVE_EOF
    archive_read_close(ar);

#if USE_LIBARCHIVE_2
    archive_read_finish(ar);
#else
    archive_read_free(ar);
#endif // USE_LIBARCHIVE_2

    if ((errmodels_.find("default") != errmodels_.end()) &&
        (acceptors_.find("default") != acceptors_.end())) {
        current_speller_ = new Speller(
            errmodels_["default"],
            acceptors_["default"]
            );
        current_sugger_ = current_speller_;
        can_spell_ = true;
        can_correct_ = true;
    } else if ((acceptors_.size() > 0) && (errmodels_.size() > 0)) {
        fprintf(stderr, "Could not find default speller, using %s %s\n",
                acceptors_.begin()->first.c_str(),
                errmodels_.begin()->first.c_str());
        current_speller_ = new Speller(
            errmodels_.begin()->second,
            acceptors_.begin()->second
            );
        current_sugger_ = current_speller_;
        can_spell_ = true;
        can_correct_ = true;
    } else if ((acceptors_.size() > 0) &&
               (acceptors_.find("default") != acceptors_.end())) {
        current_speller_ = new Speller(0, acceptors_["default"]);
        current_sugger_ = current_speller_;
        can_spell_ = true;
        can_correct_ = false;
    } else if (acceptors_.size() > 0) {
        current_speller_ = new Speller(0, acceptors_.begin()->second);
        current_sugger_ = current_speller_;
        can_spell_ = true;
        can_correct_ = false;
    } else {
        throw ZHfstZipReadingError("No automata found in zip");
    }
    can_analyse_ = can_spell_ | can_correct_;
#else
    throw ZHfstZipReadingError("Zip support was disabled");
#endif // HAVE_LIBARCHIVE
}


const ZHfstOspellerXmlMetadata&
ZHfstOspeller::get_metadata() const
{
    return metadata_;
}

string
ZHfstOspeller::metadata_dump() const
{
    return metadata_.debug_dump();

}

ZHfstException::ZHfstException() :
    what_("unknown")
{
}
ZHfstException::ZHfstException(const std::string& message) :
    what_(message)
{
}


const char*
ZHfstException::what()
{
    return what_.c_str();
}


ZHfstMetaDataParsingError::ZHfstMetaDataParsingError(const std::string& message) :
    ZHfstException(message)
{
}
ZHfstXmlParsingError::ZHfstXmlParsingError(const std::string& message) :
    ZHfstException(message)
{
}
ZHfstZipReadingError::ZHfstZipReadingError(const std::string& message) :
    ZHfstException(message)
{
}
ZHfstTemporaryWritingError::ZHfstTemporaryWritingError(const std::string& message) :
    ZHfstException(message)
{
}

} // namespace hfst_ol
