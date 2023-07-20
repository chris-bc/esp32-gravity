#ifndef GRAVITY_COMMON_H
#define GRAVITY_COMMON_H

#include <stdbool.h>
#include <string.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include "esp_flip_struct.h"
#include "esp_flip_const.h"

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

/* Moving attack_status and hop_defaults off the heap */
extern bool *attack_status;
extern bool *hop_defaults;
extern int *hop_millis_defaults;
extern long ATTACK_MILLIS;
extern char **gravityWordList;

/* scan.c */
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

typedef enum PROBE_RESPONSE_AUTH_TYPE {
    AUTH_TYPE_NONE,
    AUTH_TYPE_WEP,
    AUTH_TYPE_WPA
} PROBE_RESPONSE_AUTH_TYPE;

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

extern const char *TAG;

#endif