#ifndef BEACON_H
#define BEACON_H

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
extern int RICK_SSID_COUNT;
extern int BEACON_SSID_OFFSET;
extern int BEACON_PACKET_LEN;
extern int BEACON_SRCADDR_OFFSET;
extern int BEACON_DESTADDR_OFFSET;
extern int BEACON_BSSID_OFFSET;
extern int BEACON_SEQNUM_OFFSET;

typedef enum {
    ATTACK_BEACON_NONE,
    ATTACK_BEACON_RICKROLL,
    ATTACK_BEACON_RANDOM,
    ATTACK_BEACON_USER,
	ATTACK_BEACON_AP,
	ATTACK_BEACON_INFINITE
} beacon_attack_t;

extern int DEFAULT_SSID_COUNT;
extern int SSID_LEN_MIN;
extern int SSID_LEN_MAX;
const char* BEACON_TAG = "beacon@GRAVITY";

extern char **user_ssids;
extern int user_ssid_count;

extern uint8_t beacon_raw[];

/* SSID generation */
extern bool scrambledWords;
char *getRandomWord();
esp_err_t randomSsidWithWords(char *ssid, int len);
esp_err_t randomSsidWithChars(char *ssid, int len);

int beacon_start(beacon_attack_t type, int ssidCount);
int beacon_stop();

/* extern functions - defined in main.c */
int addSsid(char *ssid);
int rmSsid(char *ssid);
int countSsid();
char **lsSsid();

#endif