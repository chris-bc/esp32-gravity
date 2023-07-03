#include <esp_err.h>
#include <stdbool.h>

#define DEAUTH_MAC_DEFAULT true
#define DEAUTH_MILLIS_DEFAULT 0

enum DeauthMode {
    DEAUTH_MODE_OFF,
    DEAUTH_MODE_STA,
    DEAUTH_MODE_BROADCAST
};
typedef enum DeauthMode DeauthMode;

esp_err_t deauth_start(DeauthMode dMode, bool setMAC, long millis);
esp_err_t deauth_stop();
