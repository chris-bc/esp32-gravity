#include "beacon.h"
#include "esp_err.h"
#include "common.h"

int DEFAULT_SSID_COUNT = 20;
int SSID_LEN_MIN = 8;
int SSID_LEN_MAX = 32;

char **attack_ssids = NULL;

/*
 * This is the (currently unofficial) 802.11 raw frame TX API,
 * defined in esp32-wifi-lib's libnet80211.a/ieee80211_output.o
 *
 * This declaration is all you need for using esp_wifi_80211_tx in your own application.
 */
esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void *buffer, int len, bool en_sys_seq);

#define BEACON_SSID_OFFSET 38
#define SRCADDR_OFFSET 10
#define BSSID_OFFSET 16
#define SEQNUM_OFFSET 22

static beacon_attack_t attackType = ATTACK_BEACON_NONE;
static int SSID_COUNT;

static TaskHandle_t beaconTask = NULL;

static uint8_t beacon_raw[] = {
	0x80, 0x00,							    // 0-1: Frame Control
	0x00, 0x00,						    	// 2-3: Duration
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff,				// 4-9: Destination address (broadcast)
	0xba, 0xde, 0xaf, 0xfe, 0x00, 0x06,		// 10-15: Source address
	0xba, 0xde, 0xaf, 0xfe, 0x00, 0x06,		// 16-21: BSSID
	0x00, 0x00,				    		// 22-23: Sequence / fragment number
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,     // 24-31: Timestamp (GETS OVERWRITTEN TO 0 BY HARDWARE)
	0x64, 0x00,			    			// 32-33: Beacon interval
	0x31, 0x04,		    				// 34-35: Capability info
	0x00, 0x00, /* FILL CONTENT HERE */	// 36-38: SSID parameter set, 0x00:length:content
	0x01, 0x08, 0x82, 0x84,	0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24,	// 39-48: Supported rates
	0x03, 0x01, 0x01,				// 49-51: DS Parameter set, current channel 1 (= 0x01),
	0x05, 0x04, 0x01, 0x02, 0x00, 0x00,		// 52-57: Traffic Indication Map
	
};

char *currentSsid = NULL;
int currentSsidLen = 0;

/* Check whether the specified ScanResultSTA list contains the specified ScanResultSTA */
bool staResultListContainsSTA(ScanResultSTA **list, int listLen, ScanResultSTA *sta) {
	int i;
	for (i = 0; i < listLen && memcmp(list[i]->mac, sta->mac, 6); ++i) { }
	return (i < listLen);
}

/* For "APs" beacon mode we need a set of all STAs that are clients of the selected APs
   Here we will use cached data to determine this, to provide us a time sample of data
   to draw from. This does mean, however, that SCANNING SHOULD BE ENABLED while using
   this feature. It won't be forced because there are use cases not to.
   Caller must free the result
 */
ScanResultSTA **collateClientsOfSelectedAPs(int *staCount) {
	/* Start out by getting an upper bound of the number of results we'll have */
	int resUpperBound = 0;
	/* Loop through gravity_selected_aps and sum stationCount */
	for (int i = 0; i < gravity_sel_ap_count; ++i) {
		resUpperBound += gravity_selected_aps[i]->stationCount;
	}

	/* Avoid having to guard every second operation */
	if (resUpperBound == 0) {
		return NULL;
	}

	int resCount = 0;
	ScanResultSTA **resPassOne = malloc(sizeof(ScanResultSTA *) * resUpperBound);
	if (resPassOne == NULL) {
		ESP_LOGE(BEACON_TAG, "Unable to allocate temporary storage for extracting clients of selected APs");
		return NULL;
	}

	/* Loop through gravity_selected_aps, looping through ap[i]->stations, adding
	   all stations exactly once to resPassOne
	*/
	for (int i = 0; i < gravity_sel_ap_count; ++i) {
		for (int j = 0; j < gravity_selected_aps[i]->stationCount; ++j) {
			/* Does ((ScanResultSTA *)gravity_selected_aps[i]->stations[j]) need
			   to be added to resPassOne?
			*/
			if (!staResultListContainsSTA(resPassOne, resCount, (ScanResultSTA *)gravity_selected_aps[i]->stations[j])) {
				/* Add it */
				resPassOne[resCount++] = (ScanResultSTA *)gravity_selected_aps[i]->stations[j];
			}
		}
	}

	/* Before finishing shrink resPassOne to only the length required */
	if (resCount < resUpperBound) {
		ScanResultSTA **res = malloc(sizeof(ScanResultSTA *) * resCount);
		if (res == NULL) {
			ESP_LOGW(BEACON_TAG, "Unable to allocate memory to reduce space occupied by Beacon STA list. Sorry.");
		} else {
			/* Copy resPassOne into res */
			for (int i = 0; i < resCount; ++i) {
				res[i] = resPassOne[i];
			}
			free(resPassOne);
			resPassOne = res;
		}
	}
	/* Return length */
	*staCount = resCount;
	return resPassOne;
}

