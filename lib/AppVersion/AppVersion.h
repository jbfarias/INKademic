#pragma once

// PlatformIO normally supplies these through build_flags/extra_scripts. Keep
// fallbacks here so editor indexers and simulator-like tools still parse files.
#ifndef INKADEMIC_VERSION
#define INKADEMIC_VERSION "dev"
#endif

#ifndef INKADEMIC_BUILD_ENV
#define INKADEMIC_BUILD_ENV "unknown"
#endif

#ifndef INKADEMIC_FIRMWARE_DEVICE_TYPE
#define INKADEMIC_FIRMWARE_DEVICE_TYPE "unknown"
#endif
