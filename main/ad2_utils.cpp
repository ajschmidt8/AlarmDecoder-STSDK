/**
 *  @file    ad2_utils.cpp
 *  @author  Sean Mathews <coder@f34r.com>
 *  @date    02/20/2020
 *
 *  @brief AD2IOT common utils shared between main and components
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

// esp includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <lwip/netdb.h>
#include "driver/uart.h"
#include "esp_log.h"
static const char *TAG = "AD2UTIL";

// mbedtls
#include "mbedtls/base64.h"

// AlarmDecoder includes
#include "alarmdecoder_main.h"
#include "ad2_utils.h"
#include "ad2_settings.h"

#include "ad2_utils.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * build Bearer auth string from api key
 */
std::string ad2_make_bearer_auth_header(const std::string& apikey)
{
    return "Authorization: Bearer " + apikey;
}

/**
 * @brief build auth string from user and pass.
 *
 * @arg [in]user std::string * to user.
 * @arg [in]password std::string * to password.
 *
 * @return std::string basic auth string
 *
 */
std::string ad2_make_basic_auth_header(const std::string& user, const std::string& password)
{

    size_t toencodeLen = user.length() + password.length() + 2;
    size_t out_len = 0;
    char toencode[toencodeLen];
    unsigned char outbuffer[(toencodeLen + 2 - ((toencodeLen + 2) % 3)) / 3 * 4 + 1];

    memset(toencode, 0, toencodeLen);

    snprintf(
        toencode,
        toencodeLen,
        "%s:%s",
        user.c_str(),
        password.c_str()
    );

    mbedtls_base64_encode(outbuffer,sizeof(outbuffer),&out_len,(unsigned char*)toencode, toencodeLen-1);
    outbuffer[out_len] = '\0';

    std::string encoded_string = std::string((char *)outbuffer);
    return "Authorization: Basic " + encoded_string;
}

/**
 * @brief url encode a string making it safe for http protocols.
 *
 * @arg [in]str std::string to url encode.
 *
 * @return std::string url encoded string
 *
 */
std::string ad2_urlencode(const std::string str)
{
    std::string encoded = "";
    char c;
    char code0;
    char code1;
    for (int i = 0; i < str.length(); i++) {
        c = str[i];
        if (c == ' ') {
            encoded += '+';
        } else if (isalnum(c)) {
            encoded += c;
        } else {
            code1 = (c & 0xf) + '0';
            if ((c & 0xf) > 9) {
                code1 = (c & 0xf) - 10 + 'A';
            }
            c = (c >> 4) & 0xf;
            code0 = c + '0';
            if (c > 9) {
                code0 = c - 10 + 'A';
            }
            encoded += '%';
            encoded += code0;
            encoded += code1;
        }
    }
    return encoded;
}

/**
 * @brief find value by name in query string config data
 *    param1=val1&param2=val2
 *
 * @arg [in]qry_string std::string * to scan for NV data.
 * @arg [in]key char * key to search for.
 * @arg [in]val std::string &. Result buffer for value.
 * @arg [in]val_size size of output buffer.
 *
 * @return int results. < -2 error, -1 not found, >= 0 result length
 *
 * @note val is cleared after param check.
 */
int ad2_query_key_value(std::string  &qry_str, const char *key, std::string &val)
{

    /* Test parmeters. */
    if ( !qry_str.length() || key == NULL) {
        return -2;
    }

    /* Clear val first. */
    val = "";

    /* Init state machine args and get raw pointer to our query string. */
    int keylen = strlen(key);
    const char * qry_ptr = qry_str.c_str();

    /* Process until we reach null terminator on the query string. */
    while ( *qry_ptr ) {
        const char *tp = qry_ptr;
        int len = 0;
        static char chr;

        /* Scan the KEY looking for the next terminator. */
        while ( (chr = *tp) != 0 ) {
            if (chr == '=' || chr == '&') {
                break;
            }
            len++;
            tp++;
        }

        /* Test key for a match. */
        if ( len && len == keylen ) {
            if ( strncasecmp (key, qry_ptr, keylen) == 0 ) {

                /* move the index */
                len++;
                tp++;

                /* Test for null value. Still valid just return empty string. */
                if ( !chr || chr == '&' ) {
                    return val.length();
                }

                /* Save the value. */
                while ( ( chr = *tp ) != 0 ) {
                    if ( chr == '=' || chr == '&' ) {
                        break;
                    }
                    val += chr;
                    tp++;
                }
                return val.length();
            }
        }

        /* End of string and key not found. We are done. */
        if ( !chr ) {
            return -1;
        }

        /* Keep looking skip last terminator. */
        len++;

        /* Scan till we start the next set. */
        qry_ptr += len;
        while ( chr && chr != '&' ) {
            chr = *qry_ptr++;
        }

        /* End of string and not found. We are done. */
        if ( !chr ) {
            return -1;
        }
    }
    return -1;
}

