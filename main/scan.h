#ifndef SCAN_H
#define SCAN_H

#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <esp_log.h>

#include <stdbool.h>
#include <time.h>
#include <string.h>
#include "common.h"


// TODO: If there are problems with SSID filtering, scan_filter_ssid had to be changed from static to get extern working...
extern char scan_filter_ssid[33];
extern uint8_t scan_filter_ssid_bssid[6];

static const char* SCAN_TAG = "scan@GRAVITY";

esp_err_t gravity_merge_results_ap(uint16_t newCount, ScanResultAP *newAPs);
esp_err_t gravity_clear_ap();
esp_err_t gravity_list_ap(ScanResultAP **aps, int apCount, bool hideExpiredPackets);
esp_err_t gravity_list_all_aps(bool hideExpiredPackets);
esp_err_t gravity_select_ap(int selIndex);
esp_err_t gravity_add_ap(uint8_t newAP[6], char *newSSID, int channel);
esp_err_t gravity_add_sta(uint8_t newSTA[6], int channel);
esp_err_t gravity_add_sta_ap(uint8_t *sta, uint8_t *ap);
esp_err_t gravity_clear_sta();
esp_err_t gravity_list_sta(ScanResultSTA **stas, int staCount, bool hideExpiredPackets);
esp_err_t gravity_list_all_stas(bool hideExpiredPackets);
esp_err_t gravity_select_sta(int selIndex);
bool gravity_sta_isSelected(int index);
bool gravity_ap_isSelected(int index);

esp_err_t scan_wifi_parse_frame(uint8_t *payload);

#endif