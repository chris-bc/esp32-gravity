#include "scan.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include <stdbool.h>
#include <esp_log.h>
#include <time.h>
#include <string.h>

#define CHANNEL_TAG 0x03

int gravity_ap_count = 0;
int gravity_sta_count = 0;
ScanResultAP *gravity_aps;
ScanResultSTA *gravity_stas;
int gravity_sel_ap_count;
int gravity_sel_sta_count;
ScanResultAP **gravity_selected_aps;
ScanResultSTA **gravity_selected_stas;


enum GravityScanType {
    GRAVITY_SCAN_AP,
    GRAVITY_SCAN_STA,
    GRAVITY_SCAN_WIFI,
    GRAVITY_SCAN_BLE
};

void print_stas() {
    char strMac[18];
    char strSsid[33];
    memset(strSsid, '\0', 33);
    for (int i=0; i < gravity_sta_count; ++i) {
        mac_bytes_to_string(gravity_stas[i].mac, strMac);
        printf("STA %s", strMac);
        if (gravity_stas[i].ap != NULL) {
            char mac2[18];
            mac_bytes_to_string(gravity_stas[i].apMac, mac2);
            strcpy(strSsid, (char *)gravity_stas[i].ap->espRecord.ssid);
            printf(", Associated with %s (%s)", mac2, strSsid);
        }
        printf("\n");
    }
}

void print_aps() {
    char strMac[18];
    char strSsid[33];
    memset(strSsid, '\0', 33);
    for (int i=0; i < gravity_ap_count; ++i) {
        mac_bytes_to_string(gravity_aps[i].espRecord.bssid, strMac);
        strcpy(strSsid, (char *)gravity_aps[i].espRecord.ssid);
        printf("AP %s (%s)\t%d stations\n", strMac, strSsid, gravity_aps[i].stationCount);
    }
}

