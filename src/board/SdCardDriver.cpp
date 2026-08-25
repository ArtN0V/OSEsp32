#include "SdCardDriver.h"

#include "BoardConfig.h"

SdCardDriver::SdCardDriver() : spi_(VSPI) {}

bool SdCardDriver::begin() {
  spi_.begin(board::SD_SCLK, board::SD_MISO, board::SD_MOSI, board::SD_CS);
  mounted_ = SD.begin(board::SD_CS, spi_, board::SD_FREQUENCY);
  return mounted_;
}

SdTestResult SdCardDriver::runReadWriteTest() {
  SdTestResult result;
  if (!mounted_ && !begin()) return result;

  result.mounted = true;
  result.cardType = SD.cardType();
  result.sizeBytes = SD.cardSize();
  result.usedBytes = SD.usedBytes();

  static const char payload[] = "YellowOS stage 0 SD test v1\n";
  SD.remove(TEST_FILE);
  File output = SD.open(TEST_FILE, FILE_WRITE);
  if (output) {
    result.writeOk = output.print(payload) == strlen(payload);
    output.flush();
    output.close();
  }

  File input = SD.open(TEST_FILE, FILE_READ);
  if (input) {
    result.readOk = true;
    String actual = input.readString();
    input.close();
    result.contentOk = actual == payload;
  }
  SD.remove(TEST_FILE);
  return result;
}

bool SdCardDriver::appendStressRecord(uint32_t iteration, uint32_t freeHeap) {
  if (!mounted_) return false;
  File file = SD.open(STRESS_FILE, FILE_APPEND);
  if (!file) return false;
  file.printf("%lu,%lu\n", static_cast<unsigned long>(iteration),
              static_cast<unsigned long>(freeHeap));
  file.close();
  return true;
}

void SdCardDriver::finishStressTest() {
  if (mounted_) SD.remove(STRESS_FILE);
}
