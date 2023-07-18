#ifndef DEAUTH_H
#define DEAUTH_H

#include <esp_err.h>
#include <stdbool.h>

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

esp_err_t deauth_start(DeauthMode dMode, DeauthMAC setMAC, long millis);
esp_err_t deauth_stop();
esp_err_t deauth_setDelay(long millis);
long deauth_getDelay();

extern char *DEAUTH_TAG;

#endif