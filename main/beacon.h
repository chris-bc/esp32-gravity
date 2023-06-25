#include <esp_wifi.h>
#include "esp_wifi_default.h"
#include "esp_wifi_types.h"
#include "nvs.h"
#include <esp_err.h>
#include <stdbool.h>
#include <nvs_flash.h>
#include <string.h>
#include <esp_log.h>
#include <time.h>
#include <stdlib.h>

// Allowable chars for randomly-generated SSIDs
static char ssid_chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
static char *rick_ssids[] = {
	"01 Never gonna give you up",
	"02 Never gonna let you down",
	"03 Never gonna run around",
	"04 and desert you",
	"05 Never gonna make you cry",
	"06 Never gonna say goodbye",
	"07 Never gonna tell a lie",
	"08 and hurt you"
};
#define RICK_SSID_COUNT 8

typedef enum {
    ATTACK_BEACON_NONE,
    ATTACK_BEACON_RICKROLL,
    ATTACK_BEACON_RANDOM,
    ATTACK_BEACON_USER,
	ATTACK_BEACON_INFINITE
} beacon_attack_t;

extern int DEFAULT_SSID_COUNT;
extern int SSID_LEN_MIN;
extern int SSID_LEN_MAX;
static const char* BEACON_TAG = "BEACON@GRAVITY";

extern char **attack_ssids;
extern char **user_ssids;
extern int user_ssid_count;

int beacon_start(beacon_attack_t type, int ssidCount);
int beacon_stop();

/* extern functions - defined in main.c */
int addSsid(char *ssid);
int rmSsid(char *ssid);
int countSsid();
char **lsSsid();
char *generate_random_ssid();
