#include "fuzz.h"
#include "beacon.h"
#include "probe.h"
#include "common.h"
#include "esp_err.h"
#include "esp_flip_struct.h"
#include "esp_wifi_types.h"
#include <string.h>

FuzzMode fuzzMode = FUZZ_MODE_OFF;
FuzzPacketType fuzzPacketType = FUZZ_PACKET_NONE;
static TaskHandle_t fuzzTask = NULL;

int fuzzCounter = 0;
uint8_t malformedFrom = 16; /* Default at half the length */
bool firstCallback = true;
bool malformedPartOne = true;
const char *FUZZ_TAG = "fuzz@GRAVITY";

esp_err_t prepareDictionary() {
    // TODO

    return ESP_OK;
}

char *getRandomWord() {
    // TODO

    return "";
}

/* Generate a random SSID of the specified length.
   This function uses the included dictionary file to generate a sequence of
   words separated by spaces, to make up the length required.
   Generated SSIDs will never end with a space. If they are generated like
   that the algorithm replaces the final space with a numeral.
*/
esp_err_t randomSsid(char *ssid, int len) {
    // TODO: Farm off the work to prepareDictionary
    int currentLen = 0;
    memset(ssid, 0, (len + 1)); /* Including trailing NULL */

    while (currentLen < len) {
        char *word = getRandomWord();
        if (currentLen + strlen(word) + 1 < len) {
            if (currentLen > 0) {
                ssid[currentLen] = (uint8_t)' ';
                ++currentLen;
            }
            memcpy(&ssid[currentLen], (uint8_t *)word, strlen(word));
            currentLen += strlen(word);
        } else {
            if (currentLen > 0) {
                ssid[currentLen] = (uint8_t)' ';
                ++currentLen;
            }
            /* How much space do we have left? */
            int remaining = len - currentLen;
            memcpy(&ssid[currentLen], (uint8_t *)word, remaining);
            currentLen += remaining;
        }
    }
    ssid[currentLen] = '\0';

    return ESP_OK;
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

/* Set the base length of SSIDs using malformed mode */
/* Default is set to 16 as a default */
esp_err_t setMalformedSsidLength(int newLength) {
    if (newLength < 0) {
        ESP_LOGE(FUZZ_TAG, "Cannot specify a negative length.");
        return ESP_ERR_INVALID_ARG;
    }
    malformedFrom = newLength;
    return ESP_OK;
}

/* Generates a suitable packet for the SSID overflow attack */
/* Will generate any packet type defined in FuzzPacketType.
   Returns the size of the resulting packet, which will be 
   stored in outBytes upon conclusion. Caller must allocate memory */
int fuzz_overflow_pkt(FuzzPacketType ptype, int ssidSize, uint8_t *outBytes) {
    uint8_t *packet_raw = NULL;
    int packet_len = 0;
    int thisSsidOffset = 0;
    int thisBssidOffset = 0;
    int thisSrcAddrOffset = 0;
    int thisDestAddrOffset = 0;
    switch (ptype) {
        case FUZZ_PACKET_BEACON:
            packet_raw = beacon_raw;
            packet_len = BEACON_PACKET_LEN;
            thisSsidOffset = BEACON_SSID_OFFSET;
            thisBssidOffset = BEACON_BSSID_OFFSET;
            thisSrcAddrOffset = BEACON_SRCADDR_OFFSET;
            thisDestAddrOffset = BEACON_DESTADDR_OFFSET;
            break;
        case FUZZ_PACKET_PROBE_REQ:
            packet_raw = probe_raw;
            packet_len = PROBE_REQUEST_LEN;
            thisSsidOffset = PROBE_SSID_OFFSET;
            thisBssidOffset = PROBE_BSSID_OFFSET;
            thisSrcAddrOffset = PROBE_SRCADDR_OFFSET;
            thisDestAddrOffset = PROBE_DESTADDR_OFFSET;
            break;
        case FUZZ_PACKET_PROBE_RESP:
            packet_raw = probe_response_raw;
            packet_len = PROBE_RESPONSE_LEN;
            thisSsidOffset = PROBE_RESPONSE_SSID_OFFSET;
            thisBssidOffset = PROBE_RESPONSE_BSSID_OFFSET;
            thisSrcAddrOffset = PROBE_RESPONSE_SRC_ADDR_OFFSET;
            thisDestAddrOffset = PROBE_RESPONSE_DEST_ADDR_OFFSET;
            break;
        default:
            #ifdef CONFIG_FLIPPER
                printf("Invalid packet type: %d\n", ptype);
            #else
                ESP_LOGE(FUZZ_TAG, "Invalid packet type encountered: %d", pytype);
            #endif
            return 0;
    }

    /* Construct our packet in-place in outBytes */
    memcpy(outBytes, packet_raw, thisSsidOffset - 1);
    outBytes[thisSsidOffset - 1] = (uint8_t)fuzzCounter; /* ssid_len */
    /* Allocate space for an SSID of the required length */
    char *ssid = malloc(sizeof(char) * (fuzzCounter + 1)); /* Other callers may want to use it as a string */
    if (ssid == NULL) {
        #ifdef CONFIG_FLIPPER
            printf("Failed to allocate %db for an SSID\n", fuzzCounter);
        #else
            ESP_LOGE(FUZZ_TAG, "Failed to allocate %d bytes for an SSID", fuzzCounter);
        #endif
        return 0;
    }
    /* Generate a random SSID of the required length */
    if (randomSsid(ssid, fuzzCounter) != ESP_OK) {
        #ifdef CONFIG_FLIPPER
            printf("Failed to generate an SSID!\n");
        #else
            ESP_LOGE(FUZZ_TAG, "Failed to generate an SSID of length %d", fuzzCounter);
        #endif
        free(ssid);
        return 0;
    }
    memcpy(&outBytes[thisSsidOffset], (uint8_t *)ssid, fuzzCounter);

    /* Finish the packet */
    memcpy(&outBytes[thisSsidOffset + fuzzCounter], &packet_raw[thisSsidOffset], packet_len - thisSsidOffset);

    return packet_len + fuzzCounter;
}

/* Generates packets for the Malformed SSID attack */
/* Will generate any packet type defined in FuzzPacketType.
   Returns the size of the resulting packet, which will be 
   stored in outBytes upon conclusion. Caller must allocate memory */
int fuzz_malformed_pkt(FuzzPacketType ptype, int ssidSize, uint8_t *outBytes) {
    uint8_t *packet_raw = NULL;
    int packet_len = 0;
    int thisSsidOffset = 0;
    int thisBssidOffset = 0;
    int thisSrcAddrOffset = 0;
    int thisDestAddrOffset = 0;

    switch (ptype) {
        case FUZZ_PACKET_BEACON:
            packet_raw = beacon_raw;
            packet_len = BEACON_PACKET_LEN;
            thisSsidOffset = BEACON_SSID_OFFSET;
            thisBssidOffset = BEACON_BSSID_OFFSET;
            thisSrcAddrOffset = BEACON_SRCADDR_OFFSET;
            thisDestAddrOffset = BEACON_DESTADDR_OFFSET;
            break;
        case FUZZ_PACKET_PROBE_REQ:
            packet_raw = probe_raw;
            packet_len = PROBE_REQUEST_LEN;
            thisSsidOffset = PROBE_SSID_OFFSET;
            thisBssidOffset = PROBE_BSSID_OFFSET;
            thisSrcAddrOffset = PROBE_SRCADDR_OFFSET;
            thisDestAddrOffset = PROBE_DESTADDR_OFFSET;
            break;
        case FUZZ_PACKET_PROBE_RESP:
            packet_raw = probe_response_raw;
            packet_len = PROBE_RESPONSE_LEN;
            thisSsidOffset = PROBE_RESPONSE_SSID_OFFSET;
            thisBssidOffset = PROBE_RESPONSE_BSSID_OFFSET;
            thisSrcAddrOffset = PROBE_RESPONSE_SRC_ADDR_OFFSET;
            thisDestAddrOffset = PROBE_RESPONSE_DEST_ADDR_OFFSET;
            break;
        default:
            #ifdef CONFIG_FLIPPER
                printf("Invalid Packet Type \"%d\"\n", ptype);
            #else
                ESP_LOGE(FUZZ_TAG, "Encountered an invalid Packet Type, '%d'", ptype);
            #endif
            return 0;
    }

    /* Start by copying packet_raw up to SSID_len */
    memcpy(outBytes, packet_raw, thisSsidOffset - 1);
    /* What's the length we're reporting as ssid_len ? */
    outBytes[thisSsidOffset - 1] = malformedFrom;

    /* Allocate space for an SSID with our actual length */
    char *ssid = malloc(sizeof(char) * (ssidSize + 1)); /* Other callers may want to use it as a string */
    if (ssid == NULL) {
        #ifdef CONFIG_FLIPPER
            printf("Failed to allocate %db for an SSID\n", (ssidSize + 1));
        #else
            ESP_LOGE(FUZZ_TAG, "Failed to allocate %d bytes for an SSID", (ssidSize + 1));
        #endif
        return 0;
    }
    /* Generate a random SSID of the required length */
    if (randomSsid(ssid, ssidSize) != ESP_OK) {
        #ifdef CONFIG_FLIPPER
            printf("Failed to generate an SSID!\n");
        #else
            ESP_LOGE(FUZZ_TAG, "Failed to generate an SSID of length %d", fuzzCounter);
        #endif
        free(ssid);
        return 0;
    }
    /* Append SSID to our packet */
    memcpy(&outBytes[thisSsidOffset], ssid, ssidSize);
    /* And the end */
    memcpy(&outBytes[thisSsidOffset + ssidSize], &packet_raw[thisSsidOffset], packet_len - thisSsidOffset);

    return (packet_len + ssidSize);
}

/* Implementation of malformed mode */
/* This process will alternate between sending packets longer than expected
   and shorter than expected. For example, with a starting length of 16,
   Gravity will send packets with lengths 17, 15, 18, 14, etc.
*/
void fuzz_malformed_callback(void *pvParameter) {
    if (firstCallback) {
        firstCallback = false;
        fuzzCounter = 0;
        malformedPartOne = true;
    }
    /* Allocate space for the packet */
    int biggestPktLen = MAX(BEACON_PACKET_LEN, MAX(PROBE_REQUEST_LEN, PROBE_RESPONSE_LEN)) + MAX(MAX_SSID_LEN, (fuzzCounter + malformedFrom));
    uint8_t *pkt = malloc(sizeof(uint8_t) * biggestPktLen);
    if (pkt == NULL) {
        #ifdef CONFIG_FLIPPER
            printf("No memory for universal packet\n");
        #else
            ESP_LOGE(FUZZ_TAG, "Unable to reserve %d bytes for a universal packet", biggestPktLen);
        #endif
        return;
    }
    /* SSID length will be malformedFrom +/- fuzzCounter. If (malformedPartOne) we add fuzzCounter.
       If !malformedFrom we subtract. */
    int ssidLen = malformedFrom;
    if (malformedPartOne) {
        ssidLen += fuzzCounter;
    } else {
        ssidLen -= fuzzCounter;
        if (ssidLen < 0) {
            ssidLen = 0;
        }
    }
    /* Generate a packet of the required type and SSID */
    int pkt_len = fuzz_malformed_pkt(fuzzPacketType, ssidLen, pkt);

    /* TODO: Set BSSID, SRCADDR, DESTADDR */

    esp_wifi_80211_tx(WIFI_IF_AP, pkt, pkt_len, true);

    /* Prepare for the next cycle */
    if (malformedPartOne) {
        malformedPartOne = false;
    } else {
        malformedPartOne = true;
        ++fuzzCounter;
    }

    free(pkt);
}

/* Implementation of overflow mode */
void fuzz_overflow_callback(void *pvParameter) {
    if (firstCallback) {
        fuzzCounter = MAX_SSID_LEN + 1;
        firstCallback = false;
    }
    /* How much space do we need to allocate for the packet? */
    int biggestPktLen = MAX(BEACON_PACKET_LEN, MAX(PROBE_REQUEST_LEN, PROBE_RESPONSE_LEN)) + MAX(MAX_SSID_LEN, fuzzCounter);
    uint8_t *pkt = malloc(sizeof(uint8_t) * biggestPktLen);
    if (pkt == NULL) {
        #ifdef CONFIG_FLIPPER
            printf("No memory for universal packet\n");
        #else
            ESP_LOGE(FUZZ_TAG, "Unable to reserve %d bytes for a universal packet template", biggestPktLen);
        #endif
        return;
    }
    /* Generate a packet of the specified type with an appropriate SSID */
    int pkt_len = fuzz_overflow_pkt(fuzzPacketType, fuzzCounter, pkt);

    /* TODO: Different options for sender */

    /* TODO: Different options for recipient */

    esp_wifi_80211_tx(WIFI_IF_AP, pkt, pkt_len, true);

    ++fuzzCounter;
    free(pkt);
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
    fuzzCounter = 0;
    firstCallback = true;
    malformedPartOne = true;

    /* If Fuzz is already running stop it first */
    if (attack_status[ATTACK_FUZZ]) {
        fuzz_stop();
    }

    xTaskCreate(&fuzzCallback, "fuzzCallback", 2048, NULL, 5, &fuzzTask);

    return ESP_OK;
}

esp_err_t fuzz_stop() {
    fuzzCounter = 0;
    if (fuzzTask != NULL) {
        vTaskDelete(fuzzTask);
    }
    return ESP_OK;
}