#ifndef FUZZ_H
#define FUZZ_H

#include <string.h>
#include <time.h>
#include <freertos/portmacro.h>
#include <esp_err.h>
#include <esp_wifi_types.h>
#include "common.h"

/* Using binary values so we can use binary operations & and | */
typedef enum FuzzPacketType {
    FUZZ_PACKET_NONE = 0,
    FUZZ_PACKET_BEACON = 1,
    FUZZ_PACKET_PROBE_REQ = 2,
    FUZZ_PACKET_PROBE_RESP = 4,
    FUZZ_PACKET_TYPE_COUNT = 4
} FuzzPacketType;

typedef enum FuzzMode {
    FUZZ_MODE_NONE = 0,
    FUZZ_MODE_SSID_OVERFLOW,
    FUZZ_MODE_SSID_MALFORMED,
    FUZZ_MODE_OFF,
    FUZZ_MODE_COUNT
} FuzzMode;

typedef enum FuzzTarget {
    FUZZ_TARGET_BROADCAST = 0,
    FUZZ_TARGET_TARGET_SSIDS,
    FUZZ_TARGET_SELECTED_STA,
    FUZZ_TARGET_SELECTED_AP,
    FUZZ_TARGET_RANDOM
} FuzzTarget;

extern FuzzMode fuzzMode;
extern FuzzPacketType fuzzPacketType;
extern FuzzTarget fuzzTarget;
extern int fuzzCounter;
extern uint8_t malformedFrom;
extern const char *FUZZ_TAG;

/* Result may contain up to 13 bytes */
esp_err_t fuzzTargetAsString(char *result);
/* Function may use up to 38 bytes of memory at the
   location specified by result */
esp_err_t fuzzPacketTypeAsString(char *result);
/* Function may use up to 15 bytes of memory at the
   location specified by result */
esp_err_t fuzzModeAsString(char *result);
/* Initialise the module and start the fuzzing event loop */
esp_err_t fuzz_start(FuzzMode newMode, FuzzPacketType newType, FuzzTarget newTarget);
/* Clean up, free memory, halt event loop */
esp_err_t fuzz_stop();
/* Set the length of the starting SSID for malformed packets */
esp_err_t setMalformedSsidLength(int newLength);
#endif