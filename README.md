# Simple wrapper ESP32 HTTPS Over-The-Air(OTA) IDF Component

[ESP HTTPS OTA](https://github.com/espressif/esp-idf/tree/master/components/esp_https_ota)


## Install 

1. Add **semver** dependency to **idf_component.yml**
```yml
dependencies:
  espressif/led_strip: "*"
  idf:
    version: ">=5"
  ota-esp-component:
    path: .
    git: https://github.com/stan-kondrat/ota-esp-component.git
```

2. Reconfigure project
```sh
idf.py reconfigure
```

## Usage

```c
#include "ota.h"

void app_main(void)
{
    ota_install("https://example.com/firmware.bin");
}
```

- Setup [OTA partition table](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html)
- [Bundle](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_crt_bundle.html) mbedTLS Root Certificates for HTTPS required.
- Or use CONFIG_OTA_ALLOW_HTTP for testing.


## License

MIT
