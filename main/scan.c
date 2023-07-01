#include "scan.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include <stdbool.h>
#include <esp_log.h>
#include <time.h>
#include <string.h>

#define CHANNEL_TAG 0x03

static int gravity_ap_count = 0;
static int gravity_sel_ap_count = 0;
static int gravity_sta_count = 0;
static int gravity_sel_sta_count = 0;
static ScanResultAP *gravity_aps;
static ScanResultAP **gravity_selected_aps;
static ScanResultSTA *gravity_stas;
static ScanResultSTA **gravity_selected_stas;
enum ScanType activeScan;

enum GravityScanType {
    GRAVITY_SCAN_AP,
    GRAVITY_SCAN_STA,
    GRAVITY_SCAN_WIFI,
    GRAVITY_SCAN_BLE
};

bool gravity_ap_isSelected(int index) {
    int i;
    for (i=0; i < gravity_sel_ap_count && gravity_selected_aps[i]->index != index; ++i) {}
    return i < gravity_sel_ap_count;
}

bool gravity_sta_isSelected(int index) {
    int i;
    for (i=0; i < gravity_sel_sta_count && gravity_selected_stas[i]->index != index; ++i) {}
    return i < gravity_sel_sta_count;
}

/* Select / De-Select the AP with the specified index, managing both
   gravity_selected_aps and ScanResultAP.selected*/
esp_err_t gravity_select_ap(int selIndex) {
    /* Find the right GravityScanAP */
    int i;
    for (i=0; i < gravity_ap_count &&  gravity_aps[i].index != selIndex; ++i) { }
    if (i == gravity_ap_count) {
        ESP_LOGE(SCAN_TAG, "Specified index (%d) does not exist.", selIndex);
        return ESP_ERR_INVALID_ARG;
    }

    /* Invert selection */
    gravity_aps[i].selected = !gravity_aps[i].selected;
    ScanResultAP **newSel;
    /* Are we adding to, or removing from, gravity_selected_aps? */
    if (gravity_aps[i].selected) {
        /* We're adding */
        newSel = malloc(sizeof(ScanResultAP *) * ++gravity_sel_ap_count); /* Increment then return */
        if (newSel == NULL) {
            ESP_LOGE(SCAN_TAG, "Failed to allocate memory for new selected APs array[%d]", gravity_sel_ap_count);
            return ESP_ERR_NO_MEM;
        }
        /* Copy across existing selected AP pointers */
        for (int j=0; j < (gravity_sel_ap_count - 1); ++j) {
            newSel[j] = gravity_selected_aps[j];
        }
        newSel[gravity_sel_ap_count - 1] = &gravity_aps[i];

        free(gravity_selected_aps);
        gravity_selected_aps = newSel;
    } else {
        /* Removing */
        newSel = malloc(sizeof(ScanResultAP *) * --gravity_sel_ap_count); /* Decrement then return */
        if (newSel == NULL) {
            ESP_LOGE(SCAN_TAG, "Failed to allocate memory for new selected APs array[%d]", gravity_sel_ap_count);
            return ESP_ERR_NO_MEM;
        }
        /* Copy our subset */
        int targetIndex = 0;
        int sourceIndex = 0;
        for (sourceIndex = 0; sourceIndex < gravity_sel_ap_count; ++sourceIndex) {
            if (gravity_selected_aps[sourceIndex]->selected) {
                newSel[targetIndex] = gravity_selected_aps[sourceIndex];
                ++targetIndex;
            }
        }

        free(gravity_selected_aps);
        gravity_selected_aps = newSel;
    }
    return ESP_OK;
}

/* Select / De-Select the STA with the specified index, managing both
   gravity_selected_stas and ScanResultSTA.selected*/