/**
 * @brief conver bytes in a std::string to upper case.
 *
 * @param [in]str std::string & to modify
 */
void ad2_ucase(std::string &str)
{
    for (std::string::size_type i=0; i<str.length(); ++i) {
        str[i] = std::toupper(str[i]);
    }
}

/**
 * @brief conver bytes in a std::string to lower case.
 *
 * @param [in]str std::string & to modify
 */
void ad2_lcase(std::string &str)
{
    for (std::string::size_type i=0; i<str.length(); ++i) {
        str[i] = std::tolower(str[i]);
    }
}

/**
 * @brief fix missing std::to_string()
 *
 * @param [in]n int value to convert to string.
 *
 * @return std::string
 *
 */
std::string ad2_to_string(int n)
{
    std::ostringstream stm;
    stm << n;
    return stm.str();
}

/**
 * @brief split string to vector on token
 *
 * @param [in]str std::string input string
 * @param [in]delim const char delimeter
 * @param [in]out pointer to output std::vector of std:strings
 *
 */
void ad2_tokenize(std::string const &str, const char delim,
                  std::vector<std::string> &out)
{
    // construct a stream from the string
    std::stringstream ss(str);

    std::string s;
    while (std::getline(ss, s, delim)) {
        out.push_back(s);
    }
}

/**
 * @brief printf formatting for std::string.
 *
 * @param [in]fmt std::string format.
 * @param [in]size size_t buffer limits.
 * @param [in]va_list variable args list.
 *
 * @return new std::string
 */
std::string ad2_string_vasnprintf(const char *fmt, size_t size, va_list args)
{
    size_t len = size + 1; // Add room for null
    std::string out = "";
    char temp[len];
    vsnprintf(temp, len, fmt, args);
    out = temp;
    return out;
}

/**
 * @brief printf formatting for std::string.
 *
 * @param [in]fmt std::string format.
 * @param [in]va_list variable args list.
 *
 * @return new std::string
 */
std::string ad2_string_vaprintf(const char *fmt, va_list args)
{
    std::string out = "";
    int len = vsnprintf(NULL, 0, fmt, args);
    if (len < 0) {
        return out;
    }
    len++; // account for null after tests.
    char temp[len];
    len = vsnprintf(temp, len, fmt, args);

    if (len) {
        out = temp;
    }

    return out;
}

/**
 * @brief printf formatting for std::string.
 *
 * @param [in]fmt std::string format.
 * @param [in]... variable args.
 *
 * @return new std::string
 */
std::string ad2_string_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    std::string out = ad2_string_vaprintf(fmt, args);
    va_end(args);
    return out;
}

/**
 * @brief replace all occurrences of a string in another string.
 *
 * @param [in]inStr std::string source.
 * @param [in]findStr std::string & what to look for.
 * @param [in]replaceStr std::string & what to change it to.
 *
 * @return bool true|false
 *
 */
bool ad2_replace_all(std::string& inStr, const char *findStr, const char *replaceStr)
{
    int findLen = strlen(findStr);
    if (!findLen) {
        return false;
    }

    size_t i = inStr.find(findStr);
    if (i == std::string::npos) {
        return false;
    }

    int replaceLen = strlen(replaceStr);

    while (i != std::string::npos) {
        inStr.replace(i, findLen, replaceStr);
        i = inStr.find(findStr, replaceLen + i);
    }

    return true;
}


/**
 * @brief left trim.
 *
 * @param [in]s std::string &.
 */
void ad2_ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                    std::not1(std::ptr_fun<int, int>(std::isspace))));
}

/**
 * @brief right trim.
 *
 * @param [in]s std::string &.
 */
