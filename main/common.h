#ifndef GRAVITY_COMMON_H
#define GRAVITY_COMMON_H

#include <stdbool.h>
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

/* scan.c */
extern int gravity_ap_count;
extern int gravity_sel_ap_count;
extern int gravity_sta_count;
extern int gravity_sel_sta_count;
extern ScanResultAP *gravity_aps;
extern ScanResultSTA *gravity_stas;
extern ScanResultAP **gravity_selected_aps;
extern ScanResultSTA **gravity_selected_stas;

extern char **apListToStrings(ScanResultAP **aps, int apsCount);
extern ScanResultSTA **collateClientsOfSelectedAPs(int *staCount);
extern bool staResultListContainsSTA(ScanResultSTA **list, int listLen, ScanResultSTA *sta);

#endif