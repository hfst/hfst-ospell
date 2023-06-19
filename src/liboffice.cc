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
#include <fstream>
#include <vector>
#include <string>
#include <string_view>
#include <algorithm>
#include <unordered_set>
#include <map>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <cerrno>
#include <cctype>

#define U_CHARSET_IS_UTF8 1
#include <unicode/uclean.h>
#include <unicode/ucnv.h>
#include <unicode/uloc.h>
#include <unicode/uchar.h>
#include <unicode/unistr.h>
using namespace icu;

#include <boost/dynamic_bitset.hpp>

#include "ZHfstOspeller.h"
#include "liboffice.h"

namespace hfst_ospell {

class OfficeSpeller::Impl {
public:
    using valid_words_t = std::map<UnicodeString, bool>;

    valid_words_t valid_words;

    struct word_t {
        size_t start, count;
        UnicodeString buffer;
    };
    std::vector<word_t> words;
    std::string buffer, wbuf;
    std::vector<Alt> alts;
    std::unordered_set<std::string> outputs;
    UnicodeString ubuffer, uc_buffer;
    size_t cw = 0;

    OfficeSpellerOptions opts = {};
    bool uc_first = false;
    bool uc_all = true;

    ZHfstOspeller speller;

    const std::vector<Alt>& find_alternatives(ZHfstOspeller& speller, size_t suggs) {
        outputs.clear();
        alts.clear();

        // Gather corrections from all the tried variants, starting with verbatim and increasing mangling from there
        for (size_t k = 0; k < cw && alts.size() < suggs; ++k) {
            buffer.clear();
            words[k].buffer.toUTF8String(buffer);
            hfst_ospell::CorrectionQueue corrections = speller.suggest(buffer);

            if (corrections.size() == 0) {
                continue;
            }

            for (size_t i = 0, e = corrections.size(); i < e; ++i) {
                // Work around https://github.com/hfst/hfst-ospell/issues/54
                if (opts.max_weight > 0.0 && corrections.top().second > opts.max_weight) {
                    break;
                }
                auto w = corrections.top().second * (1.0 + k / 10.0);

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
                    ubuffer.append(uc_buffer, 1, uc_buffer.length() - 1);
                    ubuffer.toUTF8String(buffer);
                }
                else {
                    buffer.append(corrections.top().first);
                }
                if (k != 0) {
                    words[0].buffer.tempSubString(words[k].start + words[k].count).toUTF8String(buffer);
                }

                if (opts.debug) {
                    wbuf.resize(64);
                    wbuf.resize(sprintf(&wbuf[0], " (%.2f;%zu)", corrections.top().second, k));
                    buffer += wbuf;
                }

                if (outputs.count(buffer) == 0) {
                    alts.push_back({ w, buffer });
                    std::sort(alts.begin(), alts.end());
                    while (alts.size() > suggs) {
                        alts.pop_back();
                    }
                }
                outputs.insert(buffer);
                corrections.pop();
            }
        }

        return alts;
    }

