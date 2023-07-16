#include "fuzz.h"
#include "common.h"
#include "esp_err.h"
#include "esp_flip_struct.h"
#include <string.h>

FuzzMode fuzzMode = FUZZ_MODE_OFF;
FuzzPacketType fuzzPacketType = FUZZ_PACKET_NONE;
static TaskHandle_t fuzzTask = NULL;

static int fuzzCountUpper = 0;
static int fuzzCountLower = 0;

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
int fuzz_overflow(FuzzPacketType ptype, uint8_t *outBytes) {
    // TODO - inc current upper, copy start of pkt to outBytes, add len, gen and add SSID, finish pkt

    memcpy(outBytes, beacon_raw, BEACON_SSID_OFFSET - 1);

    return ESP_OK;
}

/* Generates packets for the Malformed SSID attack */
/* Will generate any packet type defined in FuzzPacketType.
   Returns the size of the resulting packet, which will be 
   stored in outBytes upon conclusion. Caller must allocate memory */
int fuzz_malformed(FuzzPacketType ptype) {
    // TODO

    return ESP_OK;
}

/* Master event loop */
void fuzzCallback(void *pvParameter) {
    //
    while (1) {
        vTaskDelay(1); /* TODO: use ATTACK_MILLIS */

        if (!attack_status[ATTACK_FUZZ]) {
            continue; /* Exit this loop iteration */
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