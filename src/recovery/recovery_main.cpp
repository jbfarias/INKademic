// INKademic X4 Pro Recovery
//
// Intentionally headless: the smallest useful recovery application is an
// automatic SD-card loader. Put exactly one .bin in the card root, boot this
// image, and it validates the candidate before writing the inactive OTA slot.
// There is no reader, UI, Wi-Fi, USB-MSC, logging, NVS, or display stack here.
//
// Validation is deliberately kept complete. A smaller binary must not mean a
// weaker safety check: nothing is erased until the whole candidate image has
// passed the ESP image layout, chip, XOR checksum, and appended SHA-256 checks.

#include <Arduino.h>
#include <SDCardManager.h>

#include <algorithm>
#include <cstring>
#include <memory>

#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_rom_crc.h>
#include <esp_task_wdt.h>
#include <mbedtls/sha256.h>
#include <spi_flash_mmap.h>

namespace {

constexpr uint8_t kEspImageMagic = 0xE9;
constexpr uint8_t kChecksumSeed = 0xEF;
constexpr size_t kHeaderSize = 24;
constexpr size_t kSegmentHeaderSize = 8;
constexpr size_t kSectorSize = SPI_FLASH_SEC_SIZE;
// Keep each flash API call to one sector. This is intentionally conservative:
// the recovery image exists for devices that already reset during a longer
// erase window, so throughput is less important than never starving the task
// watchdog between service points.
constexpr size_t kEraseBlock = kSectorSize;
constexpr size_t kChunk = 4096;
constexpr size_t kShaTrailerSize = 32;
constexpr size_t kMinimumImageSize = 64 * 1024;

// A fixed name avoids a file browser, vectors, strings, and display code.
constexpr char kUpdatePath[] = "/inkademic-update.bin";

enum class Result {
  Ok,
  Open,
  TooSmall,
  TooLarge,
  BadMagic,
  BadSegments,
  BadChecksum,
  BadSha,
  BadChip,
  BadSize,
  NoPartition,
  OutOfMemory,
  Read,
  Erase,
  Write,
  BootSelector,
};

void feedWatchdog() {
  esp_task_wdt_reset();
  yield();
}

uint16_t runningChipId() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  if (!running) return 0xFFFF;

  uint16_t chipId = 0xFFFF;
  if (esp_partition_read(running, 12, &chipId, sizeof(chipId)) != ESP_OK) return 0xFFFF;
  return chipId;
}

Result readAndHash(FsFile& file, size_t length, uint8_t* xorValue, mbedtls_sha256_context& sha,
                   uint8_t* buffer) {
  size_t remaining = length;
  while (remaining != 0) {
    const size_t want = std::min(kChunk, remaining);
    const int got = file.read(buffer, want);
    if (got != static_cast<int>(want)) return Result::Read;
    mbedtls_sha256_update(&sha, buffer, want);
    if (xorValue) {
      for (size_t i = 0; i < want; ++i) *xorValue ^= buffer[i];
    }
    remaining -= want;
    feedWatchdog();
  }
  return Result::Ok;
}

