#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct PageTag {
  uint16_t spineIndex = 0;
  uint16_t page = 0;
  uint16_t pageCount = 1;
  uint16_t tagId = 0;
};

class PageTagStore {
 public:
  static PageTagStore& getInstance() { return instance; }

  bool loadForBook(const std::string& filePath, const std::string& title, const std::string& author,
                  const std::string& bookType);
  void unload();

  uint16_t tagForPage(uint16_t spineIndex, uint16_t page, uint16_t pageCount) const;
  bool setTagForPage(uint16_t spineIndex, uint16_t page, uint16_t pageCount, uint16_t tagId);
  bool hasTagForPage(uint16_t spineIndex, uint16_t page, uint16_t pageCount) const {
    return tagForPage(spineIndex, page, pageCount) != 0;
  }

  static void deleteForFilePath(const std::string& filePath, const std::string& bookType);
  static bool migrateForFilePath(const std::string& oldFilePath, const std::string& newFilePath,
                                 const std::string& title, const std::string& author,
                                 const std::string& bookType);

 private:
  static PageTagStore instance;

  std::vector<PageTag> pageTags;
  std::string bookFilePath;
  std::string bookTitle;
  std::string bookAuthor;
  std::string bookDocumentId;
  std::string storeFilePath;
  bool dirty = false;

  bool readFromFile();
  bool writeToFile() const;
  static std::string storeFilePathForBook(const std::string& filePath, const std::string& bookType);
};

#define PAGE_TAGS PageTagStore::getInstance()
