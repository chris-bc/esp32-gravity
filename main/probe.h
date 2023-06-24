#include <esp_err.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#define DEFAULT_PROBE_COUNT 128

typedef enum {
	ATTACK_PROBE_NONE,
	ATTACK_PROBE_UNDIRECTED,
	ATTACK_PROBE_DIRECTED
} probe_attack_t;

static const char* PROBE_TAG = "PROBE@GRAVITY";

/*
 * This is the (currently unofficial) 802.11 raw frame TX API,
 * defined in esp32-wifi-lib's libnet80211.a/ieee80211_output.o
 *
 * This declaration is all you need for using esp_wifi_80211_tx in your own application.
 */
esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void *buffer, int len, bool en_sys_seq);

int probe_stop();
int probe_start(probe_attack_t type, int probeCount);
int probe_set_ssids(int count, char **new);
