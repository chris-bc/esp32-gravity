#ifndef GRAVITY_COMMON_H
#define GRAVITY_COMMON_H

#include <stdbool.h>
#include <string.h>

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_interface.h>
#include <esp_wifi_types.h>

/* Adding mana.h causes it to be unabe to use PROBE_RESPONSE_AUTH_TYPE */
/* Adding scan.h causes it to be unable to use ScanResultAP and ScanResultSTA */
#include "usage_const.h"
#include "dos.h"
#include "beacon.h"
#include "bluetooth.h"
#include "deauth.h"
#include "fuzz.h"
#include "gravity.h"
#include "hop.h"
#include "probe.h"
#include "sniff.h"

struct ScanResultAP {
    wifi_ap_record_t espRecord;
    time_t lastSeen;
    clock_t lastSeenClk;
    int index;
    bool selected;
    void **stations; /* Argh. I'm pretty sure there's no way I can have ScanResultAP */ 
    int stationCount;                        /* contain ScanResultSTA and vice versa */
};
typedef struct ScanResultAP ScanResultAP;

struct ScanResultSTA {
    long long lastSeen;
    clock_t lastSeenClk;
    int index;
    bool selected;
    uint8_t mac[6];
    char strMac[18];
    uint8_t apMac[6];
    ScanResultAP *ap;
    int channel;
};
typedef struct ScanResultSTA ScanResultSTA;

/*  Globals to track module status information */
enum AttackMode {
    ATTACK_BEACON,
    ATTACK_PROBE,
    ATTACK_FUZZ,
    ATTACK_SNIFF,
    ATTACK_DEAUTH,
    ATTACK_MANA,
    ATTACK_MANA_VERBOSE,
    ATTACK_MANA_LOUD,
    ATTACK_AP_DOS,
    ATTACK_AP_CLONE,
    ATTACK_SCAN,
    ATTACK_HANDSHAKE,
    ATTACK_RANDOMISE_MAC, // True
    ATTACK_BT,
    ATTACKS_COUNT
};
typedef enum AttackMode AttackMode;

enum GravityCommand {
    GRAVITY_BEACON = 0,
    GRAVITY_TARGET_SSIDS,
    GRAVITY_PROBE,
    GRAVITY_FUZZ,
    GRAVITY_SNIFF,
    GRAVITY_DEAUTH,
    GRAVITY_MANA,
    GRAVITY_STALK,
    GRAVITY_AP_DOS,
    GRAVITY_AP_CLONE,
    GRAVITY_SCAN,
    GRAVITY_HOP,
    GRAVITY_SET,
    GRAVITY_GET,
    GRAVITY_VIEW,
    GRAVITY_SELECT,
    GRAVITY_SELECTED,
    GRAVITY_CLEAR,
    GRAVITY_HANDSHAKE,
    GRAVITY_COMMANDS,
    GRAVITY_INFO,
    GRAVITY_BT,
    GRAVITY_NONE = 99
};
typedef enum GravityCommand GravityCommand;

/* Packet types TODO: Refactor old code to use them */
typedef enum WiFi_Mgmt_Frames {
    WIFI_FRAME_ASSOC_REQ = 0x00,
    WIFI_FRAME_ASSOC_RESP = 0x10,
    WIFI_FRAME_REASSOC_REQ = 0x20,
    WIFI_FRAME_REASSOC_RESP = 0x30,
    WIFI_FRAME_PROBE_REQ = 0x40,
    WIFI_FRAME_PROBE_RESP = 0x50,
    WIFI_FRAME_BEACON = 0x80,
    WIFI_FRAME_ATIMS = 0x90,
    WIFI_FRAME_DISASSOC = 0xa0,
    WIFI_FRAME_AUTH = 0xb0,
    WIFI_FRAME_DEAUTH = 0xc0,
    WIFI_FRAME_ACTION = 0xd0,
    WIFI_FRAME_COUNT = 12
} WiFi_Mgmt_Frames;

typedef enum PROBE_RESPONSE_AUTH_TYPE {
    AUTH_TYPE_NONE = 1,
    AUTH_TYPE_WEP = 2,
    AUTH_TYPE_WPA = 4
} PROBE_RESPONSE_AUTH_TYPE;
extern const char *AUTH_TYPE_NAMES[];
extern const char *AUTH_TYPE_FLIPPER_NAMES[];

/* Declared here because I get an error trying to use this enum in beacon.h (but it's fine in beacon.c) */
extern PROBE_RESPONSE_AUTH_TYPE authTypes[];

PROBE_RESPONSE_AUTH_TYPE *unpackAuthType(PROBE_RESPONSE_AUTH_TYPE input, int *outputCount);

/* Moving attack_status and hop_defaults off the heap */
extern bool *attack_status;
extern bool *hop_defaults;
extern int *hop_millis_defaults;
extern long ATTACK_MILLIS;
extern char **gravityWordList;

uint8_t *gravity_get_mac();
esp_err_t gravity_set_mac(uint8_t *newMac);

extern const uint8_t BROADCAST[];

/* scan.c */
extern double scanResultExpiry;
extern int gravity_ap_count;
extern int gravity_sel_ap_count;
extern int gravity_sta_count;
extern int gravity_sel_sta_count;
extern ScanResultAP *gravity_aps;
extern ScanResultSTA *gravity_stas;
extern ScanResultAP **gravity_selected_aps;
extern ScanResultSTA **gravity_selected_stas;
esp_err_t gravity_list_all_stas(bool hideExpiredPackets);
esp_err_t gravity_list_all_aps(bool hideExpiredPackets);
esp_err_t gravity_list_sta(ScanResultSTA **stas, int staCount, bool hideExpiredPackets);
esp_err_t gravity_list_ap(ScanResultAP **aps, int apCount, bool hideExpiredPackets);

esp_err_t authTypeToString(PROBE_RESPONSE_AUTH_TYPE authType, char theString[], bool flipperStrings);
esp_err_t send_probe_response(uint8_t *srcAddr, uint8_t *destAddr, char *ssid, enum PROBE_RESPONSE_AUTH_TYPE authType, uint16_t seqNum);


/* Confirmed as in common.c */
bool arrayContainsString(char **arr, int arrCnt, char *str);
int mac_bytes_to_string(uint8_t *bMac, char *strMac);
int mac_string_to_bytes(char *strMac, uint8_t *bMac);
GravityCommand gravityCommandFromString(char *input);
bool staResultListContainsSTA(ScanResultSTA **list, int listLen, ScanResultSTA *sta);
bool apResultListContainsAP(ScanResultAP **list, int listLen, ScanResultAP *ap);
ScanResultAP **collateAPsOfSelectedSTAs(int *apCount);
ScanResultSTA **collateClientsOfSelectedAPs(int *staCount);
char **apListToStrings(ScanResultAP **aps, int apsCount);
int max(int one, int two);
esp_err_t ssid_bytes_to_string(uint8_t *bSsid, char *ssid);

extern const char *TAG;


#endif