/* Update pointers in the object model when the underlying ScanResultSTA* and
   ScanResultAP* objects are modified. The following objects are updated:
   gravity_selected_aps , gravity_selected_stas , gravity_aps[i].stations ,
   gravity_stas[i].ap , gravity_stas[i].apMac
*/
esp_err_t update_links() {
    /* Rebuild the selected arrays from scratch from the underlying data model */
    int apIdx = 0;
    /* Start with the selected arrays */
    if (gravity_sel_ap_count > 0) {
        free(gravity_selected_aps);
        gravity_selected_aps = malloc(sizeof(ScanResultAP *) * gravity_sel_ap_count);
        if (gravity_selected_aps == NULL) {
            ESP_LOGE(SCAN_TAG, "Failed to allocate memory to rebuild gravity_selected_aps[]");
            return ESP_ERR_NO_MEM;
        }

        for (int i = 0; i < gravity_sel_ap_count; ++i) {
            if (gravity_aps[i].selected) {
                gravity_selected_aps[apIdx++] = &gravity_aps[i];
            }
        }
    }
    if (gravity_sel_sta_count) {
        free(gravity_selected_stas);
        gravity_selected_stas = malloc(sizeof(ScanResultSTA *) * gravity_sel_sta_count);
        if (gravity_selected_stas == NULL) {
            ESP_LOGE(SCAN_TAG, "Failed to allocate memory to rebuild gravity_selected_stas[]");
            return ESP_ERR_NO_MEM;
        }
        apIdx = 0;
        for (int i = 0; i < gravity_sel_sta_count; ++i) {
            if (gravity_stas[i].selected) {
                gravity_selected_stas[apIdx++] = &gravity_stas[i];
            }
        }
    }

    /* Next up is updating ap and apMac in gravity_stas, and stations in gravity_aps */
    int idxSTA = 0;
    int idxAPSearch = 0;
    ScanResultAP *pAP;
    ScanResultSTA *pSTA;

    /* First up clear out gravity_aps[i].stations[] */
    for (idxAPSearch = 0; idxAPSearch < gravity_ap_count; ++idxAPSearch) {
        //if (gravity_aps[idxAPSearch].stations != NULL &&
        if (gravity_aps[idxAPSearch].stationCount > 0) {
            free(gravity_aps[idxAPSearch].stations);
        }
        gravity_aps[idxAPSearch].stationCount = 0;
        gravity_aps[idxAPSearch].stations = NULL;
    }

    for ( ; idxSTA < gravity_sta_count; ++idxSTA) {
        if (gravity_stas[idxSTA].ap != NULL) {
            /* Find the new address for gravity_stas[idxSTA].apMac */
            for (idxAPSearch = 0; idxAPSearch < gravity_ap_count &&
                    memcmp(gravity_stas[idxSTA].apMac, gravity_aps[idxAPSearch].espRecord.bssid, 6);
                    ++idxAPSearch) { }
            if (idxAPSearch == gravity_ap_count) {
                char strSTA[18];
                char strAP[18];
                ESP_ERROR_CHECK(mac_bytes_to_string(gravity_stas[idxSTA].apMac, strAP));
                ESP_ERROR_CHECK(mac_bytes_to_string(gravity_stas[idxSTA].mac, strSTA));
                ESP_LOGW(SCAN_TAG, "Unable to find AP %s that STA %s claims to be associated with. Continuing",
                                    strAP, strSTA);
            } else {
                /* Grab the AP's info so we can update the STA */
                pAP = &gravity_aps[idxAPSearch];
                gravity_stas[idxSTA].ap = pAP;
                /* Add gravity_stas[idxSTA] to gravity_aps[idxAPSearch].stations */
                pSTA = &gravity_stas[idxSTA];
                ScanResultSTA **oldStations = (ScanResultSTA **)gravity_aps[idxAPSearch].stations;
                ScanResultSTA **newStations = malloc(sizeof(ScanResultSTA *) * (gravity_aps[idxAPSearch].stationCount + 1));
                if (newStations == NULL) {
                    ESP_LOGE(SCAN_TAG, "Failed to allocate memory to extend stations array of %d elements (%dB)", gravity_aps[idxAPSearch].stationCount + 1, sizeof(ScanResultSTA *) * (gravity_aps[idxAPSearch].stationCount + 1));
                    return ESP_ERR_NO_MEM;
                }
                for (int i=0; i < gravity_aps[idxAPSearch].stationCount; ++i) {
                    newStations[i] = oldStations[i];
                }
                newStations[gravity_aps[idxAPSearch].stationCount] = pSTA;
                ++gravity_aps[idxAPSearch].stationCount;
                if (gravity_aps[idxAPSearch].stations != NULL) {
                    free(gravity_aps[idxAPSearch].stations);
                }
                gravity_aps[idxAPSearch].stations = (void **)newStations;
            }
        }
    }

    return ESP_OK;
}

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
    printf(" ID | SSID                             | BSSID             | Cli | Last Seen                | Ch | WPS\n");
    printf("====|==================================|===================|=====|==========================|====|=====\n");
    char strBssid[18];
    char strTime[26];
    char strSsid[36];
    unsigned long nowTime;
    unsigned long elapsed;
    for (int i=0; i < gravity_ap_count; ++i) {
        ESP_ERROR_CHECK(mac_bytes_to_string(gravity_aps[i].espRecord.bssid, strBssid));

        /* Stringify timestamp */
        nowTime = clock();
        elapsed = (nowTime - gravity_aps[i].lastSeenClk) / CLOCKS_PER_SEC;
        if (elapsed < 60.0) {
            strcpy(strTime, "Under a minute ago");
        } else if (elapsed < 3600.0) {
            sprintf(strTime, "%d %s ago", (int)elapsed/60, (elapsed >= 120)?"minutes":"minute");
        } else {
            sprintf(strTime, "%d %s ago", (int)elapsed/3600, (elapsed >= 7200)?"hours":"hour");
        }

        /* Format SSID for output */
        if (gravity_aps[i].espRecord.ssid[0] == '\0') {
            strcpy(strSsid, "<hidden>");
        } else {
            strcpy(strSsid, (char *)gravity_aps[i].espRecord.ssid);
        }

        printf("%s%2d | %-32s | %-17s | %3d | %-24s | %2u | %s\n", (gravity_aps[i].selected)?"*":" ", gravity_aps[i].index,
                strSsid, strBssid, gravity_aps[i].stationCount, strTime,
                gravity_aps[i].espRecord.primary, (gravity_aps[i].espRecord.wps<<5 != 0)?"Yes":"No");
    }

    return ESP_OK;
}

