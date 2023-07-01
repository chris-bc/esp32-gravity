#include <esp_err.h>
#include <esp_wifi.h>

static const char* SCAN_TAG = "scan@GRAVITY";

struct ScanResultAP {
    wifi_ap_record_t espRecord;
    long long lastSeen;
    int index;
    bool selected;
};
typedef struct ScanResultAP ScanResultAP;

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
esp_err_t scan_wifi_parse_frame(uint8_t *payload);
bool gravity_ap_isSelected(int index);

/* External function definitions (gravity.c) */
int mac_bytes_to_string(uint8_t *bMac, char *strMac);
int mac_string_to_bytes(char *strMac, uint8_t *bMac);