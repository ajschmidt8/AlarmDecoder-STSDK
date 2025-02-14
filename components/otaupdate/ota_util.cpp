/**
 *  @file    ota_util.c
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    09/18/2020
 *  @version 1.0.3
 *
 *  @brief OTA update support
 *
 *  @copyright Copyright (C) 2020 Nu Tech Software Solutions, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

// common includes
// stdc
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <ctype.h>

// esp includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <lwip/netdb.h>
#include "driver/uart.h"
#include "esp_log.h"
static const char *TAG = "AD2OTA";

// AlarmDecoder includes
#include "alarmdecoder_main.h"
#include "ad2_utils.h"
#include "ad2_settings.h"
#include "ad2_uart_cli.h"
#include "device_control.h"

// specific includes
#include "ota_util.h"

#include "cJSON.h"
#include "mbedtls/sha256.h"
#include "mbedtls/rsa.h"
#include "mbedtls/pk.h"
#include "mbedtls/ssl.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const uint8_t public_key_start[]	asm("_binary_update_public_key_pem_start");
extern const uint8_t public_key_end[]		asm("_binary_update_public_key_pem_end");

extern const uint8_t root_pem_start[]	asm("_binary_update_root_pem_start");
extern const uint8_t root_pem_end[]		asm("_binary_update_root_pem_end");

// OTA Update task
TaskHandle_t ota_task_handle = NULL;

static unsigned int polling_day = 1;

static const char name_versioninfo[] = "versioninfo";
static const char name_latest[] = "latest";
static const char name_upgrade[] = "upgrade";
static const char name_polling[] = "polling";

static std::string ota_available_version = "N/A";

int ota_get_polling_period_day()
{
    return polling_day;
}

void _set_polling_period_day(unsigned int value)
{
    polling_day = value;
}

static void _task_fatal_error()
{
    ota_task_handle = NULL;
    ESP_LOGE(TAG, "Exiting task due to fatal error...");
    (void)vTaskDelete(NULL);

    while (1) {
        ;
    }
}

/**
 * @brief OTA task that preforms the update to the flash
 * from a verified signed firmware from an https server with
 * known keys.
 */
static void ota_task_func(void * pvParameter)
{
    ESP_LOGI(TAG, "Starting OTA");

    esp_err_t ret = ota_https_update_device();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Firmware Upgrades Failed (%d)", ret);
        _task_fatal_error();
    }

    ESP_LOGI(TAG, "Prepare to restart system!");
    hal_restart();
}

/**
 * @brief Parse the json available version info.
 */
esp_err_t ota_api_get_available_version(char *update_info, unsigned int update_info_len, char **new_version)
{
    cJSON *root = NULL;
    cJSON *profile = NULL;
    cJSON *item = NULL;
    cJSON *array = NULL;
    char *latest_version = NULL;
    char *data = NULL;
    size_t str_len = 0;

    bool is_new_version = false;

    esp_err_t ret = ESP_OK;

    if (!update_info) {
        ESP_LOGE(TAG, "%s: Invalid parameter", __func__);
        return ESP_ERR_INVALID_ARG;
    }

    data = (char*)malloc((size_t) update_info_len + 1);
    if (!data) {
        ESP_LOGE(TAG, "%s: Couldn't allocate memory to add version info", __func__);
        return ESP_ERR_NO_MEM;
    }
    memcpy(data, update_info, update_info_len);
    data[update_info_len] = '\0';

    root = cJSON_Parse((char *)data);
    profile = cJSON_GetObjectItem(root, name_versioninfo);
    if (!profile) {
        ret = ESP_FAIL;
        goto clean_up;
    }

    /* polling */
    item = cJSON_GetObjectItem(profile, name_polling);
    if (!item) {
        ret = ESP_FAIL;
        goto clean_up;
    }
    polling_day = atoi(cJSON_GetStringValue(item));

    _set_polling_period_day((unsigned int)polling_day);

    array = cJSON_GetObjectItem(profile, name_upgrade);

    ESP_LOGI(TAG, "Checking if this version supports upgrade");
    for (int i = 0 ; i < cJSON_GetArraySize(array) ; i++) {
        char *upgrade = cJSON_GetArrayItem(array, i)->valuestring;
        if (strcmp(upgrade, FIRMWARE_VERSION) == 0) {
            ESP_LOGD(TAG, "Test supported version '%s' PASS", upgrade);
            is_new_version = true;
            break;
        }
    }

    if (is_new_version) {
        ESP_LOGI(TAG, "%s: Found current version update support.", __func__);

        /* latest */
        item = cJSON_GetObjectItem(profile, name_latest);
        if (!item) {
            ESP_LOGE(TAG, "%s: IOT_ERROR_UNINITIALIZED", __func__);
            ret = ESP_FAIL;
            goto clean_up;
        }

        str_len = strlen(cJSON_GetStringValue(item));
        latest_version = (char*)malloc(str_len + 1);
        if (!latest_version) {
            ESP_LOGE(TAG, "%s: Couldn't allocate memory to add latest version", __func__);
            ret = ESP_ERR_NO_MEM;
            goto clean_up;
        }
        strncpy(latest_version, cJSON_GetStringValue(item), str_len);
        latest_version[str_len] = '\0';
        *new_version = latest_version;
    } else {
        ESP_LOGI(TAG, "%s: Not found current version update support.", __func__);
    }

clean_up:

    if (root) {
        cJSON_Delete(root);
    }
    if (data) {
        free(data);
    }

    return ret;
}