/* Available attributes are selected, index, MAC, channel, lastSeen, assocAP */
esp_err_t gravity_list_sta() {
    char strTime[26];
    unsigned long nowTime;
    unsigned long elapsed;
    printf(" ID | MAC               | Access Point      | Ch | Last Seen               \n");
    printf("====|===================|===================|====|=========================\n");

    for (int i=0; i < gravity_sta_count; ++i) {
        /* Stringify timestamp */
        nowTime = clock();
        elapsed = (nowTime - gravity_stas[i].lastSeenClk) / CLOCKS_PER_SEC;
        if (elapsed < 60.0) {
            strcpy(strTime, "Under a minute ago");
        } else if (elapsed < 3600.0) {
            sprintf(strTime, "%d %s ago", (int)elapsed / 60, (elapsed > 120)?"minutes":"minute");
        } else {
            sprintf(strTime, "%d %s ago", (int)elapsed / 3600, (elapsed > 7200)?"hours":"hour");
        }
        char strAp[18] = "";
        if (gravity_stas[i].ap == NULL) {
            strcpy(strAp, "Unknown");
        } else {
            ESP_ERROR_CHECK(mac_bytes_to_string(gravity_stas[i].apMac, strAp));
        }
        printf("%s%2d | %-17s | %-17s | %2d | %-24s\n", (gravity_stas[i].selected)?"*":" ", gravity_stas[i].index,
                gravity_stas[i].strMac, strAp, gravity_stas[i].channel, strTime);
    }    

    return ESP_OK;
}

/* Clear the contents of gravity_aps */
esp_err_t gravity_clear_ap() {
    free(gravity_aps);
    gravity_ap_count = 0;
    free(gravity_selected_aps);
    gravity_sel_ap_count = 0;
    return update_links();
}

esp_err_t gravity_clear_sta() {
    free(gravity_stas);
    gravity_sta_count = 0;
    free(gravity_selected_stas);
    gravity_sel_sta_count = 0;
    return update_links();
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
            resultAP[resultIndex].stationCount = 0;
            resultAP[resultIndex].stations = NULL;
            resultAP[resultIndex].lastSeenClk = clock();
            resultAP[resultIndex].selected = false;
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
    return update_links();
}

