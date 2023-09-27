#include "stalk.h"
#include "beacon.h"
#include "bluetooth.h"
#include "common.h"
#include "freertos/portmacro.h"
#include "probe.h"
#include <stdio.h>
#include <time.h>

static TaskHandle_t stalkTask = NULL;
const char *STALK_TAG = "stalk@GRAVITY";

#define CLEAR() printf("\033[H\033[J")
#define GOTOXY(x,y) printf("\033[%d;%dH", (y), (x))
#define CURSOR_UP(n) printf("\033[%dA", (n))
#define CURSOR_DOWN(n) printf("\033[%dB", (n))
#define CURSOR_RIGHT(n) printf("\033[%dC", (n))
#define CURSOR_LEFT(n) printf("\033[%dD", (n))

/* (Hopefully) create a workable UI without cursor positioning commands */
/* MAC takes up an entire row - 20% of the screen
   Need to get creative with space-saving:
   STA1 | -63dB  |  5s
   STA2 | -84dB  | 42s
   HCI1 | -43dB  | 21s
   BLE1 | -54dB  |  7s
    AP1 | -96dB  |118s
*/
esp_err_t drawStalkFlipper() {
    printf("\n\n\n\n\n\n\n");
    for (int i = 0; i < gravity_sel_sta_count; ++i) {
        clock_t nowTime = clock();
        unsigned long elapsed = (nowTime - gravity_selected_stas[i]->lastSeen) / CLOCKS_PER_SEC;

        printf("STA%d%s| %3ddB  |%3lds\n", i, (i == 1)?"  ":" ", gravity_selected_stas[i]->rssi, elapsed);
    }

    for (int i = 0; i < gravity_sel_ap_count; ++i) {
        /* Stringify timestamp */
        clock_t nowTime = clock();
        unsigned long elapsed = (nowTime - gravity_selected_aps[i]->lastSeen) / CLOCKS_PER_SEC;

        printf("%3sAP%d%s| %3ddB  |%3lds\n", " ", i, (i == 1)?"  ":" ", gravity_selected_aps[i]->espRecord.rssi, elapsed);
    }

    #if defined(CONFIG_IDF_TARGET_ESP32)
        for (int i = 0; i < gravity_sel_bt_count; ++i) {
            clock_t nowTime = clock();
            unsigned long elapsed = (nowTime - gravity_selected_bt[i]->lastSeen) / CLOCKS_PER_SEC;

            printf("%3sBT%d%s| %3lddB  |%3lds\n", " ", i, (i == 1)?"  ":" ", gravity_selected_bt[i]->rssi, elapsed);
        }
    #endif
    return ESP_OK;
}

/* Clear the screen and redraw stalking UI with latest data */
esp_err_t drawStalk() {
    esp_err_t err = ESP_OK;

    CLEAR();
    /* Display selectedSTA */
    GOTOXY(1, 2);
    printf("Stations          |  dB  | Age");
    GOTOXY(1, 3);
    printf("------------------|------|------");
    for (int i = 0; i < gravity_sel_sta_count; ++i) {
        GOTOXY(1, i + 4);
        printf("%s", gravity_selected_stas[i]->strMac);
        GOTOXY(19, i + 4);
        printf("| %4d |", gravity_selected_stas[i]->rssi);
        GOTOXY(27, i + 4);
        /* Stringify timestamp */
        clock_t nowTime = clock();
        unsigned long elapsed = (nowTime - gravity_selected_stas[i]->lastSeen) / CLOCKS_PER_SEC;
        printf(" %2lds", elapsed);
    }
    GOTOXY(1, gravity_sel_sta_count + 5);
    printf("Access Points     |  dB  | Age");
    GOTOXY(1, gravity_sel_sta_count + 6); // TODO: Look up cursor up/down commands
    printf("------------------|------|------");
    for (int i = 0; i < gravity_sel_ap_count; ++i) {
        GOTOXY(1, gravity_sel_sta_count + i + 7);
        char bssidStr[18] = "";
        mac_bytes_to_string(gravity_selected_aps[i]->espRecord.bssid, bssidStr);
        printf("%s", bssidStr);
        GOTOXY(19, gravity_sel_sta_count + i + 7);
        printf("| %4d |", gravity_selected_aps[i]->espRecord.rssi);
        GOTOXY(27, gravity_sel_sta_count + i + 7);
        /* Stringify timestamp */
        clock_t nowTime = clock();
        unsigned long elapsed = (nowTime - gravity_selected_aps[i]->lastSeen) / CLOCKS_PER_SEC;
        printf(" %2lds", elapsed);
    }
    #if defined(CONFIG_IDF_TARGET_ESP32)
        GOTOXY(1, gravity_sel_sta_count + gravity_sel_ap_count + 8);
        printf(" Device Name               |  dB  | Age");
        GOTOXY(1, gravity_sel_ap_count + gravity_sel_sta_count + 9);
        printf("---------------------------|------|------");
        for (int i = 0; i < gravity_sel_bt_count; ++i) {
            /* Stringify timestamp */
            clock_t nowTime = clock();
            unsigned long elapsed = (nowTime - gravity_selected_bt[i]->lastSeen) / CLOCKS_PER_SEC;
            char shortName[26];
            memset(shortName, '\0', 26);
            strncpy(shortName, gravity_selected_bt[i]->bdName, 25);
            GOTOXY(1, gravity_sel_ap_count + gravity_sel_sta_count + 10 + i);
            printf(" %-25s | %4ld | %2lds", shortName, gravity_selected_bt[i]->rssi, elapsed);
        }
    #endif
    GOTOXY(1,1);
    printf("Try to get dB up to zero:\n");

    return err;
}

