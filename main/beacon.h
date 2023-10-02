#ifndef BEACON_H
#define BEACON_H

#include <esp_err.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_wifi_default.h>
#include <esp_wifi_types.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <freertos/portmacro.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "common.h"

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
extern int BEACON_PRIVACY_OFFSET;
extern int DEFAULT_SSID_COUNT;
extern int SSID_LEN_MIN;
extern int SSID_LEN_MAX;
const char* BEACON_TAG = "beacon@GRAVITY";

extern char **user_ssids;

extern bool isDictionaryLoaded;

extern uint8_t beacon_raw[];

typedef enum {
    ATTACK_BEACON_NONE,
    ATTACK_BEACON_RICKROLL,
    ATTACK_BEACON_RANDOM,
    ATTACK_BEACON_USER,
	ATTACK_BEACON_AP,
	ATTACK_BEACON_INFINITE
} beacon_attack_t;
extern beacon_attack_t beaconAttackType;

/* SSID generation */
extern bool scrambledWords;
char *getRandomWord();
esp_err_t randomSsidWithWords(char *ssid, int len);
esp_err_t randomSsidWithChars(char *ssid, int len);
esp_err_t extendSsidWithWords(char *ssid, char *prefix, int len);
esp_err_t extendSsidWithChars(char *ssid, char *prefix, int len);

esp_err_t beacon_start(beacon_attack_t type, int authentication[], int authenticationCount, int ssidCount);
esp_err_t beacon_stop();
esp_err_t beacon_status();

/* extern functions - defined in main.c */
esp_err_t addSsid(char *ssid);
esp_err_t rmSsid(char *ssid);
int countSsid();
char **lsSsid();

#endif