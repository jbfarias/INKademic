#include "OpdsServerStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <ObfuscationUtils.h>

#include <algorithm>
#include <cstring>
#include <utility>

#include "CrossPointSettings.h"

namespace {
constexpr char FILENAME_FORMAT_AUTHOR_TITLE[] = "author_title";
constexpr char FILENAME_FORMAT_TITLE_AUTHOR[] = "title_author";
constexpr char PROJECT_GUTENBERG_NAME[] = "Project Gutenberg";
constexpr char PROJECT_GUTENBERG_OPDS_URL[] = "https://www.gutenberg.org/ebooks/search.opds/";
}  // namespace

const char* opdsFilenameFormatToJson(const OpdsFilenameFormat format) {
  switch (format) {
    case OpdsFilenameFormat::TITLE_AUTHOR:
      return FILENAME_FORMAT_TITLE_AUTHOR;
    case OpdsFilenameFormat::AUTHOR_TITLE:
    default:
      return FILENAME_FORMAT_AUTHOR_TITLE;
  }
}

OpdsFilenameFormat opdsFilenameFormatFromJson(const char* value) {
  if (value && strcmp(value, FILENAME_FORMAT_TITLE_AUTHOR) == 0) {
    return OpdsFilenameFormat::TITLE_AUTHOR;
  }
  return OpdsFilenameFormat::AUTHOR_TITLE;
}

void OpdsServerStore::toJson(JsonDocument& doc) const {
  doc["defaultCatalogsSeeded"] = defaultCatalogsSeeded_;
  JsonArray arr = doc["servers"].to<JsonArray>();
  for (const auto& server : servers) {
    JsonObject obj = arr.add<JsonObject>();
    obj["name"] = server.name;
    obj["url"] = server.url;
    obj["username"] = server.username;
    obj["password_obf"] = obfuscation::obfuscateToBase64(server.password);
    obj["filenameFormat"] = opdsFilenameFormatToJson(server.filenameFormat);
  }
}

bool OpdsServerStore::fromJson(JsonVariantConst doc) {
  // Tolerate a missing/invalid 'servers' key (treat as empty list); only a
  // JSON parse error is fatal. A null JsonArray iterates zero times.
  servers.clear();
  defaultCatalogsSeeded_ = doc["defaultCatalogsSeeded"] | false;
  JsonArrayConst arr = doc["servers"].as<JsonArrayConst>();
  servers.reserve(std::min(arr.size(), MAX_SERVERS));
  bool needsResave = false;

  for (JsonObjectConst obj : arr) {
    if (servers.size() >= OpdsServerStore::MAX_SERVERS) break;
    OpdsServer server;
    server.name = obj["name"] | "";
    server.url = obj["url"] | "";
    server.username = obj["username"] | "";
    server.filenameFormat = opdsFilenameFormatFromJson(obj["filenameFormat"] | "");
    obfuscation::DecodeStatus status = obfuscation::DecodeStatus::INVALID;
    server.password = obfuscation::deobfuscateFromBase64(obj["password_obf"] | "", &status);
    if (status == obfuscation::DecodeStatus::LEGACY && !server.password.empty()) {
      needsResave = true;
    }
    if (status == obfuscation::DecodeStatus::INVALID || status == obfuscation::DecodeStatus::EMPTY ||
        server.password.empty()) {
      server.password = obj["password"] | "";
      if (!server.password.empty()) needsResave = true;
    }
    if (status == obfuscation::DecodeStatus::INVALID && server.password.empty()) {
      LOG_ERR("OPS", "Ignoring unreadable password for OPDS server: %s", server.name.c_str());
    }
    servers.push_back(std::move(server));
  }

  if (needsResave) {
    LOG_DBG("OPS", "Resaving JSON with obfuscated passwords");
    requestResave();
  }

  // Add bundled catalogs once when upgrading an older OPDS store. The marker
  // is persisted so deleting a bundled catalog remains a user choice.
  if (!defaultCatalogsSeeded_ && addBundledCatalogs()) {
    defaultCatalogsSeeded_ = true;
    requestResave();
    LOG_DBG("OPS", "Added bundled Project Gutenberg catalog");
  }

  return true;
}

