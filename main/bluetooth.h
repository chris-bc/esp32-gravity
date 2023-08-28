#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <string.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_err.h>
#include <esp_wifi_types.h>
#include <stddef.h>

#if defined(CONFIG_IDF_TARGET_ESP32)

#include <esp_gap_bt_api.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_bt_device.h>
#include <esp_bt_defs.h>

#include "common.h"

typedef enum {
    BT_SCAN_TYPE_DISCOVERY = 0,
    BT_SCAN_TYPE_PASSIVE,
    BT_SCAN_TYPE_ACTIVE,
    BT_SCAN_TYPE_COUNT
} gravity_bt_scan_t;

typedef enum {
    APP_GAP_STATE_IDLE = 0,
    APP_GAP_STATE_DEVICE_DISCOVERING,
    APP_GAP_STATE_DEVICE_DISCOVER_COMPLETE,
    APP_GAP_STATE_SERVICE_DISCOVERING,
    APP_GAP_STATE_SERVICE_DISCOVER_COMPLETE,
} app_gap_state_t;

typedef struct {
    uint8_t bdname_len;
    uint8_t eir_len;
    int32_t rssi;
    uint32_t cod;
    uint8_t eir[ESP_BT_GAP_EIR_DATA_LEN];
    char bdName[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
    esp_bd_addr_t bda;
    gravity_bt_scan_t scanType;
} app_gap_cb_t;
extern app_gap_cb_t *gravity_bt_devices;
extern uint8_t gravity_bt_dev_count;

extern const char *BT_TAG;

esp_err_t gravity_bt_initialise();
esp_err_t gravity_bt_gap_start();
esp_err_t gravity_bt_gap_services_discover(app_gap_cb_t *device);
esp_err_t bt_scan_display_status();

esp_err_t bt_dev_add(app_gap_cb_t *dev);
esp_err_t bt_dev_add_components(esp_bd_addr_t bda, char *bdName, uint8_t bdNameLen, uint8_t *eir, uint8_t eirLen,
                        uint32_t cod, int32_t rssi, gravity_bt_scan_t devScanType);
esp_err_t bt_dev_copy(app_gap_cb_t dest, app_gap_cb_t source);

bool isBDAInArray(esp_bd_addr_t bda, app_gap_cb_t *array, uint8_t arrayLen);
//esp_err_t gravity_bt_discover_all_services();
//esp_err_t gravity_bt_discover_services(app_gap_cb_t *dev);

esp_err_t updateDevice(bool *updatedFlags, esp_bd_addr_t theBda, int32_t theCod, int32_t theRssi, uint8_t theNameLen, char *theName, uint8_t theEirLen, uint8_t *theEir);

#endif
#endif