/**
 * @brief HTTP request event handler.
 *
 * @param [in]evt http event pointer.
 *
 */
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "%s: HTTP_EVENT_ERROR", __func__);
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "%s: HTTP_EVENT_ON_CONNECTED", __func__);
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "%s: HTTP_EVENT_HEADER_SENT", __func__);
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "%s: HTTP_EVENT_ON_HEADER, key=%s, value=%s", __func__, evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "%s: HTTP_EVENT_ON_DATA, len=%d", __func__, evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "%s: HTTP_EVENT_ON_FINISH", __func__);
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "%s: HTTP_EVENT_DISCONNECTED", __func__);
        break;
    }
    return ESP_OK;
}

/**
 * @brief cleanup client memory.
 *
 * @param [in]client handle for the client.
 */
static void _http_cleanup(esp_http_client_handle_t client)
{
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}

static void _print_sha256 (const uint8_t *image_hash, const char *label)
{
    char hash_print[OTA_CRYPTO_SHA256_LEN * 2 + 1];
    hash_print[OTA_CRYPTO_SHA256_LEN * 2] = 0;
    for (int i = 0; i < OTA_CRYPTO_SHA256_LEN; ++i) {
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    }
    ESP_LOGD(TAG, "%s: %s: %s", __func__, label, hash_print);
}

static int _crypto_sha256(const unsigned char *src, size_t src_len, unsigned char *dst)
{
    int ret;

    ESP_LOGD(TAG, "%s: src: %d@%p, dst: %p", __func__, src_len, src, dst);

    ret = mbedtls_sha256_ret(src, src_len, dst, 0);
    if (ret) {
        ESP_LOGD(TAG, "%s: mbedtls_sha256_ret = -0x%04X", __func__, -ret);
        return ret;
    }

    return 0;
}

/**
 * @brief Verify the signature matches the key.
 *
 * @param [in]sig char * to the signature to check.
 * @param [in]hash char * to the hash for the check.
 */
static int _pk_verify(const unsigned char *sig, const unsigned char *hash)
{
    int ret;

    mbedtls_pk_context pk;

    unsigned char *public_key = (unsigned char *) public_key_start;
    unsigned int public_key_len = public_key_end - public_key_start;

    mbedtls_pk_init( &pk );
    // Make sure our key is a null terminated string and send the null to the parser.
    std::string t((const char *)public_key, public_key_len);
    ret = mbedtls_pk_parse_public_key( &pk, (const unsigned char *)t.c_str(), t.length() + 1 );
    if (ret != 0) {
        ESP_LOGE(TAG, "%s: Parse error: 0x%04X", __func__, ret);
        goto clean_up;
    }

    if (!mbedtls_pk_can_do( &pk, MBEDTLS_PK_RSA)) {
        ESP_LOGE(TAG, "%s: Failed! Key is not an RSA key", __func__);
        ret = MBEDTLS_ERR_SSL_PK_TYPE_MISMATCH;
        goto clean_up;
    }

    ret = mbedtls_rsa_check_pubkey(mbedtls_pk_rsa(pk));
    if (ret != 0) {
        ESP_LOGE(TAG, "%s: Check pubkey failed: 0x%04X", __func__, ret);
        goto clean_up;
    }

    if ((ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, OTA_CRYPTO_SHA256_LEN, sig, OTA_SIGNATURE_SIZE)) != 0 ) {
        ESP_LOGE(TAG, "%s: Invaild firmware : 0x%04X", __func__, ret);
        goto clean_up;
    }

