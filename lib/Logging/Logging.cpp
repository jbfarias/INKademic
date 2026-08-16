#include "Logging.h"

#include <BoardConfig.h>
#ifdef CROSSINK_ENABLE_SD_DIAGNOSTICS
#include <HalClock.h>
#include <HalStorage.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif
#include <esp_rom_sys.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <string>

#ifdef SIMULATOR
#include <Arduino.h>

MySerialImpl MySerialImpl::instance;

size_t MySerialImpl::write(uint8_t b) { return logSerial.write(b); }
size_t MySerialImpl::write(const uint8_t* buffer, size_t size) { return logSerial.write(buffer, size); }
void MySerialImpl::flush() { logSerial.flush(); }
#endif

#define MAX_ENTRY_LEN 256
#define MAX_LOG_LINES 16

#ifdef CROSSINK_ENABLE_SD_DIAGNOSTICS
namespace {
constexpr char DIAGNOSTIC_DIR[] = "/.crosspoint/diagnostics";
constexpr char DIAGNOSTIC_LOG_PATH[] = "/.crosspoint/diagnostics/diagnostic.log";
constexpr char DIAGNOSTIC_PREVIOUS_PATH[] = "/.crosspoint/diagnostics/diagnostic.previous.log";
// Static, bounded backlog: enough to cover a long display/render section
// without allowing diagnostic logging to consume the reader's heap.
constexpr size_t DIAGNOSTIC_QUEUE_LINES = 16;
constexpr uint32_t DIAGNOSTIC_SERVICE_INTERVAL_MS = 250;
constexpr uint32_t DIAGNOSTIC_MAX_FILE_BYTES = 512U * 1024U;

struct PendingDiagnosticLine {
  char text[MAX_ENTRY_LEN]{};
  uint32_t sequence = 0;
};

PendingDiagnosticLine diagnosticQueue[DIAGNOSTIC_QUEUE_LINES];
size_t diagnosticQueueHead = 0;
size_t diagnosticQueueCount = 0;
uint32_t diagnosticQueueSequence = 1;
uint32_t diagnosticDroppedCount = 0;
portMUX_TYPE diagnosticQueueMux = portMUX_INITIALIZER_UNLOCKED;
bool diagnosticEnabled = false;
uint32_t diagnosticLastServiceMs = 0;

size_t formatLogPrefix(char* buffer, const size_t bufferSize, const char* level, const char* origin) {
  if (bufferSize == 0) return 0;

  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
  if (halClock.getDateTime(year, month, day, hour, minute, second)) {
    const int written = snprintf(buffer, bufferSize, "[%04u-%02u-%02uT%02u:%02u:%02uZ] [up=%lu] [%s] [%s] ",
                                 static_cast<unsigned>(year), static_cast<unsigned>(month), static_cast<unsigned>(day),
                                 static_cast<unsigned>(hour), static_cast<unsigned>(minute),
                                 static_cast<unsigned>(second), static_cast<unsigned long>(millis()), level, origin);
    return written <= 0 ? 0 : std::min(static_cast<size_t>(written), bufferSize - 1);
  }

  // X4 devices without an external RTC can still acquire a wall-clock value
  // after NTP. Ignore the uninitialized Unix epoch when it is not trustworthy.
  const time_t now = time(nullptr);
  if (now >= static_cast<time_t>(1704067200)) {  // 2024-01-01 UTC
    struct tm utc{};
    if (gmtime_r(&now, &utc) != nullptr) {
      const int written = snprintf(buffer, bufferSize,
                                   "[%04d-%02d-%02dT%02d:%02d:%02dZ] [up=%lu] [%s] [%s] ", utc.tm_year + 1900,
                                   utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec,
                                   static_cast<unsigned long>(millis()), level, origin);
      return written <= 0 ? 0 : std::min(static_cast<size_t>(written), bufferSize - 1);
    }
  }

  const int written = snprintf(buffer, bufferSize, "[no-clock] [up=%lu] [%s] [%s] ",
                               static_cast<unsigned long>(millis()), level, origin);
  return written <= 0 ? 0 : std::min(static_cast<size_t>(written), bufferSize - 1);
}

void enqueueDiagnosticLine(const char* line) {
  if (!diagnosticEnabled || !line) return;

  portENTER_CRITICAL(&diagnosticQueueMux);
  if (diagnosticQueueCount >= DIAGNOSTIC_QUEUE_LINES) {
    diagnosticDroppedCount++;
    portEXIT_CRITICAL(&diagnosticQueueMux);
    return;
  }

  const size_t index = (diagnosticQueueHead + diagnosticQueueCount) % DIAGNOSTIC_QUEUE_LINES;
  strncpy(diagnosticQueue[index].text, line, MAX_ENTRY_LEN - 1);
  diagnosticQueue[index].text[MAX_ENTRY_LEN - 1] = '\0';
  diagnosticQueue[index].sequence = diagnosticQueueSequence++;
  diagnosticQueueCount++;
  portEXIT_CRITICAL(&diagnosticQueueMux);
}

bool hasPendingDiagnosticLines() {
  portENTER_CRITICAL(&diagnosticQueueMux);
  const bool pending = diagnosticQueueCount > 0 || diagnosticDroppedCount > 0;
  portEXIT_CRITICAL(&diagnosticQueueMux);
  return pending;
}

uint32_t pendingDroppedCount() {
  portENTER_CRITICAL(&diagnosticQueueMux);
  const uint32_t dropped = diagnosticDroppedCount;
  portEXIT_CRITICAL(&diagnosticQueueMux);
  return dropped;
}

bool peekDiagnosticLine(char* output, const size_t outputSize, uint32_t& sequence) {
  if (!output || outputSize == 0) return false;
  portENTER_CRITICAL(&diagnosticQueueMux);
  if (diagnosticQueueCount == 0) {
    portEXIT_CRITICAL(&diagnosticQueueMux);
    return false;
  }
  const PendingDiagnosticLine& pending = diagnosticQueue[diagnosticQueueHead];
  strncpy(output, pending.text, outputSize - 1);
  output[outputSize - 1] = '\0';
  sequence = pending.sequence;
  portEXIT_CRITICAL(&diagnosticQueueMux);
  return true;
}

void consumeDiagnosticLine(const uint32_t sequence) {
  portENTER_CRITICAL(&diagnosticQueueMux);
  if (diagnosticQueueCount > 0 && diagnosticQueue[diagnosticQueueHead].sequence == sequence) {
    diagnosticQueue[diagnosticQueueHead].text[0] = '\0';
    diagnosticQueueHead = (diagnosticQueueHead + 1) % DIAGNOSTIC_QUEUE_LINES;
    diagnosticQueueCount--;
  }
  portEXIT_CRITICAL(&diagnosticQueueMux);
}

void consumeDroppedCount(const uint32_t writtenCount) {
  portENTER_CRITICAL(&diagnosticQueueMux);
  diagnosticDroppedCount = diagnosticDroppedCount > writtenCount ? diagnosticDroppedCount - writtenCount : 0;
  portEXIT_CRITICAL(&diagnosticQueueMux);
}

bool rotateDiagnosticLogIfNeeded() {
  HalFile existing = Storage.open(DIAGNOSTIC_LOG_PATH, O_RDONLY);
  if (!existing) return true;
  const uint64_t size = existing.fileSize64();
  existing.close();
  if (size < DIAGNOSTIC_MAX_FILE_BYTES) return true;

  // Keep one previous file. A failed rename leaves the current log intact;
  // losing diagnostics is worse than temporarily exceeding the cap.
  if (Storage.exists(DIAGNOSTIC_PREVIOUS_PATH)) {
    Storage.remove(DIAGNOSTIC_PREVIOUS_PATH);
  }
  return Storage.rename(DIAGNOSTIC_LOG_PATH, DIAGNOSTIC_PREVIOUS_PATH);
}

bool writeDiagnosticLine(HalFile& file, const char* line) {
  const size_t length = strnlen(line, MAX_ENTRY_LEN);
  return file.write(line, length) == length;
}

void writeRetainedLogs() {
  const std::string retained = getLastLogs();
  if (retained.empty() || !rotateDiagnosticLogIfNeeded()) return;

  HalFile file = Storage.open(DIAGNOSTIC_LOG_PATH, O_RDWR | O_CREAT | O_APPEND);
  if (!file) return;
  if (file.write(retained.data(), retained.size()) == retained.size()) file.sync();
  file.close();
}
}  // namespace
#endif