    bool is_valid_word(ZHfstOspeller& speller, const std::string& word, size_t suggs = 0) {
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
        for (int32_t i = 0; i < ubuffer.length(); ++i) {
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
        const UChar* pwsz = ubuffer.getTerminatedBuffer();

        // Always test the full given input
        words[0].buffer.remove();
        words[0].start = ichStart;
        words[0].count = cchUse;
        words[0].buffer = ubuffer;
        cw = 1;

        if (cchUse > 1 && !opts.verbatim) {
            size_t count = cchUse;
            while (count && !u_isalnum(pwsz[ichStart + count - 1])) {
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
            while (start < ichStart + cchUse && !u_isalnum(pwsz[start])) {
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

        for (size_t i = 0, e = cw; i < e; ++i) {
            // If we are looking for suggestions, don't use the cache
            auto it = suggs ? valid_words.end() : valid_words.find(words[i].buffer);

            if (it == valid_words.end()) {
                buffer.clear();
                words[i].buffer.toUTF8String(buffer);
                bool valid = speller.spell(buffer);
                it = valid_words.insert(std::make_pair(words[i].buffer, valid)).first;

                if (!valid && !opts.verbatim && uc_first) {
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
                        it = valid_words.insert(std::make_pair(words[i].buffer, valid)).first;
                    }
                }
            }

            if (it->second == true) {
                return true;
            }
        }

        return false;
    }

    void ispell(std::istream& in, std::ostream& out) {
        out << "@@ hfst-ospell-office is alive" << std::endl;

        speller.set_weight_limit(opts.max_weight);
        speller.set_beam(opts.beam);
        speller.set_time_cutoff(opts.time_cutoff);

        std::string line;
        std::string word;
        std::istringstream ss;
        while (std::getline(in, line)) {
            while (!line.empty() && std::isspace(line[line.size() - 1])) {
                line.resize(line.size() - 1);
            }
            if (line.empty()) {
                continue;
            }

            if (line.size() >= 5 && line[0] == '$' && line[1] == '$' && line[3] == ' ') {
                if (line[2] == 'd' && isdigit(line[4]) && line.size() == 5) {
                    opts.debug = (line[4] != '0');
                    out << "@@ Option debug changed to " << opts.debug << std::endl;
                    continue;
                }
                if (line[2] == 'T' && isdigit(line[4]) && line.size() == 5) {
                    opts.verbatim = (line[4] != '0');
                    out << "@@ Option verbatim changed to " << opts.verbatim << std::endl;
                    continue;
                }
                if (line[2] == 'w' && isdigit(line[4])) {
                    opts.max_weight = std::stof(&line[4]);
                    speller.set_weight_limit(opts.max_weight);
                    out << "@@ Option max-weight changed to " << opts.max_weight << std::endl;
                    continue;
                }
                if (line[2] == 'b' && isdigit(line[4])) {
                    opts.beam = std::stof(&line[4]);
                    speller.set_beam(opts.beam);
                    out << "@@ Option beam changed to " << opts.beam << std::endl;
                    continue;
                }
                if (line[2] == 't' && isdigit(line[4])) {
                    opts.time_cutoff = std::stof(&line[4]);
                    speller.set_time_cutoff(opts.time_cutoff);
                    out << "@@ Option time-cutoff changed to " << opts.time_cutoff << std::endl;
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
                out << "!" << std::endl;
                continue;
            }

            if (is_valid_word(speller, line, suggs)) {
                out << "*" << std::endl;
                continue;
            }

            if (!suggs) {
                out << "#" << std::endl;
            }
            else {
                auto& alts = find_alternatives(speller, suggs);
                if (!alts.empty()) {
                    out << "&";
                    for (auto& alt : alts) {
                        out << "\t" << alt.second;
                    }
                    out << std::endl;
                }
            }
        }
    }

    void check_block(std::string& buffer, std::string& btag, std::string& etag, std::ostream& out, size_t suggs = 5) {
        trim(buffer);
        if (buffer.empty()) {
            return;
        }

        std::string word;
        std::vector<std::string_view> words;
        boost::dynamic_bitset bads;

        words.emplace_back("", 0);
        words.emplace_back("", 0);
        bads.push_back(false);
        bads.push_back(false);

        size_t b = 0, e = 0;
        do {
            e = buffer.find("\n\n", b);
            auto xe = (e == std::string::npos ? buffer.size() : e);
            std::string_view par{ &buffer[b], xe - b };
            {
                size_t b = 0, e = 0;
                do {
                    e = par.find(" ", b);
                    auto xe = (e == std::string_view::npos ? par.size() : e);
                    words.emplace_back(&par[b], xe - b);
                    word.assign(words.back());
                    bads.push_back(!is_valid_word(speller, word));
                    b = e + 1;
                } while (e != std::string::npos);
            }
            b = e + 2;

            // Ensure that words are not tried as MWEs across paragraph breaks
            words.emplace_back("", 0);
            words.emplace_back("", 0);
            bads.push_back(false);
            bads.push_back(false);
        } while (e != std::string::npos);

        // If there are any bad words, check if they are valid as MWEs
        if (bads.any()) {
            // First and last 2 words aren't real, to make MWE algorithm simpler
            for (size_t w = 2; w < words.size() - 2; ++w) {
                if (!bads.test(w)) {
                not_bad:
                    continue;
                }

                // Re-check bad word with 1 word context
                for (size_t i = w - 1; i < w + 1; ++i) {
                    word.assign(words[i]);
                    word.append(" ");
                    word.append(words[i + 1]);
                    trim(word);
                    if (is_valid_word(speller, word)) {
                        bads.reset(i);
                        bads.reset(i + 1);
                        goto not_bad;
                    }
                }

                // Re-check bad word with 2 words context
                for (size_t i = w - 2; i < w + 2; ++i) {
                    word.assign(words[i]);
                    word.append(" ");
                    word.append(words[i + 1]);
                    word.append(" ");
                    word.append(words[i + 2]);
                    trim(word);
                    if (is_valid_word(speller, word)) {
                        bads.reset(i);
                        bads.reset(i + 1);
                        bads.reset(i + 2);
                        goto not_bad;
                    }
                }
            }
        }

        // There are still bad words after MWE pass
        if (bads.any()) {
            out << btag << '\n';
            for (size_t w = 2; w < words.size() - 2; ++w) {
                out << words[w];
                if (bads.test(w)) {
                    out << '\t';
                    word.assign(words[w]);
                    is_valid_word(speller, word, suggs);
                    auto& sugs = find_alternatives(speller, suggs);
                    for (auto& sug : sugs) {
                        out << "<R:" << sug.second << "> ";
                    }
                }
                out << '\n';
            }
            out << etag << "\n\n";
        }

        buffer.clear();
        btag.clear();
        etag.clear();
    }

    void stream(std::istream& in, std::ostream& out) {
        std::string btag, etag;
        std::string buffer;

        std::string line;
        while (std::getline(in, line)) {
            etag.clear();

            normalize(line);
            trim(line);
            if (line.empty()) {
                buffer += '\n';
                while (buffer.size() > 2 && buffer[buffer.size() - 2] == '\n' && buffer[buffer.size() - 3] == '\n') {
                    buffer.pop_back();
                }
                continue;
            }

            if (line[0] == '<' && line[1] == 's') {
                btag = line;
            }
            else if (line[0] == '<' && line[1] == '/' && line[2] == 's') {
                etag = line;
                check_block(buffer, btag, etag, out);
            }
            else if (line == "<STREAMCMD:FLUSH>") {
                check_block(buffer, btag, etag, out);
                out << line << std::endl;
            }
            else {
                buffer += line;
                buffer += '\n';
            }

            // Just in case anyone decides to use the speller for a minor eternity
            if (valid_words.size() > 20480) {
                valid_words.clear();
            }
        }

        check_block(buffer, btag, etag, out);
    }

    Impl(const char* zhfst_filename, OfficeSpellerOptions opts = {})
        : words(8)
        , opts(opts)
    {
        UErrorCode status = U_ZERO_ERROR;
        u_init(&status);
        if (U_FAILURE(status) && status != U_FILE_ACCESS_ERROR) {
            std::ostringstream ss;
            ss << "Error: Cannot initialize ICU. Status = " << u_errorName(status) << std::endl;
            throw new std::runtime_error(ss.str());
        }

        ucnv_setDefaultName("UTF-8");
        uloc_setDefault("en_US_POSIX", &status);

        try {
            speller.read_zhfst(zhfst_filename);
            speller.set_time_cutoff(6.0);
        }
        catch (hfst_ospell::ZHfstMetaDataParsingError zhmdpe) {
            char ss[1024];
            sprintf(ss, "cannot finish reading zhfst archive %s:\n%s.\n", zhfst_filename, zhmdpe.what());
            throw new std::runtime_error(ss);
        }
        catch (hfst_ospell::ZHfstZipReadingError zhzre) {
            char ss[1024];
            sprintf(ss, "cannot read zhfst archive %s:\n%s.\n", zhfst_filename, zhzre.what());
            throw new std::runtime_error(ss);
        }
        catch (hfst_ospell::ZHfstXmlParsingError zhxpe) {
            char ss[1024];
            sprintf(ss, "Cannot finish reading index.xml from %s:\n%s.\n", zhfst_filename, zhxpe.what());
            throw new std::runtime_error(ss);
        }
    }

    ~Impl() {
        u_cleanup();
    }
};

OfficeSpeller::OfficeSpeller(const char* zhfst, OfficeSpellerOptions opts)
    : im(std::make_unique<Impl>(zhfst, opts))
{}

OfficeSpeller::~OfficeSpeller() = default;
OfficeSpeller::OfficeSpeller(OfficeSpeller&&) noexcept = default;
OfficeSpeller& OfficeSpeller::operator=(OfficeSpeller&&) noexcept = default;

void OfficeSpeller::ispell(std::istream& in, std::ostream& out) {
    return im->ispell(in, out);
}

void OfficeSpeller::stream(std::istream& in, std::ostream& out) {
    return im->stream(in, out);
}

bool OfficeSpeller::check(const std::string& word) {
    return im->is_valid_word(im->speller, word);
}

const std::vector<Alt>& OfficeSpeller::correct(const std::string& word, size_t suggs) {
    im->alts.clear();

    if (!im->is_valid_word(im->speller, word, suggs)) {
        im->find_alternatives(im->speller, suggs);
    }

    return im->alts;
}

}
