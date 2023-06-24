#include "beacon.h"

static char **attack_ssids = NULL;

#define BEACON_SSID_OFFSET 38
#define SRCADDR_OFFSET 10
#define BSSID_OFFSET 16
#define SEQNUM_OFFSET 22

static beacon_attack_t attackType;
static int SSID_COUNT = DEFAULT_SSID_COUNT;

static TaskHandle_t task;

int addSsid(char *ssid) {
	printf("Commencing addSsid(\"%s\"). target-ssids contains %d values:\n", ssid, user_ssid_count);
	for (int i=0; i < user_ssid_count; ++i) {
		printf("    %d: \"%s\"\n", i, user_ssids[i]);
	}
	char **newSsids = malloc(sizeof(char*) * (user_ssid_count + 1));
	if (newSsids == NULL) {
		ESP_LOGE("GRAVITY", "Insufficient memory to add new SSID");
		return ESP_ERR_NO_MEM;
	}
	for (int i=0; i < user_ssid_count; ++i) {
		newSsids[i] = user_ssids[i];
	}

	printf("After creating a larger array and copying across previous values the new array was allocated %d elements. Existing values are:\n", (user_ssid_count + 1));
	for (int i=0; i < user_ssid_count; ++i) {
		printf("    %d: \"%s\"\n", i, newSsids[i]);
	}

	newSsids[user_ssid_count] = malloc(sizeof(char) * (strlen(ssid) + 1));
	if (newSsids[user_ssid_count] == NULL) {
		ESP_LOGE("GRAVITY", "Insufficient memory to add SSID \"%s\"", ssid);
		return ESP_ERR_NO_MEM;
	}
	strcpy(newSsids[user_ssid_count], ssid);
	++user_ssid_count;

	printf("After adding the final item and incrementing length counter newSsids has %d elements. The final item is \"%s\"\n", user_ssid_count, newSsids[user_ssid_count - 1]);
	printf("Pointers are:\tuser_ssids: %p\tnewSsids: %p\n", user_ssids, newSsids);
	free(user_ssids);
	user_ssids = newSsids;
	printf("After freeing user_ssids and setting newSsids pointers are:\tuser_ssids: %p\tnewSsids: %p\n", user_ssids, newSsids);

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