#include "stalk.h"
#include "beacon.h"
#include "common.h"
#include "freertos/portmacro.h"
#include "probe.h"

static TaskHandle_t stalkTask = NULL;
const char *STALK_TAG = "stalk@GRAVITY";

#define CLEAR() printf("\033[H\033[J")
#define GOTOXY(x,y) printf("\033[%d;%dH", (y), (x))

/* Clear the screen and redraw stalking UI with latest data */
esp_err_t drawStalk() {
    esp_err_t err = ESP_OK;
    CLEAR();
    GOTOXY(1,2);
    printf("(1,2) foo");
    GOTOXY(1,3);
    printf("(1,3)bar");

    return err;
}

/* Main event loop for stalk */
/* vTaskDelay for 2/300ms - If scan is running in a separate task do I still get results, with this loop being UI only?
*/
void stalkLoop(void *pvParameter) {
    while (true) {
        /* Delay specified milliseconds, allowing the watchdog to do some things */
        vTaskDelay(hop_millis_defaults[ATTACK_STALK] / portTICK_PERIOD_MS);
        /* Update the UI */
        drawStalk();
    }
}

/* Parse the given frame for the stalking feature */
/* Our objective in this function is to identify the most recent RSSI for each selected wireless
   device. We don't care what type of frame it is, only its source (payload[10]) and RSSI.
*/
esp_err_t stalk_frame(uint8_t *payload, wifi_pkt_rx_ctrl_t rx_ctrl) {
    esp_err_t err = ESP_OK;

    uint8_t *srcAddr = &payload[BEACON_SRCADDR_OFFSET]; /* As long as we only take 6 bytes */
    /* Is srcAddr one of the selectedAPs? */
    int index = 0;
    for ( ; index < gravity_sel_ap_count && memcmp(srcAddr, gravity_selected_aps[index]->espRecord.bssid, 6); ++index) { }
    if (index < gravity_sel_ap_count) { /* Found an AP matching current frame - update age & RSSI */
        gravity_selected_aps[index]->lastSeen = time(NULL);
        gravity_selected_aps[index]->lastSeenClk = clock();
        gravity_selected_aps[index]->espRecord.rssi = rx_ctrl.rssi;
        /* In case the channel has changed */
        gravity_selected_aps[index]->espRecord.primary = rx_ctrl.channel;
        gravity_selected_aps[index]->espRecord.second = rx_ctrl.secondary_channel;
    } else {
        /* No matching selectedAP, is there a matching selectedSTA? */
        for (index = 0; index < gravity_sel_sta_count && memcmp(srcAddr, gravity_selected_stas[index]->mac, 6); ++index) { }
        if (index < gravity_sel_sta_count) { /* Found a STA matching current frame - update age & RSSI */
            gravity_selected_stas[index]->lastSeen = time(NULL);
            gravity_selected_stas[index]->lastSeenClk = clock();
            gravity_selected_stas[index]->rssi = rx_ctrl.rssi;
            /* In case channel has changed */
            gravity_selected_stas[index]->channel = rx_ctrl.channel;
            gravity_selected_stas[index]->second = rx_ctrl.secondary_channel;
        } else {
            /* It's not a selected interface */
        }
    }
    return err;
}

/* Start the stalking process */
/* To return control to the UI this will start a new task loop as for several of
   Gravity's features. Instead of transmitting packets this task will handle the
   updating of the stalk UI
*/
esp_err_t stalk_begin() {
    esp_err_t err = ESP_OK;
    if (stalkTask != NULL) {
        #ifdef CONFIG_FLIPPER
            printf("Stalk is already running.\nTerminating event loop %p.\n", &stalkTask);
        #else
            ESP_LOGI(STALK_TAG, "Attempting to start Stalk when it is already running. Terminating event loop %p...", &stalkTask);
        #endif
        stalk_end();
    }

    xTaskCreate(&stalkLoop, "stalkLoop", 2048, NULL, 5, &stalkTask);

    return err;
}

/* Terminate the stalking process */
esp_err_t stalk_end() {
    esp_err_t err = ESP_OK;

    if (stalkTask != NULL) {
        vTaskDelete(stalkTask);
        stalkTask = NULL;
    }

    return err;
}
