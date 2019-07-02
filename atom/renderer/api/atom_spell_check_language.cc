// Copyright (c) 2019 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "atom/renderer/api/atom_spell_check_language.h"

#include <utility>

#include "base/logging.h"
#include "components/spellcheck/renderer/spellcheck_worditerator.h"
#include "components/spellcheck/renderer/spelling_engine.h"

namespace atom {

SpellcheckLanguage::Word::Word() = default;

SpellcheckLanguage::Word::Word(const Word& word) = default;

SpellcheckLanguage::Word::~Word() = default;

bool SpellcheckLanguage::Word::operator==(const Word& word) const {
  return result.location == word.result.location && text == word.text;
}

SpellcheckLanguage::SpellcheckLanguage() {}

SpellcheckLanguage::~SpellcheckLanguage() {}

void SpellcheckLanguage::Init(const std::string& language) {
  character_attributes_.SetDefaultLanguage(language);
  text_iterator_.Reset();
  contraction_iterator_.Reset();
  language_ = language;
}

bool SpellcheckLanguage::InitializeIfNeeded() {
  return true;
}

std::set<base::string16> SpellcheckLanguage::SpellCheckText(
    const base::string16& text,
    std::vector<SpellcheckLanguage::Word>& word_list) {
  if (!text_iterator_.IsInitialized() &&
      !text_iterator_.Initialize(&character_attributes_, true)) {
    // We failed to initialize text_iterator_, return as spelled correctly.
    VLOG(1) << "Failed to initialize SpellcheckWordIterator";
    return std::set<base::string16>();
  }

  if (!contraction_iterator_.IsInitialized() &&
      !contraction_iterator_.Initialize(&character_attributes_, false)) {
    // We failed to initialize the word iterator, return as spelled correctly.
    VLOG(1) << "Failed to initialize contraction_iterator_";
    return std::set<base::string16>();
  }

  text_iterator_.SetText(text.c_str(), text.size());

  base::string16 word;
  size_t word_start;
  size_t word_length;
  std::set<base::string16> words;
  Word word_entry;
  for (;;) {  // Run until end of text
    const auto status =
        text_iterator_.GetNextWord(&word, &word_start, &word_length);
    if (status == SpellcheckWordIterator::IS_END_OF_TEXT)
      break;
    if (status == SpellcheckWordIterator::IS_SKIPPABLE)
      continue;

    word_entry.result.location = base::checked_cast<int>(word_start);
    word_entry.result.length = base::checked_cast<int>(word_length);
    word_entry.text = word;
    word_entry.contraction_words.clear();
    auto found = find(word_list.begin(), word_list.end(), word_entry);
    if (found != word_list.end()) {
      found->misspelled_count++;
    } else {
      word_entry.misspelled_count = 1;
      word_list.push_back(word_entry);
    }
    words.insert(word);
    // If the given word is a concatenated word of two or more valid words
    // (e.g. "hello:hello"), we should treat it as a valid word.
    if (IsContraction(word, &word_entry.contraction_words)) {
      for (const auto& w : word_entry.contraction_words) {
        words.insert(w);
      }
    }
  }
  return words;
}

// Returns whether or not the given string is a contraction.
// This function is a fall-back when the SpellcheckWordIterator class
// returns a concatenated word which is not in the selected dictionary
// (e.g. "in'n'out") but each word is valid.
// Output variable contraction_words will contain individual
// words in the contraction.
bool SpellcheckLanguage::IsContraction(
    const base::string16& contraction,
    std::vector<base::string16>* contraction_words) {
  DCHECK(contraction_iterator_.IsInitialized());

  contraction_iterator_.SetText(contraction.c_str(), contraction.length());

  base::string16 word;
  size_t word_start;
  size_t word_length;
  for (auto status =
           contraction_iterator_.GetNextWord(&word, &word_start, &word_length);
       status != SpellcheckWordIterator::IS_END_OF_TEXT;
       status = contraction_iterator_.GetNextWord(&word, &word_start,
                                                  &word_length)) {
    if (status == SpellcheckWordIterator::IS_SKIPPABLE)
      continue;

    contraction_words->push_back(word);
  }
  return contraction_words->size() > 1;
}

bool SpellcheckLanguage::IsEnabled() {
  return true;
}

}  // namespace atom