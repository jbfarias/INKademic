#include "PersistableStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <ObfuscationUtils.h>

#include <cstring>

#include "../../src/util/AtomicFile.h"

namespace {
class JsonHashPrint final : public Print {
 public:
  size_t write(uint8_t value) override { return write(&value, 1); }

  size_t write(const uint8_t* data, size_t length) override {
    for (size_t i = 0; i < length; ++i) {
      hash ^= data[i];
      hash *= 1099511628211ULL;
    }
    byteCount += length;
    return length;
  }

  uint64_t hash = 1469598103934665603ULL;
  size_t byteCount = 0;
};

bool matchesExistingFile(const char* path, const JsonHashPrint& expected) {
  if (!Storage.exists(path)) return false;
  HalFile file = Storage.open(path, O_RDONLY);
  if (!file || file.fileSize64() != expected.byteCount) return false;

  JsonHashPrint actual;
  uint8_t buffer[256];
  while (file.available() > 0) {
    const int readCount = file.read(buffer, sizeof(buffer));
    if (readCount <= 0) {
      file.close();
      return false;
    }
    actual.write(buffer, static_cast<size_t>(readCount));
  }
  file.close();
  return actual.byteCount == expected.byteCount && actual.hash == expected.hash;
}
}  // namespace

bool PersistableStoreBase::writeDocToFile(const char* path, const JsonDocument& doc) {
  if (path == nullptr || path[0] == '\0' || doc.overflowed()) {
    LOG_ERR("PERSIST", "Refusing invalid or overflowed JSON document for %s", path ? path : "(null)");
    return false;
  }

  JsonHashPrint expectedHash;
  if (serializeJson(doc, expectedHash) == 0) {
    LOG_ERR("PERSIST", "Failed to serialize %s", path);
    return false;
  }
  // Settings are often saved after a no-op selection or repeated button
  // release. Avoid touching the SD card at all when the serialized snapshot is
  // byte-for-byte equivalent in length/hash to the current generation.
  if (matchesExistingFile(path, expectedHash)) return true;

  Storage.mkdir("/.crosspoint");
  const std::string targetPath(path);
  if (!AtomicFile::prepare(targetPath, "PERSIST")) return false;

  const std::string temporaryPath = AtomicFile::temporaryPath(targetPath);
  HalFile file = Storage.open(temporaryPath.c_str(), O_WRITE | O_CREAT | O_TRUNC);
  if (!file) {
    LOG_ERR("PERSIST", "Failed to open temporary file %s", temporaryPath.c_str());
    return false;
  }

  // Stream directly to the SD card. Building a second String copy of the JSON
  // is a surprisingly large transient allocation on X4 Pro when settings or
  // state contain long paths and shortcut maps.
  const size_t expectedBytes = measureJson(doc);
  const size_t written = serializeJson(doc, file);
  const bool synced = file.sync();
  file.close();
  if (!synced || written != expectedBytes) {
    LOG_ERR("PERSIST", "Incomplete JSON write for %s (%zu of %zu, synced=%d)", path, written, expectedBytes,
            synced ? 1 : 0);
    Storage.remove(temporaryPath.c_str());
    return false;
  }

  if (!AtomicFile::commit(targetPath, "PERSIST")) {
    Storage.remove(temporaryPath.c_str());
    LOG_ERR("PERSIST", "Failed to atomically install %s", path);
    return false;
  }
  return true;
}

bool PersistableStoreBase::readDocFromFile(const char* path, JsonDocument& doc) {
  if (!Storage.exists(path)) {
    return false;  // Expected on first boot — not an error.
  }
  String json = Storage.readFile(path);
  if (json.isEmpty()) {
    LOG_ERR("PERSIST", "Failed to read %s (empty)", path);
    return false;
  }
  auto error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("PERSIST", "JSON parse error in %s: %s", path, error.c_str());
    return false;
  }
  return true;
}

std::string PersistableStoreBase::extractPassword(JsonVariantConst doc, bool& needsResave) {
  obfuscation::DecodeStatus status = obfuscation::DecodeStatus::INVALID;
  std::string pass = obfuscation::deobfuscateFromBase64(doc["password_obf"] | "", &status);
  if (status == obfuscation::DecodeStatus::LEGACY && !pass.empty()) {
    needsResave = true;
  }
  if (status == obfuscation::DecodeStatus::INVALID || status == obfuscation::DecodeStatus::EMPTY || pass.empty()) {
    // Deobfuscation failed or no obfuscated password was stored; fall back to legacy plaintext.
    pass = doc["password"] | "";
    if (!pass.empty()) needsResave = true;
  }
  return pass;
}