#ifndef CROSSINK_ENABLE_SD_DIAGNOSTICS
namespace {
size_t formatLogPrefix(char* buffer, const size_t bufferSize, const char* level, const char* origin) {
  if (bufferSize == 0) return 0;
  const int written = snprintf(buffer, bufferSize, "[%lu] [%s] [%s] ", static_cast<unsigned long>(millis()), level,
                               origin);
  return written <= 0 ? 0 : std::min(static_cast<size_t>(written), bufferSize - 1);
}
}  // namespace
#endif

// Simple ring buffer log, useful for error reporting when we encounter a crash
RTC_NOINIT_ATTR char logMessages[MAX_LOG_LINES][MAX_ENTRY_LEN];
RTC_NOINIT_ATTR size_t logHead = 0;
// Magic word written alongside logHead to detect uninitialized RTC memory.
// RTC_NOINIT_ATTR is not zeroed on cold boot, so logHead may appear in-range
// (0..MAX_LOG_LINES-1) by chance even though logMessages is garbage. The magic
// value is only set by clearLastLogs(), so its absence means the buffer was
// never properly initialized.
RTC_NOINIT_ATTR uint32_t rtcLogMagic;
static constexpr uint32_t LOG_RTC_MAGIC = 0xDEADBEEF;

void addToLogRingBuffer(const char* message) {
  // Add the message to the ring buffer, overwriting old messages if necessary.
  // If the magic is wrong or logHead is out of range (RTC_NOINIT_ATTR garbage
  // on cold boot), clear the entire buffer so subsequent reads are safe.
  if (rtcLogMagic != LOG_RTC_MAGIC || logHead >= MAX_LOG_LINES) {
    memset(logMessages, 0, sizeof(logMessages));
    logHead = 0;
    rtcLogMagic = LOG_RTC_MAGIC;
  }
  strncpy(logMessages[logHead], message, MAX_ENTRY_LEN - 1);
  logMessages[logHead][MAX_ENTRY_LEN - 1] = '\0';
  logHead = (logHead + 1) % MAX_LOG_LINES;
}

