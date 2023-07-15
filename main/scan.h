#include <esp_err.h>
#include <esp_wifi.h>
#include "common.h"

// TODO: If there are problems with SSID filtering, scan_filter_ssid had to be changed from static to get extern working...
extern char scan_filter_ssid[33];
extern uint8_t scan_filter_ssid_bssid[6];
extern double scanResultExpiry;

static const char* SCAN_TAG = "scan@GRAVITY";
static const uint8_t bBroadcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

esp_err_t gravity_merge_results_ap(uint16_t newCount, ScanResultAP *newAPs);
esp_err_t gravity_clear_ap();
esp_err_t gravity_list_ap(ScanResultAP **aps, int apCount, bool hideExpiredPackets);
esp_err_t gravity_list_all_aps(bool hideExpiredPackets);
esp_err_t gravity_select_ap(int selIndex);
bool gravity_ap_isSelected(int index);
esp_err_t gravity_add_ap(uint8_t newAP[6], char *newSSID, int channel);
esp_err_t gravity_add_sta(uint8_t newSTA[6], int channel);
esp_err_t gravity_add_sta_ap(uint8_t *sta, uint8_t *ap);
esp_err_t gravity_clear_sta();
esp_err_t gravity_list_sta(ScanResultSTA **stas, int staCount, bool hideExpiredPackets);
esp_err_t gravity_list_all_stas(bool hideExpiredPackets);
esp_err_t gravity_select_sta(int selIndex);
bool gravity_sta_isSelected(int index);

esp_err_t scan_wifi_parse_frame(uint8_t *payload);

/* External function definitions (gravity.c) */
int mac_bytes_to_string(uint8_t *bMac, char *strMac);
int mac_string_to_bytes(char *strMac, uint8_t *bMac);