/* Extract and return SSIDs from the specified ScanResultAP array */
char **apListToStrings(ScanResultAP **aps, int apsCount) {
	char **res = malloc(sizeof(char *) * apsCount);
	if (res == NULL) {
		ESP_LOGE(BEACON_TAG, "Unable to allocate memory to extract AP names");
		return NULL;
	}

	for (int i = 0; i < apsCount; ++i) {
		res[i] = malloc(sizeof(char) * 33);
		if (res[i] == NULL) {
			ESP_LOGE(BEACON_TAG, "Unable to allocate memory to hold AP %d", i);
			free(res);
			return NULL;
		}
		strcpy(res[i], (char *)aps[i]->espRecord.ssid);
	}
	return res;
}

void beaconSpam(void *pvParameter) {
	uint8_t line = 0;

	// Keep track of beacon sequence numbers on a per-songline-basis
	uint16_t *seqnum = malloc(sizeof(uint16_t) * SSID_COUNT);
	if (seqnum == NULL) {
		ESP_LOGE(BEACON_TAG, "Failed to allocate space to monitor sequence numbers. PANIC!");
		return;
	}
	for (int i=0; i<SSID_COUNT; ++i) {
		seqnum[i] = 0;
	}

	for (;;) {
		if (attackType != ATTACK_BEACON_INFINITE) {
			vTaskDelay(100 / SSID_COUNT / portTICK_PERIOD_MS);
		}
		vTaskDelay(1);

		// Pull the current SSID and SSID length into variables to more
		//   easily implement infinite beacon spam
		if (attackType == ATTACK_BEACON_INFINITE) {
			if (currentSsid != NULL) {
				free(currentSsid);
			}
			currentSsid = generate_random_ssid();
			currentSsidLen = strlen(currentSsid);
		} else {
			currentSsid = attack_ssids[line];
			currentSsidLen = strlen(attack_ssids[line]);
		}

		// Insert the next SSID into beacon packet
		uint8_t beacon_send[200];
		memcpy(beacon_send, beacon_raw, BEACON_SSID_OFFSET - 1);
		beacon_send[BEACON_SSID_OFFSET - 1] = currentSsidLen;
		memcpy(&beacon_send[BEACON_SSID_OFFSET], currentSsid, currentSsidLen);
		memcpy(&beacon_send[BEACON_SSID_OFFSET + currentSsidLen], &beacon_raw[BEACON_SSID_OFFSET], sizeof(beacon_raw) - BEACON_SSID_OFFSET);

		// Last byte of source address / BSSID will be line number - emulate multiple APs broadcasting one SSID each
		beacon_send[SRCADDR_OFFSET + 5] = line;
		beacon_send[BSSID_OFFSET + 5] = line;

		// Update sequence number
		beacon_send[SEQNUM_OFFSET] = (seqnum[line] & 0x0f) << 4;
		beacon_send[SEQNUM_OFFSET + 1] = (seqnum[line] & 0xff0) >> 4;
		seqnum[line]++;
		if (seqnum[line] > 0xfff)
			seqnum[line] = 0;

		esp_wifi_80211_tx(WIFI_IF_AP, beacon_send, sizeof(beacon_raw) + strlen(attack_ssids[line]), false);

		#ifdef CONFIG_DEBUG_VERBOSE
			printf("beaconSpam(): %s (%d)\n", currentSsid, currentSsidLen);
		#endif

		if (++line >= SSID_COUNT) {
			line = 0;
		}
	}
	free(seqnum);
}

char *generate_random_ssid() {
	// Generate a random string between SSID_LEN_MIN and SSID_LEN_MAX
	char *retVal;
	// First, how long will it be?
	int len = rand() % (SSID_LEN_MAX - SSID_LEN_MIN + 1);
	len += SSID_LEN_MIN;
	retVal = malloc(sizeof(char) * (len + 1));
	if (retVal == NULL) {
		ESP_LOGE(BEACON_TAG, "Failed to allocate %d bytes to generate a new SSID. PANIC!", len + 1);
		return NULL;
	}
	// Generate len characters
	for (int i=0; i < len; ++i) {
		retVal[i] = ssid_chars[rand() % (strlen(ssid_chars) - 1)];
	}
	retVal[len] = '\0';
	return retVal;
}

