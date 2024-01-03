#ifndef MANA_H
#define MANA_H

#include <stdint.h>
#include <string.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_wifi.h>
#include "common.h"

typedef struct NetworkList {
    uint8_t bMac[6];
    char strMac[MAC_STRLEN + 1];
    char **ssids;
    int ssidCount;
} NetworkList;
NetworkList *networkList = NULL;

extern const char *MANA_TAG;
extern NetworkList *networkList;
extern int networkCount;
extern PROBE_RESPONSE_AUTH_TYPE mana_auth;
extern uint8_t PRIVACY_OFF_BYTES[];
extern uint8_t PRIVACY_ON_BYTES[];

esp_err_t mana_handleProbeRequest(uint8_t *payload, char *ssid, int ssid_len);
esp_err_t mana_display_status();

#endif