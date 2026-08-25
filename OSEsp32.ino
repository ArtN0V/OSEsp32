#include "src/app/OSEsp32App.h"

OSEsp32App os;

void setup() {
  os.begin();
}

void loop() {
  os.update();
}