// Since logging can take a large amount of flash, we want to make the format string as short as possible.
// This logPrintf prepend the timestamp, level and origin to the user-provided message, so that the user only needs to
// provide the format string for the message itself.
void logPrintf(const char* level, const char* origin, const char* format, ...) {
  va_list args;
  va_start(args, format);
  char buf[MAX_ENTRY_LEN];
  const size_t prefixLength = formatLogPrefix(buf, sizeof(buf), level, origin);
  char* c = buf + prefixLength;
  // add the user message
  {
    const int len = vsnprintf(c, sizeof(buf) - prefixLength, format, args);
    if (len < 0) {
      va_end(args);
      return;
    }
  }
  va_end(args);
#if defined(SIMULATOR)
  std::fputs(buf, stderr);
#elif FREEINK_LOG_TRANSPORT == FREEINK_LOG_TRANSPORT_ROM_PRINTF
  // IDF/ROM console path for boards monitored over USB-Serial-JTAG, where the
  // HWCDC `operator bool` reads false under `pio device monitor` and logs would
  // otherwise be silently dropped (e.g. Sticky).
  esp_rom_printf("%s", buf);
#else
  if (logSerial) {
    logSerial.print(buf);
  }
#endif
  addToLogRingBuffer(buf);
#ifdef CROSSINK_ENABLE_SD_DIAGNOSTICS
  enqueueDiagnosticLine(buf);
#endif
}

std::string getLastLogs() {
  if (rtcLogMagic != LOG_RTC_MAGIC) {
    return {};
  }
  std::string output;
  for (size_t i = 0; i < MAX_LOG_LINES; i++) {
    size_t idx = (logHead + i) % MAX_LOG_LINES;
    if (logMessages[idx][0] != '\0') {
      const size_t len = strnlen(logMessages[idx], MAX_ENTRY_LEN);
      output.append(logMessages[idx], len);
    }
  }
  return output;
}

