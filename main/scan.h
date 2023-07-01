#include <esp_err.h>
#include <esp_wifi.h>

#define DEBUG

static const char* SCAN_TAG = "scan@GRAVITY";

struct ScanResultAP {
    wifi_ap_record_t espRecord;
    long long lastSeen;
    clock_t lastSeenClk;
    int index;
    bool selected;
};
typedef struct ScanResultAP ScanResultAP;

struct ScanResultSTA {
    long long lastSeen;
    clock_t lastSeenClk;
    int index;
    bool selected;
    uint8_t mac[6];
    char strMac[18];
    uint8_t assocAP[6];
    int channel;
};
typedef struct ScanResultSTA ScanResultSTA;

enum ScanType {
    SCAN_TYPE_NONE,
    SCAN_TYPE_AP,
    SCAN_TYPE_STA,
    SCAN_TYPE_BOTH
};
extern enum ScanType activeScan;

esp_err_t gravity_merge_results_ap(uint16_t newCount, ScanResultAP *newAPs);
esp_err_t gravity_clear_ap();
esp_err_t gravity_list_ap();
esp_err_t gravity_select_ap(int selIndex);
bool gravity_ap_isSelected(int index);
esp_err_t gravity_add_ap(uint8_t newAP[6], char *newSSID, int channel);
esp_err_t gravity_add_sta(uint8_t newSTA[6], int channel);
esp_err_t gravity_clear_sta();
esp_err_t gravity_list_sta();
esp_err_t gravity_select_sta(int selIndex);
bool gravity_sta_isSelected(int index);

esp_err_t scan_wifi_parse_frame(uint8_t *payload);

/* External function definitions (gravity.c) */
int mac_bytes_to_string(uint8_t *bMac, char *strMac);
int mac_string_to_bytes(char *strMac, uint8_t *bMac);