Result validateImage(FsFile& file, size_t destinationSize) {
  const size_t fileSize = file.fileSize();
  if (fileSize < kMinimumImageSize) return Result::TooSmall;
  if (destinationSize != 0 && fileSize > destinationSize) return Result::TooLarge;
  if (!file.seek(0)) return Result::Read;

  uint8_t header[kHeaderSize];
  if (file.read(header, sizeof(header)) != static_cast<int>(sizeof(header))) return Result::Read;
  if (header[0] != kEspImageMagic) return Result::BadMagic;

  uint16_t imageChipId = 0;
  std::memcpy(&imageChipId, header + 12, sizeof(imageChipId));
  const uint16_t deviceChipId = runningChipId();
  if (deviceChipId != 0xFFFF && imageChipId != deviceChipId) return Result::BadChip;

  auto buffer = std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[kChunk]);
  if (!buffer) return Result::OutOfMemory;

  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  mbedtls_sha256_update(&sha, header, sizeof(header));

  uint8_t checksum = kChecksumSeed;
  size_t position = kHeaderSize;
  const uint8_t segmentCount = header[1];
  for (uint8_t segment = 0; segment < segmentCount; ++segment) {
    if (position + kSegmentHeaderSize > fileSize) {
      mbedtls_sha256_free(&sha);
      return Result::BadSegments;
    }

    uint8_t segmentHeader[kSegmentHeaderSize];
    if (file.read(segmentHeader, sizeof(segmentHeader)) != static_cast<int>(sizeof(segmentHeader))) {
      mbedtls_sha256_free(&sha);
      return Result::Read;
    }
    mbedtls_sha256_update(&sha, segmentHeader, sizeof(segmentHeader));
    position += sizeof(segmentHeader);

    uint32_t dataLength = 0;
    std::memcpy(&dataLength, segmentHeader + 4, sizeof(dataLength));
    if (dataLength > fileSize - position) {
      mbedtls_sha256_free(&sha);
      return Result::BadSegments;
    }

    const Result dataResult = readAndHash(file, dataLength, &checksum, sha, buffer.get());
    if (dataResult != Result::Ok) {
      mbedtls_sha256_free(&sha);
      return dataResult;
    }
    position += dataLength;
  }

  const size_t paddedEnd = (position + 16) & ~static_cast<size_t>(15);
  const bool hasSha = header[23] != 0;
  const size_t expectedSize = paddedEnd + (hasSha ? kShaTrailerSize : 0);
  if (expectedSize != fileSize || paddedEnd < position || paddedEnd - position > 16) {
    mbedtls_sha256_free(&sha);
    return Result::BadSize;
  }

  uint8_t padding[16] = {};
  const size_t paddingLength = paddedEnd - position;
  if (file.read(padding, paddingLength) != static_cast<int>(paddingLength)) {
    mbedtls_sha256_free(&sha);
    return Result::Read;
  }
  mbedtls_sha256_update(&sha, padding, paddingLength);
  if (checksum != padding[paddingLength - 1]) {
    mbedtls_sha256_free(&sha);
    return Result::BadChecksum;
  }

  if (hasSha) {
    uint8_t computed[kShaTrailerSize];
    uint8_t stored[kShaTrailerSize];
    mbedtls_sha256_finish(&sha, computed);
    if (file.read(stored, sizeof(stored)) != static_cast<int>(sizeof(stored)) ||
        std::memcmp(computed, stored, sizeof(stored)) != 0) {
      mbedtls_sha256_free(&sha);
      return Result::BadSha;
    }
  }

  mbedtls_sha256_free(&sha);
  return Result::Ok;
}

class FlashWatchdogGuard {
 public:
  FlashWatchdogGuard() {
    const esp_task_wdt_config_t config = {60U * 1000U, 0, true};
    reconfigured_ = esp_task_wdt_reconfigure(&config) == ESP_OK;
    feedWatchdog();
  }

  ~FlashWatchdogGuard() {
    feedWatchdog();
    if (reconfigured_) {
      const esp_task_wdt_config_t config = {15U * 1000U, 0, true};
      (void)esp_task_wdt_reconfigure(&config);
    }
  }

  FlashWatchdogGuard(const FlashWatchdogGuard&) = delete;
  FlashWatchdogGuard& operator=(const FlashWatchdogGuard&) = delete;

 private:
  bool reconfigured_ = false;
};

Result flashImage(FsFile& file, const esp_partition_t* destination) {
  if (!file.seek(0)) return Result::Read;

  auto buffer = std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[kChunk]);
  if (!buffer) return Result::OutOfMemory;

  FlashWatchdogGuard watchdog;
  const size_t imageSize = file.fileSize();
  size_t position = 0;
  size_t erasedUntil = 0;
  while (position < imageSize) {
    if (position >= erasedUntil) {
      const size_t remainingPartition = static_cast<size_t>(destination->size) - position;
      size_t eraseLength = std::min(kEraseBlock, remainingPartition);
      eraseLength = (eraseLength + kSectorSize - 1) & ~(kSectorSize - 1);
      eraseLength = std::min(eraseLength, remainingPartition);
      feedWatchdog();
      if (esp_partition_erase_range(destination, position, eraseLength) != ESP_OK) return Result::Erase;
      erasedUntil = position + eraseLength;
      feedWatchdog();
      delay(1);
    }

    const size_t want = std::min(kChunk, imageSize - position);
    if (file.read(buffer.get(), want) != static_cast<int>(want)) return Result::Read;
    if (esp_partition_write(destination, position, buffer.get(), want) != ESP_OK) return Result::Write;
    position += want;
    feedWatchdog();
    delay(1);
  }
  return Result::Ok;
}