// Checks whether the RTC log state is consistent: rtcLogMagic must equal
// LOG_RTC_MAGIC and logHead must be in 0..MAX_LOG_LINES-1. Returns true if
// corruption is detected, in which case rtcLogMagic is still invalid and
// logMessages may contain garbage. Callers (e.g. HalSystem::begin on the
// panic-reboot path) must call clearLastLogs() after a true result to fully
// reinitialize the ring buffer and stamp the magic before getLastLogs() is used.
bool sanitizeLogHead() {
  if (rtcLogMagic != LOG_RTC_MAGIC || logHead >= MAX_LOG_LINES) {
    logHead = 0;
    return true;
  }
  return false;
}

void clearLastLogs() {
  for (size_t i = 0; i < MAX_LOG_LINES; i++) {
    logMessages[i][0] = '\0';
  }
  logHead = 0;
  rtcLogMagic = LOG_RTC_MAGIC;
}

#ifdef CROSSINK_ENABLE_SD_DIAGNOSTICS
void diagnosticLogBegin() {
  if (diagnosticEnabled || !Storage.ready()) return;
  if (!Storage.ensureDirectoryExists("/.crosspoint")) return;
  if (!Storage.ensureDirectoryExists(DIAGNOSTIC_DIR)) return;
  diagnosticEnabled = true;
  // Include the pre-SD boot lines retained in RTC memory. On a panic reboot
  // this also preserves the final lines emitted before the previous reset.
  writeRetainedLogs();
  logPrintf("INF", "DIAG", "SD diagnostic logger enabled path=%s\n", DIAGNOSTIC_LOG_PATH);
  diagnosticLogFlush();
}

bool diagnosticLogEnabled() { return diagnosticEnabled; }

void diagnosticLogService() {
  if (!diagnosticEnabled || !Storage.ready()) return;
  const uint32_t now = millis();
  if (now - diagnosticLastServiceMs < DIAGNOSTIC_SERVICE_INTERVAL_MS && !hasPendingDiagnosticLines()) return;
  diagnosticLastServiceMs = now;
  if (!hasPendingDiagnosticLines()) return;
  if (!rotateDiagnosticLogIfNeeded()) return;

  HalFile file = Storage.open(DIAGNOSTIC_LOG_PATH, O_RDWR | O_CREAT | O_APPEND);
  if (!file) return;

  const uint32_t droppedAtStart = pendingDroppedCount();
  if (droppedAtStart > 0) {
    char droppedLine[MAX_ENTRY_LEN];
    const size_t prefixLength = formatLogPrefix(droppedLine, sizeof(droppedLine), "WARN", "DIAG");
    snprintf(droppedLine + prefixLength, sizeof(droppedLine) - prefixLength,
             "dropped=%lu queued log lines due to SD writer backlog\n",
             static_cast<unsigned long>(droppedAtStart));
    if (!writeDiagnosticLine(file, droppedLine)) {
      file.close();
      return;
    }
    consumeDroppedCount(droppedAtStart);
  }

  char line[MAX_ENTRY_LEN];
  uint32_t sequence = 0;
  for (size_t i = 0; i < DIAGNOSTIC_QUEUE_LINES && peekDiagnosticLine(line, sizeof(line), sequence); ++i) {
    if (!writeDiagnosticLine(file, line)) break;
    consumeDiagnosticLine(sequence);
  }
  file.sync();
  file.close();
}

void diagnosticLogFlush() {
  if (!diagnosticEnabled || !Storage.ready()) return;
  for (size_t attempt = 0; attempt < 32 && hasPendingDiagnosticLines(); ++attempt) {
    diagnosticLastServiceMs = 0;
    diagnosticLogService();
  }
}

void diagnosticLogWriteCrashReport(const char* report) {
  if (!diagnosticEnabled || !Storage.ready() || !report) return;
  HalFile file = Storage.open("/.crosspoint/diagnostics/last_crash_report.txt", O_WRITE | O_CREAT | O_TRUNC);
  if (!file) return;
  const size_t length = strlen(report);
  if (file.write(report, length) == length) file.sync();
  file.close();
}
#endif
