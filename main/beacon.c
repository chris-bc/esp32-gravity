#include "beacon.h"

/*
 * This is the (currently unofficial) 802.11 raw frame TX API,
 * defined in esp32-wifi-lib's libnet80211.a/ieee80211_output.o
 *
 * This declaration is all you need for using esp_wifi_80211_tx in your own application.
 */
esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void *buffer, int len, bool en_sys_seq);

static char **attack_ssids = NULL;
static char **user_ssids = NULL;
static int user_ssid_count = 0;

#define BEACON_SSID_OFFSET 38
#define SRCADDR_OFFSET 10
#define BSSID_OFFSET 16
#define SEQNUM_OFFSET 22

static beacon_attack_t attackType;
static int SSID_COUNT = DEFAULT_SSID_COUNT;

static TaskHandle_t task;

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

int countSsid() {
	return user_ssid_count;
}

char **lsSsid() {
	return user_ssids;
}

int addSsid(char *ssid) {
	#ifdef DEBUG
		printf("Commencing addSsid(\"%s\"). target-ssids contains %d values:\n", ssid, user_ssid_count);
		for (int i=0; i < user_ssid_count; ++i) {
			printf("    %d: \"%s\"\n", i, user_ssids[i]);
		}
	#endif
	char **newSsids = malloc(sizeof(char*) * (user_ssid_count + 1));
	if (newSsids == NULL) {
		ESP_LOGE("GRAVITY", "Insufficient memory to add new SSID");
		return ESP_ERR_NO_MEM;
	}
	for (int i=0; i < user_ssid_count; ++i) {
		newSsids[i] = user_ssids[i];
	}

	#ifdef DEBUG
		printf("After creating a larger array and copying across previous values the new array was allocated %d elements. Existing values are:\n", (user_ssid_count + 1));
		for (int i=0; i < user_ssid_count; ++i) {
			printf("    %d: \"%s\"\n", i, newSsids[i]);
		}
	#endif

	newSsids[user_ssid_count] = malloc(sizeof(char) * (strlen(ssid) + 1));
	if (newSsids[user_ssid_count] == NULL) {
		ESP_LOGE("GRAVITY", "Insufficient memory to add SSID \"%s\"", ssid);
		return ESP_ERR_NO_MEM;
	}
	strcpy(newSsids[user_ssid_count], ssid);
	++user_ssid_count;

	#ifdef DEBUG
		printf("After adding the final item and incrementing length counter newSsids has %d elements. The final item is \"%s\"\n", user_ssid_count, newSsids[user_ssid_count - 1]);
		printf("Pointers are:\tuser_ssids: %p\tnewSsids: %p\n", user_ssids, newSsids);
	#endif
	free(user_ssids);
	user_ssids = newSsids;
	#ifdef DEBUG
		printf("After freeing user_ssids and setting newSsids pointers are:\tuser_ssids: %p\tnewSsids: %p\n", user_ssids, newSsids);
	#endif

	return ESP_OK;
}

int rmSsid(char *ssid) {
	int idx;

	// Get index of ssid if it exists
	for (idx = 0; (idx < user_ssid_count && strcasecmp(ssid, user_ssids[idx])); ++idx) {}
	if (idx == user_ssid_count) {
		ESP_LOGW("GRAVITY", "Asked to remove SSID \'%s\', but could not find it in user_ssids", ssid);
		return ESP_ERR_INVALID_ARG;
	}

	char **newSsids = malloc(sizeof(char*) * (user_ssid_count - 1));
	if (newSsids == NULL) {
		ESP_LOGE("GRAVITY", "Unable to allocate memory to remove SSID \'%s\'", ssid);
		return ESP_ERR_NO_MEM;
	}

	// Copy shrunk array to newSsids
	for (int i = 0; i < user_ssid_count; ++i) {
		if (i < idx) {
			newSsids[i] = user_ssids[i];
		} else if (i > idx) {
			newSsids[i-1] = user_ssids[i];
		}
	}
	free(user_ssids[idx]);
	free(user_ssids);
	user_ssids = newSsids;
	--user_ssid_count;
	return ESP_OK;
}

