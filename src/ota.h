#pragma once

#include <inttypes.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef OTA_URL_SIZE
   #define OTA_URL_SIZE 256
#endif


esp_err_t ota_install(const uint8_t * const url);

esp_err_t ota_install_encrypted(const uint8_t * const url, char * pem_start, char * pem_end);


#ifdef __cplusplus
}
#endif
