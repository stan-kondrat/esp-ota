idf_component_register(SRCS "src/ota.c"
                    INCLUDE_DIRS src
                    PRIV_REQUIRES "log" "freertos" "esp_http_client" "esp-tls" "esp_https_ota" "app_update" "esp_encrypted_img")
