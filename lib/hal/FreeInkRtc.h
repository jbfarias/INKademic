#pragma once

// Use the FreeInk SDK RTC header explicitly. The ESP-IDF package also
// exposes a legacy soc/esp32c3/Rtc.h with the same basename; relying on
// <Rtc.h> lets include-path ordering select the wrong file.
#include "../../freeink-sdk/libs/hardware/Rtc/include/Rtc.h"
