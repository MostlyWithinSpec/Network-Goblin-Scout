#pragma once
// Force-included into every file under src/ (see build_src_flags in platformio.ini).
// The framework and libraries are built at CORE_DEBUG_LEVEL 2 (warnings only) because
// the BLE library logs every advertiser's raw MAC at info level. Our own code logs at
// info so log_i() still reaches the serial monitor.
#undef CORE_DEBUG_LEVEL
#define CORE_DEBUG_LEVEL 3
