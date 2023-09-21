#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <string.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_err.h>
#include <esp_wifi_types.h>
#include <stddef.h>
#include "common.h"

#if defined(CONFIG_IDF_TARGET_ESP32)

#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#include <esp_gatt_defs.h>
#include <esp_gatt_common_api.h>
#include <esp_gap_bt_api.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_bt_device.h>
#include <esp_bt_defs.h>

typedef enum {
    GRAVITY_BT_SCAN_CLASSIC_DISCOVERY = 0,
    GRAVITY_BT_SCAN_BLE,
    GRAVITY_BT_SCAN_TYPE_ACTIVE,
    GRAVITY_BT_SCAN_TYPE_COUNT
} gravity_bt_scan_t;

typedef enum {
    APP_GAP_STATE_IDLE = 0,
    APP_GAP_STATE_DEVICE_DISCOVERING,
    APP_GAP_STATE_DEVICE_DISCOVER_COMPLETE,
    APP_GAP_STATE_SERVICE_DISCOVERING,
    APP_GAP_STATE_SERVICE_DISCOVER_COMPLETE,
} app_gap_state_t;

typedef struct {
    //esp_bt_uuid_t *uuid;
    uint16_t uuid16;
    char name[40];
} bt_uuid;

typedef struct {
    uint8_t num_services;
    esp_bt_uuid_t *service_uuids;
    bt_uuid **known_services;
    uint8_t known_services_len;
    clock_t lastSeen;
} grav_bt_svc;

typedef struct {
    uint8_t bdname_len;
    uint8_t eir_len;
    int32_t rssi;
    uint32_t cod;
    uint8_t *eir; // Was [ESP_BT_GAP_EIR_DATA_LEN];
    char *bdName; // Was [ESP_BT_GAP_MAX_BDNAME_LEN + 1];
    esp_bd_addr_t bda;
    gravity_bt_scan_t scanType;
    clock_t lastSeen;
    bool selected;
    uint8_t index;
    grav_bt_svc bt_services; /* Hold service scan results */
} app_gap_cb_t;

extern bt_uuid bt_uuids[];
extern app_gap_cb_t **gravity_bt_devices;
extern uint8_t gravity_bt_dev_count;
extern app_gap_cb_t **gravity_selected_bt;
extern uint8_t gravity_sel_bt_count;

extern const char *BT_TAG;

esp_err_t bt_listAllServices();
esp_err_t bt_listAllServicesFor(app_gap_cb_t **devices, uint8_t devCount);
esp_err_t bt_listAllServicesDev(app_gap_cb_t *thisDev);
esp_err_t bt_service_rm_all();
esp_err_t listKnownServices(app_gap_cb_t **devices, uint8_t devCount);
esp_err_t listKnownServicesDev(app_gap_cb_t *thisDev);
esp_err_t listUnknownServices();
app_gap_cb_t *deviceWithBDA(esp_bd_addr_t bda);
esp_err_t identifyKnownServices(app_gap_cb_t *thisDev);
esp_err_t gravity_ble_scan_start(gravity_bt_purge_strategy_t purgeStrat);
esp_err_t gravity_bt_initialise();
esp_err_t gravity_bt_gap_start();
esp_err_t gravity_bt_gap_services_discover(app_gap_cb_t *device);
esp_err_t gravity_bt_scan_display_status();
esp_err_t gravity_bt_list_all_devices(bool hideExpiredPackets);
esp_err_t gravity_bt_list_devices(app_gap_cb_t **devices, uint8_t deviceCount, bool hideExpiredPackets);
esp_err_t gravity_clear_bt();
esp_err_t gravity_select_bt(uint8_t selIndex);
bool gravity_bt_isSelected(uint8_t selIndex);
esp_err_t gravity_bt_disable_scan();
void *gravity_ble_purge_and_malloc(uint16_t bytes);
esp_err_t gravity_bt_shrink_devices();
char *purgeStrategyToString(gravity_bt_purge_strategy_t strategy, char *strOutput);
esp_err_t purgeBLE(gravity_bt_purge_strategy_t strategy, uint16_t minAge, int32_t maxRssi);
esp_err_t purgeBT(gravity_bt_purge_strategy_t strategy, uint16_t minAge, int32_t maxRssi);

esp_err_t bt_dev_add(app_gap_cb_t *dev);
esp_err_t bt_dev_add_components(esp_bd_addr_t bda, char *bdName, uint8_t bdNameLen, uint8_t *eir, uint8_t eirLen,
                        uint32_t cod, int32_t rssi, gravity_bt_scan_t devScanType);
esp_err_t bt_dev_copy(app_gap_cb_t dest, app_gap_cb_t source);

esp_err_t bt_scanTypeToString(gravity_bt_scan_t scanType, char *strOutput);
bool isBDAInArray(esp_bd_addr_t bda, app_gap_cb_t **array, uint8_t arrayLen);
esp_err_t gravity_bt_discover_all_services();
esp_err_t gravity_bt_discover_selected_services();
//esp_err_t gravity_bt_discover_services(app_gap_cb_t *dev);

esp_err_t updateDevice(bool *updatedFlags, esp_bd_addr_t theBda, int32_t theCod, int32_t theRssi, uint8_t theNameLen, char *theName, uint8_t theEirLen, uint8_t *theEir);

#endif
#endif