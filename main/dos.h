#ifndef DOS_H
#define DOS_H

#include "common.h"
#include <esp_wifi.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_interface.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>


esp_err_t dosParseFrame(uint8_t *payload);
esp_err_t cloneStartStop(bool isStarting, int authType);
esp_err_t dos_display_status();
esp_err_t clone_display_status();

extern const char *DOS_TAG;
extern const uint8_t INNOCENT_MAC_BYTES[];
extern const char INNOCENT_MAC_STR[];

#endif