/* Main event loop for stalk */
/* vTaskDelay for 2/300ms - If scan is running in a separate task do I still get results, with this loop being UI only?
*/
void stalkLoop(void *pvParameter) {
    /* Unbuffer stdout so we can control refreshes */
    while (true) {
        /* Delay specified milliseconds, allowing the watchdog to do some things */
        vTaskDelay(hop_millis_defaults[ATTACK_STALK] / portTICK_PERIOD_MS);
        /* Update the UI */
        if (attack_status[ATTACK_STALK]) {
            #ifdef CONFIG_FLIPPER
                drawStalkFlipper();
            #else
                drawStalk();
            #endif
        }
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
        gravity_selected_aps[index]->lastSeen = clock();
        gravity_selected_aps[index]->espRecord.rssi = rx_ctrl.rssi;
        /* In case the channel has changed */
        gravity_selected_aps[index]->espRecord.primary = rx_ctrl.channel;
        #if defined(CONFIG_IDF_TARGET_ESP32C6)
            gravity_selected_aps[index]->espRecord.second = rx_ctrl.second;
        #else
            gravity_selected_aps[index]->espRecord.second = rx_ctrl.secondary_channel;
        #endif
    } else {
        /* No matching selectedAP, is there a matching selectedSTA? */
        for (index = 0; index < gravity_sel_sta_count && memcmp(srcAddr, gravity_selected_stas[index]->mac, 6); ++index) { }
        if (index < gravity_sel_sta_count) { /* Found a STA matching current frame - update age & RSSI */
            gravity_selected_stas[index]->lastSeen = clock();
            gravity_selected_stas[index]->rssi = rx_ctrl.rssi;
            /* In case channel has changed */
            gravity_selected_stas[index]->channel = rx_ctrl.channel;
            #if defined(CONFIG_IDF_TARGET_ESP32C6)
                gravity_selected_stas[index]->second = rx_ctrl.second;
            #else
                gravity_selected_stas[index]->second = rx_ctrl.secondary_channel;
            #endif
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

    /* Ensure Bluetooth is scanning if we have Bluetooth and selected devices */
    #if defined(CONFIG_IDF_TARGET_ESP32)
        /* If selected BT devices */
        if (gravity_selected_bt != NULL && gravity_sel_bt_count > 0) {
            /* And not currently scanning BT */
            if (!attack_status[ATTACK_SCAN_BT_DISCOVERY]) {
                /* Start scanning BT */
                attack_status[ATTACK_SCAN_BT_DISCOVERY] = true;
                gravity_bt_gap_start();
            }
        }
    #endif

    return err;
}

/* Terminate the stalking process */
esp_err_t stalk_end() {
    esp_err_t err = ESP_OK;

    if (stalkTask != NULL) {
        printf("about to kill stalk task\n");
        vTaskDelete(stalkTask);
        printf("killed stalk task\n");
        stalkTask = NULL;
    }

    CLEAR();
    GOTOXY(1,1);
    printf("Stalk disabled\n\n");

    return err;
}