esp_err_t gravity_add_ap(uint8_t newAP[6], char *newSSID, int channel) {
    /* Don't store the broadcast address */
    if (!memcmp(bBroadcast, newAP, 6)) {
        return ESP_OK;
    }
    /* First make sure the MAC doesn't exist (multiple APs can share a SSID) */
    int i;
    char strMac[18];
    mac_bytes_to_string(newAP, strMac);

    for (i=0; i < gravity_ap_count && memcmp(newAP, gravity_aps[i].espRecord.bssid, 6); ++i) {}
    if (i < gravity_ap_count) {
        /* Found the MAC. Update SSID if necessary and update lastSeen */
        if (newSSID != NULL && strcasecmp(newSSID, (char *)gravity_aps[i].espRecord.ssid)) {
            /* This is probably unnecessary ... */
            memset(gravity_aps[i].espRecord.ssid, '\0', 33);
            strcpy((char *)gravity_aps[i].espRecord.ssid, (char *)newSSID);
        }

        gravity_aps[i].lastSeen = time(NULL);
        gravity_aps[i].lastSeenClk = clock();
        if (channel > 0) {
            gravity_aps[i].espRecord.primary = channel;
        }
    } else {
        #ifdef DEBUG
            ESP_LOGI(SCAN_TAG, "Found new AP %s serving \"%s\"", strMac, (newSSID==NULL)?"":newSSID);
        #endif
        /* AP is a new device */
        ScanResultAP *newAPs = malloc(sizeof(ScanResultAP) * (gravity_ap_count + 1));
        if (newAPs == NULL) {
            ESP_LOGE(SCAN_TAG, "Insufficient memmory to cache new AP %s", strMac);
            return ESP_ERR_NO_MEM;
        }
        
        int maxIndex = 0;
        /* Copy previous records across */
        for (int j=0; j < gravity_ap_count; ++j) {
            newAPs[j].stationCount = gravity_aps[j].stationCount;
            newAPs[j].stations = gravity_aps[j].stations;
            newAPs[j].espRecord = gravity_aps[j].espRecord;
            newAPs[j].index = gravity_aps[j].index;
            newAPs[j].lastSeen = gravity_aps[j].lastSeen;
            newAPs[j].lastSeenClk = gravity_aps[j].lastSeenClk;
            newAPs[j].selected = gravity_aps[j].selected;
            if (newAPs[j].index > maxIndex) {
                maxIndex = newAPs[j].index;
            }
        }

        newAPs[gravity_ap_count].selected = false;
        newAPs[gravity_ap_count].lastSeen = time(NULL);
        newAPs[gravity_ap_count].lastSeenClk = clock();
        newAPs[gravity_ap_count].index = maxIndex + 1;
        newAPs[gravity_ap_count].espRecord.primary = channel;
        newAPs[gravity_ap_count].stationCount = 0;
        newAPs[gravity_ap_count].stations = NULL;
        if (newSSID != NULL) {
            strcpy((char *)newAPs[gravity_ap_count].espRecord.ssid, newSSID);
        } else {
            newAPs[gravity_ap_count].espRecord.ssid[0] = '\0';
        }
        memcpy(newAPs[gravity_ap_count].espRecord.bssid, newAP, 6);

        ++gravity_ap_count;

        free(gravity_aps);
        gravity_aps = newAPs;
    }

    return update_links();
}

esp_err_t gravity_add_sta(uint8_t newSTA[6], int channel) {
    /* Don't store the broadcast address */
    if (!memcmp(bBroadcast, newSTA, 6)) {
        return ESP_OK;
    }
    /* First make sure the MAC doesn't exist */
    int i;
    for (i=0; i < gravity_sta_count && memcmp(newSTA, gravity_stas[i].mac, 6); ++i) {}

    if (i < gravity_sta_count) {
        /* Found the MAC. Update lastSeen */
        gravity_stas[i].lastSeen = time(NULL);
        gravity_stas[i].lastSeenClk = clock();
        if (channel != 0) {
            gravity_stas[i].channel = channel;
        }
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
            /* newSTAs[j] = gravity_stas[j]; ID10T */
            newSTAs[j].ap = gravity_stas[j].ap;
            memcpy(newSTAs[j].apMac, gravity_stas[j].apMac, 6);
            newSTAs[j].channel = gravity_stas[j].channel;
            newSTAs[j].index = gravity_stas[j].index;
            newSTAs[j].lastSeen = gravity_stas[j].lastSeen;
            newSTAs[j].lastSeenClk = gravity_stas[j].lastSeenClk;
            newSTAs[j].selected = gravity_stas[j].selected;
            memcpy(newSTAs[j].mac, gravity_stas[j].mac, 6);
            strcpy(newSTAs[j].strMac, gravity_stas[j].strMac);
            if (newSTAs[j].index > maxIndex) {
                maxIndex = newSTAs[j].index;
            }
        }

        newSTAs[gravity_sta_count].selected = false;
        newSTAs[gravity_sta_count].lastSeen = time(NULL);
        newSTAs[gravity_sta_count].lastSeenClk = clock();
        newSTAs[gravity_sta_count].index = maxIndex + 1;
        newSTAs[gravity_sta_count].channel = channel;
        newSTAs[gravity_sta_count].ap = NULL;
        newSTAs[gravity_sta_count].apMac[0] = 0;
        memcpy(newSTAs[gravity_sta_count].mac, newSTA, 6);
        ESP_ERROR_CHECK(mac_bytes_to_string(newSTA, newSTAs[gravity_sta_count].strMac));

        ++gravity_sta_count;

        free(gravity_stas);
        gravity_stas = newSTAs;
    }

    return update_links();
}

