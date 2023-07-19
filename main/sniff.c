#include "sniff.h"

const char *SNIFF_TAG = "sniff@GRAVITY";

esp_err_t sniffPacket(uint8_t *payload) {
    switch (payload[0]) {
        case 0x00:
            return sniffAssocReq(payload);
            break;
        case 0x10:
            return sniffAssocResp(payload);
            break;
        case 0x20:
            return sniffReassocReq(payload);
            break;
        case 0x30:
            return sniffReassocResp(payload);
            break;
        case 0x40:
            return sniffProbeReq(payload);
            break;
        case 0x50:
            return sniffProbeResp(payload);
            break;
        case 0x80:
            return sniffBeacon(payload);
            break;
        case 0x90:
            return sniffAtims(payload);
            break;
        case 0xa0:
            return sniffDisassoc(payload);
            break;
        case 0xb0:
            return sniffAuth(payload);
            break;
        case 0xc0:
            return sniffDeauth(payload);
            break;
        case 0xd0:
            return sniffAction(payload);
            break;
        default:
            #ifdef CONFIG_FLIPPER
                printf("Unknown packet type %02x\n", payload[0]);
            #else
                ESP_LOGW(SNIFF_TAG, "Unknown packet type: %02x", payload[0]);
            #endif
    }

    return ESP_OK;
}

esp_err_t sniffAssocReq(uint8_t *payload) {
    //

    return ESP_OK;
}

esp_err_t sniffAssocResp(uint8_t *payload) {
    //

    return ESP_OK;
}

esp_err_t sniffReassocReq(uint8_t *payload) {
    //

    return ESP_OK;
}

esp_err_t sniffReassocResp(uint8_t *payload) {
    //

    return ESP_OK;
}

esp_err_t sniffProbeReq(uint8_t *payload) {
    //

    return ESP_OK;
}

esp_err_t sniffProbeResp(uint8_t *payload) {
    //

    return ESP_OK;
}

esp_err_t sniffBeacon(uint8_t *payload) {
    //

    return ESP_OK;
}

esp_err_t sniffAtims(uint8_t *payload) {
    //

    return ESP_OK;
}

esp_err_t sniffDisassoc(uint8_t *payload) {
    //

    return ESP_OK;
}

esp_err_t sniffAuth(uint8_t *payload) {
    //

    return ESP_OK;
}

esp_err_t sniffDeauth(uint8_t *payload) {
    //

    return ESP_OK;
}

esp_err_t sniffAction(uint8_t *payload) {
    //

    return ESP_OK;
}