#ifndef HOP_H
#define HOP_H

#include <stdint.h>
#include <stdbool.h>
#include <esp_wifi_types.h>
#include <esp_system.h>
#include <esp_err.h>
#include <freertos/portmacro.h>
#include "common.h"

#define MAX_CHANNEL 9

enum HopStatus {
    HOP_STATUS_OFF,
    HOP_STATUS_ON,
    HOP_STATUS_DEFAULT
};

typedef enum HopMode {
    HOP_MODE_SEQUENTIAL = 0,
    HOP_MODE_RANDOM,
    HOP_MODE_COUNT
} HopMode;

extern const char *HOP_TAG;
extern long hop_millis;
extern enum HopStatus hopStatus;
extern HopMode hopMode;
extern TaskHandle_t channelHopTask;

void channelHopCallback(void *pvParameter);
bool isHopEnabledByDefault();
bool isHopEnabled();
int dwellForCurrentFeatures();
int dwellTime();
esp_err_t hopModeToString(HopMode mode, char *str);
esp_err_t setHopForNewCommand();

#endif