/* Found a station association. Typically this is a data packet to/from the router.
   Record this association in: ap.stations, sta.apMac, sta.ap */
esp_err_t gravity_add_sta_ap(uint8_t *sta, uint8_t *ap) {
    /* Don't store the broadcast address */
    if (!memcmp(bBroadcast, sta, 6) || !memcmp(bBroadcast, ap, 6)) {
        return ESP_OK;
    }

    /* Find the ScanResultAP and ScanResultSTA representing the specified elements */
    int idxSta = 0;
    int idxAp = 0;
    ScanResultSTA *specSTA;
    ScanResultAP *specAP;
    for (idxSta = 0; idxSta < gravity_sta_count && memcmp(gravity_stas[idxSta].mac, sta, 6); ++idxSta) { }
    if (idxSta == gravity_sta_count) {
        char strSTA[18];
        ESP_ERROR_CHECK(mac_bytes_to_string(sta, strSTA));
        ESP_LOGE(SCAN_TAG, "Unable to find specified STA %s", strSTA);
        return ESP_ERR_INVALID_ARG;
    }
    specSTA = &gravity_stas[idxSta];

    for (idxAp = 0; idxAp < gravity_ap_count && memcmp(gravity_aps[idxAp].espRecord.bssid, ap, 6); ++idxAp) { }
    if (idxAp == gravity_ap_count) {
        char strAP[18];
        ESP_ERROR_CHECK(mac_bytes_to_string(ap, strAP));
        ESP_LOGE(SCAN_TAG, "Unable to find specified AP %s", strAP);
        return ESP_ERR_INVALID_ARG;
    }
    specAP = &gravity_aps[idxAp];
char strMacAP[18];
mac_bytes_to_string(specAP->espRecord.bssid, strMacAP);

    /* Is the STA already associated with an AP? */
    if (specSTA->ap != NULL) {
        /* Maintain specAP.stations array before anything else */
        /* Check whether the STA is already present in the AP */
        if (!memcmp(ap, specSTA->apMac, 6)) {
            /* The STA is already associated with the AP */
            return ESP_OK;
        } else {
char strOldAp[18];
mac_bytes_to_string(specSTA->apMac, strOldAp);
            /* STA has moved from one AP to another */
            /* First shrink specSTA->ap->stations */
            ScanResultSTA **oldSTA = (ScanResultSTA **)specSTA->ap->stations;
            if (specSTA->ap->stationCount <= 1) {
                return ESP_OK; // TODO : Is this right? Shouldn't I free stations?
            }
            
            ScanResultSTA **newSTA = malloc(sizeof(ScanResultSTA *) * (specSTA->ap->stationCount - 1));
            if (newSTA == NULL) {
                ESP_LOGE(SCAN_TAG, "Failed to allocate memory to shrink stations");
                return ESP_ERR_NO_MEM;
            }
            int idxOld = 0;
            int idxNew = 0;
            for (; idxOld < specSTA->ap->stationCount; ++idxOld) {
                /* Copy across everything except sta */
                if (memcmp(sta, oldSTA[idxOld]->mac, 6)) {
                    newSTA[idxNew++] = oldSTA[idxOld];
                }
            }
            free(oldSTA);
            specSTA->ap->stations = (void **)newSTA;
            --specSTA->ap->stationCount;

            /* Now extend specAP.stations */
            oldSTA = (ScanResultSTA **)specAP->stations;
            newSTA = malloc(sizeof(ScanResultSTA *) * (specAP->stationCount + 1));
            if (newSTA == NULL) {
                ESP_LOGE(SCAN_TAG, "Failed to allocate memory to extend stations");
                return ESP_ERR_NO_MEM;
            }
            for (int i=0; i < specAP->stationCount; ++i) {
                newSTA[i] = oldSTA[i];
            }
            newSTA[specAP->stationCount] = specSTA;
            ++specAP->stationCount;
            specSTA->ap = specAP;
            memcpy(specSTA->apMac, ap, 6);

            /* Finally move newSTA into place */
            free(specAP->stations);
            specAP->stations = (void **)newSTA;
        }
    } else {
        /* specSTA is not associated with an AP. Just need to extend specAP.stations */
        ScanResultSTA **oldSTA = (ScanResultSTA **)specAP->stations;
        int size = sizeof(ScanResultSTA *) * (specAP->stationCount + 1);
        ScanResultSTA **newSTA = malloc(size);
        if (newSTA == NULL) {
            ESP_LOGE(SCAN_TAG, "Unable to allocate memory to extend stations array");
            return ESP_ERR_NO_MEM;
        }
        for (int i=0; i < specAP->stationCount; ++i) {
            newSTA[i] = oldSTA[i];
        }
        newSTA[specAP->stationCount] = specSTA;
        ++specAP->stationCount;
        if (specAP->stations != NULL) {
            free(specAP->stations);
        }
        specAP->stations = (void **)newSTA;

        specSTA->ap = specAP;
        memcpy(specSTA->apMac, ap, 6);
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
            // TODO: Will crash here sometimes. Need a way to identify the frame check sequence at end of packet
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

esp_err_t parse_data(uint8_t *payload) {
    // Get AP, STA, and associated AP
    int sta_offset = 4; // Rx
    int ap_offset = 10; // Tx
    uint8_t sta[6];
    uint8_t ap[6];
    memcpy(sta, &payload[sta_offset], 6);
    memcpy(ap, &payload[ap_offset], 6);

    bool adding = false;
    uint8_t *adding_sta = NULL;
    uint8_t *adding_ap = NULL;

    /* If we know Rx or Tx is a STA we might have a new AP */
    int idxSearch = 0;
    for (idxSearch = 0; idxSearch < gravity_sta_count && memcmp(sta, gravity_stas[idxSearch].mac, 6) &&
            memcmp(ap, gravity_stas[idxSearch].mac, 6); ++idxSearch) { }
    if (idxSearch < gravity_sta_count) {
        /* We found a known station */
        if (!memcmp(sta, gravity_stas[idxSearch].mac, 6)) {
            adding = true;
            adding_sta = sta;
            adding_ap = ap;
        } else if (!memcmp(ap, gravity_stas[idxSearch].mac, 6)) {
            adding = true;
            adding_ap = sta;
            adding_sta = ap;
        }
    }
    if (adding) {
        ESP_ERROR_CHECK(gravity_add_ap(adding_ap, NULL, 0));
        ESP_ERROR_CHECK(gravity_add_sta(adding_sta, 0));
        ESP_ERROR_CHECK(gravity_add_sta_ap(adding_sta, adding_ap));
    }
    adding = false;

    /* If we know Rx or Tx is an AP we might have a new STA */
    for (idxSearch = 0; idxSearch < gravity_ap_count && memcmp(sta, gravity_aps[idxSearch].espRecord.bssid, 6) &&
            memcmp(ap, gravity_aps[idxSearch].espRecord.bssid, 6); ++idxSearch) { }
    if (idxSearch < gravity_ap_count) {
        /* We found a known AP */
        if (!memcmp(sta, gravity_aps[idxSearch].espRecord.bssid, 6)) {
            adding = true;
            adding_ap = sta;
            adding_sta = ap;
        } else if (!memcmp(ap, gravity_aps[idxSearch].espRecord.bssid, 6)) {
            adding = true;
            adding_ap = ap;
            adding_sta = sta;
        }
        ESP_ERROR_CHECK(gravity_add_ap(adding_ap, NULL, 0));
        ESP_ERROR_CHECK(gravity_add_sta(adding_sta, 0));
        ESP_ERROR_CHECK(gravity_add_sta_ap(adding_sta, adding_ap));

    }
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
    /* TODO: Scan a specified SSID
    Given SSID, check data model for a match. If so great.
    Otherwise enable hopping and monitor just beacon and probe responses
        until the SSID is seen - Get BSSID
    Try to get channel from that frame - fix channel if so, otherwise keep hopping
    Begin collecting most frames again, but discard any that don't meet the
        following criteria:
    - Source or Destination Address is AP's BSSID
    - Source or Destination Address is in the list of STAs associated with the specified AP
    */

    /* If scan_filter_ssid is set then limit the
          observed packets to the specified SSID */
          // 20:E8:82:EE:D7:D4 - Whymper2.4
    if (strlen(scan_filter_ssid) > 0) {
        /* Do we know the MAC associated with the SSID? */
        // TODO Do I need to check all bytes or can I just do the first?
        if (scan_filter_ssid_bssid[0] == 0x00 && scan_filter_ssid_bssid[1] == 0x00 &&
                scan_filter_ssid_bssid[2] == 0x00 && scan_filter_ssid_bssid[3] == 0x00 &&
                scan_filter_ssid_bssid[4] == 0x00 && scan_filter_ssid_bssid[5] == 0x00) {
            /* No MAC yet. Is there one in the current packet? */
            if (payload[0] == 0x50 || payload[0] == 0x80) {
                /* Probe response or beacon - Is it directed? Check SSID length field */
                if (payload[37] == 0x00) {
                    /* No SSID. Skip this frame - we'll get one soon */
                    return ESP_OK;
                }
                char pktSsid[33];
                for (int i=0; i < 33; ++i) {
                    pktSsid[i] = '\0';
                }
                memcpy(pktSsid, &payload[38], payload[37]);
                pktSsid[payload[37]] = '\0';

                /* Is this the SSID we're looking for? */
                if (!strcasecmp(pktSsid, scan_filter_ssid)) {
                    /* It is! Record the MAC and proceed to parse the packet */
                    memcpy(scan_filter_ssid_bssid, &payload[10], 6);
                    // TODO : I THINK that's all I need to do?
                } else {
                    return ESP_OK;
                }
            } else {
                return ESP_OK;
            }
        } else {
            /* AP's MAC is in scan_filter_ssid_bssid - see if this frame involves it */
            uint8_t destAddr[6];
            uint8_t srcAddr[6];
            memcpy(destAddr, &payload[4], 6);
            memcpy(srcAddr, &payload[10], 6);
            if (memcmp(scan_filter_ssid_bssid, destAddr, 6) && memcmp(scan_filter_ssid_bssid, srcAddr, 6)) {
                /* AP isn't a direct sender or receiver. Check whether any known stations are */
                /* First find the struct instance representing the AP */
                int apIdx;
                for (apIdx = 0; apIdx < gravity_ap_count && 
                            memcmp(gravity_aps[apIdx].espRecord.bssid, scan_filter_ssid_bssid, 6); ++apIdx) { }
                if (apIdx == gravity_ap_count) {
                    char strAP[18];
                    mac_bytes_to_string(scan_filter_ssid_bssid, strAP);
                    ESP_LOGE(SCAN_TAG, "Unable to find object representing selected AP %s", strAP);
                    return ESP_OK;
                }
                ScanResultAP *selectedAP = &gravity_aps[apIdx];
                /* Go through selectedAP->stations and see if any are the source or destination */
                ScanResultSTA **stations = (ScanResultSTA **)selectedAP->stations;
                int idxStations;
                for (idxStations = 0; idxStations < selectedAP->stationCount &&
                                memcmp(stations[idxStations]->mac, destAddr, 6) &&
                                memcmp(stations[idxStations]->mac, srcAddr, 6); ++idxStations) { }
                if (idxStations == selectedAP->stationCount) {
                    /* No AP clients have been observed */
                    return ESP_OK; // TODO? Braindead...
                }
            }
        }
    }

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
        ESP_LOGI(SCAN_TAG, "Received RTS frame");
        return parse_rts(payload);
        break;
    case 0xC4:
        ESP_LOGI(SCAN_TAG, "Received CTS frame");
        return parse_cts(payload);
        break;
    case 0x88:
    case 0x08:
        //ESP_LOGI(SCAN_TAG, "Data frame");
        return parse_data(payload);
        break;
    }

    return ESP_OK;
}