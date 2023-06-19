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

#include "hfst-ol.h"
#include <string>
#if HAVE_CONFIG_H
#  include <config.h>
#endif

namespace hfst_ospell {

template <typename T>
inline T hfst_deref(T* ptr)
{
    T dest;
    memcpy(&dest, ptr, sizeof(dest));
    return dest;
}

void skip_c_string(char ** raw)
{
    while (**raw != 0) {
        ++(*raw);
    }
    ++(*raw);
}

bool is_big_endian()
{
#ifdef WORDS_BIGENDIAN
    return true;
#else
    return false;
#endif
}

uint16_t read_uint16_flipping_endianness(FILE * f)
{
    uint16_t result = 0;
    unsigned char byte1 = static_cast<uint8_t>(getc(f));
    unsigned char byte2 = static_cast<uint8_t>(getc(f));
    result |= byte2;
    result <<= 8;
    result |= byte1;
    return result;
}

uint16_t read_uint16_flipping_endianness(char * raw)
{
    uint16_t result = 0;
    result |= *(raw + 1);
    result <<= 8;
    result |= *raw;
    return result;
}

uint32_t read_uint32_flipping_endianness(FILE * f)
{
    uint32_t result = 0;
    unsigned char byte1 = static_cast<uint8_t>(getc(f));
    unsigned char byte2 = static_cast<uint8_t>(getc(f));
    unsigned char byte3 = static_cast<uint8_t>(getc(f));
    unsigned char byte4 = static_cast<uint8_t>(getc(f));
    result |= byte4;
    result <<= 8;
    result |= byte3;
    result <<= 8;
    result |= byte2;
    result <<= 8;
    result |= byte1;
    return result;
}

uint32_t read_uint32_flipping_endianness(char * raw)
{
    uint32_t result = 0;
    result |= *(raw + 3);
    result <<= 8;
    result |= *(raw + 2);
    result <<= 8;
    result |= *(raw + 1);
    result <<= 8;
    result |= *raw;
    return result;
}

float read_float_flipping_endianness(FILE * f)
{
    union {
        float f;
        char c[4];
    } flipper;

    (void)f;
    flipper.f = 0;

    return flipper.f;
}

void
TransducerHeader::read_property(bool& property, FILE* f)
{
    if (is_big_endian()) {
        // four little-endian bytes
        int c = getc(f);
        property = !(c == 0);
        c = getc(f);
        c = getc(f);
        c = getc(f);
    } else {
        unsigned int prop;
        if (fread(&prop,sizeof(uint32_t),1,f) != 1) {
            HFSTOSPELL_THROW_MESSAGE(HeaderParsingException,
                               "Header ended unexpectedly\n");
        }
        if (prop == 0)
        {
            property = false;
        }
        else
        {
            property = true;
        }
    }
}

void
TransducerHeader::read_property(bool& property, char** raw)
{
    if (is_big_endian()) {
        property = !(**raw == 0);
    } else {
        unsigned int prop = *((unsigned int *) *raw);
        property = !(prop == 0);
    }
    (*raw) += sizeof(uint32_t);
}

void TransducerHeader::skip_hfst3_header(FILE * f)
{
    const char* header1 = "HFST";
    unsigned int header_loc = 0; // how much of the header has been found
    int c = 0;
    for(header_loc = 0; header_loc < strlen(header1) + 1; header_loc++)
    {
        c = getc(f);
        if(c != header1[header_loc]) {
            //std::cerr << header_loc << ": " << int(c) << " != " << header1[header_loc] << std::endl;
            break;
        }
    }
    if(header_loc == strlen(header1) + 1) // we found it
    {
        uint16_t remaining_header_len = 0;
        if (is_big_endian()) {
            remaining_header_len = read_uint16_flipping_endianness(f);
        } else {
            if (fread(&remaining_header_len,
                      sizeof(remaining_header_len), 1, f) != 1) {
                HFSTOSPELL_THROW_MESSAGE(HeaderParsingException,
                                   "Found broken HFST3 header\n");
            }
        }
        //std::cerr << "remaining_header_len " << remaining_header_len << std::endl;
        if (getc(f) != '\0') {
            HFSTOSPELL_THROW_MESSAGE(HeaderParsingException,
                               "Found broken HFST3 header\n");
        }
        std::string headervalue(remaining_header_len, '\0');
        if (fread(&headervalue[0], remaining_header_len, 1, f) != 1)
        {
            HFSTOSPELL_THROW_MESSAGE(HeaderParsingException,
                               "HFST3 header ended unexpectedly\n");
        }
        if (headervalue[remaining_header_len - 1] != '\0') {
            HFSTOSPELL_THROW_MESSAGE(HeaderParsingException,
                               "Found broken HFST3 header\n");
        }
        auto type_field = headervalue.find("type");
        if (type_field != std::string::npos) {
            if (headervalue.find("HFST_OL") != type_field + 5 &&
                headervalue.find("HFST_OLW") != type_field + 5) {
                HFSTOSPELL_THROW_MESSAGE(
                    TransducerTypeException,
                    "Transducer has incorrect type, should be "
                    "hfst-optimized-lookup\n");
            }
        }
    } else // nope. put back what we've taken
    {
        ungetc(c, f); // first the non-matching character
        for(int i = header_loc - 1; i>=0; i--) {
            // then the characters that did match (if any)
            ungetc(header1[i], f);
        }
    }
}

void TransducerHeader::skip_hfst3_header(char ** raw)
{
    const char* header1 = "HFST";
    unsigned int header_loc = 0; // how much of the header has been found

    for(header_loc = 0; header_loc < strlen(header1) + 1; header_loc++)
    {
        if(**raw != header1[header_loc]) {
            //std::cerr << header_loc << ": " << int(**raw) << " != " << header1[header_loc] << std::endl;
            break;
        }
        ++(*raw);
    }
    if(header_loc == strlen(header1) + 1) // we found it
    {
        uint16_t remaining_header_len = 0;
        if (is_big_endian()) {
            remaining_header_len = read_uint16_flipping_endianness(*raw);
        } else {
            remaining_header_len = *((unsigned short *) *raw);
        }
        //std::cerr << "remaining_header_len " << remaining_header_len << std::endl;
        (*raw) += sizeof(uint16_t) + 1 + remaining_header_len;
    } else // nope. put back what we've taken
    {
        --(*raw); // first the non-matching character
        for(int i = header_loc - 1; i>=0; i--) {
            // then the characters that did match (if any)
            --(*raw);
        }
    }
}

TransducerHeader::TransducerHeader(FILE* f)
{
    skip_hfst3_header(f); // skip header iff it is present
    if (is_big_endian()) {
        // no error checking yet
        number_of_input_symbols = read_uint16_flipping_endianness(f);
        number_of_symbols = read_uint16_flipping_endianness(f);
        size_of_transition_index_table = read_uint32_flipping_endianness(f);
        size_of_transition_target_table = read_uint32_flipping_endianness(f);
        number_of_states = read_uint32_flipping_endianness(f);
        number_of_transitions = read_uint32_flipping_endianness(f);
    } else {
        /* The following conditional clause does all the numerical reads
           and throws an exception if any fails to return 1 */
        if (fread(&number_of_input_symbols,
                  sizeof(SymbolNumber),1,f) != 1||
            fread(&number_of_symbols,
                  sizeof(SymbolNumber),1,f) != 1||
            fread(&size_of_transition_index_table,
                  sizeof(TransitionTableIndex),1,f) != 1||
            fread(&size_of_transition_target_table,
                  sizeof(TransitionTableIndex),1,f) != 1||
            fread(&number_of_states,
                  sizeof(TransitionTableIndex),1,f) != 1||
            fread(&number_of_transitions,
                  sizeof(TransitionTableIndex),1,f) != 1) {
            HFSTOSPELL_THROW_MESSAGE(HeaderParsingException,
                               "Header ended unexpectedly\n");
        }
    }
    /*
    std::cerr << "number_of_input_symbols " << number_of_input_symbols << std::endl;
    std::cerr << "number_of_symbols " << number_of_symbols << std::endl;
    std::cerr << "size_of_transition_index_table " << size_of_transition_index_table << std::endl;
    std::cerr << "size_of_transition_target_table " << size_of_transition_target_table << std::endl;
    std::cerr << "number_of_states " << number_of_states << std::endl;
    std::cerr << "number_of_transitions " << number_of_transitions << std::endl;
    //*/
    read_property(weighted,f);
    read_property(deterministic,f);
    read_property(input_deterministic,f);
    read_property(minimized,f);
    read_property(cyclic,f);
    read_property(has_epsilon_epsilon_transitions,f);
    read_property(has_input_epsilon_transitions,f);
    read_property(has_input_epsilon_cycles,f);
    read_property(has_unweighted_input_epsilon_cycles,f);
}

TransducerHeader::TransducerHeader(char** raw)
{
    skip_hfst3_header(raw); // skip header iff it is present
    if (is_big_endian()) {
        number_of_input_symbols = read_uint16_flipping_endianness(*raw);
        (*raw) += sizeof(SymbolNumber);
        number_of_symbols = read_uint16_flipping_endianness(*raw);
        (*raw) += sizeof(SymbolNumber);
        size_of_transition_index_table = read_uint32_flipping_endianness(*raw);
        (*raw) += sizeof(TransitionTableIndex);
        size_of_transition_target_table = read_uint32_flipping_endianness(*raw);
        (*raw) += sizeof(TransitionTableIndex);
        number_of_states = read_uint32_flipping_endianness(*raw);
        (*raw) += sizeof(TransitionTableIndex);
        number_of_transitions = read_uint32_flipping_endianness(*raw);
        (*raw) += sizeof(TransitionTableIndex);
    } else {
        number_of_input_symbols = *(SymbolNumber*) *raw;
        (*raw) += sizeof(SymbolNumber);
        number_of_symbols = *(SymbolNumber*) *raw;
        (*raw) += sizeof(SymbolNumber);
        size_of_transition_index_table = *(TransitionTableIndex*) *raw;
        (*raw) += sizeof(TransitionTableIndex);
        size_of_transition_target_table = *(TransitionTableIndex*) *raw;
        (*raw) += sizeof(TransitionTableIndex);
        number_of_states = *(TransitionTableIndex*) *raw;
        (*raw) += sizeof(TransitionTableIndex);
        number_of_transitions = *(TransitionTableIndex*) *raw;
        (*raw) += sizeof(TransitionTableIndex);
    }
    /*
    std::cerr << "number_of_input_symbols " << number_of_input_symbols << std::endl;
    std::cerr << "number_of_symbols " << number_of_symbols << std::endl;
    std::cerr << "size_of_transition_index_table " << size_of_transition_index_table << std::endl;
    std::cerr << "size_of_transition_target_table " << size_of_transition_target_table << std::endl;
    std::cerr << "number_of_states " << number_of_states << std::endl;
    std::cerr << "number_of_transitions " << number_of_transitions << std::endl;
    //*/
    read_property(weighted,raw);
    read_property(deterministic,raw);
    read_property(input_deterministic,raw);
    read_property(minimized,raw);
    read_property(cyclic,raw);
    read_property(has_epsilon_epsilon_transitions,raw);
    read_property(has_input_epsilon_transitions,raw);
    read_property(has_input_epsilon_cycles,raw);
    read_property(has_unweighted_input_epsilon_cycles,raw);
}

SymbolNumber
TransducerHeader::symbol_count()
{
    return number_of_symbols;
}

SymbolNumber
TransducerHeader::input_symbol_count()
{
    return number_of_input_symbols;
}
TransitionTableIndex
TransducerHeader::index_table_size()
{
    return size_of_transition_index_table;
}

TransitionTableIndex
TransducerHeader::target_table_size()
{
    return size_of_transition_target_table;
}

bool
TransducerHeader::probe_flag(HeaderFlag flag)
{
    switch (flag) {
    case Weighted:
        return weighted;
    case Deterministic:
        return deterministic;
    case Input_deterministic:
        return input_deterministic;
    case Minimized:
        return minimized;
    case Cyclic:
        return cyclic;
    case Has_epsilon_epsilon_transitions:
        return has_epsilon_epsilon_transitions;
    case Has_input_epsilon_transitions:
        return has_input_epsilon_transitions;
    case Has_input_epsilon_cycles:
        return has_input_epsilon_cycles;
    case Has_unweighted_input_epsilon_cycles:
        return has_unweighted_input_epsilon_cycles;
    }
    return false;
}

bool
FlagDiacriticOperation::isFlag() const
{
    return feature != NO_SYMBOL;
}

FlagDiacriticOperator
FlagDiacriticOperation::Operation() const
{
    return operation;
}

SymbolNumber
FlagDiacriticOperation::Feature() const
{
    return feature;
}


ValueNumber
FlagDiacriticOperation::Value() const
{
    return value;
}


void TransducerAlphabet::read(FILE * f, SymbolNumber number_of_symbols)
{
    char * line = (char *) malloc(MAX_SYMBOL_BYTES);
    std::map<std::string, SymbolNumber> feature_bucket;
    std::map<std::string, ValueNumber> value_bucket;
    value_bucket[std::string()] = 0; // empty value = neutral
    ValueNumber val_num = 1;
    SymbolNumber feat_num = 0;

    kt.push_back(std::string("")); // zeroth symbol is epsilon
    int byte;
    while ( (byte = fgetc(f)) != 0 ) {
        /* pass over epsilon */
        if (byte == EOF) {
            HFSTOSPELL_THROW(AlphabetParsingException);
        }
    }

    for (SymbolNumber k = 1; k < number_of_symbols; ++k) {
        char * sym = line;
        while ( (byte = fgetc(f)) != 0 ) {
            if (byte == EOF) {
                HFSTOSPELL_THROW(AlphabetParsingException);
            }
            *sym = static_cast<char>(byte);
            ++sym;
        }
        *sym = 0;
        // Detect and handle special symbols, which begin and end with @
        if (line[0] == '@' && line[strlen(line) - 1] == '@') {
            if (strlen(line) >= 5 && line[2] == '.') { // flag diacritic
                std::string feat;
                std::string val;
                FlagDiacriticOperator op = P; // for the compiler
                switch (line[1]) {
                case 'P': op = P; break;
                case 'N': op = N; break;
                case 'R': op = R; break;
                case 'D': op = D; break;
                case 'C': op = C; break;
                case 'U': op = U; break;
                }
                char * c = line;
                for (c +=3; *c != '.' && *c != '@'; c++) { feat.append(c,1); }
                if (*c == '.')
                {
                    for (++c; *c != '@'; c++) { val.append(c,1); }
                }
                if (feature_bucket.count(feat) == 0)
                {
                    feature_bucket[feat] = feat_num;
                    ++feat_num;
                }
                if (value_bucket.count(val) == 0)
                {
                    value_bucket[val] = val_num;
                    ++val_num;
                }

                operations.insert(
                    std::pair<SymbolNumber, FlagDiacriticOperation>(
                        k,
                        FlagDiacriticOperation(
                            op, feature_bucket[feat], value_bucket[val])));

                kt.push_back(std::string(""));
                continue;

            } else if (strcmp(line, "@_UNKNOWN_SYMBOL_@") == 0) {
                unknown_symbol = k;
                kt.push_back(std::string(line));
                continue;
            } else if (strcmp(line, "@_IDENTITY_SYMBOL_@") == 0) {
                identity_symbol = k;
                kt.push_back(std::string(line));
                continue;
            } else { // we don't know what this is, ignore and suppress
                kt.push_back(std::string(""));
                continue;
            }
        }
        kt.push_back(std::string(line));
        string_to_symbol[std::string(line)] = k;
    }
    free(line);
    flag_state_size = static_cast<SymbolNumber>(feature_bucket.size());
}

void TransducerAlphabet::read(char ** raw, SymbolNumber number_of_symbols)
{
    std::map<std::string, SymbolNumber> feature_bucket;
    std::map<std::string, ValueNumber> value_bucket;
    value_bucket[std::string()] = 0; // empty value = neutral
    ValueNumber val_num = 1;
    SymbolNumber feat_num = 0;

    kt.push_back(std::string("")); // zeroth symbol is epsilon
    skip_c_string(raw);

    for (SymbolNumber k = 1; k < number_of_symbols; ++k) {

        // Detect and handle special symbols, which begin and end with @
        if ((*raw)[0] == '@' && (*raw)[strlen(*raw) - 1] == '@') {
            if (strlen(*raw) >= 5 && (*raw)[2] == '.') { // flag diacritic
                std::string feat;
                std::string val;
                FlagDiacriticOperator op = P; // for the compiler
                switch ((*raw)[1]) {
                case 'P': op = P; break;
                case 'N': op = N; break;
                case 'R': op = R; break;
                case 'D': op = D; break;
                case 'C': op = C; break;
                case 'U': op = U; break;
                }
                char * c = *raw;
                for (c +=3; *c != '.' && *c != '@'; c++) { feat.append(c,1); }
                if (*c == '.')
                {
                    for (++c; *c != '@'; c++) { val.append(c,1); }
                }
                if (feature_bucket.count(feat) == 0)
                {
                    feature_bucket[feat] = feat_num;
                    ++feat_num;
                }
                if (value_bucket.count(val) == 0)
                {
                    value_bucket[val] = val_num;
                    ++val_num;
                }

                operations.insert(
                    std::pair<SymbolNumber, FlagDiacriticOperation>(
                        k,
                        FlagDiacriticOperation(
                            op, feature_bucket[feat], value_bucket[val])));

                kt.push_back(std::string(""));
                skip_c_string(raw);
                continue;

            } else if (strcmp(*raw, "@_UNKNOWN_SYMBOL_@") == 0) {
                unknown_symbol = k;
                kt.push_back(std::string(""));
                skip_c_string(raw);
                continue;
            } else if (strcmp(*raw, "@_IDENTITY_SYMBOL_@") == 0) {
                identity_symbol = k;
                kt.push_back(std::string(""));
                skip_c_string(raw);
                continue;
            } else { // we don't know what this is, ignore and suppress
                kt.push_back(std::string(""));
                skip_c_string(raw);
                continue;
            }
        }
        kt.push_back(std::string(*raw));
        string_to_symbol[std::string(*raw)] = k;
        skip_c_string(raw);
    }
    flag_state_size = static_cast<SymbolNumber>(feature_bucket.size());
}

TransducerAlphabet::TransducerAlphabet(FILE* f, SymbolNumber number_of_symbols):
    unknown_symbol(NO_SYMBOL),
    identity_symbol(NO_SYMBOL),
    orig_symbol_count(number_of_symbols)
{
    read(f, number_of_symbols);
}

TransducerAlphabet::TransducerAlphabet(char** raw,
                                       SymbolNumber number_of_symbols):
    unknown_symbol(NO_SYMBOL),
    identity_symbol(NO_SYMBOL),
    orig_symbol_count(number_of_symbols)
{
    read(raw, number_of_symbols);
}

void TransducerAlphabet::add_symbol(std::string & sym)
{
    string_to_symbol[sym] = static_cast<SymbolNumber>(kt.size());
    kt.push_back(sym);
}

void TransducerAlphabet::add_symbol(char * sym)
{
    std::string s(sym);
    add_symbol(s);
}

KeyTable*
TransducerAlphabet::get_key_table()
{
    return &kt;
}

OperationMap*
TransducerAlphabet::get_operation_map()
{
    return &operations;
}

SymbolNumber
TransducerAlphabet::get_state_size()
{
    return flag_state_size;
}

SymbolNumber
TransducerAlphabet::get_unknown() const
{
    return unknown_symbol;
}

SymbolNumber
TransducerAlphabet::get_identity() const
{
    return identity_symbol;
}

SymbolNumber TransducerAlphabet::get_orig_symbol_count() const
{
    return orig_symbol_count;
}

StringSymbolMap*
TransducerAlphabet::get_string_to_symbol()
{
    return &string_to_symbol;
}

bool TransducerAlphabet::has_string(std::string const & s) const
{
    return string_to_symbol.count(s) != 0;
}

bool
TransducerAlphabet::is_flag(SymbolNumber symbol)
{
    return operations.count(symbol) == 1;
}

void IndexTable::read(FILE * f,
                      TransitionTableIndex number_of_table_entries)
{
    size_t table_size = number_of_table_entries*TransitionIndex::SIZE;
    indices = (char*)(malloc(table_size));
    if (fread(indices,table_size, 1, f) != 1) {
        HFSTOSPELL_THROW(IndexTableReadingException);
    }
    if (is_big_endian()) {
        convert_to_big_endian();
    }
}

void IndexTable::read(char ** raw,
                      TransitionTableIndex number_of_table_entries)
{
    size_t table_size = number_of_table_entries*TransitionIndex::SIZE;
    indices = (char*)(malloc(table_size));
    memcpy((void *) indices, (const void *) *raw, table_size);
    (*raw) += table_size;
    if (is_big_endian()) {
        convert_to_big_endian();
    }
}

void IndexTable::convert_to_big_endian()
{
    //std::cerr << "IndexTable::convert_to_big_endian " << size << std::endl;
    // We have a sequence of uint_16's and uint_32's in memory,
    // and we're going to flip endianness of each one.
    for(size_t i = 0; i < size; ++i) {
        // first the uint_16
        std::swap(*(indices + i * TransitionIndex::SIZE + 0), *(indices + i * TransitionIndex::SIZE + 1));
        // then the uint_32
        std::swap(*(indices + i * TransitionIndex::SIZE + 2), *(indices + i * TransitionIndex::SIZE + 5));
        std::swap(*(indices + i * TransitionIndex::SIZE + 3), *(indices + i * TransitionIndex::SIZE + 4));
    }
}

void TransitionTable::read(FILE * f,
                           TransitionTableIndex number_of_table_entries)
{
    size_t table_size = number_of_table_entries*Transition::SIZE;
    transitions = (char*)(malloc(table_size));
    if (fread(transitions, table_size, 1, f) != 1) {
        HFSTOSPELL_THROW(TransitionTableReadingException);
    }
    if (is_big_endian()) {
        convert_to_big_endian();
    }
}

void TransitionTable::read(char ** raw,
                           TransitionTableIndex number_of_table_entries)
{
    size_t table_size = number_of_table_entries*Transition::SIZE;
    transitions = (char*)(malloc(table_size));
    memcpy((void *) transitions, (const void *) *raw, table_size);
    (*raw) += table_size;
    if (is_big_endian()) {
        convert_to_big_endian();
    }
}

void TransitionTable::convert_to_big_endian()
{
    //std::cerr << "TransitionTable::convert_to_big_endian " << size << std::endl;
    // We have a sequence of [ uint_16, uint_16, uint_32, float ] in memory,
    // and we're going to flip endianness of each one.
    for(size_t i = 0; i < size; ++i) {
        // first the uint_16s
        std::swap(*(transitions + i * Transition::SIZE + 0), *(transitions + i * Transition::SIZE + 1));
        std::swap(*(transitions + i * Transition::SIZE + 2), *(transitions + i * Transition::SIZE + 3));
        // then the uint_32
        std::swap(*(transitions + i * Transition::SIZE + 4), *(transitions + i * Transition::SIZE + 7));
        std::swap(*(transitions + i * Transition::SIZE + 5), *(transitions + i * Transition::SIZE + 6));
        // then the float
        std::swap(*(transitions + i * Transition::SIZE + 8), *(transitions + i * Transition::SIZE + 11));
        std::swap(*(transitions + i * Transition::SIZE + 9), *(transitions + i * Transition::SIZE + 10));
    }
}

void LetterTrie::add_string(const char * p, SymbolNumber symbol_key)
{
    if (*(p+1) == 0)
    {
        symbols[(unsigned char)(*p)] = symbol_key;
        return;
    }
    if (letters[(unsigned char)(*p)] == NULL)
    {
        letters[(unsigned char)(*p)] = new LetterTrie();
    }
    letters[(unsigned char)(*p)]->add_string(p+1,symbol_key);
}

SymbolNumber LetterTrie::find_key(char ** p)
{
    const char * old_p = *p;
    ++(*p);
    if (letters[(unsigned char)(*old_p)] == NULL)
    {
        return symbols[(unsigned char)(*old_p)];
    }
    SymbolNumber s = letters[(unsigned char)(*old_p)]->find_key(p);
    if (s == NO_SYMBOL)
    {
        --(*p);
        return symbols[(unsigned char)(*old_p)];
    }
    return s;
}

bool LetterTrie::has_key_starting_with(const char c) const
{
    return letters[(unsigned char) c] != NULL;
}

LetterTrie::~LetterTrie()
{
    for (auto& i : letters)
    {
        delete i;
    }
}

Encoder::Encoder(KeyTable * kt, SymbolNumber number_of_input_symbols):
    ascii_symbols(UCHAR_MAX+1,NO_SYMBOL)
{
    read_input_symbols(kt, number_of_input_symbols);
}

void Encoder::read_input_symbol(const char * s, const int s_num)
{
    if (strlen(s) == 0) { // ignore empty strings
        return;
    }
    if ((strlen(s) == 1) && (unsigned char)(*s) <= 127 &&
        !letters.has_key_starting_with((unsigned char)(*s)))
    {
        // If it's in the ascii range and there isn't a longer symbol starting
        // with the same character, add it to the shortcut ascii table
        ascii_symbols[(unsigned char)(*s)] = static_cast<SymbolNumber>(s_num);
    } else if ((unsigned char)(*s) <= 127 &&
               ascii_symbols[(unsigned char)(*s)] != NO_SYMBOL) {
        // If this is shadowed by an ascii symbol, unshadow
        ascii_symbols[(unsigned char)(*s)] = NO_SYMBOL;
    }

    letters.add_string(s, static_cast<SymbolNumber>(s_num));
}

void Encoder::read_input_symbol(std::string const & s, const int s_num)
{
    read_input_symbol(s.c_str(), s_num);
}

void Encoder::read_input_symbols(KeyTable * kt,
                                 SymbolNumber number_of_input_symbols)
{
    for (SymbolNumber k = 0; k < number_of_input_symbols; ++k)
    {
        const char * p = kt->at(k).c_str();
        read_input_symbol(p, k);
    }
}

TransitionTableIndex
TransitionIndex::target() const
{
    return first_transition_index;
}

bool
TransitionIndex::final() const
{
    return input_symbol == NO_SYMBOL &&
        first_transition_index != NO_TABLE_INDEX;
}

Weight
TransitionIndex::final_weight() const
{
    union to_weight
    {
        TransitionTableIndex i;
        Weight w;
    } weight;
    weight.i = first_transition_index;
    return weight.w;
}

SymbolNumber
TransitionIndex::get_input() const
{
    return input_symbol;
}

TransitionTableIndex
Transition::target() const
{
    return target_index;
}

SymbolNumber
Transition::get_output() const
{
    return output_symbol;
}

SymbolNumber
Transition::get_input() const
{
    return input_symbol;
}

Weight
Transition::get_weight() const
{
    return transition_weight;
}

bool
Transition::final() const
{
    return input_symbol == NO_SYMBOL &&
        output_symbol == NO_SYMBOL &&
        target_index == 1;
}

IndexTable::IndexTable(FILE* f,
                       TransitionTableIndex number_of_table_entries):
    indices(NULL),
    size(number_of_table_entries)
{
    read(f, number_of_table_entries);
}

IndexTable::IndexTable(char ** raw,
                       TransitionTableIndex number_of_table_entries):
    indices(NULL),
    size(number_of_table_entries)
{
    read(raw, number_of_table_entries);
}

IndexTable::~IndexTable()
{
    if (indices) {
        free(indices);
    }
}

SymbolNumber
IndexTable::input_symbol(TransitionTableIndex i) const
{
    if (i < size) {
        return hfst_deref((SymbolNumber *)
                          (indices + TransitionIndex::SIZE * i));
    } else {
        return NO_SYMBOL;
    }
}

TransitionTableIndex
IndexTable::target(TransitionTableIndex i) const
{
    if (i < size) {
        return hfst_deref((TransitionTableIndex *)
                          (indices + TransitionIndex::SIZE * i +
                           sizeof(SymbolNumber)));
    } else {
        return NO_TABLE_INDEX;
    }
}

bool
IndexTable::final(TransitionTableIndex i) const
{
    return input_symbol(i) == NO_SYMBOL && target(i) != NO_TABLE_INDEX;
}

Weight
IndexTable::final_weight(TransitionTableIndex i) const
{
    if (i < size) {
        return hfst_deref((Weight *)
                          (indices + TransitionIndex::SIZE * i +
                           sizeof(SymbolNumber)));
    } else {
        return INFINITE_WEIGHT;
    }
}

TransitionTable::TransitionTable(FILE * f,
                                 TransitionTableIndex transition_count):
    transitions(NULL),
    size(transition_count)
{
    read(f, transition_count);
}

TransitionTable::TransitionTable(char ** raw,
                                 TransitionTableIndex transition_count):
    transitions(NULL),
    size(transition_count)
{
    read(raw, transition_count);
}

TransitionTable::~TransitionTable()
{
    if (transitions) {
        free(transitions);
    }
}

SymbolNumber
TransitionTable::input_symbol(TransitionTableIndex i) const
{
    if (i < size) {
        return hfst_deref((SymbolNumber *)
                          (transitions + Transition::SIZE * i));
    } else {
        return NO_SYMBOL;
    }
}

SymbolNumber
TransitionTable::output_symbol(TransitionTableIndex i) const
{
    if (i < size) {
        return hfst_deref((SymbolNumber *)
                          (transitions + Transition::SIZE * i +
                           sizeof(SymbolNumber)));
    } else {
        return NO_SYMBOL;
    }
}

TransitionTableIndex
TransitionTable::target(TransitionTableIndex i) const
{
    if (i < size) {
        return hfst_deref((TransitionTableIndex *)
                          (transitions + Transition::SIZE * i +
                           2*sizeof(SymbolNumber)));
    } else {
        return NO_TABLE_INDEX;
    }
}

Weight
TransitionTable::weight(TransitionTableIndex i) const
{
    if (i < size) {
        return hfst_deref((Weight *)
                          (transitions + Transition::SIZE * i +
                           2*sizeof(SymbolNumber) +
                           sizeof(TransitionTableIndex)));
    } else {
        return INFINITE_WEIGHT;
    }
}

bool
TransitionTable::final(TransitionTableIndex i) const
{
    return input_symbol(i) == NO_SYMBOL &&
        output_symbol(i) == NO_SYMBOL &&
        target(i) == 1;
}

SymbolNumber Encoder::find_key(char ** p)
{
    if (ascii_symbols[(unsigned char)(**p)] == NO_SYMBOL)
    {
        return letters.find_key(p);
    }
    SymbolNumber s = ascii_symbols[(unsigned char)(**p)];
    ++(*p);
    return s;
}

} // namespace hfst_ospell
