#include "beacon.h"
#include "common.h"
#include "esp_err.h"
#include "probe.h"

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
int BEACON_PRIVACY_OFFSET = 34; /* 0x31 set, 0x21 unset */

char **attack_ssids = NULL;

beacon_attack_t beaconAttackType = ATTACK_BEACON_NONE;
static PROBE_RESPONSE_AUTH_TYPE *beaconAuthTypes = NULL;
static int beaconAuthCount = 0;
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
	0x31, 0x04,		    				// 34-35: Capability info: 0x31 0x04 => Privacy bit set  0x21 0x04 => Privacy bit not set
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
		if (beaconAttackType != ATTACK_BEACON_INFINITE && ATTACK_MILLIS < CONFIG_MIN_ATTACK_MILLIS) {
			vTaskDelay(CONFIG_MIN_ATTACK_MILLIS  / portTICK_PERIOD_MS);
		} else {
			vTaskDelay(ATTACK_MILLIS / portTICK_PERIOD_MS);
		}

		// Pull the current SSID and SSID length into variables to more
		//   easily implement infinite beacon spam
		if (beaconAttackType == ATTACK_BEACON_INFINITE) {
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

		/* Work out how many iterations are needed to cover all specified auth types */
		int authTypeCount = 0;
		PROBE_RESPONSE_AUTH_TYPE thisAuthType = 0;
		if (beaconAuthCount == 1) {
			thisAuthType = beaconAuthTypes[0];
		} else {
			thisAuthType = beaconAuthTypes[line]; /* I think this works ... But I also thought the var was named i ...*/
		}
		PROBE_RESPONSE_AUTH_TYPE *selectedAuthTypes = unpackAuthType(thisAuthType, &authTypeCount);

		for (int authLoop = 0; authLoop < authTypeCount; ++authLoop) {
			/* Set privacy bit as appropriate */
			switch (selectedAuthTypes[authLoop]) {
				case AUTH_TYPE_WEP:
				case AUTH_TYPE_WPA:
					beacon_send[BEACON_PRIVACY_OFFSET] = 0x31;
					break;
				case AUTH_TYPE_NONE:
				default:
					beacon_send[BEACON_PRIVACY_OFFSET] = 0x21;
					break;
			}

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
		}
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
			if (beaconAttackType == ATTACK_BEACON_INFINITE && currentSsid != NULL) {
				free(currentSsid);
			} else if (beaconAttackType == ATTACK_BEACON_RANDOM || beaconAttackType == ATTACK_BEACON_AP) {
				for (int j = 0; j < SSID_COUNT; ++j) {
					free(attack_ssids[j]);
				}
				free(attack_ssids);
			} else if (beaconAttackType == ATTACK_BEACON_AP) {
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
	if (beaconAttackType == ATTACK_BEACON_AP) {
		for (int i = 0; i < SSID_COUNT; ++i) {
			free(attack_ssids[i]);
		}
	}
	if (beaconAttackType == ATTACK_BEACON_INFINITE) {
		if (currentSsid != NULL) {
			free(currentSsid);
		}
// Maybe		free(attack_ssids);
	} else if (beaconAttackType == ATTACK_BEACON_RANDOM) {
		/* Free attack_ssids[i] */
		for (int i = 0; i < SSID_COUNT; ++i) {
			free(attack_ssids[i]);
		}
		free(attack_ssids);
	} else if (beaconAttackType == ATTACK_BEACON_AP) {
		free(attack_ssids);
	}
	beaconAttackType = ATTACK_BEACON_NONE;

    return ESP_OK;
}

/* Start a beacon spam attack
   Start an attack of the specified attack type
   The optional parameter ssidCount can be used for the Random Beacon attack,
      where it specifies the number of random SSIDs to generate. Where defaults
	  are desired specify 0.
   authentication is a PROBE_RESPONSE_AUTH_TYPE array. This array must have either:
      * A single element containing a PROBE_RESPONSE_AUTH_TYPE value.
	  	This value is used to determine authentication type(s) advertised in all beacons.
		NOTE that a single value can be provided like so:
				PROBE_RESPONSE_AUTH_TYPE authType = AUTH_TYPE_NONE | AUTH_TYPE_WPA;
				beacon_start(type, &authType, 1, 0);
	  * Exactly the same number of elements as there are SSIDs being broadcast.
	  	Element i of authentication provides the authType for SSID i.
		All attack modes, excluding infinite, support this model.
	  * Note: Because PROBE_RESPONSE_AUTH_TYPE is a bitwise enum, an SSID may
		specify both open and secured authentication types. If this occurs
		Gravity will send two beacons, one advertising privacy and one without.
*/
esp_err_t beacon_start(beacon_attack_t type, int authentication[], int authenticationCount, int ssidCount) {
    /* Stop an existing beacon attack if one exists */
    if (beaconAttackType != ATTACK_BEACON_NONE) {
        beacon_stop();
    }
	// And initialise the random number generator
	// It'll happen more than once here, but that's OK
	srand(time(NULL));

    beaconAttackType = type;

	// Prepare the appropriate beacon array
	if (beaconAttackType == ATTACK_BEACON_RICKROLL) {
		SSID_COUNT = RICK_SSID_COUNT;
		attack_ssids = rick_ssids;
		#ifdef CONFIG_FLIPPER
			printf("RickRoll: %d SSIDs\n", SSID_COUNT);
		#else
			ESP_LOGI(BEACON_TAG, "Starting RickRoll: %d SSIDs", SSID_COUNT);
		#endif
	} else if (beaconAttackType == ATTACK_BEACON_RANDOM) {
		SSID_COUNT = (ssidCount>0)?ssidCount:DEFAULT_SSID_COUNT;
		attack_ssids = generateRandomSSids();
		#ifdef CONFIG_FLIPPER
			printf("%d random SSIDs\n", SSID_COUNT);
		#else
			ESP_LOGI(BEACON_TAG, "Starting %d random SSIDs", SSID_COUNT);
		#endif
	} else if (beaconAttackType == ATTACK_BEACON_USER) {
		SSID_COUNT = user_ssid_count;
		attack_ssids = user_ssids;
		#ifdef CONFIG_FLIPPER
			printf("%d User SSIDs\n", SSID_COUNT);
		#else
			ESP_LOGI(BEACON_TAG, "Starting %d SSIDs", SSID_COUNT);
		#endif
	} else if (beaconAttackType == ATTACK_BEACON_AP) {
		attack_ssids = apListToStrings(gravity_selected_aps, gravity_sel_ap_count);
		SSID_COUNT = gravity_sel_ap_count;
		#ifdef CONFIG_FLIPPER
			printf("%d scanned SSIDs\n", SSID_COUNT);
		#else
			ESP_LOGI(BEACON_TAG, "Starting %d scanned SSIDs", SSID_COUNT);
		#endif
	} else if (beaconAttackType == ATTACK_BEACON_INFINITE) {
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

	/* Validate authentication[] */
	if (authenticationCount != SSID_COUNT && authenticationCount != 1) {
		#ifdef CONFIG_FLIPPER
			printf("ERROR: SSID Count (%d) does not match authType Count (%d)\n", SSID_COUNT, authenticationCount);
		#else
			ESP_LOGE(BEACON_TAG, "SSID Count (%d) does not match authentication type count (%d)", SSID_COUNT, authenticationCount);
		#endif
		return ESP_ERR_INVALID_ARG;
	}
	for (int i = 0; i < authenticationCount; ++i) {
		/* Validate authentication[i] */
		if ((authentication[i] & AUTH_TYPE_NONE) != AUTH_TYPE_NONE &&
				(authentication[i] & AUTH_TYPE_WPA) != AUTH_TYPE_WPA) {
			#ifdef CONFIG_FLIPPER
				printf("ERROR: Invalid authType \"%d\"\n", authentication[i]);
			#else
				ESP_LOGE(BEACON_TAG, "Invalid authentication type provided in element %d: %d", i, authentication[i]);
			#endif
			return ESP_ERR_INVALID_ARG;
		}
	}

	/* Store the new authentication types */
	if (beaconAuthTypes != NULL) {
		free(beaconAuthTypes);
	}
	beaconAuthCount = authenticationCount;
	beaconAuthTypes = malloc(sizeof(PROBE_RESPONSE_AUTH_TYPE) * beaconAuthCount);
	if (beaconAuthTypes == NULL) {
		#ifdef CONFIG_FLIPPER
			printf("Unable to allocate memory for authTypes\n");
		#else
			ESP_LOGE(BEACON_TAG, "Unable to allocate memory for authentication types");
		#endif
		return ESP_ERR_NO_MEM;
	}
	for (int i = 0; i < beaconAuthCount; ++i) {
		beaconAuthTypes[i] = authentication[i];
		#ifdef CONFIG_DEBUG
			char authTypeStr[45];
			#ifdef CONFIG_FLIPPER
				if (authTypeToString(beaconAuthTypes[i], authTypeStr, true) != ESP_OK) {
					sprintf(authTypeStr, "Unknown (%d)", beaconAuthTypes[i]);
				}
				printf("Adding auth type: %s\n", authTypeStr);
			#else
				if (authTypeToString(beaconAuthTypes[i], authTypeStr, false) != ESP_OK) {
					sprintf(authTypeStr, "Unknown (%d)", beaconAuthTypes[i]);
				}
				ESP_LOGI(BEACON_TAG, "Adding auth type: %s", authTypeStr);
			#endif
		#endif
	}

    xTaskCreate(&beaconSpam, "beaconSpam", 2048, NULL, 5, &beaconTask);

    return ESP_OK;
}

/* Display the SSIDs that are currently being targeted by beacon (attack_ssids) */
/* SSIDs are prefixed by prefix (e.g. " >  ")
   Auth Types, where included, are specified after SSID names, for example:
    >  Whymper                          : AUTH_TYPE_OPEN
   Thoughts on options/config as I go:
	- Use %32s as a format spec for SSID in console mode
	- Flipper mode go ssid\n\t\tauthType
   Format strings: flipper, auth: %s%s\n\t\t%s\n, pre, ssid, authType
	  			   flipper, no auth: %s%s\n, pre, ssid
				   console, auth: %s%32s :  %s, pre, ssid, authType
				   console, no auth: %s%s, pre, ssid
*/
esp_err_t displayBeaconSsids(char *prefix, bool includeAuthTypes) {
	char *pre = prefix;
	if (pre == NULL) {
		pre = "";
	}
	for (int i = 0; i < SSID_COUNT; ++i) {
		#ifdef CONFIG_FLIPPER
			printf("%s%s\n", pre, attack_ssids[i]);
		#else
			ESP_LOGI(BEACON_TAG, "%s%s", pre, attack_ssids[i]);
		#endif

		if (includeAuthTypes) {
			char thisAuthType[45] = "";
			
			#ifdef CONFIG_FLIPPER
				if (authTypeToString(beaconAuthTypes[i], thisAuthType, true) != ESP_OK) {
					// TODO
				}
				printf("%s%s\n\t\t%s\n", pre, attack_ssids[i], thisAuthType);
			#else
				if (authTypeToString(beaconAuthTypes[i], thisAuthType, false) != ESP_OK) {
					// TODO
				}
				ESP_LOGI(BEACON_TAG, "%s%32s :  %s", pre, attack_ssids[i], thisAuthType);
			#endif
		} else {
			#ifdef CONFIG_FLIPPER
				printf("%s%s\n", pre, attack_ssids[i]);
			#else
				ESP_LOGI(BEACON_TAG, "%s%s", pre, attack_ssids[i]);
			#endif
		}
	}
	return ESP_OK;
}

esp_err_t displayBeaconSsidsWithoutAuth(char *prefix) {
	return displayBeaconSsids(prefix, false);
}

esp_err_t displayBeaconSsidsWithAuth(char *prefix) {
	return displayBeaconSsids(prefix, true);
}

/* Display information about the current beacon attack mode */
esp_err_t displayBeaconMode() {
	esp_err_t err = ESP_OK;
	switch (beaconAttackType) {
		case ATTACK_BEACON_AP:
			#ifdef CONFIG_FLIPPER
				printf("Beacon Mode is Running selectedAPs\n");
			#else
				ESP_LOGI(BEACON_TAG, "Beacon Mode is Currently Running selectedAPs");
			#endif
			break;
		case ATTACK_BEACON_INFINITE:
			#ifdef CONFIG_FLIPPER
				printf("Beacon Mode is Running Infinite (non-stop stream of unique Beacons)\n");
			#else
				ESP_LOGI(BEACON_TAG, "Beacon Mode is Currently Running Infinite\nThis sends an unending stream of unique Beacon announcements");
			#endif
			break;
		case ATTACK_BEACON_RANDOM:
			#ifdef CONFIG_FLIPPER
				printf("Beacon Mode is Running: %d random SSIDs\n", SSID_COUNT);
			#else
				ESP_LOGI(BEACON_TAG, "Beacon Mode is Currently Running: Beacons for %d randomly-named SSIDs being transmitted", SSID_COUNT);
			#endif
			break;
		case ATTACK_BEACON_NONE:
			#ifdef CONFIG_FLIPPER
				printf("Beacon Mode Disabled\n");
			#else
				ESP_LOGI(BEACON_TAG, "Beacon Mode is Currently Disabled");
			#endif
			break;
		case ATTACK_BEACON_RICKROLL:
			#ifdef CONFIG_FLIPPER
				printf("Beacon Mode is Running RickRoll\n");
			#else
				ESP_LOGI(BEACON_TAG, "Beacon Mode is Currently Running RickRoll");
			#endif
			break;
		case ATTACK_BEACON_USER:
			#ifdef CONFIG_FLIPPER
				printf("Beacon Mode is Running: %d user-specified SSIDs\n", SSID_COUNT);
			#else
				ESP_LOGI(BEACON_TAG, "Beacon Mode is Current Running: Beacons for %d user-specified SSIDs being transmitted", SSID_COUNT);
			#endif
			break;
		default:
			#ifdef CONFIG_FLIPPER
				printf("Invalid Beacon Mode \"%d\"\n", beaconAttackType);
			#else
				ESP_LOGE(BEACON_TAG, "Invalid Beacon Mode \"%d\"", beaconAttackType);
			#endif
			err = ESP_ERR_INVALID_ARG;
			break;
	}
	return err;
}

/* Display the current status of the beacon attack */
esp_err_t beacon_status() {
	esp_err_t err = ESP_OK;

	#ifdef CONFIG_FLIPPER
		printf("Beacon Mode: %s\n", (attack_status[ATTACK_BEACON])?"ON":"OFF");
	#else
		ESP_LOGI(BEACON_TAG, "Beacon Mode: %sRunning", (attack_status[ATTACK_BEACON])?"Not ":"");
	#endif

	/* Display additional info if Beacon is running */
	if (attack_status[ATTACK_BEACON]) {
		err |= displayBeaconMode();
	}

	/* Display info on configured auth types */
	if (beaconAuthCount == 1) {
		char authType[45];
		#ifdef CONFIG_FLIPPER
			authTypeToString(beaconAuthTypes[0], authType, true);
			printf("Auth Type: %s\n", authType);
		#else
			authTypeToString(beaconAuthTypes[0], authType, false);
			ESP_LOGI(BEACON_TAG, "Authentication Type :  %s", authType);
		#endif
	} else {
		/* SSID-specific auth counts must be specified */
		#ifdef CONFIG_FLIPPER
			printf("Auth Type: VARIOUS\n");
		#else
			ESP_LOGI(BEACON_TAG, "Authentication Type :  VARIOUS");
		#endif
	}

	/* Only NOW, that we have both SSID and auth info, can we display the SSIDs (with authType alongside if needed) */
	if (beaconAuthCount == 1) {
		err |= displayBeaconSsidsWithoutAuth(" >  ");
	} else {
		err |= displayBeaconSsidsWithAuth(" >  ");
	}

	return err;
}