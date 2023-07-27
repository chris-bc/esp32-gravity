#ifndef DOS_H
#define DOS_H

#include <esp_wifi.h>
#include <esp_err.h>
#include <esp_log.h>
#include <stdio.h>
#include <stdint.h>

esp_err_t dosParseFrame(uint8_t *payload);

#endif