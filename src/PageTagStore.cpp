#include "PageTagStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Serialization.h>
#include <uzlib.h>

#include <algorithm>
#include <utility>

#include "KOReaderDocumentId.h"

namespace {
constexpr uint8_t FILE_VERSION = 1;
constexpr uint16_t MAX_PAGE_TAGS = 256;
constexpr char PAGE_TAGS_DIR[] = "/.crosspoint/page-tags";
}

PageTagStore PageTagStore::instance;

std::string PageTagStore::storeFilePathForBook(const std::string& filePath, const std::string& bookType) {
  const uint32_t crc = uzlib_crc32(filePath.data(), static_cast<unsigned int>(filePath.size()), 0);
  return std::string(PAGE_TAGS_DIR) + "/" + bookType + "_" + std::to_string(crc) + ".bin";
}

bool PageTagStore::loadForBook(const std::string& filePath, const std::string& title, const std::string& author,
                               const std::string& bookType) {
  if (bookType != "epub") {
    LOG_ERR("TAGS", "Unknown page tag book type: %s", bookType.c_str());
    return false;
  }

  bookFilePath = filePath;
  bookTitle = title;
  bookAuthor = author;
  bookDocumentId = KOReaderDocumentId::calculate(filePath);
  storeFilePath = storeFilePathForBook(filePath, bookType);
  pageTags.clear();
  pageTags.reserve(8);
  dirty = false;

  const std::string backupPath = storeFilePath + ".bak";
  if (!Storage.exists(storeFilePath.c_str()) && Storage.exists(backupPath.c_str())) {
    if (!Storage.rename(backupPath.c_str(), storeFilePath.c_str())) {
      LOG_ERR("TAGS", "Failed to recover page tag store backup: %s", backupPath.c_str());
      return false;
    }
  }
  if (!Storage.exists(storeFilePath.c_str())) return true;
  return readFromFile();
}

void PageTagStore::unload() {
  if (dirty) writeToFile();
  pageTags.clear();
  bookFilePath.clear();
  bookTitle.clear();
  bookAuthor.clear();
  bookDocumentId.clear();
  storeFilePath.clear();
  dirty = false;
}

uint16_t PageTagStore::tagForPage(const uint16_t spineIndex, const uint16_t page, const uint16_t pageCount) const {
  const auto it = std::find_if(pageTags.begin(), pageTags.end(), [&](const PageTag& entry) {
    return entry.spineIndex == spineIndex && entry.page == page &&
           (pageCount == 0 || entry.pageCount == pageCount);
  });
  return it == pageTags.end() ? 0 : it->tagId;
}

bool PageTagStore::setTagForPage(const uint16_t spineIndex, const uint16_t page, const uint16_t pageCount,
                                 const uint16_t tagId) {
  auto it = std::find_if(pageTags.begin(), pageTags.end(), [&](const PageTag& entry) {
    return entry.spineIndex == spineIndex && entry.page == page;
  });

  if (tagId == 0) {
    if (it == pageTags.end()) return true;
    const size_t index = static_cast<size_t>(it - pageTags.begin());
    const PageTag removed = *it;
    pageTags.erase(it);
    dirty = true;
    if (writeToFile()) {
      dirty = false;
      return true;
    }
    pageTags.insert(pageTags.begin() + index, removed);
  } else if (it != pageTags.end()) {
    const PageTag previous = *it;
    it->pageCount = std::max<uint16_t>(1, pageCount);
    it->tagId = tagId;
    dirty = true;
    if (writeToFile()) {
      dirty = false;
      return true;
    }
    *it = previous;
  } else {
    if (pageTags.size() >= MAX_PAGE_TAGS) {
      LOG_ERR("TAGS", "Page tag limit (%u) reached", MAX_PAGE_TAGS);
      return false;
    }
    pageTags.push_back(PageTag{spineIndex, page, std::max<uint16_t>(1, pageCount), tagId});
    dirty = true;
    if (writeToFile()) {
      dirty = false;
      return true;
    }
    pageTags.pop_back();
  }

  dirty = true;
  return false;
}

bool PageTagStore::readFromFile() {
  FsFile file;
  if (!Storage.openFileForRead("TAGS", storeFilePath, file)) return false;

  uint8_t version = 0;
  uint16_t count = 0;
  std::string title;
  std::string author;
  std::string path;
  std::string documentId;
  if (!serialization::tryReadPod(file, version) || version != FILE_VERSION ||
      !serialization::tryReadPod(file, count) || count > MAX_PAGE_TAGS ||
      !serialization::tryReadString(file, title) || !serialization::tryReadString(file, author) ||
      !serialization::tryReadString(file, path) || !serialization::tryReadString(file, documentId)) {
    file.close();
    LOG_ERR("TAGS", "Invalid page tag store: %s", storeFilePath.c_str());
    return false;
  }

  if (path != bookFilePath || (!documentId.empty() && documentId != bookDocumentId)) {
    file.close();
    LOG_ERR("TAGS", "Ignoring page tags belonging to another document: %s", storeFilePath.c_str());
    return false;
  }

  std::vector<PageTag> loaded;
  loaded.reserve(count);
  for (uint16_t i = 0; i < count; ++i) {
    PageTag entry;
    if (!serialization::tryReadPod(file, entry.spineIndex) || !serialization::tryReadPod(file, entry.page) ||
        !serialization::tryReadPod(file, entry.pageCount) || !serialization::tryReadPod(file, entry.tagId)) {
      file.close();
      LOG_ERR("TAGS", "Truncated page tag store at record %u", i);
      return false;
    }
    if (entry.tagId != 0) loaded.push_back(entry);
  }
  file.close();
  pageTags = std::move(loaded);
  return true;
}

