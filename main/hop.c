#include "hop.h"

const char *HOP_TAG = "hop@GRAVITY";
long hop_millis = 0;
enum HopStatus hopStatus = HOP_STATUS_DEFAULT;
HopMode hopMode = HOP_MODE_SEQUENTIAL;
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
            /* Changing to a random channel or the next channel? */
            if (hopMode == HOP_MODE_RANDOM) {
                ch = (random() % MAX_CHANNEL) + 1;
            } else if (hopMode != HOP_MODE_SEQUENTIAL) {
                /* Not random and not sequential */
                #ifdef CONFIG_FLIPPER
                    printf("hopMode is invalid, reverting to sequential\n");
                #else
                    ESP_LOGW(HOP_TAG, "hopMode contains an unkown value (%d), reverting to HOP_MODE_SEQUENTIAL", hopMode);
                #endif
                hopMode = HOP_MODE_SEQUENTIAL;
            }
            if (hopMode == HOP_MODE_SEQUENTIAL) {
                ch++;
                if (ch >= MAX_CHANNEL) {
                    ch -= (MAX_CHANNEL - 1);
                }
            }
            if (esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_ABOVE) != ESP_OK) {
                ESP_LOGW(TAG, "Failed to change to channel %d", ch);
            }
        }
    }
}

/* Convert the specified hopMode into a string.
   The provided char* must contain enough free memory for at least 19 characters */
esp_err_t hopModeToString(HopMode mode, char *str) {
    char tmpStr[19] = "";
    switch (mode) {
        case HOP_MODE_SEQUENTIAL:
            strcpy(tmpStr, "HOP_MODE_SEQUENTAL");
            break;
        case HOP_MODE_RANDOM:
            strcpy(tmpStr, "HOP_MODE_RANDOM");
            break;
        case HOP_MODE_COUNT:
            strcpy(tmpStr, "HOP_MODE_COUNT");
            break;
        default:
            #ifdef CONFIG_FLIPPER
                printf("Invalid HopMode: %d\n", mode);
            #else
                ESP_LOGE(HOP_TAG, "Invalid HopMode specified: \"%d\"", mode);
            #endif
            return ESP_ERR_INVALID_ARG;
    }
    strcpy(str, tmpStr);

    return ESP_OK;
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

/* Using the helper functions above, figure out whether hopping should be
   enabled and, if so, the required dwell time and make it so.
*/
esp_err_t setHopForNewCommand() {
    esp_err_t retVal = ESP_OK;

    /* Start/stop hopping task loop as needed */
    hop_millis = dwellTime();
    /* Make sure the hop task is running if it needs to be */
    if (isHopEnabled()) {
        createHopTaskIfNeeded();
    }
    #ifdef CONFIG_DEBUG
        #ifdef CONFIG_FLIPPER
            printf("Ch. Hop %s, Dwell %ldms\n", (isHopEnabled())?"ON":"OFF", hop_millis);
        #else
            ESP_LOGW(HOP_TAG, "Channel Hopping %s\t\tDwell time %ldms", (isHopEnabled())?"Enabled":"Disabled", hop_millis);
        #endif
    #endif
    return retVal;
}

void createHopTaskIfNeeded() {
    if (channelHopTask == NULL) {
        ESP_LOGI(HOP_TAG, "Gravity's channel hopping event task is not running, starting it now.");
        xTaskCreate(&channelHopCallback, "channelHopCallback", 2048, NULL, 5, &channelHopTask);
    }
}