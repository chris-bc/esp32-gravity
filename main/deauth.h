#ifndef DEAUTH_H
#define DEAUTH_H

#include <esp_err.h>
#include <esp_log.h>
#include <esp_interface.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"


enum DeauthMode {
    DEAUTH_MODE_OFF,
    DEAUTH_MODE_STA,
    DEAUTH_MODE_BROADCAST,
    DEAUTH_MODE_AP
};
typedef enum DeauthMode DeauthMode;

enum DeauthMAC {
    DEAUTH_MAC_FRAME,
    DEAUTH_MAC_DEVICE,
    DEAUTH_MAC_SPOOF
};
typedef enum DeauthMAC DeauthMAC;

esp_err_t deauth_standalone_packet(uint8_t *src, uint8_t *dest);
esp_err_t disassoc_standalone_packet(uint8_t *src, uint8_t *dest);
esp_err_t deauth_start(DeauthMode dMode, DeauthMAC setMAC, long millis);
esp_err_t deauth_stop();
esp_err_t deauth_setDelay(long millis);
long deauth_getDelay();

extern char *DEAUTH_TAG;

#endif