bool PageTagStore::writeToFile() const {
  Storage.mkdir("/.crosspoint");
  Storage.mkdir(PAGE_TAGS_DIR);

  const std::string tmpPath = storeFilePath + ".tmp";
  const std::string backupPath = storeFilePath + ".bak";
  if (Storage.exists(tmpPath.c_str())) Storage.remove(tmpPath.c_str());
  if (Storage.exists(backupPath.c_str()) && Storage.exists(storeFilePath.c_str())) {
    Storage.remove(backupPath.c_str());
  }

  FsFile file;
  if (!Storage.openFileForWrite("TAGS", tmpPath, file)) {
    LOG_ERR("TAGS", "Failed to open page tag store for write: %s", storeFilePath.c_str());
    return false;
  }

  const uint16_t count = static_cast<uint16_t>(std::min<size_t>(pageTags.size(), MAX_PAGE_TAGS));
  bool ok = serialization::tryWritePod(file, FILE_VERSION) && serialization::tryWritePod(file, count) &&
            serialization::tryWriteString(file, bookTitle) && serialization::tryWriteString(file, bookAuthor) &&
            serialization::tryWriteString(file, bookFilePath) && serialization::tryWriteString(file, bookDocumentId);
  for (uint16_t i = 0; ok && i < count; ++i) {
    const PageTag& entry = pageTags[i];
    ok = serialization::tryWritePod(file, entry.spineIndex) && serialization::tryWritePod(file, entry.page) &&
         serialization::tryWritePod(file, entry.pageCount) && serialization::tryWritePod(file, entry.tagId);
  }
  ok = ok && file.sync();
  file.close();
  if (!ok) {
    Storage.remove(tmpPath.c_str());
    LOG_ERR("TAGS", "Failed to write page tag store: %s", storeFilePath.c_str());
    return false;
  }

  const bool hadCurrent = Storage.exists(storeFilePath.c_str());
  if (hadCurrent && !Storage.rename(storeFilePath.c_str(), backupPath.c_str())) {
    Storage.remove(tmpPath.c_str());
    LOG_ERR("TAGS", "Failed to back up page tag store: %s", storeFilePath.c_str());
    return false;
  }
  if (!Storage.rename(tmpPath.c_str(), storeFilePath.c_str())) {
    if (hadCurrent) Storage.rename(backupPath.c_str(), storeFilePath.c_str());
    LOG_ERR("TAGS", "Failed to replace page tag store: %s", storeFilePath.c_str());
    return false;
  }
  if (hadCurrent && Storage.exists(backupPath.c_str())) Storage.remove(backupPath.c_str());
  return true;
}

void PageTagStore::deleteForFilePath(const std::string& filePath, const std::string& bookType) {
  const std::string path = storeFilePathForBook(filePath, bookType);
  if (Storage.exists(path.c_str())) Storage.remove(path.c_str());
  const std::string tmpPath = path + ".tmp";
  const std::string backupPath = path + ".bak";
  if (Storage.exists(tmpPath.c_str())) Storage.remove(tmpPath.c_str());
  if (Storage.exists(backupPath.c_str())) Storage.remove(backupPath.c_str());
}

bool PageTagStore::migrateForFilePath(const std::string& oldFilePath, const std::string& newFilePath,
                                      const std::string& title, const std::string& author,
                                      const std::string& bookType) {
  if (bookType != "epub" || oldFilePath.empty() || newFilePath.empty() || oldFilePath == newFilePath) return true;

  const std::string oldStorePath = storeFilePathForBook(oldFilePath, bookType);
  if (!Storage.exists(oldStorePath.c_str())) return true;

  PageTagStore reader;
  reader.bookFilePath = oldFilePath;
  reader.bookDocumentId = KOReaderDocumentId::calculate(newFilePath);
  reader.storeFilePath = oldStorePath;
  if (!reader.readFromFile()) return false;

  PageTagStore writer;
  writer.bookFilePath = newFilePath;
  writer.bookTitle = title;
  writer.bookAuthor = author;
  writer.bookDocumentId = KOReaderDocumentId::calculate(newFilePath);
  writer.storeFilePath = storeFilePathForBook(newFilePath, bookType);
  writer.pageTags = std::move(reader.pageTags);
  if (!writer.writeToFile()) return false;

  if (oldStorePath != writer.storeFilePath && Storage.exists(oldStorePath.c_str())) {
    if (!Storage.remove(oldStorePath.c_str())) {
      LOG_ERR("TAGS", "Failed to remove migrated page tag source: %s", oldStorePath.c_str());
      return false;
    }
  }
  return true;
}
