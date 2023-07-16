#include "fuzz.h"
#include "beacon.h"
#include "common.h"
#include "esp_err.h"
#include "esp_flip_struct.h"
#include "esp_wifi_types.h"
#include <string.h>

FuzzMode fuzzMode = FUZZ_MODE_OFF;
FuzzPacketType fuzzPacketType = FUZZ_PACKET_NONE;
static TaskHandle_t fuzzTask = NULL;

static int fuzzCountUpper = 0;
static int fuzzCountLower = 0;

char *randomSsid(int len) {
    //

    return "";
}
/* Function may use up to 15 bytes of memory at the
   location specified by result */
esp_err_t fuzzModeAsString(char *result) {
    if (result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    switch (fuzzMode) {
        case FUZZ_MODE_NONE:
            strcpy(result, "Inactive");
            break;
        case FUZZ_MODE_SSID_OVERFLOW:
            strcpy(result, "SSID Overflow");
            break;
        case FUZZ_MODE_SSID_MALFORMED:
            strcpy(result, "Malformed SSID");
            break;
        case FUZZ_MODE_OFF:
            strcpy(result, "Shutting Down");
            break;
        default:
            #ifdef CONFIG_FLIPPER
                printf("Unknown mode %d!\n", fuzzMode);
            #else
                ESP_LOGE(FUZZ_TAG, "Unknonw mode found: %d", fuzzMode);
            #endif
            result = "";
            return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

/* Function may use up to 38 bytes of memory at the
   location specified by result */
esp_err_t fuzzPacketTypeAsString(char *result) {
    if (result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    strcpy(result, "");
    if (fuzzPacketType & FUZZ_PACKET_BEACON) {
        if (strlen(result) > 0) {
            strcat(result, ", ");
        }
        strcat(result, "Beacon");
    }
    if (fuzzPacketType & FUZZ_PACKET_PROBE_REQ) {
        if (strlen(result) > 0) {
            strcat(result, ", ");
        }
        strcat(result, "Probe Request");
    }
    if (fuzzPacketType & FUZZ_PACKET_PROBE_RESP) {
        if (strlen(result) > 0) {
            strcat(result, ", ");
        }
        strcat(result, "Probe Response");
    }
    return ESP_OK;
}

/* Generates a suitable packet for the SSID overflow attack */
/* Will generate any packet type defined in FuzzPacketType.
   Returns the size of the resulting packet, which will be 
   stored in outBytes upon conclusion. Caller must allocate memory */
int fuzz_overflow_pkt(FuzzPacketType ptype, int ssidSize, uint8_t *outBytes) {
    // TODO - inc current upper, copy start of pkt to outBytes, add len, gen and add SSID, finish pkt
    
    /* If variables unset then set it to max SSID length + 1 */
    if (fuzzCountUpper == 0) {
        fuzzCountUpper = MAX_SSID_LEN + 1;
    }
    /* Construct our packet in-place in outBytes */
    memcpy(outBytes, beacon_raw, BEACON_SSID_OFFSET - 1);
    outBytes[BEACON_SSID_OFFSET - 1] = (uint8_t)fuzzCountUpper;
    /* Generate a random SSID of the desired length and append to the packet */
    char *ssid = randomSsid(fuzzCountUpper);
    memcpy(&outBytes[BEACON_SSID_OFFSET], (uint8_t *)ssid, fuzzCountUpper);

    /* Finish the packet */
    memcpy(&outBytes[BEACON_SSID_OFFSET + fuzzCountUpper], &beacon_raw[BEACON_SSID_OFFSET], BEACON_PACKET_LEN - BEACON_SSID_OFFSET);

    return BEACON_PACKET_LEN + fuzzCountUpper;
}

/* Generates packets for the Malformed SSID attack */
/* Will generate any packet type defined in FuzzPacketType.
   Returns the size of the resulting packet, which will be 
   stored in outBytes upon conclusion. Caller must allocate memory */
int fuzz_malformed_pkt(FuzzPacketType ptype) {
    // TODO

    return ESP_OK;
}

/* Implementation of malformed mode */
void fuzz_malformed_callback(void *pvParameter) {
    // TODO
}

/* Implementation of overflow mode */
void fuzz_overflow_callback(void *pvParameter) {
    if (fuzzCountUpper == 0) {
        fuzzCountUpper = MAX_SSID_LEN + 1;
    }
    /* Generate a beacon packet with an appropriate SSID */
    uint8_t *beacon_pkt = malloc(sizeof(uint8_t) * (BEACON_PACKET_LEN + MAX_SSID_LEN));
    int beacon_len = fuzz_overflow_pkt(fuzzPacketType, fuzzCountUpper, beacon_pkt);

    /* TODO: Different options for sender */

    /* TODO: Different options for recipient */

    esp_wifi_80211_tx(WIFI_IF_AP, beacon_pkt, beacon_len, true);

    ++fuzzCountUpper;
}

/* Master event loop */
void fuzzCallback(void *pvParameter) {
    //
    while (1) {
        vTaskDelay(1); /* TODO: use ATTACK_MILLIS */

        if (!attack_status[ATTACK_FUZZ]) {
            continue; /* Exit this loop iteration */
        }

        if (fuzzMode == FUZZ_MODE_SSID_OVERFLOW) {
            fuzz_overflow_callback(pvParameter);
        } else if (fuzzMode == FUZZ_MODE_SSID_MALFORMED) {
            fuzz_malformed_callback(pvParameter);
        }
    }
}

esp_err_t fuzz_start(FuzzMode newMode, FuzzPacketType newType) {
    fuzzMode = newMode;
    fuzzPacketType = newType;

    /* If Fuzz is already running stop it first */
    if (attack_status[ATTACK_FUZZ]) {
        fuzz_stop();
    }

    xTaskCreate(&fuzzCallback, "fuzzCallback", 2048, NULL, 5, &fuzzTask);

    return ESP_OK;
}

esp_err_t fuzz_stop() {
    fuzzCountLower = 0;
    fuzzCountUpper = 0;
    if (fuzzTask != NULL) {
        vTaskDelete(fuzzTask);
    }
    return ESP_OK;
}