bool OpdsServerStore::loadFromFile() {
  servers.clear();
  loaded_ = true;
  const bool hasStoreFile = Storage.exists(getFilePath());
  if (PersistableStore<OpdsServerStore>::loadFromFile()) {
    return true;
  }
  if (hasStoreFile) {
    return false;
  }

  if (migrateFromSettings()) {
    LOG_DBG("OPS", "Migrated legacy OPDS settings");
    return true;
  }
  // A failed legacy migration must not fall through to the default catalog
  // creation path, otherwise the legacy URL could be stranded in settings.
  if (strlen(SETTINGS.opdsServerUrl) != 0) return false;

  if (!addBundledCatalogs()) return false;
  defaultCatalogsSeeded_ = true;
  if (saveToFile()) {
    LOG_DBG("OPS", "Created default OPDS catalog store");
    return true;
  }

  // Do not expose an in-memory catalog if its persistent store could not be
  // created; the next boot can retry safely.
  servers.clear();
  defaultCatalogsSeeded_ = false;
  return false;
}

void OpdsServerStore::ensureLoaded() const {
  if (loaded_) return;
  const_cast<OpdsServerStore*>(this)->loadFromFile();
}

void OpdsServerStore::release() {
  std::vector<OpdsServer>().swap(servers);
  defaultCatalogsSeeded_ = false;
  loaded_ = false;
}

bool OpdsServerStore::addBundledCatalogs() {
  for (const auto& server : servers) {
    if (server.url == PROJECT_GUTENBERG_OPDS_URL) return true;
  }

  if (servers.size() >= MAX_SERVERS) {
    LOG_DBG("OPS", "Cannot add bundled catalogs: limit of %zu reached", MAX_SERVERS);
    return false;
  }

  OpdsServer gutenberg;
  gutenberg.name = PROJECT_GUTENBERG_NAME;
  gutenberg.url = PROJECT_GUTENBERG_OPDS_URL;
  gutenberg.filenameFormat = OpdsFilenameFormat::AUTHOR_TITLE;
  servers.push_back(std::move(gutenberg));
  return true;
}

bool OpdsServerStore::migrateFromSettings() {
  if (strlen(SETTINGS.opdsServerUrl) == 0) {
    return false;
  }

  OpdsServer server;
  server.name = "OPDS Server";
  server.url = SETTINGS.opdsServerUrl;
  server.username = SETTINGS.opdsUsername;
  server.password = SETTINGS.opdsPassword;
  servers.push_back(std::move(server));

  if (!addBundledCatalogs()) {
    servers.clear();
    return false;
  }
  defaultCatalogsSeeded_ = true;

  if (saveToFile()) {
    // Clear legacy fields so migration won't run again on next boot.
    SETTINGS.opdsServerUrl[0] = '\0';
    SETTINGS.opdsUsername[0] = '\0';
    SETTINGS.opdsPassword[0] = '\0';
    SETTINGS.saveToFile();
    LOG_DBG("OPS", "Migrated single-server OPDS config to opds.json");
    return true;
  }

  // Save failed; roll back in-memory state so callers don't see a partial migration.
  servers.clear();
  return false;
}

bool OpdsServerStore::addServer(const OpdsServer& server) {
  ensureLoaded();
  if (servers.size() >= MAX_SERVERS) {
    LOG_DBG("OPS", "Cannot add more servers, limit of %zu reached", MAX_SERVERS);
    return false;
  }

  servers.push_back(server);
  return saveToFile();
}

bool OpdsServerStore::updateServer(size_t index, const OpdsServer& server) {
  ensureLoaded();
  if (index >= servers.size()) {
    return false;
  }

  servers[index] = server;
  return saveToFile();
}

bool OpdsServerStore::removeServer(size_t index) {
  ensureLoaded();
  if (index >= servers.size()) {
    return false;
  }

  servers.erase(servers.begin() + static_cast<ptrdiff_t>(index));
  return saveToFile();
}

const OpdsServer* OpdsServerStore::getServer(size_t index) const {
  ensureLoaded();
  if (index >= servers.size()) {
    return nullptr;
  }
  return &servers[index];
}