char **generate_random_ssids() {
	// Generate SSID_COUNT random strings between SSID_LEN_MIN and SSID_LEN_MAX
	char **ret = malloc(sizeof(char*) * SSID_COUNT);
	if (ret == NULL) {
		ESP_LOGE(BEACON_TAG, "Failed to allocate %d strings for random SSIDs. PANIC!", SSID_COUNT);
		return NULL;
	}
	for (int i=0; i < SSID_COUNT; ++i) {
		// How long will this SSID be?
		int len = rand() % (SSID_LEN_MAX - SSID_LEN_MIN + 1);
		len += SSID_LEN_MIN;
		ret[i] = generate_random_ssid();
		if (ret[i] == NULL) {
			ESP_LOGE(BEACON_TAG, "generate_random_ssid() returned NULL. Panicking.");
			return NULL;
		}
	}
	return ret;
}

int beacon_stop() {
	if (beaconTask != NULL) {
		vTaskDelete(beaconTask);
		beaconTask = NULL;
	}

	/* Clean up generated SSIDs */
	/* For AP free the individual strings */
	if (attackType == ATTACK_BEACON_AP) {
		for (int i = 0; i < SSID_COUNT; ++i) {
			free(attack_ssids[i]);
		}
	}
	if (attackType == ATTACK_BEACON_INFINITE && currentSsid != NULL) {
		free(currentSsid);
	} else if (attackType == ATTACK_BEACON_RANDOM || attackType == ATTACK_BEACON_AP) {
		free(attack_ssids);
	}
	attackType = ATTACK_BEACON_NONE;

    return ESP_OK;
}

int beacon_start(beacon_attack_t type, int ssidCount) {
	if (SSID_COUNT == 0) {
		SSID_COUNT = DEFAULT_SSID_COUNT;
	}
    /* Stop an existing beacon attack if one exists */
    if (attackType != ATTACK_BEACON_NONE) {
        beacon_stop();
    }
	// And initialise the random number generator
	// It'll happen more than once here, but that's OK
	srand(time(NULL));

    attackType = type;

	// Prepare the appropriate beacon array
	if (attackType == ATTACK_BEACON_RICKROLL) {
		SSID_COUNT = RICK_SSID_COUNT;
		attack_ssids = rick_ssids;
		#ifdef CONFIG_FLIPPER
			printf("RickRoll: %d SSIDs\n", SSID_COUNT);
		#else
			ESP_LOGI(BEACON_TAG, "Starting RickRoll: %d SSIDs", SSID_COUNT);
		#endif
	} else if (attackType == ATTACK_BEACON_RANDOM) {
		SSID_COUNT = (ssidCount>0)?ssidCount:DEFAULT_SSID_COUNT;
		attack_ssids = generate_random_ssids();
		#ifdef CONFIG_FLIPPER
			printf("%d random SSIDs\n", SSID_COUNT);
		#else
			ESP_LOGI(BEACON_TAG, "Starting %d random SSIDs", SSID_COUNT);
		#endif
	} else if (attackType == ATTACK_BEACON_USER) {
		SSID_COUNT = user_ssid_count;
		attack_ssids = user_ssids;
		#ifdef CONFIG_FLIPPER
			printf("%d User SSIDs\n", SSID_COUNT);
		#else
			ESP_LOGI(BEACON_TAG, "Starting %d SSIDs", SSID_COUNT);
		#endif
	} else if (attackType == ATTACK_BEACON_AP) {
		attack_ssids = apListToStrings(gravity_selected_aps, gravity_sel_ap_count);
		SSID_COUNT = gravity_sel_ap_count;
		#ifdef CONFIG_FLIPPER
			printf("%d scanned SSIDs\n", SSID_COUNT);
		#else
			ESP_LOGI(BEACON_TAG, "Starting %d scanned SSIDs", SSID_COUNT);
		#endif
	} else if (attackType == ATTACK_BEACON_INFINITE) {
		SSID_COUNT = 1;
		attack_ssids = malloc(sizeof(char *));
		if (attack_ssids == NULL) {
			ESP_LOGE(BEACON_TAG, "Failed to allocate memory to initialise infinite beacon spam attack. PANIC!");
			return ESP_ERR_NO_MEM;
		}
		#ifdef CONFIG_FLIPPER
			printf("Infinite SSIDs. Fun!\n");
		#else
			ESP_LOGI(BEACON_TAG, "Starting infinite SSIDs. Good luck!");
		#endif
	}
    
    xTaskCreate(&beaconSpam, "beaconSpam", 2048, NULL, 5, &beaconTask);
    

    return ESP_OK;
}