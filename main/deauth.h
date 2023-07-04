#include <esp_err.h>
#include <stdbool.h>

#define DEAUTH_MILLIS_DEFAULT 0

enum DeauthMode {
    DEAUTH_MODE_OFF,
    DEAUTH_MODE_STA,
    DEAUTH_MODE_BROADCAST
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

extern char *DEAUTH_TAG;