void ad2_rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
}

/**
 * @brief all trim.
 *
 * @param [in]s std::string &.
 */
void ad2_trim(std::string &s)
{
    ad2_ltrim(s);
    ad2_rtrim(s);
}

/**
 * @brief Generic get NV string value by key and slot(0-99).
 *
 * @param [in]key to search for.
 * @param [in]slot inter slot from 0 - 99.
 * @param [in]s char * suffix.
 * @param [out]valueout int * to store search results.
 *
 */
void ad2_get_nv_slot_key_int(const char *key, int slot, const char *s, int *valueout)
{
    esp_err_t err;

    // Open NVS
    nvs_handle my_handle;
    err = nvs_open(key, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: Error (%s) opening NVS handle!", __func__, esp_err_to_name(err));
    } else {
        std::string tkey = ad2_string_printf("%02i%s", slot, s == nullptr ? "" : s);
        err = nvs_get_i32(my_handle, tkey.c_str(), valueout);
        nvs_close(my_handle);
    }
}

/**
 * @brief Generic set NV string value by key and slot(0-99).
 *
 * @param [in]key to search for.
 * @param [in]slot inter slot from 0 - 99.
 * @param [in]s char * suffix.
 * @param [in]value int value to store for search results.
 *
 * @note  value < 0 will remove entry
 */
void ad2_set_nv_slot_key_int(const char *key, int slot, const char *s, int value)
{
    esp_err_t err;

    // Open NVS
    nvs_handle my_handle;
    err = nvs_open(key, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: Error (%s) opening NVS handle!", __func__, esp_err_to_name(err));
    } else {
        std::string tkey = ad2_string_printf("%02i%s", slot, s == nullptr ? "" : s);
        if (value == -1) {
            err = nvs_erase_key(my_handle, tkey.c_str());
        } else {
            err = nvs_set_i32(my_handle, tkey.c_str(), value);
        }

        err = nvs_commit(my_handle);
        nvs_close(my_handle);
    }
}

/**
 * @brief Generic get NV string value by key and slot(0-99).
 *
 * @param [in]key to search for.
 * @param [in]slot inter slot from 0 - 99.
 * @param [in]s char * suffix.
 * @param [out]valueout std::string * to store search results.
 *
 */
void ad2_get_nv_slot_key_string(const char *key, int slot, const char *s, std::string &valueout)
{
    esp_err_t err;

    // Open NVS
    nvs_handle my_handle;
#ifdef DEBUG_NVS
    ESP_LOGI(TAG, "%s: opening key(%s)", __func__, key);
#endif
    err = nvs_open(key, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: Error (%s) opening NVS handle!", __func__, esp_err_to_name(err));
    } else {
        size_t size;
        std::string tkey = ad2_string_printf("%02i%s", slot, s == nullptr ? "" : s);
#ifdef DEBUG_NVS
        ESP_LOGI(TAG, "%s: reading sub key(%s)", __func__, tkey.c_str());
#endif
        // get size including terminator.
        err = nvs_get_str(my_handle, tkey.c_str(), NULL, &size);
        if (err == ESP_OK && size) {
            if (size > AD2_MAX_VALUE_SIZE) {
                size = AD2_MAX_VALUE_SIZE;
            }
            // set size excluding terminator
            valueout.resize(size-1);
            // load the string to valueout.
            err = nvs_get_str(my_handle, tkey.c_str(), (char*)valueout.c_str(), &size);
        }

        nvs_close(my_handle);
    }
}

/**
 * @brief Generic set NV string value by key and slot(0-99).
 * A value pointer of 0 or NULL will remove the entry if found.
 *
 * @param [in]key pointer to key to save value under
 * @param [in]slot int slot# from 0 - 99
 * @param [in]s char * suffix.
 * @param [in]value pointer to string to store for search results.
 *
 */
