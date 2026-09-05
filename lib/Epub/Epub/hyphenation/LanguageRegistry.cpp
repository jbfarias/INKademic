#include "LanguageRegistry.h"

#include <algorithm>
#include <array>

#include "HyphenationCommon.h"
#include "generated/hyph-de.trie.h"
#include "generated/hyph-en.trie.h"
#include "generated/hyph-es.trie.h"
#include "generated/hyph-fr.trie.h"
#include "generated/hyph-it.trie.h"
#ifndef INKADEMIC_LANGUAGE_SET
#include "generated/hyph-pl.trie.h"
#endif
#include "generated/hyph-pt.trie.h"
#ifndef INKADEMIC_LANGUAGE_SET
#include "generated/hyph-ru.trie.h"
#include "generated/hyph-sv.trie.h"
#include "generated/hyph-uk.trie.h"
#endif

namespace {

// English hyphenation patterns (3/3 minimum prefix/suffix length)
LanguageHyphenator englishHyphenator(en_patterns, isLatinLetter, toLowerLatin, 3, 3);
LanguageHyphenator frenchHyphenator(fr_patterns, isLatinLetter, toLowerLatin);
LanguageHyphenator germanHyphenator(de_patterns, isLatinLetter, toLowerLatin);
#ifndef INKADEMIC_LANGUAGE_SET
LanguageHyphenator russianHyphenator(ru_patterns, isCyrillicLetter, toLowerCyrillic);
#endif
LanguageHyphenator spanishHyphenator(es_patterns, isLatinLetter, toLowerLatin);
LanguageHyphenator italianHyphenator(it_patterns, isLatinLetter, toLowerLatin);
#ifndef INKADEMIC_LANGUAGE_SET
LanguageHyphenator swedishHyphenator(sv_patterns, isLatinLetter, toLowerLatin);
LanguageHyphenator ukrainianHyphenator(uk_patterns, isCyrillicLetter, toLowerCyrillic);
LanguageHyphenator polishHyphenator(pl_patterns, isLatinLetter, toLowerLatin);
#endif
LanguageHyphenator portugueseHyphenator(pt_patterns, isLatinLetter, toLowerLatin);

#ifdef INKADEMIC_LANGUAGE_SET
using EntryArray = std::array<LanguageEntry, 6>;
#else
using EntryArray = std::array<LanguageEntry, 10>;
#endif

const EntryArray& entries() {
  static const EntryArray kEntries = {{{"english", "en", &englishHyphenator},
                                       {"french", "fr", &frenchHyphenator},
                                       {"german", "de", &germanHyphenator},
#ifndef INKADEMIC_LANGUAGE_SET
                                       {"russian", "ru", &russianHyphenator},
#endif
                                       {"spanish", "es", &spanishHyphenator},
                                       {"italian", "it", &italianHyphenator},
#ifndef INKADEMIC_LANGUAGE_SET
                                       {"polish", "pl", &polishHyphenator},
#endif
                                       {"portuguese", "pt", &portugueseHyphenator},
#ifndef INKADEMIC_LANGUAGE_SET
                                       {"swedish", "sv", &swedishHyphenator},
                                       {"ukrainian", "uk", &ukrainianHyphenator}
#endif
  }};
  return kEntries;
}

}  // namespace

const LanguageHyphenator* getLanguageHyphenatorForPrimaryTag(const std::string& primaryTag) {
  const auto& allEntries = entries();
  const auto it = std::find_if(allEntries.begin(), allEntries.end(),
                               [&primaryTag](const LanguageEntry& entry) { return primaryTag == entry.primaryTag; });
  return (it != allEntries.end()) ? it->hyphenator : nullptr;
}

LanguageEntryView getLanguageEntries() {
  const auto& allEntries = entries();
  return LanguageEntryView{allEntries.data(), allEntries.size()};
}
