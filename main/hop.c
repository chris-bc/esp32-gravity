#include "hop.h"

const char *HOP_TAG = "hop@GRAVITY";
long hop_millis = 0;
enum HopStatus hopStatus = HOP_STATUS_DEFAULT;
TaskHandle_t channelHopTask = NULL;

/* Channel hopping task  */
void channelHopCallback(void *pvParameter) {
    uint8_t ch;
    wifi_second_chan_t sec;
    if (hop_millis == 0) {
        hop_millis = CONFIG_DEFAULT_HOP_MILLIS;
    }

    while (true) {
        // Delay hop_millis ms
        vTaskDelay(hop_millis / portTICK_PERIOD_MS);

        /* Check whether we should be hopping or not */
        if (isHopEnabled()) {
            ESP_ERROR_CHECK(esp_wifi_get_channel(&ch, &sec));
            ch++;
            if (ch >= MAX_CHANNEL) {
                ch -= (MAX_CHANNEL - 1);
            }
            if (esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_ABOVE) != ESP_OK) {
                ESP_LOGW(TAG, "Failed to change to channel %d", ch);
            }
        }
    }
}

/* Evaluate whether channel hopping should be enabled based on currently-active
   features and their defaults
*/
bool isHopEnabledByDefault() {
    int i = 0;
    /* Loop exits either at the end of the array or when an active task
       with hopping enabled is found
    */
    for (; i < ATTACKS_COUNT && (!attack_status[i] || !hop_defaults[i]); ++i) { }
    return (i < ATTACKS_COUNT);
}

/* Evaluate whether channel hopping is enabled, using the combination of
   hopStatus and isHopEnabledByDefault()
*/
bool isHopEnabled() {
    return (hopStatus == HOP_STATUS_ON || (hopStatus == HOP_STATUS_DEFAULT && isHopEnabledByDefault()));
}

/* Calculate the default dwell time (hop_millis) based on the features that are currently active */
/* Where multiple features are active the greatest dwell time will be returned */
int dwellForCurrentFeatures() {
    int retVal = 0;
    for (int i = 0; i < ATTACKS_COUNT; ++i) {
        if (attack_status[i] && hop_defaults[i] && hop_millis_defaults[i] > retVal) {
            retVal = hop_millis_defaults[i];
        }
    }
    /* If no features are active use the global default */
    if (retVal == 0) {
        retVal = CONFIG_DEFAULT_HOP_MILLIS;
    }
    return retVal;
}

int dwellTime() {
    switch (hopStatus) {
    case HOP_STATUS_ON:
        return hop_millis;
        break;
    case HOP_STATUS_DEFAULT:
    case HOP_STATUS_OFF:        /* Don't really care what we do here */
        return dwellForCurrentFeatures();
        break;
    }
    return -1;                  /* Error condition */
}