void ad2_set_nv_slot_key_string(const char *key, int slot, const char *s, const char *value)
{
    esp_err_t err;

    // Open NVS
    nvs_handle my_handle;
#ifdef DEBUG_NVS
    ESP_LOGI(TAG, "%s: opening key(%s)", __func__, key);
#endif
    err = nvs_open(key, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: Error (%s) opening NVS handle!", __func__, esp_err_to_name(err));
    } else {
        std::string tkey;
        tkey = ad2_string_printf("%02i%s", slot, s == nullptr ? "" : s);
#ifdef DEBUG_NVS
        ESP_LOGI(TAG, "%s: saving sub key(%s)", __func__, tkey.c_str());
#endif
        if (value == NULL) {
            err = nvs_erase_key(my_handle, tkey.c_str());
        } else {
            err = nvs_set_str(my_handle, tkey.c_str(), value);
        }
        err = nvs_commit(my_handle);
        nvs_close(my_handle);
    }
}

/**
 * @brief Generic get NV value by key.
 *
 * @param [in]key pointer to key for the value to find
 * @param [out]valueout pointer std::string value string if found
 *
 * @note string will be truncated if larger than size including
 * the null terminator.
 */
void ad2_get_nv_arg(const char *key, std::string &valueout)
{
    esp_err_t err;

    // Open NVS
    nvs_handle my_handle;
    err = nvs_open(key, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: Error (%s) opening NVS handle!", __func__, esp_err_to_name(err));
    } else {
        size_t size;
        // get size including terminator.
        err = nvs_get_str(my_handle, key, NULL, &size);
        if (err == ESP_OK && size) {
            if (size > AD2_MAX_VALUE_SIZE) {
                size = AD2_MAX_VALUE_SIZE;
            }
            // set size excluding terminator
            valueout.resize(size-1);
            // load the string to valueout.
            err = nvs_get_str(my_handle, key, (char*)valueout.c_str(), &size);
        }
        nvs_close(my_handle);
    }
}

/**
 * @brief Generic set NV value by key.
 *
 * @param [in]key pointer to key for stored value
 * @param [in]value pointer to value string to store
 *
 */
void ad2_set_nv_arg(const char *key, const char *value)
{
    esp_err_t err;

    // Open NVS
    nvs_handle my_handle;
    err = nvs_open(key, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: Error (%s) opening NVS handle!", __func__, esp_err_to_name(err));
    } else {
        err = nvs_set_str(my_handle, key, value);
        err = nvs_commit(my_handle);
        nvs_close(my_handle);
    }
}


/**
 * @brief Copy the Nth space seperated word from a string.
 *
 * @param dest pointer to std::string for output
 * @param [in]src pointer to input bytes
 * @param [in]n argument number to return if possible
 * @param [in]remaining Default false. If true will consume remaining string as the final result.
 *
 * @return 0 on success -1 on failure
 */
int ad2_copy_nth_arg(std::string &dest, char* src, int n, bool remaining)
{
    int start = 0, end = -1;
    int i = 0, word_index = 0;
    int len;

    for (i = 0; src[i] != '\0'; i++) {
        if ((src[i] == ' ') && (src[i+1]!=' ') && (src[i+1]!='\0')) { //start check
            word_index++;
            if (word_index == n) {
                start = i+1;
            }
        } else if ((src[i] != ' ') && ((src[i+1]==' ')||(src[i+1]=='\0'))) { //end check
            if (word_index == n) {
                if (remaining) {
                    // Fast Forward to \0
                    end = strlen(src);
                } else {
                    end = i;
                }
                break;
            }
        }
    }

    if (end == -1) {
        ESP_LOGD(TAG, "%s: Fail to find %dth arg", __func__, n);
        return -1;
    }

    len = end - start + 1;

    dest = std::string(&src[start], len);
    return 0;

}

/**
 * @brief Send the ARM AWAY command to the alarm panel.
 *
 * @details using the code in slot codeId and address in
 * slot vpartId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]codeId int [0 - AD2_MAX_CODE]
 * @param [in]vpartId int [0 - AD2_MAX_VPARTITION]
 *
 */
void ad2_arm_away(int codeId, int vpartId)
{

    // Get user code
    std::string code;
    ad2_get_nv_slot_key_string(CODES_CONFIG_KEY, codeId, nullptr, code);

    int address = -1;
    ad2_get_nv_slot_key_int(VPADDR_CONFIG_KEY, vpartId, nullptr, &address);

    std::string msg;

    // @brief get the vpart state
    AD2VirtualPartitionState *s = AD2Parse.getAD2PState(address, false);

    if (s) {
        if (s->panel_type == ADEMCO_PANEL) {
            msg = ad2_string_printf("K%02i%s%s", address, code.c_str(), "2");
        } else if (s->panel_type == DSC_PANEL) {
            msg = "<S5>";
        }

        ESP_LOGI(TAG,"Sending ARM AWAY command");
        ad2_send(msg);
    } else {
        ESP_LOGE(TAG, "No partition state found for address %i. Waiting for messages from the AD2?", address);
    }
}