clean_up:

    mbedtls_pk_free( &pk );

    return ret;
}

/**
 * @brief Check firmware signature is valid
 *
 * @param [in]sha256 hash for server cert the firmware is signed with.
 * @param [in]sig_data signature for cert teh firmware is signed with.
 * @param [in]sig_len length of signature.
 *
 * @return bool true on success false on fail.
 */
static bool _check_firmware_validation(const unsigned char *sha256, unsigned char *sig_data, unsigned int sig_len)
{
    bool ret = false;
    unsigned char sig[OTA_SIGNATURE_SIZE] = {0,};
    unsigned char hash[OTA_CRYPTO_SHA256_LEN] = {0,};

    // Get the message digest info structure for SHA256
    if (!sig_data) {
        ESP_LOGE(TAG, "%s: invalid sig data", __func__);
        goto clean_up;
    }

    if (sig_len < OTA_DEFAULT_SIGNATURE_BUF_SIZE) {
        ESP_LOGE(TAG, "%s: invalid sig len : %d", __func__, sig_len);
        goto clean_up;
    }

    // Dump buffer for debugging
    _print_sha256(sha256, "SHA-256 for current firmware: ");

    // Get the message digest info structure for SHA256
    if (_crypto_sha256(sha256, OTA_CRYPTO_SHA256_LEN, hash) != 0) {
        ESP_LOGE(TAG, "%s: invalid digest", __func__);
        goto clean_up;
    }

    // check signature's header
    if (sig_data[0] != 0xff || sig_data[1] != 0xff ||
            sig_data[2] != 0xff || sig_data[3] != 0xff) {
        ESP_LOGE(TAG, "%s: invalid signature header", __func__);
        goto clean_up;
    }

    // check signature's footer
    if (sig_data[OTA_DEFAULT_SIGNATURE_BUF_SIZE - 1] != 0xff || sig_data[OTA_DEFAULT_SIGNATURE_BUF_SIZE - 2] != 0xff ||
            sig_data[OTA_DEFAULT_SIGNATURE_BUF_SIZE - 3] != 0xff || sig_data[OTA_DEFAULT_SIGNATURE_BUF_SIZE - 4] != 0xff) {
        ESP_LOGE(TAG, "%s: Invalid footer", __func__);
        goto clean_up;
    }

    memcpy(sig, sig_data + OTA_SIGNATURE_PREFACE_SIZE, OTA_SIGNATURE_SIZE);

    if (_pk_verify((const unsigned char *)sig, hash) != 0) {
        ESP_LOGE(TAG, "%s: _pk_verify is failed", __func__);
    } else {
        ret = true;
    }

clean_up:

    return ret;
}

/**
 * @brief Fetch check and flash firmware from remote server.
 *
 * @return esp_err_t results.
 */