esp_err_t gravity_select_sta(int selIndex) {
    /* Find the right GravityScanSTA */
    int i;
    for (i=0; i < gravity_sta_count &&  gravity_stas[i].index != selIndex; ++i) { }
    if (i == gravity_sta_count) {
        ESP_LOGE(SCAN_TAG, "Specified index (%d) does not exist.", selIndex);
        return ESP_ERR_INVALID_ARG;
    }

    /* Invert selection */
    gravity_stas[i].selected = !gravity_stas[i].selected;
    ScanResultSTA **newSel;
    /* Are we adding to, or removing from, gravity_selected_stas? */
    if (gravity_stas[i].selected) {
        /* We're adding */
        newSel = malloc(sizeof(ScanResultSTA *) * ++gravity_sel_sta_count); /* Increment then return */
        if (newSel == NULL) {
            ESP_LOGE(SCAN_TAG, "Failed to allocate memory for new selected STAs array[%d]", gravity_sel_sta_count);
            return ESP_ERR_NO_MEM;
        }
        /* Copy across existing selected STA pointers */
        for (int j=0; j < (gravity_sel_sta_count - 1); ++j) {
            newSel[j] = gravity_selected_stas[j];
        }
        newSel[gravity_sel_sta_count - 1] = &gravity_stas[i];

        free(gravity_selected_stas);
        gravity_selected_stas = newSel;
    } else {
        /* Removing */
        newSel = malloc(sizeof(ScanResultSTA *) * --gravity_sel_sta_count); /* Decrement then return */
        if (newSel == NULL) {
            ESP_LOGE(SCAN_TAG, "Failed to allocate memory for new selected STAs array[%d]", gravity_sel_sta_count);
            return ESP_ERR_NO_MEM;
        }
        /* Copy our subset */
        int targetIndex = 0;
        int sourceIndex = 0;
        for (sourceIndex = 0; sourceIndex < gravity_sel_sta_count; ++sourceIndex) {
            if (gravity_selected_stas[sourceIndex]->selected) {
                newSel[targetIndex] = gravity_selected_stas[sourceIndex];
                ++targetIndex;
            }
        }

        free(gravity_selected_stas);
        gravity_selected_stas = newSel;
    }
    return ESP_OK;
}

/* Display found APs
   Attributes available for display are:
   authmode, bssid, index, lastSeen, primary, rssi, second, selected, ssid, wps
   Will display: selected (*), index, ssid, bssid, lastseen, primary, wps
   YAGNI: Make display configurable - if not through console then menuconfig! :)
*/
esp_err_t gravity_list_ap() {
    // Attributes: lastSeen, index, selected, espRecord.authmode, espRecord.bssid, espRecord.primary,
    //             espRecord.rssi, espRecord.second, espRecord.ssid, espRecord.wps
    printf(" ID | SSID                             | BSSID             | Last Seen              | Ch | WPS\n");
    printf("====|==================================|===================|========================|====|=====\n");
    char strBssid[18];
    for (int i=0; i < gravity_ap_count; ++i) {
        ESP_ERROR_CHECK(mac_bytes_to_string(gravity_aps[i].espRecord.bssid, strBssid));
        // TODO: Stringify timestamp
        printf("%s%2d | %-32s | %-17s | %22lu | %2u | %s\n", (gravity_aps[i].selected)?"*":" ", gravity_aps[i].index,
                gravity_aps[i].espRecord.ssid, strBssid, gravity_aps[i].lastSeenClk,
                gravity_aps[i].espRecord.primary, (gravity_aps[i].espRecord.wps<<5 != 0)?"Yes":"No");
    }

    return ESP_OK;
}

/* Available attributes are selected, index, MAC, channel, lastSeen, assocAP */
esp_err_t gravity_list_sta() {
    printf(" ID | MAC               | Ch | Last Seen             | Associated BSSID  \n");
    printf("====|===================|====|=======================|===================\n");

    for (int i=0; i < gravity_sta_count; ++i) {
        // TODO: Stringify timestamp
        printf("%s%2d | %-17s | %2d | %22lu | Not Implemented\n", (gravity_stas[i].selected)?"*":" ",
                gravity_stas[i].index, gravity_stas[i].strMac, gravity_stas[i].channel, gravity_stas[i].lastSeenClk);
    }    

    return ESP_OK;
}

/* Clear the contents of gravity_aps */
esp_err_t gravity_clear_ap() {
    free(gravity_aps);
    gravity_ap_count = 0;
    free(gravity_selected_aps);
    gravity_sel_ap_count = 0;
    return ESP_OK;
}

