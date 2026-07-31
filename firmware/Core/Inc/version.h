#ifndef __VERSION_H
#define __VERSION_H

#define FIRMWARE_VERSION_MAJOR  0
#define FIRMWARE_VERSION_MINOR  0
#define FIRMWARE_VERSION_PATCH  1
#define FIRMWARE_VERSION_STRING "0.0.1-vulnerable"

#define FIRMWARE_BUILD_DATE     __DATE__
#define FIRMWARE_BUILD_TIME     __TIME__

// Security status
#define SECURITY_STATUS         "VULNERABLE"
#define SECURITY_LEVEL          0  // 0=vulnerable, 1=hardened

#endif /* __VERSION_H */