esp_err_t ota_https_update_device()
{
    ESP_LOGI(TAG, "%s: ota_https_update_device", __func__);

    esp_err_t ret = ESP_FAIL;

    unsigned int content_len;
    unsigned int firmware_len;

    esp_http_client_config_t* config = (esp_http_client_config_t*)calloc(sizeof(esp_http_client_config_t), 1);
    config->url = CONFIG_FIRMWARE_UPGRADE_URL;
    config->cert_pem = (char *)root_pem_start;
    config->event_handler = _http_event_handler;

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init( &ctx );
    if (mbedtls_sha256_starts_ret( &ctx, 0) != 0 ) {
        ESP_LOGE(TAG, "%s: Failed to initialise api", __func__);
        return ESP_FAIL;
    }

    esp_http_client_handle_t client = esp_http_client_init(config);
    if (client == NULL) {
        ESP_LOGE(TAG, "%s: Failed to initialise HTTP connection", __func__);
        return ESP_FAIL;
    }

    if (esp_http_client_get_transport_type(client) != HTTP_TRANSPORT_OVER_SSL) {
        ESP_LOGE(TAG, "%s: Transport is not over HTTPS", __func__);
        return ESP_FAIL;
    }

    ret = esp_http_client_open(client, 0);
    if (ret != ESP_OK) {
        esp_http_client_cleanup(client);
        ESP_LOGE(TAG, "%s: Failed to open HTTP connection: %d", __func__, ret);
        return ret;
    }
    content_len = esp_http_client_fetch_headers(client);
    if (content_len <= OTA_DEFAULT_SIGNATURE_BUF_SIZE) {
        ESP_LOGE(TAG, "%s: content size error", __func__);
        _http_cleanup(client);
        return ESP_FAIL;
    }

    firmware_len = content_len - (OTA_DEFAULT_SIGNATURE_BUF_SIZE);

    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = NULL;
    ESP_LOGI(TAG, "%s: Starting OTA upgrade", __func__);
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "%s: Passive OTA partition not found", __func__);
        _http_cleanup(client);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "%s: Writing to partition subtype %d at offset 0x%x", __func__,
             update_partition->subtype, update_partition->address);

    ret = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "%s: esp_ota_begin failed, error=%d", __func__, ret);
        _http_cleanup(client);
        return ret;
    }
    ESP_LOGI(TAG, "%s: esp_ota_begin succeeded. Please Wait. This may take time.", __func__);

    esp_err_t ota_write_err = ESP_OK;
    char *upgrade_data_buf = (char *)malloc(OTA_DEFAULT_BUF_SIZE);
    if (!upgrade_data_buf) {
        ESP_LOGE(TAG, "%s: Couldn't allocate memory to upgrade data buffer", __func__);
        _http_cleanup(client);
        return ESP_ERR_NO_MEM;
    }
    unsigned char *sig_ptr = NULL;
    unsigned char *sig = NULL;
    unsigned int sig_len = 0;
    unsigned int total_read_len = 0;
    unsigned int remain_len = 0;
    unsigned int excess_len = 0;

    sig = (unsigned char *)malloc(OTA_DEFAULT_SIGNATURE_BUF_SIZE);
    if (!sig) {
        ESP_LOGE(TAG, "%s: Couldn't allocate memory to add sig buffer", __func__);
        free(upgrade_data_buf);
        _http_cleanup(client);
        return ESP_ERR_NO_MEM;
    }
    memset(sig, '\0', OTA_DEFAULT_SIGNATURE_BUF_SIZE);

    sig_ptr = sig;

    while (1) {
        int data_read = esp_http_client_read(client, upgrade_data_buf, OTA_DEFAULT_BUF_SIZE);
        if (data_read == 0) {
            ESP_LOGI(TAG, "%s: Connection closed,all data received", __func__);
            break;
        }
        if (data_read < 0) {
            ESP_LOGE(TAG, "%s: Error: SSL data read error", __func__);
            break;
        }

        // checking data to get signature info
        if (data_read + total_read_len > firmware_len) {

            remain_len = firmware_len - total_read_len;
            excess_len = data_read - remain_len;

            if (sig_len + excess_len > OTA_DEFAULT_SIGNATURE_BUF_SIZE) {
                ESP_LOGE(TAG, "%s: Invalid signature len : %d, %d", __func__, sig_len, excess_len);
                break;
            }

            memcpy(sig_ptr, upgrade_data_buf + remain_len, excess_len);
            sig_ptr += excess_len;
            sig_len += excess_len;

            /* Change value to write firmware data */
            data_read = remain_len;
        }

        if (data_read > 0) {
            if (mbedtls_sha256_update_ret(&ctx, (const unsigned char *)upgrade_data_buf, data_read) != 0) {
                ESP_LOGE(TAG, "%s: Failed getting HASH", __func__);
            }

            ota_write_err = esp_ota_write( update_handle, (const void *)upgrade_data_buf, data_read);
            if (ota_write_err != ESP_OK) {
                break;
            }

            total_read_len += data_read;
        }
    }

    ESP_LOGI(TAG, "%s: Total binary data length writen: %d", __func__, total_read_len);

    unsigned char md[OTA_CRYPTO_SHA256_LEN] = {0,};

    if (mbedtls_sha256_finish_ret( &ctx, md) != 0) {
        ESP_LOGE(TAG, "%s: Failed getting HASH", __func__);
        ret = ESP_FAIL;
        goto clean_up;
    }
    mbedtls_sha256_free(&ctx);

    /* Check firmware validation */
    if (_check_firmware_validation((const unsigned char *)md, sig, sig_len) != true) {
        ESP_LOGE(TAG,"%s: Signature verified NOK", __func__);
        ret = ESP_FAIL;
        goto clean_up;
    }

    ret = esp_ota_end(update_handle);
    if (ota_write_err != ESP_OK) {
        ESP_LOGE(TAG, "%s: esp_ota_write failed! err=0x%d", __func__, ota_write_err);
        goto clean_up;
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "%s: esp_ota_end failed! err=0x%d. Image is invalid", __func__, ret);
        goto clean_up;
    }

    ret = esp_ota_set_boot_partition(update_partition);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "%s: esp_ota_set_boot_partition failed! err=0x%d", __func__, ret);
        goto clean_up;
    }

    ESP_LOGI(TAG, "%s: esp_ota_set_boot_partition succeeded", __func__);