esp_err_t gravity_clear_sta() {
    free(gravity_stas);
    gravity_sta_count = 0;
    free(gravity_selected_stas);
    gravity_sel_sta_count = 0;
    return ESP_OK;
}

/* Merge the provided results into gravity_aps
   When a duplicate SSID is found the lastSeen attribute is used to select the most recent result */
esp_err_t gravity_merge_results_ap(uint16_t newCount, ScanResultAP *newAPs) {
    /* Determine what the new total will be */
    int resultCount = gravity_ap_count;
printf("begin merge. Orig count %d, new count %d\n",gravity_ap_count, newCount);
    for (int i=0; i < newCount; ++i) {
        /* is newAPs[i] in gravity_aps[]? */
        bool foundAP = false;
        for (int j=0; j < gravity_ap_count && !foundAP; ++j) {
            if (!strcasecmp((char *)newAPs[i].espRecord.ssid, (char *)gravity_aps[j].espRecord.ssid)) {
                foundAP = true;
                printf("Found AP %s\n",(char *)gravity_aps[j].espRecord.ssid);
            }
        }
        if (!foundAP) {
            ++resultCount;
            printf("Incrementing resultCount: %d\n",resultCount);
        }
    }

    /* Allocate resultCount elements for the future AP array */
    ScanResultAP *resultAP = malloc(sizeof(ScanResultAP) * resultCount);
    int resultIndex;
    if (resultAP == NULL) {
        ESP_LOGE(SCAN_TAG, "Unable to allocate memory to hold %d ScanResultAP objects", resultCount);
        return ESP_ERR_NO_MEM;
    }

    /* Start with the current set */
    for (int i=0; i < gravity_ap_count; ++i) {
        resultAP[i].espRecord = gravity_aps[i].espRecord;
        resultAP[i].lastSeen = gravity_aps[i].lastSeen;
        printf("copying orig element %d (ID %d) to new array\n", i, resultAP[i].index);
    }
    resultIndex = gravity_ap_count; /* resultAP index for the next element */
    /* Loop through the new set looking for new SSIDs or old SSIDs with new timestamps */
    for (int i=0; i < newCount; ++i) {
        /* Is newAPs[i] in resultAP? */
        /* Because gravity_aps were put at the front of resultAP we're instead going to loop through gravity_aps[]
           as well as use its index */
printf("checking new AP \"%s\"\n", (char *)newAPs[i].espRecord.ssid);
        int j;
        for (j=0; j < gravity_ap_count && strcasecmp((char *)newAPs[i].espRecord.ssid, (char *)gravity_aps[j].espRecord.ssid); ++j) { }
        if (j < gravity_ap_count) { /* Found it - Only add it if it's newer */
            printf("Found it in original array\n");
            if (newAPs[i].lastSeen >= gravity_aps[j].lastSeen) {
                resultAP[j].lastSeen = newAPs[i].lastSeen;
                resultAP[j].espRecord = newAPs[i].espRecord;
            }
        } else {
            printf("Not found in orig array, adding it to element %d\n", resultIndex);
            /* newAPs[i] isn't in gravity_aps[] - Add it */
            resultAP[resultIndex].espRecord = newAPs[resultIndex].espRecord;
            resultAP[resultIndex].lastSeen = newAPs[resultIndex].lastSeen;
            ++resultIndex;
        }
    }

    /* Free gravity_aps and put resultAP in its place */
    free(gravity_aps);
    gravity_aps = resultAP;

    /* Re-index APs */
    for (int i=0; i < gravity_ap_count; ++i) {
        gravity_aps[i].index = i + 1;
    }

    return ESP_OK;
}

