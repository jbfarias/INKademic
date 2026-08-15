#pragma once

#include <string>

class ClippingsManager {
 public:
  static bool saveClipping(const std::string& bookTitle, const std::string& author, const std::string& chapterTitle,
                           int pageNumber, const std::string& selectedText, const char* tagName = nullptr);

  static constexpr const char* CLIPPINGS_PATH = "/My Clippings.txt";
};
