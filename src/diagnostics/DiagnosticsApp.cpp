#include "DiagnosticsApp.h"

#include <ctype.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include "../board/BoardConfig.h"

namespace {
constexpr uint16_t COLOR_BG = 0x10A2;
constexpr uint16_t COLOR_PANEL = 0x2945;
constexpr uint16_t COLOR_ACCENT = 0x2D7F;
constexpr uint16_t COLOR_OK = 0x07E0;
constexpr uint16_t COLOR_WARN = 0xFD20;
constexpr uint16_t COLOR_ERROR = 0xF800;
constexpr uint16_t COLOR_TEXT = 0xFFFF;
constexpr uint16_t COLOR_MUTED = 0xBDF7;

const char* cardTypeName(uint8_t type) {
  switch (type) {
    case CARD_MMC: return "MMC";
    case CARD_SD: return "SDSC";
    case CARD_SDHC: return "SDHC/SDXC";
    default: return "NONE/UNKNOWN";
  }
}
}  // namespace

void DiagnosticsApp::begin(SystemKernel& kernel) {
  kernel_ = &kernel;
  printBanner();
  configurePeripherals();

  const bool displayOk = display_.begin();
  touch_.begin();
  calibration_.begin(touch_, kernel.events(), kernel.logger());
  Serial.printf("[BOOT] Display geometry: %ux%u (%s)\n", display_.width(),
                display_.height(), displayOk ? "PASS" : "CHECK");
  Serial.println("[BOOT] XPT2046 software SPI initialized");
  const TouchCalibration& calibration = touch_.calibration();
  Serial.printf("[BOOT] Touch calibration: X=%u..%u%s Y=%u..%u%s (%s)\n",
                calibration.rawXMin, calibration.rawXMax,
                calibration.invertX ? " inverted" : "",
                calibration.rawYMin, calibration.rawYMax,
                calibration.invertY ? " inverted" : "",
                calibration.stored ? "NVS" : "defaults");
  printSystemInfo();

  display_.fillScreen(COLOR_BG);
  display_.setTextColor(COLOR_TEXT, COLOR_BG);
  display_.setTextSize(1);
  display_.setCursor(18, 94);
  display_.println("OSEsp32 hardware diagnostics");
  display_.setTextColor(COLOR_MUTED, COLOR_BG);
  display_.setCursor(18, 116);
  display_.println("Recovery hardware diagnostics");
  delay(900);
  drawMenu();
  printHelp();
}

void DiagnosticsApp::configurePeripherals() {
  pinMode(board::LED_RED, OUTPUT);
  pinMode(board::LED_GREEN, OUTPUT);
  pinMode(board::LED_BLUE, OUTPUT);
  pinMode(board::LIGHT_SENSOR, INPUT);
  pinMode(board::SPEAKER, OUTPUT);
  setRgb(false, false, false);
  digitalWrite(board::SPEAKER, LOW);
}

void DiagnosticsApp::printBanner() {
  Serial.println();
  Serial.println("================================================");
  Serial.println(" OSEsp32 - recovery hardware diagnostics");
  Serial.println("================================================");
}

void DiagnosticsApp::printHelp() {
  Serial.println("Commands:");
  Serial.println("  h/? help       a all automatic tests");
  Serial.println("  d display      b backlight");
  Serial.println("  t touch live   s SD read/write");
  Serial.println("  c calibrate    r reset touch calibration");
  Serial.println("  o onboard I/O  m memory/system report");
  Serial.println("  k kernel info  x stress       q return to menu");
  Serial.println("  u reboot into graphical shell");
}

void DiagnosticsApp::printKernelInfo() {
  const MemorySnapshot& memory = kernel_->monitor().latest();
  Serial.println("[KERNEL]");
  Serial.printf("  lifecycle=%u handled-events=%lu dropped-events=%lu faults=%lu\n",
                static_cast<unsigned>(kernel_->lifecycle()),
                static_cast<unsigned long>(kernel_->handledEventCount()),
                static_cast<unsigned long>(kernel_->events().droppedCount()),
                static_cast<unsigned long>(kernel_->faults().count()));
  Serial.printf("  log entries=%u/%u\n", kernel_->logger().count(),
                Logger::CAPACITY);
  Serial.printf("  monitored heap=%u minimum=%u largest=%u psram=%u\n",
                memory.freeHeap, memory.minimumFreeHeap,
                memory.largestFreeBlock, memory.freePsram);
}