/**
 * @brief Send the ARM STAY command to the alarm panel.
 *
 * @details using the code in slot codeId and address in
 * slot vpartId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]codeId int [0 - AD2_MAX_CODE]
 * @param [in]vpartId int [0 - AD2_MAX_VPARTITION]
 *
 */
void ad2_arm_stay(int codeId, int vpartId)
{

    // Get user code
    std::string code;
    ad2_get_nv_slot_key_string(CODES_CONFIG_KEY, codeId, nullptr, code);

    int address = -1;
    ad2_get_nv_slot_key_int(VPADDR_CONFIG_KEY, vpartId, nullptr, &address);

    std::string msg;

    // @brief get the vpart state
    AD2VirtualPartitionState *s = AD2Parse.getAD2PState(address, false);

    if (s) {
        if (s->panel_type == ADEMCO_PANEL) {
            msg = ad2_string_printf("K%02i%s%s", address, code.c_str(), "3");
        } else if (s->panel_type == DSC_PANEL) {
            msg = "<S4>";
        }
        ESP_LOGI(TAG,"Sending ARM STAY command");
        ad2_send(msg);
    } else {
        ESP_LOGE(TAG, "No partition state found for address %i. Waiting for messages from the AD2?", address);
    }
}

/**
 * @brief Send the DISARM command to the alarm panel.
 *
 * @details using the code in slot codeId and address in
 * slot vpartId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]codeId int [0 - AD2_MAX_CODE]
 * @param [in]vpartId int [0 - AD2_MAX_VPARTITION]
 *
 */
void ad2_disarm(int codeId, int vpartId)
{

    // Get user code
    std::string code;
    ad2_get_nv_slot_key_string(CODES_CONFIG_KEY, codeId, nullptr, code);

    int address = -1;
    ad2_get_nv_slot_key_int(VPADDR_CONFIG_KEY, vpartId, nullptr, &address);

    std::string msg;

    // @brief get the vpart state
    AD2VirtualPartitionState *s = AD2Parse.getAD2PState(address, false);

    if (s) {
        if (s->panel_type == ADEMCO_PANEL) {
            msg = ad2_string_printf("K%02i%s%s", address, code.c_str(), "1");
        } else if (s->panel_type == DSC_PANEL) {
            // QUIRK: For DSC don't disarm if already disarmed. Unlike Ademoc no specific command AFAIK exists to disarm just the code. If I find one I will change this.
            if (s->armed_away || s->armed_stay) {
                msg = ad2_string_printf("K%02i%s", address, code.c_str());
            } else {
                ESP_LOGI(TAG,"DSC: Already DISARMED not sending DISARM command");
            }
        }
        ESP_LOGI(TAG,"Sending DISARM command");
        ad2_send(msg);
    } else {
        ESP_LOGE(TAG, "No partition state found for address %i. Waiting for messages from the AD2?", address);
    }
}

/**
 * @brief Toggle Chime mode.
 *
 * @details using the code in slot codeId and address in
 * slot vpartId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]codeId int [0 - AD2_MAX_CODE]
 * @param [in]vpartId int [0 - AD2_MAX_VPARTITION]
 *
 */
void ad2_chime_toggle(int codeId, int vpartId)
{

    // Get user code
    std::string code;
    ad2_get_nv_slot_key_string(CODES_CONFIG_KEY, codeId, nullptr, code);

    int address = -1;
    ad2_get_nv_slot_key_int(VPADDR_CONFIG_KEY, vpartId, nullptr, &address);

    std::string msg;

    // @brief get the vpart state
    AD2VirtualPartitionState *s = AD2Parse.getAD2PState(address, false);

    if (s) {
        if (s->panel_type == ADEMCO_PANEL) {
            msg = ad2_string_printf("K%02i%s%s", address, code.c_str(), "9");
        } else if (s->panel_type == DSC_PANEL) {
            msg = "<S6>";
        }

        ESP_LOGI(TAG,"Sending CHIME toggle command");
        ad2_send(msg);
    } else {
        ESP_LOGE(TAG, "No partition state found for address %i. Waiting for messages from the AD2?", address);
    }
}

