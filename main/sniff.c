#include "sniff.h"

const char *SNIFF_TAG = "sniff@GRAVITY";

esp_err_t sniffPacket(uint8_t *payload) {
    switch (payload[0]) {
        case WIFI_FRAME_ASSOC_REQ:
            return sniffAssocReq(payload);
            break;
        case WIFI_FRAME_ASSOC_RESP:
            return sniffAssocResp(payload);
            break;
        case WIFI_FRAME_REASSOC_REQ:
            return sniffReassocReq(payload);
            break;
        case WIFI_FRAME_REASSOC_RESP:
            return sniffReassocResp(payload);
            break;
        case WIFI_FRAME_PROBE_REQ:
            return sniffProbeReq(payload);
            break;
        case WIFI_FRAME_PROBE_RESP:
            return sniffProbeResp(payload);
            break;
        case WIFI_FRAME_BEACON:
            return sniffBeacon(payload);
            break;
        case WIFI_FRAME_ATIMS:
            return sniffAtims(payload);
            break;
        case WIFI_FRAME_DISASSOC:
            return sniffDisassoc(payload);
            break;
        case WIFI_FRAME_AUTH:
            return sniffAuth(payload);
            break;
        case WIFI_FRAME_DEAUTH:
            return sniffDeauth(payload);
            break;
        case WIFI_FRAME_ACTION:
            return sniffAction(payload);
            break;
        default:
            /* Not a management frame */
    }

    return ESP_OK;
}

esp_err_t sniffAssocReq(uint8_t *payload) {
    printf("%s: Association Request\n", SNIFF_TAG);

    return ESP_OK;
}

esp_err_t sniffAssocResp(uint8_t *payload) {
    printf("%s: Association Response\n", SNIFF_TAG);

    return ESP_OK;
}

esp_err_t sniffReassocReq(uint8_t *payload) {
    printf("%s: Reassociation Request\n", SNIFF_TAG);

    return ESP_OK;
}

esp_err_t sniffReassocResp(uint8_t *payload) {
    printf("%s: Reassociation Response\n", SNIFF_TAG);

    return ESP_OK;
}

esp_err_t sniffProbeReq(uint8_t *payload) {
    printf("%s: Probe Request\n", SNIFF_TAG);

    return ESP_OK;
}

esp_err_t sniffProbeResp(uint8_t *payload) {
    printf("%s: Probe Response\n", SNIFF_TAG);

    return ESP_OK;
}

esp_err_t sniffBeacon(uint8_t *payload) {
    printf("%s: Beacon\n", SNIFF_TAG);

    return ESP_OK;
}

esp_err_t sniffAtims(uint8_t *payload) {
    printf("%s: Atims Packet\n", SNIFF_TAG);

    return ESP_OK;
}

esp_err_t sniffDisassoc(uint8_t *payload) {
    printf("%s: Disassociation Packet\n", SNIFF_TAG);

    return ESP_OK;
}

esp_err_t sniffAuth(uint8_t *payload) {
    printf("%s: Authentication Packet\n", SNIFF_TAG);

    return ESP_OK;
}

esp_err_t sniffDeauth(uint8_t *payload) {
    printf("%s: Deauthentication Packet", SNIFF_TAG);

    return ESP_OK;
}

esp_err_t sniffAction(uint8_t *payload) {
    printf("%s: Actions Packet\n", SNIFF_TAG);

    return ESP_OK;
}