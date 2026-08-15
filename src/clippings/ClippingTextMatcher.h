#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ClippingTextMatcher {

enum class TokenFragmentMatch : uint8_t {
  MISMATCH,
  CONTINUES_TOKEN,
  COMPLETES_TOKEN,
};

// Compares one rendered word fragment with a saved clipping token. An inserted
// layout hyphen joins this fragment to the following rendered word instead of
// consuming a clipping token by itself.
inline TokenFragmentMatch matchTokenFragment(const char* word, const bool endsWithInsertedHyphen, const char* token,
                                             const size_t tokenLen, const size_t tokenOffset) {
  if (!word || !token || tokenLen == 0 || tokenOffset >= tokenLen) {
    return TokenFragmentMatch::MISMATCH;
  }

  size_t wordLen = std::strlen(word);
  if (endsWithInsertedHyphen) {
    if (wordLen == 0 || word[wordLen - 1] != '-') {
      return TokenFragmentMatch::MISMATCH;
    }
    wordLen--;
  }

  const size_t remainingLen = tokenLen - tokenOffset;
  if (wordLen == 0 || wordLen > remainingLen || std::strncmp(word, token + tokenOffset, wordLen) != 0) {
    return TokenFragmentMatch::MISMATCH;
  }

  if (endsWithInsertedHyphen) {
    return wordLen < remainingLen ? TokenFragmentMatch::CONTINUES_TOKEN : TokenFragmentMatch::MISMATCH;
  }
  return wordLen == remainingLen ? TokenFragmentMatch::COMPLETES_TOKEN : TokenFragmentMatch::MISMATCH;
}

}  // namespace ClippingTextMatcher
