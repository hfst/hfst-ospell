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

#include "ospell.h"

namespace hfst_ospell {

int nByte_utf8(unsigned char c)
{
    /* utility function to determine how many bytes to peel off as
       a utf-8 character for representing as unknown symbol */
    if (c <= 127) {
        return 1;
    } else if ( (c & (128 + 64 + 32 + 16)) == (128 + 64 + 32 + 16) ) {
        return 4;
    } else if ( (c & (128 + 64 + 32 )) == (128 + 64 + 32) ) {
        return 3;
    } else if ( (c & (128 + 64 )) == (128 + 64)) {
        return 2;
    } else {
        return 0;
    }
}

void WeightQueue::push(Weight w)
{
    for (WeightQueue::iterator it = begin(); it != end(); ++it) {
        if (*it > w) {
            insert(it, w);
            return;
        }
    }
    push_back(w);
}

void WeightQueue::pop(void)
{
    pop_back();
}

Weight WeightQueue::get_lowest(void) const
{
    if (size() == 0) {
        return std::numeric_limits<Weight>::max();
    }
    return front();
}

Weight WeightQueue::get_highest(void) const
{
    if (size() == 0) {
        return std::numeric_limits<Weight>::max();
    }
    return back();
}

Transducer::Transducer(FILE* f):
    header(TransducerHeader(f)),
    alphabet(TransducerAlphabet(f, header.symbol_count())),
    keys(alphabet.get_key_table()),
    encoder(keys,header.input_symbol_count()),
    indices(f,header.index_table_size()),
    transitions(f,header.target_table_size())
    {}

Transducer::Transducer(char* raw):
    header(TransducerHeader(&raw)),
    alphabet(TransducerAlphabet(&raw, header.symbol_count())),
    keys(alphabet.get_key_table()),
    encoder(keys,header.input_symbol_count()),
    indices(&raw,header.index_table_size()),
    transitions(&raw,header.target_table_size())
    {}

TreeNode TreeNode::update_lexicon(SymbolNumber symbol,
                                  TransitionTableIndex next_lexicon,
                                  Weight weight)
{
    SymbolVector str(this->string);
    if (symbol != 0) {
        str.push_back(symbol);
    }
    return TreeNode(str,
                    this->input_state,
                    this->mutator_state,
                    next_lexicon,
                    this->flag_state,
                    this->weight + weight);
}

TreeNode TreeNode::update_mutator(TransitionTableIndex next_mutator,
                                  Weight weight)
{
    return TreeNode(this->string,
                    this->input_state,
                    next_mutator,
                    this->lexicon_state,
                    this->flag_state,
                    this->weight + weight);
}

TreeNode TreeNode::update(SymbolNumber symbol,
                          unsigned int next_input,
                          TransitionTableIndex next_mutator,
                          TransitionTableIndex next_lexicon,
                          Weight weight)
{
    SymbolVector str(this->string);
    if (symbol != 0) {
        str.push_back(symbol);
    }
    return TreeNode(str,
                    next_input,
                    next_mutator,
                    next_lexicon,
                    this->flag_state,
                    this->weight + weight);
}

TreeNode TreeNode::update(SymbolNumber symbol,
                          TransitionTableIndex next_mutator,
                          TransitionTableIndex next_lexicon,
                          Weight weight)
{
    SymbolVector str(this->string);
    if (symbol != 0) {
        str.push_back(symbol);
    }
    return TreeNode(str,
                    this->input_state,
                    next_mutator,
                    next_lexicon,
                    this->flag_state,
                    this->weight + weight);
}

bool TreeNode::try_compatible_with(FlagDiacriticOperation op)
{
    switch (op.Operation()) {

    case P: // positive set
        flag_state[op.Feature()] = op.Value();
        return true;

    case N: // negative set (literally, in this implementation)
        flag_state[op.Feature()] = -1*op.Value();
        return true;

    case R: // require
        if (op.Value() == 0) { // "plain" require, return false if unset
            return (flag_state[op.Feature()] != 0);
        }
        return (flag_state[op.Feature()] == op.Value());

    case D: // disallow
        if (op.Value() == 0) { // "plain" disallow, return true if unset
            return (flag_state[op.Feature()] == 0);
        }
        return (flag_state[op.Feature()] != op.Value());

    case C: // clear
        flag_state[op.Feature()] = 0;
        return true;

    case U: // unification
        /* if the feature is unset OR the feature is to this value already OR
           the feature is negatively set to something else than this value */
        if (flag_state[op.Feature()] == 0 ||
            flag_state[op.Feature()] == op.Value() ||
            (flag_state[op.Feature()] < 0 &&
             (flag_state[op.Feature()] * -1 != op.Value()))
            ) {
            flag_state[op.Feature()] = op.Value();
            return true;
        }
        return false;
    }

    return false; // to make the compiler happy
}

Speller::Speller(Transducer* mutator_ptr, Transducer* lexicon_ptr):
        mutator(mutator_ptr),
        lexicon(lexicon_ptr),
        input(),
        queue(TreeNodeQueue()),
        next_node(FlagDiacriticState(get_state_size(), 0)),
        limit(std::numeric_limits<Weight>::max()),
        alphabet_translator(SymbolVector()),
        operations(lexicon->get_operations()),
        limiting(None),
        mode(Correct),
        max_time(-1.0),
        start_clock(0),
        call_counter(0),
        limit_reached(false)
            {
                if (mutator != NULL) {
                    build_alphabet_translator();
                    cache = std::vector<CacheContainer>(
                        mutator->get_key_table()->size(), CacheContainer());
                }
            }


SymbolNumber
Speller::get_state_size()
{
    return static_cast<SymbolNumber>(lexicon->get_state_size());
}


void Speller::lexicon_epsilons(void)
{
    if (!lexicon->has_epsilons_or_flags(next_node.lexicon_state + 1)) {
        return;
    }
    TransitionTableIndex next = lexicon->next(next_node.lexicon_state, 0);
    STransition i_s = lexicon->take_epsilons_and_flags(next);

    while (i_s.symbol != NO_SYMBOL) {
        if (is_under_weight_limit(next_node.weight + i_s.weight)) {
            if (lexicon->transitions.input_symbol(next) == 0) {
                queue.push_back(next_node.update_lexicon((mode == Correct) ? 0 : i_s.symbol,
                                                         i_s.index,
                                                         i_s.weight));
            } else {
                FlagDiacriticState old_flags = next_node.flag_state;
                if (next_node.try_compatible_with( // this is terrible
                        operations->operator[](
                            lexicon->transitions.input_symbol(next)))) {
                    queue.push_back(next_node.update_lexicon(0,
                                                             i_s.index,
                                                             i_s.weight));
                    next_node.flag_state = old_flags;
                }
            }
        }
        ++next;
        i_s = lexicon->take_epsilons_and_flags(next);
    }
}

void Speller::lexicon_consume(void)
{
    unsigned int input_state = next_node.input_state;
    if (input_state >= input.size()) {
        // no more input
        return;
    }
    SymbolNumber this_input;
    if (mutator != NULL && mode != Check) {
        this_input = alphabet_translator[input[input_state]];
    } else {
        // To support zhfst spellers without error models, we allow
        // for the case with plain lexicon symbols
        this_input = input[input_state];
    }
    if(!lexicon->has_transitions(
           next_node.lexicon_state + 1, this_input)) {
        // we have no regular transitions for this
        if (this_input >= lexicon->get_alphabet()->get_orig_symbol_count()) {
            // this input was not originally in the alphabet, so unknown or identity
            // may apply
            if (lexicon->get_unknown() != NO_SYMBOL &&
                lexicon->has_transitions(next_node.lexicon_state + 1,
                                         lexicon->get_unknown())) {
                queue_lexicon_arcs(lexicon->get_unknown(),
                                   next_node.mutator_state,
                                   0.0, 1);
            }
            if (lexicon->get_identity() != NO_SYMBOL &&
                lexicon->has_transitions(next_node.lexicon_state + 1,
                                         lexicon->get_identity())) {
                queue_lexicon_arcs(lexicon->get_identity(),
                                   next_node.mutator_state,
                                   0.0, 1);
            }
        }
        return;
    }
    queue_lexicon_arcs(this_input,
                       next_node.mutator_state, 0.0, 1);
}

void Speller::queue_lexicon_arcs(SymbolNumber input_sym,
                                 unsigned int mutator_state,
                                 Weight mutator_weight,
                                 int input_increment)
{
    TransitionTableIndex next = lexicon->next(next_node.lexicon_state,
                                              input_sym);
    STransition i_s = lexicon->take_non_epsilons(next, input_sym);
    while (i_s.symbol != NO_SYMBOL) {
        if (i_s.symbol == lexicon->get_identity()) {
            i_s.symbol = input[next_node.input_state];
        }
        if (is_under_weight_limit(next_node.weight + i_s.weight + mutator_weight)) {
            queue.push_back(next_node.update(
                                (mode == Correct) ? input_sym : i_s.symbol,
                                next_node.input_state + input_increment,
                                mutator_state,
                                i_s.index,
                                i_s.weight + mutator_weight));
        }
        ++next;
        i_s = lexicon->take_non_epsilons(next, input_sym);
    }
}

void Speller::mutator_epsilons(void)
{
    if (!mutator->has_transitions(next_node.mutator_state + 1, 0)) {
        return;
    }
    TransitionTableIndex next_m = mutator->next(next_node.mutator_state, 0);
    STransition mutator_i_s = mutator->take_epsilons(next_m);

    while (mutator_i_s.symbol != NO_SYMBOL) {
        if (mutator_i_s.symbol == 0) {
            if (is_under_weight_limit(
                    next_node.weight + mutator_i_s.weight)) {
                queue.push_back(next_node.update_mutator(mutator_i_s.index,
                                                         mutator_i_s.weight));
            }
            ++next_m;
            mutator_i_s = mutator->take_epsilons(next_m);
            continue;
        } else if (!lexicon->has_transitions(
                       next_node.lexicon_state + 1,
                       alphabet_translator[mutator_i_s.symbol])) {
            // we have no regular transitions for this
            if (alphabet_translator[mutator_i_s.symbol] >= lexicon->get_alphabet()->get_orig_symbol_count()) {
                // this input was not originally in the alphabet, so unknown or identity
                // may apply
                if (lexicon->get_unknown() != NO_SYMBOL &&
                    lexicon->has_transitions(next_node.lexicon_state + 1,
                                             lexicon->get_unknown())) {
                    queue_lexicon_arcs(lexicon->get_unknown(),
                                       mutator_i_s.index, mutator_i_s.weight);
                }
                if (lexicon->get_identity() != NO_SYMBOL &&
                    lexicon->has_transitions(next_node.lexicon_state + 1,
                                             lexicon->get_identity())) {
                    queue_lexicon_arcs(lexicon->get_identity(),
                                       mutator_i_s.index, mutator_i_s.weight);
                }
            }
            ++next_m;
            mutator_i_s = mutator->take_epsilons(next_m);
            continue;
        }
        queue_lexicon_arcs(alphabet_translator[mutator_i_s.symbol],
                           mutator_i_s.index, mutator_i_s.weight);
        ++next_m;
        mutator_i_s = mutator->take_epsilons(next_m);
    }
}


bool Speller::is_under_weight_limit(Weight w) const
{
    if (limiting == Nbest) {
        return w < limit;
    }
    return w <= limit;
}

void Speller::consume_input()
{
    if (next_node.input_state >= input.size()) {
        return; // not enough input to consume
    }
    SymbolNumber input_sym = input[next_node.input_state];
    if (!mutator->has_transitions(next_node.mutator_state + 1,
                                  input_sym)) {
        // we have no regular transitions for this
        if (input_sym >= mutator->get_alphabet()->get_orig_symbol_count()) {
            // this input was not originally in the alphabet, so unknown or identity
            // may apply
            if (mutator->get_identity() != NO_SYMBOL &&
                mutator->has_transitions(next_node.mutator_state + 1,
                                         mutator->get_identity())) {
                queue_mutator_arcs(mutator->get_identity());
            }
            if (mutator->get_unknown() != NO_SYMBOL &&
                mutator->has_transitions(next_node.mutator_state + 1,
                                         mutator->get_unknown())) {
                queue_mutator_arcs(mutator->get_unknown());
            }
        }
    } else {
        queue_mutator_arcs(input_sym);
    }
}

void Speller::queue_mutator_arcs(SymbolNumber input_sym)
{
    TransitionTableIndex next_m = mutator->next(next_node.mutator_state,
                                                input_sym);
    STransition mutator_i_s = mutator->take_non_epsilons(next_m,
                                                         input_sym);
    while (mutator_i_s.symbol != NO_SYMBOL) {
        if (mutator_i_s.symbol == 0) {
            if (is_under_weight_limit(
                    next_node.weight + mutator_i_s.weight)) {
                queue.push_back(next_node.update(0, next_node.input_state + 1,
                                                 mutator_i_s.index,
                                                 next_node.lexicon_state,
                                                 mutator_i_s.weight));
            }
            ++next_m;
            mutator_i_s = mutator->take_non_epsilons(next_m, input_sym);
            continue;
        } else if (!lexicon->has_transitions(
                    next_node.lexicon_state + 1,
                    alphabet_translator[mutator_i_s.symbol])) {
                // we have no regular transitions for this
                if (alphabet_translator[mutator_i_s.symbol] >= lexicon->get_alphabet()->get_orig_symbol_count()) {
                    // this input was not originally in the alphabet, so unknown or identity
                    // may apply
                    if (lexicon->get_unknown() != NO_SYMBOL &&
                        lexicon->has_transitions(next_node.lexicon_state + 1,
                                                 lexicon->get_unknown())) {
                        queue_lexicon_arcs(lexicon->get_unknown(),
                                           mutator_i_s.index, mutator_i_s.weight, 1);
                    }
                    if (lexicon->get_identity() != NO_SYMBOL &&
                        lexicon->has_transitions(next_node.lexicon_state + 1,
                                                 lexicon->get_identity())) {
                        queue_lexicon_arcs(lexicon->get_identity(),
                                           mutator_i_s.index, mutator_i_s.weight, 1);
                    }
                }
                ++next_m;
                mutator_i_s = mutator->take_non_epsilons(next_m, input_sym);
                continue;
        }
        queue_lexicon_arcs(alphabet_translator[mutator_i_s.symbol],
                           mutator_i_s.index, mutator_i_s.weight, 1);
        ++next_m;
        mutator_i_s = mutator->take_non_epsilons(next_m,
                                                 input_sym);
    }
}

bool Transducer::initialize_input_vector(SymbolVector & input_vector,
                                         Encoder * encoder,
                                         char * line)
{
    input_vector.clear();
    char ** inpointer = &line;
    while (**inpointer != '\0') {
        SymbolNumber k = encoder->find_key(inpointer);
        if (k == NO_SYMBOL) { // no tokenization from alphabet
            // for real handling of other and identity for unseen symbols,
            // use the Speller interface analyse()!
            return false;
        }
        input_vector.push_back(k);
    }
    return true;
}

AnalysisQueue Transducer::lookup(char * line)
{
    std::map<std::string, Weight> outputs;
    AnalysisQueue analyses;
    SymbolVector input;
    TreeNodeQueue queue;
    if (!initialize_input_vector(input, &encoder, line)) {
        return analyses;
    }
    TreeNode start_node(FlagDiacriticState(get_state_size(), 0));
    queue.assign(1, start_node);

    while (queue.size() > 0) {
        TreeNode next_node = queue.back();
        queue.pop_back();

        // Final states
        if (next_node.input_state == input.size() &&
            is_final(next_node.lexicon_state)) {
            Weight weight = next_node.weight +
                final_weight(next_node.lexicon_state);
            std::string output = stringify(get_key_table(),
                                           next_node.string);
            /* if the result is novel or lower weighted than before, insert it */
            if (outputs.count(output) == 0 ||
                outputs[output] > weight) {
                outputs[output] = weight;
            }
        }

        TransitionTableIndex next_index;
        // epsilon loop
        if (has_epsilons_or_flags(next_node.lexicon_state + 1)) {
            next_index = next(next_node.lexicon_state, 0);
            STransition i_s = take_epsilons_and_flags(next_index);
            while (i_s.symbol != NO_SYMBOL) {
                if (transitions.input_symbol(next_index) == 0) {
                    queue.push_back(next_node.update_lexicon(i_s.symbol,
                                                             i_s.index,
                                                             i_s.weight));
                    // Not a true epsilon but a flag diacritic
                } else {
                    FlagDiacriticState old_flags = next_node.flag_state;
                    if (next_node.try_compatible_with(
                            get_operations()->operator[](
                                transitions.input_symbol(next_index)))) {
                        queue.push_back(next_node.update_lexicon(i_s.symbol,
                                                                 i_s.index,
                                                                 i_s.weight));
                        next_node.flag_state = old_flags;
                    }
                }
                ++next_index;
                i_s = take_epsilons_and_flags(next_index);
            }
        }

        // input consumption loop
        unsigned int input_state = next_node.input_state;
        if (input_state < input.size() &&
            has_transitions(
                next_node.lexicon_state + 1, input[input_state])) {

            next_index = next(next_node.lexicon_state,
                              input[input_state]);
            STransition i_s = take_non_epsilons(next_index,
                                                input[input_state]);

            while (i_s.symbol != NO_SYMBOL) {
                queue.push_back(next_node.update(
                                    i_s.symbol,
                                    input_state + 1,
                                    next_node.mutator_state,
                                    i_s.index,
                                    i_s.weight));

                ++next_index;
                i_s = take_non_epsilons(next_index, input[input_state]);
            }
        }

    }

    for (auto& it : outputs) {
        analyses.push(StringWeightPair(it.first, it.second));
    }

    return analyses;
}

bool
Transducer::final_transition(TransitionTableIndex i)
{
    return transitions.final(i);
}

bool
Transducer::final_index(TransitionTableIndex i)
{
    return indices.final(i);
}

KeyTable*
Transducer::get_key_table()
{
    return keys;
}

SymbolNumber
Transducer::find_next_key(char** p)
{
    return encoder.find_key(p);
}

Encoder*
Transducer::get_encoder()
{
    return &encoder;
}

unsigned int
Transducer::get_state_size()
{
    return alphabet.get_state_size();
}

SymbolNumber
Transducer::get_unknown() const
{
    return alphabet.get_unknown();
}

SymbolNumber
Transducer::get_identity() const
{
    return alphabet.get_identity();
}

TransducerAlphabet*
Transducer::get_alphabet()
{
    return &alphabet;
}

OperationMap*
Transducer::get_operations()
{
    return alphabet.get_operation_map();
}

TransitionTableIndex Transducer::next(const TransitionTableIndex i,
                                      const SymbolNumber symbol) const
{
    if (i >= TARGET_TABLE) {
        return i - TARGET_TABLE + 1;
    } else {
        return indices.target(i+1+symbol) - TARGET_TABLE;
    }
}

bool Transducer::has_transitions(const TransitionTableIndex i,
                                 const SymbolNumber symbol) const
{
    if (symbol == NO_SYMBOL) {
        return false;
    }
    if (i >= TARGET_TABLE) {
        return (transitions.input_symbol(i - TARGET_TABLE) == symbol);
    } else {
        return (indices.input_symbol(i+symbol) == symbol);
    }
}

bool Transducer::has_epsilons_or_flags(const TransitionTableIndex i)
{
    if (i >= TARGET_TABLE) {
        return(transitions.input_symbol(i - TARGET_TABLE) == 0||
               is_flag(transitions.input_symbol(i - TARGET_TABLE)));
    } else {
        return (indices.input_symbol(i) == 0);
    }
}

bool Transducer::has_non_epsilons_or_flags(const TransitionTableIndex i)
{
    if (i >= TARGET_TABLE) {
        SymbolNumber this_input = transitions.input_symbol(i - TARGET_TABLE);
        return((this_input != 0 && this_input != NO_SYMBOL) &&
               !is_flag(this_input));
    } else {
        SymbolNumber max_symbol = static_cast<SymbolNumber>(get_key_table()->size());
        for (SymbolNumber sym = 1; sym < max_symbol; ++sym) {
            if (indices.input_symbol(i + sym) == sym) {
                return true;
            }
        }
        return false;
    }
}

STransition Transducer::take_epsilons(const TransitionTableIndex i) const
{
    if (transitions.input_symbol(i) != 0) {
        return STransition(0, NO_SYMBOL);
    }
    return STransition(transitions.target(i),
                       transitions.output_symbol(i),
                       transitions.weight(i));
}

STransition Transducer::take_epsilons_and_flags(const TransitionTableIndex i)
{
    if (transitions.input_symbol(i) != 0 &&
        !is_flag(transitions.input_symbol(i))) {
        return STransition(0, NO_SYMBOL);
    }
    return STransition(transitions.target(i),
                       transitions.output_symbol(i),
                       transitions.weight(i));
}

STransition Transducer::take_non_epsilons(const TransitionTableIndex i,
                                          const SymbolNumber symbol) const
{
    if (transitions.input_symbol(i) != symbol) {
        return STransition(0, NO_SYMBOL);
    }
    return STransition(transitions.target(i),
                       transitions.output_symbol(i),
                       transitions.weight(i));
}

bool Transducer::is_final(const TransitionTableIndex i)
{
    if (i >= TARGET_TABLE) {
        return final_transition(i - TARGET_TABLE);
    } else {
        return final_index(i);
    }
}

Weight Transducer::final_weight(const TransitionTableIndex i) const
{
    if (i >= TARGET_TABLE) {
        return transitions.weight(i - TARGET_TABLE);
    } else {
        return indices.final_weight(i);
    }
}

bool
Transducer::is_flag(const SymbolNumber symbol)
{
    return alphabet.is_flag(symbol);
}

bool
Transducer::is_weighted(void)
{
    return header.probe_flag(Weighted);
}


AnalysisQueue Speller::analyse(char * line, int nbest)
{
    (void)nbest;
    mode = Lookup;
    if (!init_input(line)) {
        return AnalysisQueue();
    }
    std::map<std::string, Weight> outputs;
    AnalysisQueue analyses;
    TreeNode start_node(FlagDiacriticState(get_state_size(), 0));
    queue.assign(1, start_node);
    while (queue.size() > 0) {
        next_node = queue.back();
        queue.pop_back();
        // Final states
        if (next_node.input_state == input.size() &&
            lexicon->is_final(next_node.lexicon_state)) {
            Weight weight = next_node.weight +
                lexicon->final_weight(next_node.lexicon_state);
            std::string output = stringify(lexicon->get_key_table(),
                                           next_node.string);
            /* if the result is novel or lower weighted than before, insert it */
            if (outputs.count(output) == 0 ||
                outputs[output] > weight) {
                outputs[output] = weight;
            }
        }
        lexicon_epsilons();
        lexicon_consume();
    }

    for (auto& it : outputs) {
        analyses.push(StringWeightPair(it.first, it.second));
    }
    return analyses;
}


AnalysisSymbolsQueue Speller::analyseSymbols(char * line, int nbest)
{
    (void)nbest;
    mode = Lookup;
    if (!init_input(line)) {
        return AnalysisSymbolsQueue();
    }
    std::map<std::vector<std::string>, Weight> outputs;
    AnalysisSymbolsQueue analyses;
    TreeNode start_node(FlagDiacriticState(get_state_size(), 0));
    queue.assign(1, start_node);
    while (queue.size() > 0) {
        next_node = queue.back();
        queue.pop_back();
        // Final states
        if (next_node.input_state == input.size() &&
            lexicon->is_final(next_node.lexicon_state)) {
            Weight weight = next_node.weight +
                lexicon->final_weight(next_node.lexicon_state);
            std::vector<std::string> output = symbolify(lexicon->get_key_table(),
                                                        next_node.string);
            /* if the result is novel or lower weighted than before, insert it */
            if (outputs.count(output) == 0 ||
                outputs[output] > weight) {
                outputs[output] = weight;
            }
        }
        lexicon_epsilons();
        lexicon_consume();
    }

    for (auto& it : outputs) {
        analyses.push(SymbolsWeightPair(it.first, it.second));
    }
    return analyses;
}



void Speller::build_cache(SymbolNumber first_sym)
{
    TreeNode start_node(FlagDiacriticState(get_state_size(), 0));
    queue.assign(1, start_node);
    limit = std::numeric_limits<Weight>::max();
    // A placeholding map, only one weight per correction
    StringWeightMap corrections_len_0;
    StringWeightMap corrections_len_1;
    while (queue.size() > 0) {
        next_node = queue.back();
        queue.pop_back();
        lexicon_epsilons();
        mutator_epsilons();
        if (mutator->is_final(next_node.mutator_state) &&
            lexicon->is_final(next_node.lexicon_state)) {
            // complete result of length 0 or 1
            Weight weight = next_node.weight +
                lexicon->final_weight(next_node.lexicon_state) +
                mutator->final_weight(next_node.mutator_state);
            std::string string = stringify(lexicon->get_key_table(), next_node.string);
            /* if the correction is novel or better than before, insert it
             */
            if (next_node.input_state == 0) {
                if (corrections_len_0.count(string) == 0 ||
                    corrections_len_0[string] > weight) {
                    corrections_len_0[string] = weight;
                }
            } else {
                if (corrections_len_1.count(string) == 0 ||
                    corrections_len_1[string] > weight) {
                    corrections_len_1[string] = weight;
                }
            }
        }
        if (next_node.input_state == 1) {
            cache[first_sym].nodes.push_back(next_node);
        } else {
//            std::cerr << "discarded node\n";
        }
        if (first_sym > 0 && next_node.input_state == 0) {
            consume_input();
        }
    }
    cache[first_sym].results_len_0.assign(corrections_len_0.begin(), corrections_len_0.end());
    cache[first_sym].results_len_1.assign(corrections_len_1.begin(), corrections_len_1.end());
    cache[first_sym].empty = false;
}

CorrectionQueue Speller::correct(char * line, int nbest,
                                 Weight maxweight, Weight beam,
                                 float time_cutoff)
{
    mode = Correct;
    // if input initialization fails, return empty correction queue
    if (!init_input(line)) {
        return CorrectionQueue();
    }
    max_time = 0.0;
    if (time_cutoff > 0.0) {
        max_time = time_cutoff;
        start_clock = clock();
        call_counter = 0;
        limit_reached = false;
    }
    set_limiting_behaviour(nbest, maxweight, beam);
    nbest_queue = WeightQueue();
    // The queue for our suggestions
    CorrectionQueue correction_queue;
    // A placeholding map, only one weight per correction
    std::map<std::string, Weight> corrections;
    SymbolNumber first_input = (input.size() == 0) ? 0 : input[0];
    if (cache[first_input].empty) {
        build_cache(first_input); // XXX: cache corrupts limit!
    }
    if (input.size() <= 1) {
        // get the cached results and we're done
        StringWeightVector * results;
        if (input.size() == 0) {
            results = &cache[first_input].results_len_0;
        } else {
            results = &cache[first_input].results_len_1;
        }
        for(auto& it : *results) {
              // First get the correct weight limit
                best_suggestion = std::min(best_suggestion, it.second);
                if (nbest > 0) {
                    nbest_queue.push(it.second);
                    if (nbest_queue.size() > nbest) {
                        nbest_queue.pop();
                    }
                }
            }
        set_limiting_behaviour(nbest, maxweight, beam);
        adjust_weight_limits(nbest, beam);
        for(auto& it : *results) {
              // Then collect the results
            if (it.second <= limit && (nbest == 0 || // we either don't have an nbest condition or
                                        (it.second <= nbest_queue.get_highest() && // we're below the worst nbest weight and
                                         correction_queue.size() < nbest &&
                                         nbest_queue.size() > 0))) { // number of results
                correction_queue.push(StringWeightPair(it.first, it.second));
                if (nbest != 0) {
                    nbest_queue.pop();
                }
            }
        }
        return correction_queue;
    } else {
        // populate the tree node queue
        queue.assign(cache[first_input].nodes.begin(), cache[first_input].nodes.end());
    }
    // TreeNode start_node(FlagDiacriticState(get_state_size(), 0));
    // queue.assign(1, start_node);

    while (queue.size() > 0) {
        // Have we spent too much time?
        if (max_time > 0.0) {
            ++call_counter;
            if (limit_reached ||
                (call_counter % 1000000 == 0 &&
                 (((double)(clock() - start_clock)) / CLOCKS_PER_SEC) > max_time)) {
                limit_reached = true;
                break;
            }
        }
        /*
          For depth-first searching, we save the back node now, remove it
          from the queue and add new nodes to the search at the back.
        */
        next_node = queue.back();
        queue.pop_back();
        set_limiting_behaviour(nbest, maxweight, beam); // XXX: need to reset
        adjust_weight_limits(nbest, beam);
        // if we can't get an acceptable result, never mind
        if (next_node.weight > limit) {
            continue;
        }
        if (next_node.input_state > 1) {
            // Early epsilons were handled during the caching stage
            lexicon_epsilons();
            mutator_epsilons();
        }
        if (next_node.input_state == input.size()) {
            /* if our transducers are in final states
             * we generate the correction
             */
            if (!mutator->is_final(next_node.mutator_state)) {
            }
            if (!lexicon->is_final(next_node.lexicon_state)) {
            }
            if (mutator->is_final(next_node.mutator_state) &&
                lexicon->is_final(next_node.lexicon_state)) {
                Weight weight = next_node.weight +
                    lexicon->final_weight(next_node.lexicon_state) +
                    mutator->final_weight(next_node.mutator_state);
                if (weight > limit) {
                    continue;
                }
                std::string string = stringify(lexicon->get_key_table(), next_node.string);
                /* if the correction is novel or better than before, insert it
                 */
                if (corrections.count(string) == 0 ||
                    corrections[string] > weight) {
                    corrections[string] = weight;
                    best_suggestion = std::min(best_suggestion, weight);
                    if (nbest > 0) {
                        nbest_queue.push(weight);
                        if (nbest_queue.size() > nbest) {
                            nbest_queue.pop();
                        }
                    }
                }
            }
        } else {
            consume_input();
        }
    }
    adjust_weight_limits(nbest, beam);

    for (auto& it : corrections) {
        if (it.second <= limit && // we're not over our weight limit and
            (nbest == 0 || // we either don't have an nbest condition or
             (it.second <= nbest_queue.get_highest() && // we're below the worst nbest weight and
              correction_queue.size() < nbest &&
              nbest_queue.size() > 0))) { // number of results
            correction_queue.push(StringWeightPair(it.first, it.second));
            if (nbest != 0) {
                nbest_queue.pop();
            }
        }
    }
    //cache[first_input].clear();
    return correction_queue;
}

void Speller::set_limiting_behaviour(int nbest, Weight maxweight, Weight beam)
{
    limiting = None;
    limit = std::numeric_limits<Weight>::max();
    best_suggestion = std::numeric_limits<Weight>::max();
    if (maxweight >= 0.0 && nbest > 0 && beam >= 0.0) {
        limiting = MaxWeightNbestBeam;
        limit = maxweight;
    } else if (maxweight >= 0.0 && nbest > 0 && beam < 0.0) {
        limiting = MaxWeightNbest;
        limit = maxweight;
    } else if (maxweight >= 0.0 && beam >= 0.0 && nbest == 0) {
        limiting = MaxWeightBeam;
        limit = maxweight;
    } else if (maxweight < 0.0 && nbest > 0 && beam >= 0.0) {
        limiting = NbestBeam;
    } else if (maxweight >= 0.0 && nbest == 0 && beam < 0.0) {
        limiting = MaxWeight;
        limit = maxweight;
    } else if (maxweight < 0.0 && nbest > 0 && beam < 0.0) {
        limiting = Nbest;
    } else if (maxweight < 0.0 && nbest == 0 && beam >= 0.0) {
        limiting = Beam;
    } else {
        return;
    }
}

void Speller::adjust_weight_limits(int nbest, Weight beam)
{
    if (limiting == MaxWeight) {
        return;
    } else if (limiting == Nbest && nbest_queue.size() >= nbest) {
        limit = nbest_queue.get_highest();
    } else if (limiting == MaxWeightNbest && nbest_queue.size() >= nbest) {
        limit = std::min(limit, nbest_queue.get_lowest());
    } else if (limiting == Beam && best_suggestion < std::numeric_limits<Weight>::max()) {
        limit = best_suggestion + beam;
    } else if (limiting == NbestBeam) {
        if (best_suggestion < std::numeric_limits<Weight>::max()) {
            if (nbest_queue.size() >= nbest) {
                limit = std::min(best_suggestion + beam, nbest_queue.get_lowest());
            } else {
                limit = best_suggestion + beam;
            }
        }
    } else if (limiting == MaxWeightBeam) {
        if (best_suggestion < std::numeric_limits<Weight>::max()) {
            limit = std::min(best_suggestion + beam, limit);
        }
    } else if (limiting == MaxWeightNbestBeam) {
        if (best_suggestion < std::numeric_limits<Weight>::max()) {
            limit = std::min(limit, best_suggestion + beam);
        }
        if (nbest_queue.size() >= nbest) {
            limit = std::min(limit, nbest_queue.get_lowest());
        }
    }
}

bool Speller::check(char * line)
{
    mode = Check;
    if (!init_input(line)) {
        return false;
    }
    TreeNode start_node(FlagDiacriticState(get_state_size(), 0));
    queue.assign(1, start_node);
    limit = std::numeric_limits<Weight>::max();

    while (queue.size() > 0) {
        next_node = queue.back();
        queue.pop_back();
        if (next_node.input_state == input.size() &&
            lexicon->is_final(next_node.lexicon_state)) {
            return true;
        }
        lexicon_epsilons();
        lexicon_consume();
    }
    return false;
}

std::string stringify(KeyTable * key_table,
                      SymbolVector & symbol_vector)
{
    std::string s;
    for (auto& it : symbol_vector) {
        if (it < key_table->size()) {
            s.append(key_table->at(it));
        }
    }
    return s;
}

std::vector<std::string> symbolify(KeyTable * key_table,
                                   SymbolVector & symbol_vector)
{
    std::vector<std::string> s;
    for (auto& it : symbol_vector) {
        if (it < key_table->size()) {
            s.push_back(key_table->at(it));
        }
    }
    return s;
}

void Speller::build_alphabet_translator(void)
{
    TransducerAlphabet * from = mutator->get_alphabet();
    TransducerAlphabet * to = lexicon->get_alphabet();
    KeyTable * from_keys = from->get_key_table();
    StringSymbolMap * to_symbols = to->get_string_to_symbol();
    alphabet_translator.push_back(0); // zeroth element is always epsilon
    for (SymbolNumber i = 1; i < from_keys->size(); ++i) {
        if (to_symbols->count(from_keys->operator[](i)) != 1) {
            // A symbol in the error source isn't present in the
            // lexicon, so we add it.
            std::string sym = from_keys->operator[](i);
            SymbolNumber lexicon_key = static_cast<SymbolNumber>(lexicon->get_key_table()->size());
            lexicon->get_encoder()->read_input_symbol(sym, lexicon_key);
            lexicon->get_alphabet()->add_symbol(sym);
            alphabet_translator.push_back(lexicon_key);
            continue;
        }
        // translator at i points to lexicon's symbol for mutator's string for
        // mutator's symbol number i
        alphabet_translator.push_back(
            to_symbols->operator[](
                from_keys->operator[](i)));
    }
}

bool Speller::init_input(char * line)
{
    // Initialize the symbol vector to the tokenization given by encoder.
    // In the case of tokenization failure, valid utf-8 characters
    // are tokenized as unknown and tokenization is reattempted from
    // such a character onwards. The empty string is tokenized as an
    // empty vector; there is no end marker.
    input.clear();
    SymbolNumber k = NO_SYMBOL;
    char ** inpointer = &line;
    char * oldpointer;

    while (**inpointer != '\0') {
        oldpointer = *inpointer;
        if (mutator != NULL && mode != Check) {
            k = mutator->get_encoder()->find_key(inpointer);
        } else {
            k = lexicon->get_encoder()->find_key(inpointer);
        }
        if (k == NO_SYMBOL) { // no tokenization from alphabet
            int bytes_to_tokenize = nByte_utf8(static_cast<unsigned char>(*oldpointer));
            if (bytes_to_tokenize == 0) {
                return false; // can't parse utf-8 character, admit failure
            } else {
                char new_symbol[5]; // max size plus NUL
                memcpy(new_symbol, oldpointer, bytes_to_tokenize);
                new_symbol[bytes_to_tokenize] = '\0';
                std::string new_symbol_string(new_symbol);
                oldpointer += bytes_to_tokenize;
                *inpointer = oldpointer;
                cache.push_back(CacheContainer());
                if (!lexicon->get_alphabet()->has_string(new_symbol_string)) {
                    lexicon->get_alphabet()->add_symbol(new_symbol_string);
                }
                SymbolNumber k_lexicon = lexicon->get_alphabet()->get_string_to_symbol()
                    ->operator[](new_symbol_string);
                lexicon->get_encoder()->read_input_symbol(new_symbol, k_lexicon);
                if (mutator != NULL) {
                    if (!mutator->get_alphabet()->has_string(new_symbol_string)) {
                        mutator->get_alphabet()->add_symbol(new_symbol_string);
                    }
                    k = mutator->get_alphabet()->get_string_to_symbol()->
                        operator[](new_symbol_string);
                    mutator->get_encoder()->read_input_symbol(new_symbol, k);
                    if (k >= alphabet_translator.size()) {
                        add_symbol_to_alphabet_translator(k_lexicon);
                    }
                }
                input.push_back(k);
                continue;
            }
        } else {
            input.push_back(k);
        }
    }
    return true;
}

void Speller::add_symbol_to_alphabet_translator(SymbolNumber to_sym)
{
    alphabet_translator.push_back(to_sym);
}

} // namespace hfst_ospell

char*
hfst_strndup(const char* s, size_t n)
  {
    char* rv = static_cast<char*>(malloc(sizeof(char)*n+1));
    if (rv == NULL)
      {
          return rv;
      }
    rv = static_cast<char*>(memcpy(rv, s, n));
    if (rv == NULL)
      {
        return rv;
      }
    rv[n] = '\0';
    return rv;
  }
