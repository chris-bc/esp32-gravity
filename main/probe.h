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

static const char* PROBE_TAG = "probe@GRAVITY";

extern char **user_ssids;
extern int user_ssid_count;
extern bool *attack_status;
extern bool *hop_defaults;

/*
 * This is the (currently unofficial) 802.11 raw frame TX API,
 * defined in esp32-wifi-lib's libnet80211.a/ieee80211_output.o
 *
 * This declaration is all you need for using esp_wifi_80211_tx in your own application.
 */
esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void *buffer, int len, bool en_sys_seq);

int probe_stop();
int probe_start(probe_attack_t type, bool *stats);

/* extern functions - defined in gravity.c */
int addSsid(char *ssid);
int rmSsid(char *ssid);
int countSsid();
char **lsSsid();
char *generate_random_ssid();
int mac_bytes_to_string(uint8_t *bMac, char *strMac);
int mac_string_to_bytes(char *strMac, uint8_t *bMac);