esp_err_t gravity_add_ap(uint8_t newAP[6], char *newSSID, int channel) {
    /* First make sure the MAC doesn't exist (multiple APs can share a SSID) */
    int i;
    char strMac[18];
    mac_bytes_to_string(newAP, strMac);

    for (i=0; i < gravity_ap_count && memcmp(newAP, gravity_aps[i].espRecord.bssid, 6); ++i) {}
    if (i < gravity_ap_count) {
        /* Found the MAC. Update SSID if necessary and update lastSeen */
        if (strcasecmp(newSSID, (char *)gravity_aps[i].espRecord.ssid)) {
            /* This is probably unnecessary ... */
            memset(gravity_aps[i].espRecord.ssid, '\0', 33);
            strcpy((char *)gravity_aps[i].espRecord.ssid, (char *)newSSID);
        }
        gravity_aps[i].lastSeen = time(NULL);
        gravity_aps[i].lastSeenClk = clock();
        gravity_aps[i].espRecord.primary = channel;
    } else {
        #ifdef DEBUG
            ESP_LOGI(SCAN_TAG, "Found new AP %s serving \"%s\"", strMac, newSSID);
        #endif

        /* AP is a new device */
        char strNewAP[18];
        ESP_ERROR_CHECK(mac_bytes_to_string(newAP, strNewAP));
        ScanResultAP *newAPs = malloc(sizeof(ScanResultAP) * (gravity_ap_count + 1));
        if (newAPs == NULL) {
            ESP_LOGE(SCAN_TAG, "Insufficient memmory to cache new AP %s", strNewAP);
            return ESP_ERR_NO_MEM;
        }
        
        int maxIndex = 0;
        /* Copy previous records across */
        for (int j=0; j < gravity_ap_count; ++j) {
            newAPs[j] = gravity_aps[j];
            if (newAPs[j].index > maxIndex) {
                maxIndex = newAPs[j].index;
            }
        }

        newAPs[gravity_ap_count].selected = false;
        newAPs[gravity_ap_count].lastSeen = time(NULL);
        newAPs[gravity_ap_count].lastSeenClk = clock();
        newAPs[gravity_ap_count].index = maxIndex + 1;
        newAPs[gravity_ap_count].espRecord.primary = channel;
        strcpy((char *)newAPs[gravity_ap_count].espRecord.ssid, newSSID);
        memcpy(newAPs[gravity_ap_count].espRecord.bssid, newAP, 6);

        ++gravity_ap_count;

        free(gravity_aps);
        gravity_aps = newAPs;
    }

    return ESP_OK;
}

esp_err_t gravity_add_sta(uint8_t newSTA[6], int channel) {
    /* First make sure the MAC doesn't exist */
    int i;
    for (i=0; i < gravity_sta_count && memcmp(newSTA, gravity_stas[i].mac, 6); ++i) {}

    if (i < gravity_sta_count) {
        /* Found the MAC. Update lastSeen */
        gravity_stas[i].lastSeen = time(NULL);
        gravity_stas[i].lastSeenClk = clock();
        gravity_stas[i].channel = channel;
    } else {
        /* STA is a new device */
        char strNewSTA[18];
        ESP_ERROR_CHECK(mac_bytes_to_string(newSTA, strNewSTA));

        #ifdef DEBUG
            ESP_LOGI(SCAN_TAG, "Found new STA %s", strNewSTA);
        #endif

        ScanResultSTA *newSTAs = malloc(sizeof(ScanResultSTA) * (gravity_sta_count + 1));
        if (newSTAs == NULL) {
            ESP_LOGE(SCAN_TAG, "Insufficient memmory to cache new STA %s", strNewSTA);
            return ESP_ERR_NO_MEM;
        }
        
        int maxIndex = 0;
        /* Copy previous records across */
        for (int j=0; j < gravity_sta_count; ++j) {
            newSTAs[j] = gravity_stas[j];
            if (newSTAs[j].index > maxIndex) {
                maxIndex = newSTAs[j].index;
            }
        }

        newSTAs[gravity_sta_count].selected = false;
        newSTAs[gravity_sta_count].lastSeen = time(NULL);
        newSTAs[gravity_sta_count].lastSeenClk = clock();
        newSTAs[gravity_sta_count].index = maxIndex + 1;
        newSTAs[gravity_sta_count].channel = channel;
        memcpy(newSTAs[gravity_sta_count].mac, newSTA, 6);
        ESP_ERROR_CHECK(mac_bytes_to_string(newSTA, newSTAs[gravity_sta_count].strMac));

        ++gravity_sta_count;

        free(gravity_stas);
        gravity_stas = newSTAs;
    }

    return ESP_OK;
}