struct __attribute__((packed)) OtaSelectEntry {
  uint32_t otaSeq;
  uint8_t label[20];
  uint32_t state;
  uint32_t crc;
};
static_assert(sizeof(OtaSelectEntry) == 32, "invalid otadata entry size");

constexpr uint32_t kOtaImageNew = 0;
constexpr uint32_t kOtaImageInvalid = 3;
constexpr uint32_t kOtaImageAborted = 4;

uint32_t otaSequenceCrc(uint32_t sequence) {
  return esp_rom_crc32_le(UINT32_MAX, reinterpret_cast<const uint8_t*>(&sequence), sizeof(sequence));
}

bool selectPartition(const esp_partition_t* destination) {
  const esp_partition_t* otadata =
      esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr);
  if (!otadata || otadata->size < 2 * kSectorSize) return false;

  OtaSelectEntry entries[2] = {};
  if (esp_partition_read(otadata, 0, &entries[0], sizeof(entries[0])) != ESP_OK ||
      esp_partition_read(otadata, kSectorSize, &entries[1], sizeof(entries[1])) != ESP_OK) {
    return false;
  }

  int activeEntry = -1;
  uint32_t activeSequence = 0;
  for (int i = 0; i < 2; ++i) {
    if (entries[i].otaSeq == UINT32_MAX || entries[i].crc != otaSequenceCrc(entries[i].otaSeq) ||
        entries[i].state == kOtaImageInvalid || entries[i].state == kOtaImageAborted) {
      continue;
    }
    if (activeEntry < 0 || entries[i].otaSeq > activeSequence) {
      activeEntry = i;
      activeSequence = entries[i].otaSeq;
    }
  }

  const uint32_t destinationIndex = static_cast<uint32_t>(destination->subtype) -
                                    static_cast<uint32_t>(ESP_PARTITION_SUBTYPE_APP_OTA_0);
  if (destinationIndex > 1) return false;

  uint32_t newSequence = activeSequence + 1;
  while (((newSequence - 1U) & 1U) != destinationIndex) ++newSequence;

  OtaSelectEntry next = {};
  next.otaSeq = newSequence;
  std::memset(next.label, 0xFF, sizeof(next.label));
  next.state = kOtaImageNew;
  next.crc = otaSequenceCrc(next.otaSeq);

  const int targetEntry = activeEntry == 0 ? 1 : 0;
  const size_t targetOffset = static_cast<size_t>(targetEntry) * kSectorSize;
  return esp_partition_erase_range(otadata, targetOffset, kSectorSize) == ESP_OK &&
         esp_partition_write(otadata, targetOffset, &next, sizeof(next)) == ESP_OK;
}

[[noreturn]] void failForever() {
  while (true) {
    delay(1000);
    feedWatchdog();
  }
}

}  // namespace

void setup() {
  // No UI or logging is initialized. The SD manager still performs the X4 Pro's
  // native SDMMC power-cycle and real sector-0 probe before exposing the card.
  if (!SdMan.begin()) failForever();

  const esp_partition_t* destination = esp_ota_get_next_update_partition(nullptr);
  if (!destination) failForever();

  FsFile image = SdMan.open(kUpdatePath, O_RDONLY);
  if (!image) failForever();

  // Validation completes before flash erase. Rewind once, then stream the same
  // file into the inactive partition in bounded chunks.
  if (validateImage(image, destination->size) != Result::Ok || !image.seek(0)) {
    image.close();
    failForever();
  }

  const Result flashResult = flashImage(image, destination);
  image.close();
  if (flashResult != Result::Ok || !selectPartition(destination)) failForever();

  // The selected slot is only marked healthy by the full firmware after its
  // first complete boot. If that application fails early, bootloader rollback
  // remains available.
  delay(100);
  ESP.restart();
}

void loop() {
  delay(1000);
}
