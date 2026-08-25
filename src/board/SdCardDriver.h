#pragma once

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

struct SdTestResult {
  bool mounted = false;
  bool writeOk = false;
  bool readOk = false;
  bool contentOk = false;
  uint8_t cardType = CARD_NONE;
  uint64_t sizeBytes = 0;
  uint64_t usedBytes = 0;
};

class SdCardDriver {
 public:
  SdCardDriver();
  bool begin();
  SdTestResult runReadWriteTest();
  bool appendStressRecord(uint32_t iteration, uint32_t freeHeap);
  void finishStressTest();
  bool mounted() const { return mounted_; }

 private:
  SPIClass spi_;
  bool mounted_ = false;
  static constexpr const char* TEST_FILE = "/osesp32_sd_test.tmp";
  static constexpr const char* STRESS_FILE = "/osesp32_stress.tmp";
};
