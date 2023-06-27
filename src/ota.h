#pragma once

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ota_config_t {
    uint8_t url[256];
} ota_config_t;

void ota_install(ota_config_t config);

#ifdef __cplusplus
}
#endif