void DiagnosticsApp::printSystemInfo() {
  Serial.println("[SYSTEM]");
  Serial.printf("  Chip: %s rev %u, %u core(s), %u MHz\n", ESP.getChipModel(),
                ESP.getChipRevision(), ESP.getChipCores(), ESP.getCpuFreqMHz());
  Serial.printf("  SDK: %s\n", ESP.getSdkVersion());
  Serial.printf("  Flash: %u bytes, speed %u Hz\n", ESP.getFlashChipSize(),
                ESP.getFlashChipSpeed());
  Serial.printf("  Sketch: %u bytes, free app space %u bytes\n", ESP.getSketchSize(),
                ESP.getFreeSketchSpace());
  Serial.printf("  Heap: free %u, minimum %u, largest block %u bytes\n",
                ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  Serial.printf("  PSRAM: total %u, free %u bytes\n", ESP.getPsramSize(),
                ESP.getFreePsram());
  Serial.printf("  Reset reason CPU0: %d\n", static_cast<int>(esp_reset_reason()));
  Serial.println("  Expected classic CYD: 4 MiB flash and usually 0 bytes PSRAM");
}

void DiagnosticsApp::update() {
  while (Serial.available()) {
    const char command = static_cast<char>(tolower(Serial.read()));
    if (command != '\n' && command != '\r' && command != ' ') handleSerial(command);
  }

  TouchPoint point;
  const bool pressed = touch_.read(point);
  const bool newPress = pressed && !previousTouch_;

  if (screen_ == Screen::Menu && newPress) handleMenuTouch(point);
  if (screen_ == Screen::Touch && newPress && point.x >= 248 && point.y < 34) {
    drawMenu();
  } else if (screen_ == Screen::Touch && newPress &&
      point.x >= 100 && point.x <= 220 && point.y >= 198) {
    startTouchCalibration(true);
  } else if (screen_ == Screen::Touch && pressed) {
    updateTouchScreen(point);
  }
  if (screen_ == Screen::Calibration && newPress && point.x >= 120 &&
      point.x <= 200 && point.y < 34) {
    calibration_.cancel();
    runTouchTest();
  } else if (screen_ == Screen::Calibration) {
    updateTouchCalibration(pressed, point);
  }

  previousTouch_ = pressed;
  delay(3);
}

void DiagnosticsApp::drawMenu() {
  screen_ = Screen::Menu;
  display_.fillScreen(COLOR_BG);
  display_.fillRect(0, 0, board::SCREEN_WIDTH, 32, 0x18E3);
  display_.setTextColor(COLOR_TEXT, 0x18E3);
  display_.setTextDatum(textdatum_t::middle_left);
  display_.drawString("OSEsp32 DIAGNOSTICS", 8, 16);
  display_.fillRoundRect(244, 3, 72, 26, 3, COLOR_PANEL);
  display_.drawRoundRect(244, 3, 72, 26, 3, COLOR_ACCENT);
  display_.setTextColor(COLOR_TEXT, COLOR_PANEL);
  display_.setTextDatum(textdatum_t::middle_center);
  display_.drawString("SHELL", 280, 16);

  static const char* labels[] = {"DISPLAY", "TOUCH", "SD CARD",
                                 "I/O", "MEMORY", "STRESS"};
  for (int index = 0; index < 6; ++index) {
    const int column = index % 2;
    const int row = index / 2;
    const int x = 8 + column * 156;
    const int y = 40 + row * 62;
    display_.fillRoundRect(x, y, 148, 54, 4, COLOR_PANEL);
    display_.drawRoundRect(x, y, 148, 54, 4, COLOR_ACCENT);
    display_.setTextColor(COLOR_TEXT, COLOR_PANEL);
    display_.setTextDatum(textdatum_t::middle_center);
    display_.drawString(labels[index], x + 74, y + 27);
  }
  display_.setTextDatum(textdatum_t::top_left);
}

void DiagnosticsApp::drawStatus(const char* title, const char* line1,
                                const char* line2, uint16_t color) {
  display_.fillScreen(COLOR_BG);
  display_.fillRect(0, 0, board::SCREEN_WIDTH, 36, color);
  display_.setTextColor(COLOR_TEXT, color);
  display_.setTextDatum(textdatum_t::middle_center);
  display_.drawString(title, board::SCREEN_WIDTH / 2, 18);
  display_.setTextColor(COLOR_TEXT, COLOR_BG);
  display_.drawString(line1, board::SCREEN_WIDTH / 2, 104);
  display_.setTextColor(COLOR_MUTED, COLOR_BG);
  display_.drawString(line2, board::SCREEN_WIDTH / 2, 132);
  display_.setTextDatum(textdatum_t::top_left);
}

void DiagnosticsApp::handleMenuTouch(const TouchPoint& point) {
  if (point.x >= 240 && point.y < 34) {
    returnToShell();
    return;
  }
  if (point.y < 40 || point.y >= 226) return;
  const int column = point.x < 160 ? 0 : 1;
  const int row = (point.y - 40) / 62;
  if (row < 0 || row > 2) return;

  switch (row * 2 + column) {
    case 0: runDisplayTest(); break;
    case 1: runTouchTest(); return;
    case 2: runSdTest(); break;
    case 3: runIoTest(); break;
    case 4: runMemoryTest(); break;
    case 5: runStressTest(); break;
  }
  delay(700);
  drawMenu();
}

void DiagnosticsApp::returnToShell() {
  drawStatus("RETURN TO SHELL", "Restarting OSEsp32...",
             "No computer connection required", COLOR_ACCENT);
  Serial.println("[SYSTEM] Rebooting into graphical shell...");
  delay(350);
  ESP.restart();
}

void DiagnosticsApp::handleSerial(char command) {
  switch (command) {
    case 'h':
    case '?': printHelp(); break;
    case 'a': runAllTests(); break;
    case 'd': runDisplayTest(); drawMenu(); break;
    case 'b': runBacklightTest(); drawMenu(); break;
    case 't': runTouchTest(); break;
    case 'c': startTouchCalibration(false); break;
    case 'r':
      Serial.printf("[TOUCH] Reset calibration: %s\n",
                    touch_.resetCalibration() ? "OK" : "FAIL");
      runTouchTest();
      break;
    case 's': runSdTest(); delay(900); drawMenu(); break;
    case 'o': runIoTest(); delay(900); drawMenu(); break;
    case 'm': runMemoryTest(); delay(900); drawMenu(); break;
    case 'k': printKernelInfo(); break;
    case 'x': runStressTest(); delay(900); drawMenu(); break;
    case 'q': drawMenu(); break;
    case 'u':
      returnToShell();
      break;
    default: Serial.printf("Unknown command '%c'. Send h for help.\n", command); break;
  }
}

void DiagnosticsApp::runAllTests() {
  Serial.println("[ALL] Starting hardware verification sequence");
  runDisplayTest();
  runBacklightTest();
  runSdTest();
  runIoTest();
  runMemoryTest();
  runStressTest();
  Serial.println("[ALL] Finished. Touch calibration remains an interactive check.");
  drawStatus("AUTOMATIC TESTS COMPLETE", "Review the Serial log",
             "Run TOUCH separately", COLOR_OK);
  delay(1500);
  drawMenu();
}

void DiagnosticsApp::runDisplayTest() {
  Serial.println("[DISPLAY] Visual test: red, green, blue, white, geometry");
  const uint16_t colors[] = {0xF800, 0x07E0, 0x001F, 0xFFFF};
  for (uint16_t color : colors) {
    display_.fillScreen(color);
    delay(300);
  }
  display_.fillScreen(0x0000);
  for (int x = 0; x < 320; x += 20) display_.drawFastVLine(x, 0, 240, 0x39E7);
  for (int y = 0; y < 240; y += 20) display_.drawFastHLine(0, y, 320, 0x39E7);
  display_.drawRect(0, 0, 320, 240, 0xFFFF);
  display_.drawLine(0, 0, 319, 239, 0xFFE0);
  display_.drawLine(319, 0, 0, 239, 0xF81F);
  display_.setTextColor(0xFFFF, 0x0000);
  display_.setTextDatum(textdatum_t::middle_center);
  display_.drawString("320 x 240 / ILI9341", 160, 120);
  display_.setTextDatum(textdatum_t::top_left);
  Serial.println("[DISPLAY] PASS if colors, grid, border and both diagonals are correct");
  delay(1200);
}

void DiagnosticsApp::runBacklightTest() {
  Serial.println("[BACKLIGHT] 25% -> 60% -> 100%");
  drawStatus("BACKLIGHT", "25% -> 60% -> 100%", "Brightness must change smoothly", COLOR_ACCENT);
  display_.setBrightness(64);
  delay(700);
  display_.setBrightness(153);
  delay(700);
  display_.setBrightness(255);
  delay(700);
  Serial.println("[BACKLIGHT] Visual confirmation required");
}

void DiagnosticsApp::runTouchTest() {
  screen_ = Screen::Touch;
  display_.fillScreen(COLOR_BG);
  display_.fillRect(0, 0, 320, 32, COLOR_ACCENT);
  display_.setTextColor(COLOR_TEXT, COLOR_ACCENT);
  display_.setCursor(8, 11);
  display_.print("TOUCH LIVE TEST");
  display_.fillRoundRect(250, 3, 66, 26, 3, COLOR_PANEL);
  display_.drawRoundRect(250, 3, 66, 26, 3, COLOR_TEXT);
  display_.setTextColor(COLOR_TEXT, COLOR_PANEL);
  display_.setTextDatum(textdatum_t::middle_center);
  display_.drawString("BACK", 283, 16);
  display_.setTextColor(COLOR_MUTED, COLOR_BG);
  display_.setCursor(8, 42);
  display_.print("Touch corners and center");
  display_.drawRect(2, 35, 16, 16, COLOR_WARN);
  display_.drawRect(302, 35, 16, 16, COLOR_WARN);
  display_.drawRect(2, 222, 16, 16, COLOR_WARN);
  display_.drawRect(302, 222, 16, 16, COLOR_WARN);
  display_.drawCircle(160, 120, 10, COLOR_WARN);
  display_.fillRoundRect(100, 198, 120, 36, 4, COLOR_PANEL);
  display_.drawRoundRect(100, 198, 120, 36, 4, COLOR_ACCENT);
  display_.setTextColor(COLOR_TEXT, COLOR_PANEL);
  display_.setTextDatum(textdatum_t::middle_center);
  display_.drawString("CALIBRATE", 160, 216);
  display_.setTextDatum(textdatum_t::top_left);
  Serial.println("[TOUCH] Live mode. Press CALIBRATE or send c; q exits.");
}

void DiagnosticsApp::updateTouchScreen(const TouchPoint& point) {
  const uint32_t now = millis();
  if (now - lastTouchDraw_ < 30) return;
  lastTouchDraw_ = now;

  display_.fillRect(0, 64, 320, 28, COLOR_BG);
  display_.setTextColor(COLOR_TEXT, COLOR_BG);
  display_.setCursor(8, 70);
  display_.printf("raw %4u,%4u  p=%4u  screen %3d,%3d", point.rawX,
                  point.rawY, point.pressure, point.x, point.y);
  display_.fillCircle(point.x, point.y, 2, COLOR_OK);
  Serial.printf("[TOUCH] raw=%u,%u pressure=%u screen=%d,%d\n", point.rawX,
                point.rawY, point.pressure, point.x, point.y);
}

void DiagnosticsApp::startTouchCalibration(bool ignoreCurrentPress) {
  screen_ = Screen::Calibration;
  calibration_.start(ignoreCurrentPress);
  drawCalibrationTarget();
}

void DiagnosticsApp::drawCalibrationTarget() {
  display_.fillScreen(COLOR_BG);
  display_.setTextColor(COLOR_TEXT, COLOR_BG);
  display_.setTextDatum(textdatum_t::middle_center);
  char title[32];
  snprintf(title, sizeof(title), "CALIBRATION %u / 5",
           calibration_.pointIndex() + 1);
  display_.drawString(title, 160, 92);
  display_.setTextColor(COLOR_MUTED, COLOR_BG);
  display_.drawString("Hold the stylus on the cross", 160, 112);
  display_.fillRoundRect(122, 3, 76, 26, 3, COLOR_PANEL);
  display_.drawRoundRect(122, 3, 76, 26, 3, COLOR_ACCENT);
  display_.setTextColor(COLOR_TEXT, COLOR_PANEL);
  display_.drawString("CANCEL", 160, 16);

  const int16_t x = calibration_.targetX();
  const int16_t y = calibration_.targetY();
  display_.drawCircle(x, y, 10, COLOR_WARN);
  display_.drawCircle(x, y, 4, COLOR_TEXT);
  display_.drawFastHLine(x - 14, y, 29, COLOR_TEXT);
  display_.drawFastVLine(x, y - 14, 29, COLOR_TEXT);
  display_.setTextDatum(textdatum_t::top_left);
}

void DiagnosticsApp::updateTouchCalibration(bool pressed, const TouchPoint& point) {
  const CalibrationUpdate result = calibration_.update(pressed, point);
  switch (result) {
    case CalibrationUpdate::Sampling: {
      const int barWidth = map(calibration_.sampleCount(), 0, 24, 0, 120);
      display_.fillRect(100, 132, constrain(barWidth, 0, 120), 6, COLOR_OK);
      break;
    }
    case CalibrationUpdate::PointAccepted:
    case CalibrationUpdate::PointRetry:
      drawCalibrationTarget();
      break;
    case CalibrationUpdate::Completed:
      drawStatus("CALIBRATION SAVED", "Five points accepted",
                 "Returning to live test", COLOR_OK);
      delay(1400);
      runTouchTest();
      break;
    case CalibrationUpdate::Failed:
      drawStatus("CALIBRATION FAILED", "Invalid raw range",
                 "Try again with firm, centered presses", COLOR_ERROR);
      delay(1400);
      runTouchTest();
      break;
    case CalibrationUpdate::None:
      break;
  }
}

void DiagnosticsApp::runSdTest() {
  drawStatus("SD CARD", "Mounting and read/write test...", "SPI 10 MHz", COLOR_ACCENT);
  const SdTestResult result = sd_.runReadWriteTest();
  Serial.println("[SD]");
  Serial.printf("  Mounted: %s\n", result.mounted ? "PASS" : "FAIL");
  Serial.printf("  Type: %s\n", cardTypeName(result.cardType));
  Serial.printf("  Capacity: %.2f MiB, used: %.2f MiB\n",
                result.sizeBytes / 1048576.0, result.usedBytes / 1048576.0);
  Serial.printf("  Write/read/content: %s/%s/%s\n",
                result.writeOk ? "PASS" : "FAIL", result.readOk ? "PASS" : "FAIL",
                result.contentOk ? "PASS" : "FAIL");

  const bool ok = result.mounted && result.writeOk && result.readOk && result.contentOk;
  char capacity[48];
  snprintf(capacity, sizeof(capacity), "%s, %.0f MiB", cardTypeName(result.cardType),
           result.sizeBytes / 1048576.0);
  drawStatus(ok ? "SD PASS" : "SD FAIL", capacity,
             ok ? "Temporary test file removed" : "Check card, FAT32 and GPIO mapping",
             ok ? COLOR_OK : COLOR_ERROR);
}

void DiagnosticsApp::setRgb(bool red, bool green, bool blue) {
  const uint8_t on = board::LED_ACTIVE_LOW ? LOW : HIGH;
  const uint8_t off = board::LED_ACTIVE_LOW ? HIGH : LOW;
  digitalWrite(board::LED_RED, red ? on : off);
  digitalWrite(board::LED_GREEN, green ? on : off);
  digitalWrite(board::LED_BLUE, blue ? on : off);
}

void DiagnosticsApp::runIoTest() {
  drawStatus("ONBOARD I/O", "RGB LED + speaker + LDR", "Watch and listen", COLOR_ACCENT);
  Serial.println("[I/O] RGB sequence: red, green, blue");
  setRgb(true, false, false); delay(350);
  setRgb(false, true, false); delay(350);
  setRgb(false, false, true); delay(350);
  setRgb(false, false, false);

  Serial.println("[I/O] Speaker: two tones");
  tone(board::SPEAKER, 880, 180); delay(240);
  tone(board::SPEAKER, 1320, 180); delay(240);
  noTone(board::SPEAKER);

  const int lightRaw = analogRead(board::LIGHT_SENSOR);
  Serial.printf("[I/O] Light sensor GPIO%d raw value: %d (cover it and repeat)\n",
                board::LIGHT_SENSOR, lightRaw);
  char line[48];
  snprintf(line, sizeof(line), "Light sensor raw: %d", lightRaw);
  drawStatus("I/O TEST COMPLETE", line, "Confirm RGB and two tones", COLOR_OK);
}

void DiagnosticsApp::runMemoryTest() {
  printSystemInfo();
  char heapLine[48];
  char psramLine[48];
  snprintf(heapLine, sizeof(heapLine), "Heap free: %u KiB", ESP.getFreeHeap() / 1024);
  snprintf(psramLine, sizeof(psramLine), "Flash: %u MiB  PSRAM: %u KiB",
           ESP.getFlashChipSize() / 1048576, ESP.getPsramSize() / 1024);
  drawStatus("SYSTEM MEMORY", heapLine, psramLine, COLOR_ACCENT);
}

void DiagnosticsApp::runStressTest() {
  Serial.println("[STRESS] 600 cycles: display + touch + SD + heap tracking");
  drawStatus("BUS STRESS TEST", "Display + touch + SD", "Running 600 cycles...", COLOR_WARN);

  if (!sd_.mounted()) sd_.begin();
  const uint32_t startingHeap = ESP.getFreeHeap();
  uint32_t minimumHeap = startingHeap;
  uint32_t sdWrites = 0;
  uint32_t sdErrors = 0;
  uint32_t touchReads = 0;
  int previousX = 0;

  for (uint32_t iteration = 0; iteration < 600; ++iteration) {
    const int x = iteration % (board::SCREEN_WIDTH - 18);
    display_.fillRect(previousX, 175, 18, 18, COLOR_BG);
    display_.fillRect(x, 175, 18, 18, static_cast<uint16_t>(0x001F + iteration * 31));
    previousX = x;

    TouchPoint point;
    if (touch_.read(point)) ++touchReads;

    if (iteration % 30 == 0) {
      ++sdWrites;
      if (!sd_.appendStressRecord(iteration, ESP.getFreeHeap())) ++sdErrors;
    }

    minimumHeap = min(minimumHeap, ESP.getFreeHeap());
    if (iteration % 60 == 0) {
      display_.fillRect(0, 112, 320, 26, COLOR_BG);
      display_.setTextColor(COLOR_TEXT, COLOR_BG);
      display_.setCursor(20, 120);
      display_.printf("%lu/600  heap %lu KiB  SD errors %lu",
                      static_cast<unsigned long>(iteration),
                      static_cast<unsigned long>(ESP.getFreeHeap() / 1024),
                      static_cast<unsigned long>(sdErrors));
    }
    delay(5);
  }
  sd_.finishStressTest();

  Serial.printf("[STRESS] Finished. SD writes=%lu errors=%lu touch reads=%lu\n",
                static_cast<unsigned long>(sdWrites), static_cast<unsigned long>(sdErrors),
                static_cast<unsigned long>(touchReads));
  Serial.printf("[STRESS] Heap start/current/minimum=%u/%u/%u, largest block=%u\n",
                startingHeap, ESP.getFreeHeap(), minimumHeap,
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  const bool ok = sd_.mounted() && sdErrors == 0;
  drawStatus(ok ? "STRESS PASS" : "STRESS CHECK",
             ok ? "No SD errors" : "SD unavailable or write errors",
             "Temporary stress file removed", ok ? COLOR_OK : COLOR_WARN);
}