/**
 * @brief Send the FIRE PANIC command to the alarm panel.
 *
 * @details using the code in slot codeId and address in
 * slot vpartId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]codeId int [0 - AD2_MAX_CODE]
 * @param [in]vpartId int [0 - AD2_MAX_VPARTITION]
 *
 */
void ad2_fire_alarm(int codeId, int vpartId)
{

    // Get the address/partition mask for multi partition support.
    int address = -1;
    ad2_get_nv_slot_key_int(VPADDR_CONFIG_KEY, vpartId, nullptr, &address);

    std::string msg;
    msg = ad2_string_printf("K%02i<S1>", address);

    ESP_LOGI(TAG,"Sending FIRE PANIC button command");
    ad2_send(msg);
}

/**
 * @brief Send the PANIC command to the alarm panel.
 *
 * @details using the code in slot codeId and address in
 * slot vpartId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]codeId int [0 - AD2_MAX_CODE]
 * @param [in]vpartId int [0 - AD2_MAX_VPARTITION]
 *
 */
void ad2_panic_alarm(int codeId, int vpartId)
{

    // Get the address/partition mask for multi partition support.
    int address = -1;
    ad2_get_nv_slot_key_int(VPADDR_CONFIG_KEY, vpartId, nullptr, &address);

    std::string msg;
    msg = ad2_string_printf("K%02i<S2>", address);

    ESP_LOGI(TAG,"Sending PANIC button command");
    ad2_send(msg);
}


/**
 * @brief Send the AUX(medical) PANIC command to the alarm panel.
 *
 * @details using the code in slot codeId and address in
 * slot vpartId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]codeId int [0 - AD2_MAX_CODE]
 * @param [in]vpartId int [0 - AD2_MAX_VPARTITION]
 *
 */
void ad2_aux_alarm(int codeId, int vpartId)
{

    // Get the address/partition mask for multi partition support.
    int address = -1;
    ad2_get_nv_slot_key_int(VPADDR_CONFIG_KEY, vpartId, nullptr, &address);

    std::string msg;
    msg = ad2_string_printf("K%02i<S3>", address);

    ESP_LOGI(TAG,"Sending AUX PANIC button command");
    ad2_send(msg);
}


/**
 * @brief Send the EXIT now command to the alarm panel.
 *
 * @details using the code in slot codeId and address in
 * slot vpartId send the correct command to the AD2 device
 * based upon the alarm type. Slots 0 are used as defaults.
 * The message will be sent using the AlarmDecoder 'K' protocol.
 *
 * @param [in]codeId int [0 - AD2_MAX_CODE]
 * @param [in]vpartId int [0 - AD2_MAX_VPARTITION]
 *
 */
void ad2_exit_now(int vpartId)
{

    int address = -1;
    ad2_get_nv_slot_key_int(VPADDR_CONFIG_KEY, vpartId, nullptr, &address);

    std::string msg;

    // @brief get the vpart state
    AD2VirtualPartitionState *s = AD2Parse.getAD2PState(address, false);

    if (s) {
        if (s->panel_type == ADEMCO_PANEL) {
            msg = ad2_string_printf("K%02i%s", address, "*");
        } else if (s->panel_type == DSC_PANEL) {
            msg = "<S8>";
        }

        ESP_LOGI(TAG,"Sending EXIT NOW command");
        ad2_send(msg);
    } else {
        ESP_LOGE(TAG, "No partition state found for address %i. Waiting for messages from the AD2?", address);
    }
}


/**
 * @brief Send string to the AD2 devices after macro translation.
 *
 * @param [in]buf Pointer to string to send to AD2 devices.
 *
 * @note Macros <SX> for sending panel specific special keys.
 *       http://www.alarmdecoder.com/wiki/index.php/Protocol#Special_Keys
 * This makes it more simple to send complex sequences with a simple human
 * readable macro.
 */
