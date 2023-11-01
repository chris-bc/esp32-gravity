#ifndef GRAVITY_SYNC_H
#define GRAVITY_SYNC_H

#include "common.h"

/* Gravity Sync
   Implements synchronisation features to provide settings, configuration and
   other data to a non-console client such as the Flipper Zero Gravity application
*/

typedef enum GravitySyncItem {
    GRAVITY_SYNC_HOP_ON = 0,
    GRAVITY_SYNC_SSID_MIN,
    GRAVITY_SYNC_SSID_MAX,
    GRAVITY_SYNC_SSID_COUNT,
    GRAVITY_SYNC_CHANNEL,
    GRAVITY_SYNC_MAC,
    GRAVITY_SYNC_ATTACK_PKTS,
    GRAVITY_SYNC_ATTACK_MILLIS,
    GRAVITY_SYNC_MAC_RAND,
    GRAVITY_SYNC_PKT_EXPIRY,
    GRAVITY_SYNC_HOP_MODE,
    GRAVITY_SYNC_DICT_DISABLED,
    GRAVITY_SYNC_PURGE_STRAT,
    GRAVITY_SYNC_PURGE_RSSI_MAX,
    GRAVITY_SYNC_PURGE_AGE_MIN,
    GRAVITY_SYNC_ITEM_COUNT
} GravitySyncItem;

esp_err_t gravity_sync_items(GravitySyncItem *items, uint8_t itemCount);
esp_err_t gravity_sync_item(GravitySyncItem item, bool flushBuffer);
esp_err_t gravity_sync_all();


#endif