#pragma once

#include <Arduino.h>
#include <BoardConfig.h>

#ifdef CROSSINK_ENABLE_SD_DIAGNOSTICS
// Keep the low-level SdFat type visible from the public logging header so
// PlatformIO's library dependency finder associates SdFat's include path
// with the Logging library. Do not include SDCardManager here: it would pull
// in the FsFile class, which conflicts with HalStorage's FsFile facade.
#include <common/FsApiConstants.h>
#endif

#include <string>

/*
Define ENABLE_SERIAL_LOG to enable logging
Can be set in platformio.ini build_flags or as a compile definition

Define LOG_LEVEL to control log verbosity:
0 = ERR only
1 = ERR + INF
2 = ERR + INF + DBG
If not defined, defaults to 0

If you have a legitimate need for raw Serial access (e.g., binary data,
special formatting), use the underlying logSerial object directly:
    logSerial.printf("Special case: %d\n", value);
    logSerial.write(binaryData, length);

The logSerial reference (defined below) points to the board's physical USB
serial transport and won't trigger deprecation warnings.
*/

#ifndef LOG_LEVEL
#define LOG_LEVEL 0
#endif

static auto& logSerial = BoardConfig::serialTransport();
#define LOG_SERIAL_HAS_TX_TIMEOUT FREEINK_SERIAL_HAS_TX_TIMEOUT

void logPrintf(const char* level, const char* origin, const char* format, ...);

#ifdef ENABLE_SERIAL_LOG
#if LOG_LEVEL >= 0
#define LOG_ERR(origin, format, ...) logPrintf("ERR", origin, format "\n", ##__VA_ARGS__)
#else
#define LOG_ERR(origin, format, ...)
#endif

#if LOG_LEVEL >= 1
#define LOG_INF(origin, format, ...) logPrintf("INF", origin, format "\n", ##__VA_ARGS__)
#else
#define LOG_INF(origin, format, ...)
#endif

#if LOG_LEVEL >= 2
#define LOG_DBG(origin, format, ...) logPrintf("DBG", origin, format "\n", ##__VA_ARGS__)
#else
#define LOG_DBG(origin, format, ...)
#endif
#else
#define LOG_DBG(origin, format, ...)
#define LOG_ERR(origin, format, ...)
#define LOG_INF(origin, format, ...)
#endif

std::string getLastLogs();
void clearLastLogs();
// Validates the RTC log state (magic word + logHead range). Returns true if
// corruption was detected (magic mismatch or logHead out of range), meaning
// logMessages is untrusted garbage. Callers should call clearLastLogs() when
// this returns true so getLastLogs() does not dump corrupt data into crash reports.
bool sanitizeLogHead();

// The diagnostic build mirrors the normal log stream to a bounded queue and
// drains it to the SD card from the main loop. Keeping the SD write out of
// logPrintf() is important: some storage error paths log while holding the
// storage mutex, so a synchronous write there could deadlock.
#ifdef CROSSINK_ENABLE_SD_DIAGNOSTICS
void diagnosticLogBegin();
void diagnosticLogService();
void diagnosticLogFlush();
void diagnosticLogWriteCrashReport(const char* report);
bool diagnosticLogEnabled();
#else
inline void diagnosticLogBegin() {}
inline void diagnosticLogService() {}
inline void diagnosticLogFlush() {}
inline void diagnosticLogWriteCrashReport(const char*) {}
inline bool diagnosticLogEnabled() { return false; }
#endif

class MySerialImpl : public Print {
 public:
  void begin(unsigned long baud) { logSerial.begin(baud); }

  // Support boolean conversion for compatibility with code like:
  //   if (Serial) or while (!Serial)
  operator bool() const { return logSerial; }

  __attribute__((deprecated("Use LOG_* macro instead"))) size_t printf(const char* format, ...);
  size_t write(uint8_t b) override;
  size_t write(const uint8_t* buffer, size_t size) override;
  void flush() override;
  static MySerialImpl instance;
};

#ifdef Serial
#undef Serial
#endif
#define Serial MySerialImpl::instance