void beaconSpam(void *pvParameter) {
	uint8_t line = 0;

	// Keep track of beacon sequence numbers on a per-songline-basis
	uint16_t *seqnum = malloc(sizeof(uint16_t) * SSID_COUNT);
	if (seqnum == NULL) {
		ESP_LOGE("GRAVITY", "Failed to allocate space to monitor sequence numbers. PANIC!");
		return;
	}
	for (int i=0; i<SSID_COUNT; ++i) {
		seqnum[i] = 0;
	}

	for (;;) {
		vTaskDelay(100 / SSID_COUNT / portTICK_PERIOD_MS);
		vTaskDelay(1);

		// Insert line of Rick Astley's "Never Gonna Give You Up" into beacon packet
		uint8_t beacon_rick[200];
		memcpy(beacon_rick, beacon_raw, BEACON_SSID_OFFSET - 1);
		beacon_rick[BEACON_SSID_OFFSET - 1] = strlen(attack_ssids[line]);
		memcpy(&beacon_rick[BEACON_SSID_OFFSET], attack_ssids[line], strlen(attack_ssids[line]));
		memcpy(&beacon_rick[BEACON_SSID_OFFSET + strlen(attack_ssids[line])], &beacon_raw[BEACON_SSID_OFFSET], sizeof(beacon_raw) - BEACON_SSID_OFFSET);

		// Last byte of source address / BSSID will be line number - emulate multiple APs broadcasting one song line each
		beacon_rick[SRCADDR_OFFSET + 5] = line;
		beacon_rick[BSSID_OFFSET + 5] = line;

		// Update sequence number
		beacon_rick[SEQNUM_OFFSET] = (seqnum[line] & 0x0f) << 4;
		beacon_rick[SEQNUM_OFFSET + 1] = (seqnum[line] & 0xff0) >> 4;
		seqnum[line]++;
		if (seqnum[line] > 0xfff)
			seqnum[line] = 0;

		esp_wifi_80211_tx(WIFI_IF_AP, beacon_rick, sizeof(beacon_raw) + strlen(attack_ssids[line]), false);

		if (++line >= SSID_COUNT)
			line = 0;
	}
	free(seqnum);
}

char **generate_random_ssids() {
	srand(time(NULL));
	// Generate SSID_COUNT random strings between SSID_LEN_MIN and SSID_LEN_MAX
	char **ret = malloc(sizeof(char*) * SSID_COUNT);
	if (ret == NULL) {
		ESP_LOGE("GRAVITY", "Failed to allocate %d strings for random SSIDs. PANIC!", SSID_COUNT);
		return NULL;
	}
	for (int i=0; i < SSID_COUNT; ++i) {
		// How long will this SSID be?
		int len = rand() % (SSID_LEN_MAX - SSID_LEN_MIN + 1);
		len += SSID_LEN_MIN;
		ret[i] = malloc(sizeof(char) * (len + 1));
		if (ret[i] == NULL) {
			ESP_LOGE("GRAVITY", "Failed to allocate %d bytes for SSID %d. PANIC!", (len + 1), i);
			return NULL;
		}
		// Generate len characters
		for (int j=0; j < len; ++j) {
			ret[i][j] = ssid_chars[rand() % (strlen(ssid_chars) - 1)];
		}
		ret[i][len] = '\0';
	}
	return ret;
}

int beacon_stop() {
	vTaskDelete(task);
	attackType = ATTACK_BEACON_NONE;
    return ESP_OK;
}

int beacon_start(beacon_attack_t type, int ssidCount) {
    /* Stop an existing beacon attack if one exists */
    if (attackType != ATTACK_BEACON_NONE) {
        beacon_stop();
    }
    attackType = type;

	// Prepare the appropriate beacon array
	if (attackType == ATTACK_BEACON_RICKROLL) {
		SSID_COUNT = RICK_SSID_COUNT;
		attack_ssids = rick_ssids;
		ESP_LOGI("GRAVITY", "Starting RickRoll: %d SSIDs", SSID_COUNT);
	} else if (attackType == ATTACK_BEACON_RANDOM) {
		SSID_COUNT = (ssidCount>0)?ssidCount:DEFAULT_SSID_COUNT;
		attack_ssids = generate_random_ssids();
		ESP_LOGI("GRAVITY", "Starting %d random SSIDs", SSID_COUNT);
	} else if (attackType == ATTACK_BEACON_USER) {
		SSID_COUNT = user_ssid_count;
		attack_ssids = user_ssids;
		ESP_LOGI("GRAVITY", "Starting %d SSIDs", SSID_COUNT);
	}
    
    xTaskCreate(&beaconSpam, "beaconSpam", 2048, NULL, 5, &task);
    

    return ESP_OK;
}