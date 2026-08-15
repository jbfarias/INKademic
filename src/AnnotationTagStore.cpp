#include "AnnotationTagStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Serialization.h>

#include <cstring>
#include <string>

AnnotationTagStore AnnotationTagStore::instance;

const AnnotationTag* AnnotationTagStore::at(const uint8_t index) const {
  return index < tagCount ? &tags[index] : nullptr;
}

uint16_t AnnotationTagStore::idAt(const uint8_t index) const {
  const AnnotationTag* tag = at(index);
  return tag ? tag->id : 0;
}

const char* AnnotationTagStore::nameForId(const uint16_t id) const {
  if (id == 0) return nullptr;
  for (uint8_t i = 0; i < tagCount; ++i) {
    if (tags[i].id == id) return tags[i].name;
  }
  return nullptr;
}

bool AnnotationTagStore::validName(const char* name) const {
  if (!name || name[0] == '\0' || strlen(name) >= ANNOTATION_TAG_NAME_MAX) return false;
  for (uint8_t i = 0; i < tagCount; ++i) {
    if (strcmp(tags[i].name, name) == 0) return false;
  }
  return true;
}

bool AnnotationTagStore::load() {
  if (loaded) return true;
  loaded = true;
  tagCount = 0;
  nextId = 1;

  const std::string backupPath = std::string(FILE_PATH) + ".bak";
  if (!Storage.exists(FILE_PATH) && Storage.exists(backupPath.c_str())) {
    if (!Storage.rename(backupPath.c_str(), FILE_PATH)) {
      LOG_ERR("TAGS", "Failed to recover tag store backup");
      return false;
    }
  }
  if (!Storage.exists(FILE_PATH)) return true;

  FsFile file;
  if (!Storage.openFileForRead("TAGS", FILE_PATH, file)) {
    LOG_ERR("TAGS", "Failed to open tag store");
    return false;
  }

  uint8_t version = 0;
  uint8_t storedCount = 0;
  if (!serialization::tryReadPod(file, version) || version != FILE_VERSION ||
      !serialization::tryReadPod(file, storedCount) || storedCount > ANNOTATION_TAG_MAX ||
      !serialization::tryReadPod(file, nextId)) {
    file.close();
    LOG_ERR("TAGS", "Invalid tag store header");
    tagCount = 0;
    nextId = 1;
    return false;
  }

  for (uint8_t i = 0; i < storedCount; ++i) {
    AnnotationTag tag;
    if (!serialization::tryReadPod(file, tag.id) ||
        file.read(reinterpret_cast<uint8_t*>(tag.name), sizeof(tag.name)) != static_cast<int>(sizeof(tag.name))) {
      file.close();
      tagCount = 0;
      nextId = 1;
      LOG_ERR("TAGS", "Truncated tag store at record %u", i);
      return false;
    }
    tag.name[sizeof(tag.name) - 1] = '\0';
    if (tag.id == 0 || tag.name[0] == '\0') continue;
    tags[tagCount++] = tag;
  }
  file.close();

  if (nextId == 0) nextId = 1;
  return true;
}

bool AnnotationTagStore::save() const {
  Storage.mkdir("/.crosspoint");

  const std::string tmpPath = std::string(FILE_PATH) + ".tmp";
  const std::string backupPath = std::string(FILE_PATH) + ".bak";
  if (Storage.exists(tmpPath.c_str())) Storage.remove(tmpPath.c_str());
  if (Storage.exists(backupPath.c_str()) && Storage.exists(FILE_PATH)) Storage.remove(backupPath.c_str());

  FsFile file;
  if (!Storage.openFileForWrite("TAGS", tmpPath, file)) {
    LOG_ERR("TAGS", "Failed to open tag store for write");
    return false;
  }

  bool ok = serialization::tryWritePod(file, FILE_VERSION) && serialization::tryWritePod(file, tagCount) &&
            serialization::tryWritePod(file, nextId);
  for (uint8_t i = 0; ok && i < tagCount; ++i) {
    ok = serialization::tryWritePod(file, tags[i].id) &&
         file.write(reinterpret_cast<const uint8_t*>(tags[i].name), sizeof(tags[i].name)) ==
             static_cast<int>(sizeof(tags[i].name));
  }
  ok = ok && file.sync();
  file.close();
  if (!ok) {
    Storage.remove(tmpPath.c_str());
    LOG_ERR("TAGS", "Failed to write tag store");
    return false;
  }

  const bool hadCurrent = Storage.exists(FILE_PATH);
  if (hadCurrent && !Storage.rename(FILE_PATH, backupPath.c_str())) {
    Storage.remove(tmpPath.c_str());
    LOG_ERR("TAGS", "Failed to back up tag store");
    return false;
  }
  if (!Storage.rename(tmpPath.c_str(), FILE_PATH)) {
    if (hadCurrent) Storage.rename(backupPath.c_str(), FILE_PATH);
    LOG_ERR("TAGS", "Failed to replace tag store");
    return false;
  }
  if (hadCurrent && Storage.exists(backupPath.c_str())) Storage.remove(backupPath.c_str());
  return true;
}

bool AnnotationTagStore::add(const char* name) {
  if (!loaded) load();
  if (tagCount >= ANNOTATION_TAG_MAX || !validName(name)) return false;

  AnnotationTag& tag = tags[tagCount++];
  tag.id = nextId++;
  if (tag.id == 0) tag.id = nextId++;
  strncpy(tag.name, name, sizeof(tag.name) - 1);
  tag.name[sizeof(tag.name) - 1] = '\0';
  if (save()) return true;

  --tagCount;
  return false;
}

bool AnnotationTagStore::rename(const uint8_t index, const char* name) {
  if (!loaded) load();
  if (index >= tagCount || !name || name[0] == '\0' || strlen(name) >= ANNOTATION_TAG_NAME_MAX) return false;
  for (uint8_t i = 0; i < tagCount; ++i) {
    if (i != index && strcmp(tags[i].name, name) == 0) return false;
  }

  char oldName[ANNOTATION_TAG_NAME_MAX];
  strncpy(oldName, tags[index].name, sizeof(oldName));
  strncpy(tags[index].name, name, sizeof(tags[index].name) - 1);
  tags[index].name[sizeof(tags[index].name) - 1] = '\0';
  if (save()) return true;

  strncpy(tags[index].name, oldName, sizeof(tags[index].name));
  tags[index].name[sizeof(tags[index].name) - 1] = '\0';
  return false;
}

bool AnnotationTagStore::remove(const uint8_t index) {
  if (!loaded) load();
  if (index >= tagCount) return false;

  const AnnotationTag removed = tags[index];
  for (uint8_t i = index + 1; i < tagCount; ++i) tags[i - 1] = tags[i];
  --tagCount;
  if (save()) return true;

  for (uint8_t i = tagCount; i > index; --i) tags[i] = tags[i - 1];
  tags[index] = removed;
  ++tagCount;
  return false;
}