clean_up:

    if (sig) {
        free(sig);
    }

    if (upgrade_data_buf) {
        free(upgrade_data_buf);
    }

    _http_cleanup(client);

    return ret;
}

/**
 * @brief Fetch the current version info from remote server json file.
 *
 * @param [in]version_info pointer to pointer to place version info.
 * @param [in]version_info_len pointer to int length of version buffer.
 *
 * @return esp_err_t ret
 */
esp_err_t ota_https_read_version_info(char **version_info, unsigned int *version_info_len)
{
    esp_err_t ret = ESP_FAIL;

    esp_http_client_config_t* config = (esp_http_client_config_t*)calloc(sizeof(esp_http_client_config_t), 1);
    config->url = CONFIG_FIRMWARE_VERSOIN_INFO_URL;
    config->cert_pem = (char *)root_pem_start;
    config->event_handler = _http_event_handler;

    esp_http_client_handle_t client = esp_http_client_init(config);
    free(config);
    if (client == NULL) {
        ESP_LOGE(TAG, "%s: Failed to initialise HTTP connection", __func__);
        return ESP_FAIL;
    }

    if (esp_http_client_get_transport_type(client) != HTTP_TRANSPORT_OVER_SSL) {
        ESP_LOGE(TAG, "%s: Transport is not over HTTPS", __func__);
        return ESP_FAIL;
    }

    ret = esp_http_client_open(client, 0);
    if (ret != ESP_OK) {
        esp_http_client_cleanup(client);
        ESP_LOGE(TAG, "%s: Failed to open HTTP connection: %d", __func__, ret);
        return ret;
    }
    esp_http_client_fetch_headers(client);

    char *upgrade_data_buf = (char *)malloc(OTA_DEFAULT_BUF_SIZE);
    if (!upgrade_data_buf) {
        esp_http_client_cleanup(client);
        ESP_LOGE(TAG, "%s: Couldn't allocate memory to upgrade data buffer", __func__);
        return ESP_ERR_NO_MEM;
    }

    unsigned int total_read_len = 0;

    char *read_data_buf = (char *)malloc(OTA_VERSION_INFO_BUF_SIZE);
    if (!read_data_buf) {
        esp_http_client_cleanup(client);
        free(upgrade_data_buf);
        ESP_LOGE(TAG, "%s: Couldn't allocate memory to read data buffer", __func__);
        return ESP_ERR_NO_MEM;
    }
    memset(read_data_buf, '\0', OTA_VERSION_INFO_BUF_SIZE);
    char *ptr = read_data_buf;

    while (1) {
        int data_read = esp_http_client_read(client, upgrade_data_buf, OTA_DEFAULT_BUF_SIZE);
        if (data_read == 0) {
            ESP_LOGI(TAG, "%s: Connection closed,all data received", __func__);
            break;
        }
        if (data_read < 0) {
            ESP_LOGE(TAG, "%s: Error: SSL data read error", __func__);
            break;
        }
        if (data_read > 0) {
            total_read_len += data_read;

            if (total_read_len >= OTA_VERSION_INFO_BUF_SIZE) {
                ESP_LOGE(TAG, "%s: Max length of data is exceeded.", __func__);
                break;
            }

            memcpy(ptr, upgrade_data_buf, data_read);
            ptr += data_read;
        }
    }

    ESP_LOGI(TAG, "%s: received body size %d", __func__, total_read_len);

    _http_cleanup(client);

    free(upgrade_data_buf);

    if (total_read_len == 0) {
        ESP_LOGE(TAG, "%s: read error", __func__);
        free(read_data_buf);
        return ESP_ERR_INVALID_SIZE;
    }

    *version_info = read_data_buf;
    *version_info_len = total_read_len;
    return ESP_OK;
}

