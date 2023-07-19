#include "sniff.h"

const char *SNIFF_TAG = "sniff@GRAVITY";

esp_err_t sniffPacket(uint8_t *payload) {
    if (payload[0] == 0x40) {
        printf("inner probe req\n");
    }

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