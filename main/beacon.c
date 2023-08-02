#include "beacon.h"

int DEFAULT_SSID_COUNT = 20;
int SSID_LEN_MIN = 8;
int SSID_LEN_MAX = 32;
int RICK_SSID_COUNT = 8;
int BEACON_SSID_OFFSET = 38;
int BEACON_PACKET_LEN = 57;
int BEACON_SRCADDR_OFFSET = 10;
int BEACON_DESTADDR_OFFSET = 4;
int BEACON_BSSID_OFFSET = 16;
int BEACON_SEQNUM_OFFSET = 22;


char **attack_ssids = NULL;

/*
 * This is the (currently unofficial) 802.11 raw frame TX API,
 * defined in esp32-wifi-lib's libnet80211.a/ieee80211_output.o
 *
 * This declaration is all you need for using esp_wifi_80211_tx in your own application.
 */
esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void *buffer, int len, bool en_sys_seq);

static beacon_attack_t attackType = ATTACK_BEACON_NONE;
static int SSID_COUNT;

static TaskHandle_t beaconTask = NULL;

/* If true, scrambleWords generates random characters rather than random words */
bool scrambledWords = false;
#ifdef CONFIG_DEFAULT_SCRAMBLE_WORDS
    scrambleWords = true;
#endif

uint8_t beacon_raw[] = {
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

/* Callback for the beacon spam attack. This will pause 50ms if ATTACK_MILLIS is < 50ms >*/
void beaconSpam(void *pvParameter) {
	esp_err_t err;
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
		/* Enforce the use of CONFIG_MIN_ATTACK_MILLIS if not Infinite Beacon spam and
		   ATTACK_MILLIS < CONFIG_MIN_ATTACK_MILLIS
		*/
		if (attackType != ATTACK_BEACON_INFINITE && ATTACK_MILLIS < CONFIG_MIN_ATTACK_MILLIS) {
			vTaskDelay(CONFIG_MIN_ATTACK_MILLIS  / portTICK_PERIOD_MS);
		} else {
			vTaskDelay(ATTACK_MILLIS / portTICK_PERIOD_MS);
		}

		// Pull the current SSID and SSID length into variables to more
		//   easily implement infinite beacon spam
		if (attackType == ATTACK_BEACON_INFINITE) {
			if (currentSsid != NULL) {
				free(currentSsid);
			}
			currentSsidLen = random() % MAX_SSID_LEN;
			currentSsid = malloc(sizeof(char) * (currentSsidLen + 1));
			if (currentSsid == NULL) {
				#ifdef CONFIG_FLIPPER
					printf("Out of memory for random SSID\n");
				#else
					ESP_LOGE(BEACON_TAG, "Unable to allocate memory for a random %s SSID", (scrambledWords)?"chars":"words");
				#endif
				return;
			}
			if (scrambledWords) {
				err = randomSsidWithChars(currentSsid, currentSsidLen);
			} else {
				err = randomSsidWithWords(currentSsid, currentSsidLen);
			}
			if (err != ESP_OK) {
				#ifdef CONFIG_FLIPPER
					printf("SSID Generation failed:\n%25s\n", esp_err_to_name(err));
				#else
					ESP_LOGE(BEACON_TAG, "%s SSID Generation failed: %s", (scrambledWords)?"Character":"Word", esp_err_to_name(err));
				#endif
				free(currentSsid);
				return;
			}
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
		beacon_send[BEACON_SRCADDR_OFFSET + 5] = line;
		beacon_send[BEACON_BSSID_OFFSET + 5] = line;

		// Update sequence number
		beacon_send[BEACON_SEQNUM_OFFSET] = (seqnum[line] & 0x0f) << 4;
		beacon_send[BEACON_SEQNUM_OFFSET + 1] = (seqnum[line] & 0xff0) >> 4;
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

/* Alternative SSID generation, using 1000 dictionary words */
char *getRandomWord() {
    #include "words.c"

    int index = rand() % gravityWordCount;
    return gravityWordList[index];
}

/* Generate a random SSID of the specified length.
   This function uses the included dictionary file to generate a sequence of
   words separated by hyphen, to make up the length required.
*/
esp_err_t randomSsidWithWords(char *ssid, int len) {
    int currentLen = 0;
    memset(ssid, 0, (len + 1)); /* Including trailing NULL */
    while (currentLen < len) {
        char *word = getRandomWord();
        if (currentLen + strlen(word) + 1 < len) {
            if (currentLen > 0) {
                ssid[currentLen] = (uint8_t)'-';
                ++currentLen;
            }
            memcpy(&ssid[currentLen], (uint8_t *)word, strlen(word));
            currentLen += strlen(word);
			ssid[currentLen] = '\0'; /* Temporarily append NULL for debug outputs */
        } else {
            if (currentLen > 0) {
                ssid[currentLen] = (uint8_t)'-';
                ++currentLen;
            }
            /* How much space do we have left? */
            int remaining = len - currentLen;
            memcpy(&ssid[currentLen], (uint8_t *)word, remaining);
            currentLen += remaining;
        }
    }
    ssid[currentLen] = '\0';

    return ESP_OK;
}



esp_err_t randomSsidWithChars(char *newSsid, int len) {
	// Generate a random string of length len
	// Generate len characters
	for (int i=0; i < len; ++i) {
		newSsid[i] = ssid_chars[rand() % (strlen(ssid_chars) - 1)];
	}
	newSsid[len] = '\0';
	return ESP_OK;
}

/* Generates SSID_COUNT random SSID names.
   Each SSID name will be between SSID_LEN_MIN and SSID_LEN_MAX
*/
char **generateRandomSSids() {
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
		ret[i] = malloc(sizeof(char) * (len + 1));
		if (ret[i] == NULL) {
			#ifdef CONFIG_FLIPPER
				printf("Unable to allocate memory for SSID %d\n", i);
			#else
				ESP_LOGE(TAG, "Unable to allocate memory to store random SSID %d.", i);
			#endif
			/* free ret[j] and ret */
			for (int j = 0; j < i; ++j) {
				free(ret[i]);
			}
			free(ret);
			return NULL;
		}

		/* Generate a word or letter sequence */
		char ssidType[6] = "";
		if (scrambledWords) {
			esp_err_t err = randomSsidWithChars(ret[i], len);
			if (err != ESP_OK) {
				#ifdef CONFIG_FLIPPER
					printf("randomSsidWithChars: %s\n", esp_err_to_name(err));
				#else
					ESP_LOGE(TAG, "randomSsidsWithChars() failed: %s", esp_err_to_name(err));
				#endif
				/* Free allocated memory */
				for (int j = 0; j < i; ++j) {
					free(ret[j]);
				}
				free(ret);
				return NULL;
			}
			strcpy(ssidType, "Chars");
		} else {
			esp_err_t err = randomSsidWithWords(ret[i], len);
			if (err != ESP_OK) {
				#ifdef CONFIG_FLIPPER
					printf("randomSsidWithWords: %s\n", esp_err_to_name(err));
				#else
					ESP_LOGE(TAG, "randomSsidWithWords() failed: %s", esp_err_to_name(err));
				#endif
				/* Free allocated memory */
				for (int j = 0; j < i; ++j) {
					free(ret[j]);
				}
				free(ret);
				return NULL;
			}
			strcpy(ssidType, "Words");
		}
		
		if (len == 0) {
			#ifdef CONFIG_FLIPPER
				printf("randomSsidWith%s() failed!\n", ssidType);
			#else
				ESP_LOGE(BEACON_TAG, "randomSsidWith%s() Failed", ssidType);
			#endif

			/* Free memory if required */
			if (attackType == ATTACK_BEACON_INFINITE && currentSsid != NULL) {
				free(currentSsid);
			} else if (attackType == ATTACK_BEACON_RANDOM || attackType == ATTACK_BEACON_AP) {
				for (int j = 0; j < SSID_COUNT; ++j) {
					free(attack_ssids[j]);
				}
				free(attack_ssids);
			} else if (attackType == ATTACK_BEACON_AP) {
				for (int j = 0; j < SSID_COUNT; ++j) {
					free(attack_ssids[i]);
				}
			}

			return NULL;
		}
	}
	return ret;
}

esp_err_t beacon_stop() {
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
	} else if (attackType == ATTACK_BEACON_RANDOM) {
		/* Free attack_ssids[i] */
		for (int i = 0; i < SSID_COUNT; ++i) {
			free(attack_ssids[i]);
		}
		free(attack_ssids);
	} else if (attackType == ATTACK_BEACON_AP) {
		free(attack_ssids);
	}
	attackType = ATTACK_BEACON_NONE;

    return ESP_OK;
}

esp_err_t beacon_start(beacon_attack_t type, int ssidCount) {
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
		attack_ssids = generateRandomSSids();
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