/**
 * @brief Check if an update is available then sleep a long time.
 *
 * @param [in]arg void * passed in on init
 */
static void ota_polling_task_func(void *arg)
{
    while (1) {

        vTaskDelay(30000 / portTICK_PERIOD_MS);

        ESP_LOGI(TAG, "Starting check new version with current version '%s'", FIRMWARE_VERSION);

        if (ota_task_handle != NULL) {
            ESP_LOGI(TAG, "Device is currently updating skipping checks for now.");
            continue;
        }

        if (g_ad2_network_state != AD2_CONNECTED) {
            ESP_LOGI(TAG, "Device update check aborted. No internet connection.");
            continue;
        }

        char *read_data = NULL;
        unsigned int read_data_len = 0;

        esp_err_t ret = ota_https_read_version_info(&read_data, &read_data_len);
        if (ret == ESP_OK) {
            char *available_version = NULL;
            esp_err_t err = ota_api_get_available_version(read_data, read_data_len, &available_version);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "ota_api_get_available_version failed : %d", err);
                continue;
            }

            // Update and notify subscribers of a new version
            if (available_version) {
                ota_available_version = available_version;
                AD2Parse.updateVersion(available_version);
                ESP_LOGI(TAG, "Get avail version found '%s' on the server.", available_version);
                free(available_version);
            } else {
                // if nothing available then it must be the same we have installed.
                ota_available_version = FIRMWARE_VERSION;
                ESP_LOGI(TAG, "Get avail version found NO available version on the server.");
            }
        }

        /* Set polling period */
        unsigned int polling_day = ota_get_polling_period_day();
        unsigned int task_delay_sec = polling_day * 24 * 3600;
        vTaskDelay(task_delay_sec * 1000 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief command list for module
 */
static struct cli_command ota_cmd_list[] = {
    {
        (char*)OTA_UPGRADE_CMD,(char*)
        "- Preform an OTA upgrade now download and install new flash.\r\n\r\n"
        "  ```" OTA_UPGRADE_CMD "```\r\n\r\n", ota_do_update
    },
    {
        (char*)OTA_VERSION_CMD,(char*)
        "- Report the current and available version.\r\n\r\n"
        "  ```" OTA_VERSION_CMD "```\r\n\r\n", ota_do_version
    }
};

/**
 * @brief Start the OTA check task.
 */
void ota_init()
{
    // Register twilio CLI commands
    for (int i = 0; i < ARRAY_SIZE(ota_cmd_list); i++) {
        cli_register_command(&ota_cmd_list[i]);
    }

    xTaskCreate(ota_polling_task_func, "ota_polling_task_func", 8096, NULL, tskIDLE_PRIORITY+1, NULL);
}

/**
 * @brief Initiate and OTA update
 */
void ota_do_update(char *arg)
{
    xTaskCreate(&ota_task_func, "ota_task_func", 8096, NULL, tskIDLE_PRIORITY+2, &ota_task_handle);
}

/**
 * @brief Show installed and available version
 */
void ota_do_version(char *arg)
{
    ad2_printf_host("Installed version(" FIRMWARE_VERSION  ") available version(%s)\r\n", ota_available_version.c_str());
}

#ifdef __cplusplus
} // extern "C"
#endif
