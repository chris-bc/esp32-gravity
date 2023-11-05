#include "sync.h"

/* Format a string to be interpreted by the client for sync purposes
   Resultant string is of the form (<item>:<value>) e.g. (4:11) to
   represent channel 11.
*/
esp_err_t gravity_sync_item(GravitySyncItem item, bool flushBuffer) {
    esp_err_t result = ESP_OK;
    esp_err_t tmpErr;
    printf("(%d:", item);
    switch (item) {
        case GRAVITY_SYNC_HOP_ON:
            printf("%d", hopStatus);
            break;
        case GRAVITY_SYNC_SSID_MIN:
            printf("%d", SSID_LEN_MIN);
            break;
        case GRAVITY_SYNC_SSID_MAX:
            printf("%d", SSID_LEN_MAX);
            break;
        case GRAVITY_SYNC_SSID_COUNT:
            printf("%d", DEFAULT_SSID_COUNT);
            break;
        case GRAVITY_SYNC_CHANNEL:
            uint8_t channel;
            wifi_second_chan_t second;
            tmpErr = esp_wifi_get_channel(&channel, &second);
            if (tmpErr != ESP_OK) {
                printf("ERROR");
                result |= tmpErr;
            } else {
                printf("%u", channel);
            }
            break;
        case GRAVITY_SYNC_MAC:
            uint8_t bMac[6];
            char strMac[18];
            tmpErr = esp_wifi_get_mac(WIFI_IF_AP, bMac);
            if (tmpErr != ESP_OK) {
                printf("ERROR");
                result |= tmpErr;
            } else {
                tmpErr = mac_bytes_to_string(bMac, strMac);
                if (tmpErr != ESP_OK) {
                    printf("ERROR");
                    result |= tmpErr;
                } else {
                    printf("%s", strMac);
                }
            }
            break;
        case GRAVITY_SYNC_ATTACK_MILLIS:
            printf("%ld", ATTACK_MILLIS);
            break;
        case GRAVITY_SYNC_MAC_RAND:
            // Broken, but *is* implemented *shrugs*
            printf("%d", attack_status[ATTACK_RANDOMISE_MAC]);
            break;
        case GRAVITY_SYNC_PKT_EXPIRY:
            printf("%f", scanResultExpiry);
            break;
        case GRAVITY_SYNC_HOP_MODE:
            printf("%d", hopMode);
            break;
        case GRAVITY_SYNC_DICT_DISABLED:
            printf("%d", scrambledWords);
            break;
        case GRAVITY_SYNC_PURGE_STRAT:
            printf("%d", purgeStrategy);
            break;
        case GRAVITY_SYNC_PURGE_RSSI_MAX:
            printf("%ld", PURGE_MAX_RSSI);
            break;
        case GRAVITY_SYNC_PURGE_AGE_MIN:
            printf("%d", PURGE_MIN_AGE);
            break;
        default:
            printf("ERROR");
            result = ESP_ERR_INVALID_ARG;
            break;
    }
    printf(")");
    /* Send newline and flush stdout buffer if needed */
    if (flushBuffer) {
        printf("\n");
        fflush(stdout);
    }
    return result;
}

esp_err_t gravity_sync_items(GravitySyncItem *items, uint8_t itemCount) {
    esp_err_t result = ESP_OK;
    for (int i = 0; i < itemCount; ++i) {
        result |= gravity_sync_item(items[i], false);
    }
    printf("\n");
    fflush(stdout);
    return result;
}

esp_err_t gravity_sync_all() {
    /* Build an array containing all sync items */
    GravitySyncItem *items = malloc(sizeof(GravitySyncItem) * GRAVITY_SYNC_ITEM_COUNT);
    if (items == NULL) {
            printf("(ERROR)\n");
            return ESP_ERR_NO_MEM;
    }
    for (uint8_t i = 0; i < GRAVITY_SYNC_ITEM_COUNT; ++i) {
        items[i] = i;
    }
    /* Pass to gravity_sync_items */
    return gravity_sync_items(items, GRAVITY_SYNC_ITEM_COUNT);
}