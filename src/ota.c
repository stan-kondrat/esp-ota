#include <stdio.h>

#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_encrypted_img.h"


#include "ota.h"

void ota_update_task(void *);
esp_err_t validate_image_header(const esp_app_desc_t *new_app_info);

#if CONFIG_ESP_HTTPS_OTA_DECRYPT_CB
static esp_err_t _decrypt_cb(decrypt_cb_arg_t *args, void *user_ctx);
#endif

static const char *TAG = "OTA";

TaskHandle_t xOTAUpdateTaskHandle = NULL;
uint8_t ota_firmware_upgrade_url[OTA_URL_SIZE] = {0};
char * ota_rsa_private_pem_start = NULL;
char * ota_rsa_private_pem_end = NULL;

esp_err_t ota_install(const uint8_t * const url) {
    return ota_install_encrypted(url, NULL, NULL);
}

esp_err_t ota_install_encrypted(const uint8_t * const url, char * pem_start, char * pem_end) {
    if (xOTAUpdateTaskHandle != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    strncpy((char *)ota_firmware_upgrade_url, (char *)url, OTA_URL_SIZE);
    ota_rsa_private_pem_start = pem_start;
    ota_rsa_private_pem_end = pem_end;
    ESP_LOGI(TAG, "ota_install (%s)", (char *)ota_firmware_upgrade_url);
    xTaskCreate(&ota_update_task, "ota_update_task", 1024 * 8, NULL, 5, &xOTAUpdateTaskHandle);
    return ESP_OK;
}



void ota_update_task(void *) {
    ESP_LOGI(TAG, "Starting OTA, (%s)", (char *)ota_firmware_upgrade_url);

    esp_err_t ota_finish_err = ESP_OK;
    esp_http_client_config_t http_config = {
        .url = (char *)ota_firmware_upgrade_url,
        .keep_alive_enable = true,
        .use_global_ca_store = true,
        .buffer_size_tx = 1024,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    #if CONFIG_ESP_HTTPS_OTA_DECRYPT_CB
    if (ota_rsa_private_pem_start != NULL && ota_rsa_private_pem_end != NULL) {
        esp_decrypt_cfg_t cfg = {};
        cfg.rsa_pub_key = ota_rsa_private_pem_start;
        cfg.rsa_pub_key_len = ota_rsa_private_pem_end - ota_rsa_private_pem_start;
        esp_decrypt_handle_t decrypt_handle = esp_encrypted_img_decrypt_start(&cfg);
        if (!decrypt_handle) {
            ESP_LOGE(TAG, "OTA upgrade failed");
            vTaskDelete(xOTAUpdateTaskHandle);
        }
        ota_config.decrypt_cb = _decrypt_cb;
        ota_config.decrypt_user_ctx = (void *)decrypt_handle;
    }
    #endif

    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed");
        vTaskDelete(xOTAUpdateTaskHandle);
    }

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_read_img_desc failed");
        goto ota_end;
    }
    err = validate_image_header(&app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "image header verification failed");
        goto ota_end;
    }

    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        // esp_https_ota_perform returns after every read operation which gives user the ability to
        // monitor the status of OTA upgrade by calling esp_https_ota_get_image_len_read, which gives length of image
        // data read so far.
        ESP_LOGD(TAG, "Image bytes read: %d", esp_https_ota_get_image_len_read(https_ota_handle));
    }

    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true) {
        // the OTA image was not completely received and user can customise the response to this situation.
        ESP_LOGE(TAG, "Complete data was not received.");
    } else {
        ota_finish_err = esp_https_ota_finish(https_ota_handle);
        if ((err == ESP_OK) && (ota_finish_err == ESP_OK)) {
            ESP_LOGI(TAG, "ESP_HTTPS_OTA upgrade successful. Rebooting ...");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            esp_restart();
        } else {
            if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {
                ESP_LOGE(TAG, "Image validation failed, image is corrupted");
            }
            ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed 0x%x", ota_finish_err);
            vTaskDelete(xOTAUpdateTaskHandle);
        }
    }

ota_end:
    esp_https_ota_abort(https_ota_handle);
    ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed");
    vTaskDelete(xOTAUpdateTaskHandle);
}

esp_err_t validate_image_header(const esp_app_desc_t *new_app_info) {
    if (new_app_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
        ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
    }

    return ESP_OK;
}

#if CONFIG_ESP_HTTPS_OTA_DECRYPT_CB
static esp_err_t _decrypt_cb(decrypt_cb_arg_t *args, void *user_ctx)
{
    if (args == NULL || user_ctx == NULL) {
        ESP_LOGE(TAG, "_decrypt_cb: Invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err;
    pre_enc_decrypt_arg_t pargs = {};
    pargs.data_in = args->data_in;
    pargs.data_in_len = args->data_in_len;
    err = esp_encrypted_img_decrypt_data((esp_decrypt_handle_t *)user_ctx, &pargs);
    if (err != ESP_OK && err != ESP_ERR_NOT_FINISHED) {
        return err;
    }
    static bool is_image_verified = false;
    if (pargs.data_out_len > 0) {
        args->data_out = pargs.data_out;
        args->data_out_len = pargs.data_out_len;
        if (!is_image_verified) {
            is_image_verified = true;
            const int app_desc_offset = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);
            // It is unlikely to not have App Descriptor available in first iteration of decrypt callback.
            assert(args->data_out_len >= app_desc_offset + sizeof(esp_app_desc_t));
            esp_app_desc_t *app_info = (esp_app_desc_t *) &args->data_out[app_desc_offset];
            err = validate_image_header(app_info);
            if (err != ESP_OK) {
                free(pargs.data_out);
            }
            return err;
        }
    } else {
        args->data_out_len = 0;
    }

    return ESP_OK;
}
#endif