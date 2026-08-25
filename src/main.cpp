// PlatformIO compatibility entry point. Arduino IDE ignores this block and
// uses OSEsp32.ino, so there is still exactly one setup()/loop().
#ifdef PLATFORMIO

#include "app/OSEsp32App.h"

OSEsp32App os;

void setup() {
  os.begin();
}

void loop() {
  os.update();
}

#endif