/* Parse the channel parameter out of 802.11 tagged parameters */
int parseChannel(uint8_t *payload) {
    int startIndex = 0;
    switch (payload[0]) {
    case 0x40:
        // Probe request
        startIndex = 24;
        break;
    case 0x50:
        // Probe response
        startIndex = 36;
        break;
    case 0x80:
        // Beacon
        startIndex = 36;
        break;
    }
    unsigned int ch=0;
    int found = false;
    int thisIndex = startIndex;
    while (!found) {
        if (payload[thisIndex] == 0x03) {
            ch = payload[thisIndex + 2];
            found = true;
        } else {
            thisIndex += 2 + payload[thisIndex + 1];
        }
    }

    return ch;
}

esp_err_t parse_beacon(uint8_t *payload) {
    int ap_offset = 10;
    int ssid_offset = 38;
    uint8_t ap[6];
    char *ssid;
    int ssid_len;

    ssid_len = payload[ssid_offset - 1];
    memcpy(ap, &payload[ap_offset], 6);
    ssid = malloc(sizeof(char) * (ssid_len + 1));
    if (ssid == NULL) {
        ESP_LOGE(SCAN_TAG, "Unable to allocate memory for SSID");
        return ESP_ERR_NO_MEM;
    }
    memcpy(ssid, &payload[ssid_offset], ssid_len);
    ssid[ssid_len] = '\0';

    char strAp[18];
    mac_bytes_to_string(ap, strAp);

    int channel = parseChannel(payload);

    gravity_add_ap(ap, ssid, channel);

    free(ssid);
    return ESP_OK;
}

esp_err_t parse_probe_request(uint8_t *payload) {
    int sta_offset = 10;
    uint8_t sta[6];

    memcpy(sta, &payload[sta_offset], 6);
    int channel = parseChannel(payload);
    gravity_add_sta(sta, channel);

    return ESP_OK;
}

esp_err_t parse_probe_response(uint8_t *payload) {
    int sta_offset = 4;
    int ap_offset = 10;
    int ssid_offset = 38;
    uint8_t ap[6];
    uint8_t sta[6];
    char *ssid;
    int ssid_len;

    memcpy(ap, &payload[ap_offset], 6);
    memcpy(sta, &payload[sta_offset], 6);
    ssid_len = payload[ssid_offset - 1];
    ssid = malloc(sizeof(char) * (ssid_len + 1));
    if (ssid == NULL) {
        ESP_LOGE(SCAN_TAG, "Failed to allocate memory for SSID");
        return ESP_ERR_NO_MEM;
    }
    memcpy(ssid, &payload[ssid_offset], ssid_len);
    ssid[ssid_len] = '\0';
    int channel = parseChannel(payload);

    gravity_add_sta(sta, channel);
    gravity_add_ap(ap, ssid, channel);

    free(ssid);
    return ESP_OK;
}

esp_err_t parse_rts(uint8_t *payload) {
    /* RTS is sent from STA to AP */
    int sta_offset = 0;
    int ap_offset = 0;

    return ESP_OK;
}

esp_err_t parse_cts(uint8_t *payload) {
    /* CTS is sent from AP to STA */
    int ap_offset = 0;
    // Does it contain STA?

    return ESP_OK;
}

/* Parse any scanning information out of the current frame
   The following frames are used to identify wireless devices:
   AP: Beacon, Probe Response, auth response, in future maybe assoc response
   STA: Probe request, auth request, maybe assoc request?
   A STA's AP: Control frame RTS - from STA to its AP - type/subtype 0x001b frame control 0xb400
        CTS has receiver address, may not even have source address

*/
esp_err_t scan_wifi_parse_frame(uint8_t *payload) {
    //
    switch (payload[0]) {
    case 0x40:
        return parse_probe_request(payload);
        break;
    case 0x50:
        return parse_probe_response(payload);
        break;
    case 0x80:
        return parse_beacon(payload);
        break;
    case 0xB4:
        return parse_rts(payload);
        ESP_LOGI(SCAN_TAG, "Received RTS frame");
        break;
    case 0xC4:
        return parse_cts(payload);
        ESP_LOGI(SCAN_TAG, "Received CTS frame");
        break;
    }

    return ESP_OK;
}
