#ifndef STALK_H
#define STALK_H

#include "common.h"
#include <time.h>
#include "esp_wifi_types.h"

esp_err_t stalk_begin();
esp_err_t stalk_end();
esp_err_t stalk_frame(uint8_t *payload, wifi_pkt_rx_ctrl_t rx_ctrl);

extern const char *STALK_TAG;

#endif