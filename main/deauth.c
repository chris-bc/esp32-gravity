#include <stdlib.h>
#include <stdio.h>
#include "deauth.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ========== DEAUTH PACKET ========== //
const uint8_t deauth_pkt[26] = {
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

static DeauthMode mode = DEAUTH_MODE_OFF;
static bool spoofMAC = DEAUTH_MAC_DEFAULT;
static long delay = DEAUTH_MILLIS_DEFAULT;
static TaskHandle_t deauthTask = NULL;

void deauthLoop(void *pvParameter) {
    //
    while (true) {
        vTaskDelay(100 / portTICK_PERIOD_MS); /* Delay 100ms */
    }
}

esp_err_t deauth_start(DeauthMode dMode, bool setMAC, long millis) {
    if (mode != DEAUTH_MODE_OFF) {
        /* Deauth is already running, stop that one */
        deauth_stop();
    }
    mode = dMode;
    spoofMAC = setMAC;
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
    }

    return ESP_OK;
}