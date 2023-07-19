#include <stdint.h>
#include <esp_err.h>

extern const char *SNIFF_TAG;

esp_err_t sniffPacket(uint8_t *payload);
esp_err_t sniffAssocReq(uint8_t *payload);
esp_err_t sniffAssocResp(uint8_t *payload);
esp_err_t sniffReassocReq(uint8_t *payload);
esp_err_t sniffReassocResp(uint8_t *payload);
esp_err_t sniffProbeReq(uint8_t *payload);
esp_err_t sniffProbeResp(uint8_t *payload);
esp_err_t sniffBeacon(uint8_t *payload);
esp_err_t sniffAtims(uint8_t *payload);
esp_err_t sniffDisassoc(uint8_t *payload);
esp_err_t sniffAuth(uint8_t *payload);
esp_err_t sniffDeauth(uint8_t *payload);
esp_err_t sniffAction(uint8_t *payload);