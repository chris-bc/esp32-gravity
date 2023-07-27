#include "dos.h"

/* Determine whether we need to do anything in response to the current frame */
esp_err_t dosParseFrame(uint8_t *payload) {
    // Get frame type

    // If it's a type that can clue me in on whether one party is an AP extract relevant bytes

    // If the AP is one of the selectedAPs

    // global variable to store our current MAC
    // if current MAC doesn't match the AP in the frame

    // set MAC to match AP
    // update current MAC

    // send a deauth packet to STA, originating from AP's MAC

    return ESP_OK;
}