void ad2_send(std::string &buf)
{
    if(g_ad2_client_handle>-1) {

        /* replace macros <S1>-<S8> with real values */
        for (int x = 1; x < 9; x++) {
            std::string key = ad2_string_printf("<S%01i>", x);
            std::string out;
            out.append(3, (char)x);
            ad2_replace_all( buf, key.c_str(), out.c_str());
        }

        ESP_LOGD(TAG, "sending '%s' to AD2*", buf.c_str());

        if (g_ad2_mode == 'C') {
            uart_write_bytes((uart_port_t)g_ad2_client_handle, buf.c_str(), buf.length());
        } else if (g_ad2_mode == 'S') {
            // the handle is a socket fd use send()
            send(g_ad2_client_handle, buf.c_str(), buf.length(), 0);
        } else {
            ESP_LOGE(TAG, "invalid ad2 connection mode");
        }
    } else {
        ESP_LOGE(TAG, "invalid handle in send_to_ad2");
        return;
    }
}

/**
 * @brief Format and send bytes to the host uart.
 *
 * @param [in]format const char * format string.
 * @param [in]... variable args
 */
void ad2_printf_host(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    std::string out = ad2_string_vaprintf(fmt, args);
    va_end(args);
    uart_write_bytes(UART_NUM_0, out.c_str(), out.length());
}

/**
 * @brief Format and send bytes to the host uart with sized buffer.
 *
 * @param [in]format const char * format string.
 * @param [in]size size_t buffer size limiter.
 * @param [in]... variable args
 */
void ad2_snprintf_host(const char *fmt, size_t size, ...)
{
    va_list args;
    va_start(args, size);
    std::string out = ad2_string_vasnprintf(fmt, size, args);
    va_end(args);
    uart_write_bytes(UART_NUM_0, out.c_str(), out.length());
}

/**
 * @brief send RAW string to the AD2 devices.
 *
 * @param [in]vpartId Address slot for address to use for
 * returning partition info. The AlarmDecoderParser class tracks
 * every message and parses each into a status by virtual partition.
 * Each state is stored by an address mask. To fetch the state of
 * a partition all that is needed is an address that is known to
 * be on that partition. For DSC panels the address is the partition.
 *
 */
AD2VirtualPartitionState *ad2_get_partition_state(int vpartId)
{
    AD2VirtualPartitionState * s = nullptr;
    int x = -1;
    ad2_get_nv_slot_key_int(VPADDR_CONFIG_KEY, vpartId, nullptr, &x);
    // if we found a NV record then initialize the AD2PState for the mask.
    if (x != -1) {
        s = AD2Parse.getAD2PState(x, false);
    }
    return s;
}

/**
 * @brief return the current network mode value
 *
 * @return char mode
 */
char ad2_network_mode(std::string &args)
{
    int mode = 0;
    ad2_get_nv_slot_key_int(NETMODE_CONFIG_KEY, 0, nullptr, &mode);
    switch(mode) {
    case 'W':
    case 'E':
        ad2_get_nv_slot_key_string(NETMODE_CONFIG_KEY, 1, nullptr, args);
        break;
    case 'N':
    default:
        args = "";
        mode = 'N';
        break;
    }
    return (char) mode & 0xff;
}


/**
 * @brief return the current log mode value
 *
 * @return char mode
 */
char ad2_log_mode()
{
    int mode = 0;
    ad2_get_nv_slot_key_int(LOGMODE_CONFIG_KEY, 0, nullptr, &mode);
    switch(mode) {
    case 'I':
    case 'D':
    case 'N':
    case 'V':
        break;
    default:
        mode = 'N';
        break;
    }
    return mode;
}

/**
 * @brief set the current log mode value
 *
 * @param [in]m char mode
 */
void ad2_set_log_mode(char m)
{
    char lm = ad2_log_mode();
    if (lm == 'I') {
        esp_log_level_set("*", ESP_LOG_INFO);        // set all components to INFO level
    } else if (lm == 'D')  {
        esp_log_level_set("*", ESP_LOG_DEBUG);       // set all components to DEBUG level
    } else if (lm == 'V') {
        esp_log_level_set("*", ESP_LOG_VERBOSE);     // set all components to VERBOSE level
    } else {
        esp_log_level_set("*", ESP_LOG_WARN);        // set all components to WARN level
    }
}


#ifdef __cplusplus
} // extern "C"
#endif
