#ifndef FUZZ_H
#define FUZZ_H

#include "common.h"
#include "esp_flip_struct.h"
#include "esp_flip_const.h"
#include "beacon.h"

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

extern FuzzMode fuzzMode;
extern FuzzPacketType fuzzPacketType;

/* Function may use up to 38 bytes of memory at the
   location specified by result */
esp_err_t fuzzPacketTypeAsString(char *result);
/* Function may use up to 15 bytes of memory at the
   location specified by result */
esp_err_t fuzzModeAsString(char *result);
/* Initialise the module and start the fuzzing event loop */
esp_err_t fuzz_start(FuzzMode newMode, FuzzPacketType newType);
/* Clean up, free memory, halt event loop */
esp_err_t fuzz_stop();
#endif