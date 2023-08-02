#include "common.h"
#include "esp_err.h"

const char *TAG = "GRAVITY";
const uint8_t BROADCAST[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
const char *AUTH_TYPE_NAMES[] = {"none", "AUTH_TYPE_OPEN", "AUTH_TYPE_WEP", "none", "AUTH_TYPE_WPA"};

int max(int one, int two) {
    if (one >= two) {
        return one;
    }
    return two;
}

bool arrayContainsString(char **arr, int arrCnt, char *str) {
    int i;
    for (i=0; i < arrCnt && strcmp(arr[i], str); ++i) { }
    return (i != arrCnt);
}

/* Convert the specified string to a byte array
   bMac must be a pointer to 6 bytes of allocated memory */
int mac_string_to_bytes(char *strMac, uint8_t *bMac) {
    int values[6];

    if (6 == sscanf(strMac, "%x:%x:%x:%x:%x:%x%*c", &values[0],
        &values[1], &values[2], &values[3], &values[4], &values[5])) {
        // Now convert to uint8_t
        for (int i = 0; i < 6; ++i) {
            bMac[i] = (uint8_t)values[i];
        }
    } else {
        ESP_LOGE(TAG, "Invalid MAC specified. Format: 12:34:56:78:90:AB");
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

/* Convert the specified byte array to a string representing
   a MAC address. strMac must be a pointer initialised to
   contain at least 18 bytes (MAC + '\0') */
int mac_bytes_to_string(uint8_t *bMac, char *strMac) {
    sprintf(strMac, "%02X:%02X:%02X:%02X:%02X:%02X", bMac[0],
            bMac[1], bMac[2], bMac[3], bMac[4], bMac[5]);
    return ESP_OK;
}

/* Return the GravityCommand (typedef enum) associated with
   the specified string. If the string could not be converted
   GRAVITY_NONE is returned.
*/
GravityCommand gravityCommandFromString(char *input) {
    if (!strcasecmp(input, "beacon")) {
        return GRAVITY_BEACON;
    }
    if (!strcasecmp(input, "target-ssids")) {
        return GRAVITY_TARGET_SSIDS;
    }
    if (!strcasecmp(input, "probe")) {
        return GRAVITY_PROBE;
    }
    if (!strcasecmp(input, "fuzz")) {
        return GRAVITY_FUZZ;
    }
    if (!strcasecmp(input, "sniff")) {
        return GRAVITY_SNIFF;
    }
    if (!strcasecmp(input, "deauth")) {
        return GRAVITY_DEAUTH;
    }
    if (!strcasecmp(input, "mana")) {
        return GRAVITY_MANA;
    }
    if (!strcasecmp(input, "stalk")) {
        return GRAVITY_STALK;
    }
    if (!strcasecmp(input, "ap-dos")) {
        return GRAVITY_AP_DOS;
    }
    if (!strcasecmp(input, "ap-clone")) {
        return GRAVITY_AP_CLONE;
    }
    if (!strcasecmp(input, "scan")) {
        return GRAVITY_SCAN;
    }
    if (!strcasecmp(input, "hop")) {
        return GRAVITY_HOP;
    }
    if (!strcasecmp(input, "set")) {
        return GRAVITY_SET;
    }
    if (!strcasecmp(input, "get")) {
        return GRAVITY_GET;
    }
    if (!strcasecmp(input, "view")) {
        return GRAVITY_VIEW;
    }
    if (!strcasecmp(input, "select")) {
        return GRAVITY_SELECT;
    }
    if (!strcasecmp(input, "selected")) {
        return GRAVITY_SELECTED;
    }
    if (!strcasecmp(input, "clear")) {
        return GRAVITY_CLEAR;
    }
    if (!strcasecmp(input, "handshake")) {
        return GRAVITY_HANDSHAKE;
    }
    if (!strcasecmp(input, "commands")) {
        return GRAVITY_COMMANDS;
    }
    if (!strcasecmp(input, "info")) {
        return GRAVITY_INFO;
    }
    return GRAVITY_NONE;
}

/* Check whether the specified ScanResultSTA list contains the specified ScanResultSTA */
bool staResultListContainsSTA(ScanResultSTA **list, int listLen, ScanResultSTA *sta) {
	int i;
	for (i = 0; i < listLen && memcmp(list[i]->mac, sta->mac, 6); ++i) { }
	return (i < listLen);
}

/* Check whether the specified ScanResultAP list contains the specified ScanResultAP */
bool apResultListContainsAP(ScanResultAP **list, int listLen, ScanResultAP *ap) {
    int i;
    for (i = 0; i < listLen && memcmp(list[i]->espRecord.bssid, ap->espRecord.bssid, 6); ++i) { }
    return (i < listLen);
}

/* The reverse side of the below function - collate all APs that are associated
   with the selected stations.
   Caller must free the result
*/
ScanResultAP **collateAPsOfSelectedSTAs(int *apCount) {
    /* Loop through selectedSTA, for each STA:
       - If it has an AP
       - And the AP isn't in the result set
       - Add the AP to the result set
    */
    if (gravity_sel_sta_count == 0) {
        #ifdef CONFIG_FLIPPER
            printf("No STAs selected\n");
        #else
            ESP_LOGW(TAG, "No selected STAs to retrieve APs from");
        #endif
        *apCount = 0;
        return NULL;
    }
    /* Use STA count as an upper limit on the AP count */
    ScanResultAP **resPassOne = malloc(sizeof(ScanResultAP *) * gravity_sel_sta_count);
    int resCount = 0;
    if (resPassOne == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for collated APs");
        return NULL;
    }
    for (int i = 0; i < gravity_sel_sta_count; ++i) {
        if (gravity_selected_stas[i]->ap != NULL && !apResultListContainsAP(resPassOne, resCount, gravity_selected_stas[i]->ap)) {
            /* Add the current STA to our result set */
            resPassOne[resCount++] = gravity_selected_stas[i]->ap;
        }
    }
    *apCount = resCount;
    if (resCount < gravity_sel_sta_count) {
        /* Shrink resPassOne down to size */
        ScanResultAP **resPassTwo = malloc(sizeof(ScanResultAP *) * resCount);
        if (resPassTwo == NULL) {
            #ifndef CONFIG_FLIPPER
                ESP_LOGW(TAG, "Unable to allocate memory to shrink AP list. That's OK.");
            #endif
        } else {
            /* Copy elements from resPassOne to resPassTwo */
            for (int i = 0; i < resCount; ++i) {
                resPassTwo[i] = resPassOne[i];
            }
            free(resPassOne);
            resPassOne = resPassTwo;
        }
    }
    return resPassOne;
}

/* Retain current MAC */
static uint8_t current_mac[6] = {NULL, NULL, NULL, NULL, NULL, NULL};

uint8_t *gravity_get_mac() {
    if (current_mac[0] == NULL && current_mac[1] == NULL && current_mac[2] == NULL && current_mac[3] == NULL && current_mac[4] == NULL && current_mac[5] == NULL) {
        if (esp_wifi_get_mac(WIFI_IF_AP, current_mac) != ESP_OK) {
            #ifdef CONFIG_FLIPPER
                printf("Failed to get MAC\n");
            #else
                ESP_LOGE(TAG, "Failed to get MAC!");
            #endif
            return NULL;
        }
    }
    return current_mac;
}

esp_err_t gravity_set_mac(uint8_t *newMac) {
    if (esp_wifi_set_mac(ESP_IF_WIFI_AP, newMac) != ESP_OK) {
        #ifdef CONFIG_FLIPPER
            printf("Failed to set MAC\n");
        #else
            ESP_LOGE(TAG, "Failed to set MAC");
        #endif
        return ESP_ERR_WIFI_MAC;
    }
    #ifdef CONFIG_DEBUG
        #ifdef CONFIG_FLIPPER
            printf("Gravity MAC now: %02x:%02x:%02x:%02x:%02x:%02x\n", newMac[0], newMac[1], newMac[2], newMac[3], newMac[4], newMac[5]);
        #else
            ESP_LOGI(TAG, "Successfully updated Gravity MAC to %02x:%02x:%02x:%02x:%02x:%02x\n", newMac[0], newMac[1], newMac[2], newMac[3], newMac[4], newMac[5]);
        #endif
    #endif
    memcpy(current_mac, newMac, 6);
    return ESP_OK;
}

/* Generate a string representing the specified PROBE_RESPONSE_AUTH_TYPE
   PROBE_RESPONSE_AUTH_TYPE is a binary enum:
   * Multiple auth types can be specified with bitwise 'or'
      doSomething(AUTH_TYPE_WEP | AUTH_TYPE_WPA)
   * Multiple auth types can be read with bitwise 'and'
      if (authType & AUTH_TYPE_WPA) // Is WPA one of the specified types?
   theString is a char array initialised with enough space to hold the spacified authType
    * That's 45 bytes, including the NULL terminator 
*/
esp_err_t authTypeToString(PROBE_RESPONSE_AUTH_TYPE authType, char theString[]) {
    esp_err_t err = ESP_OK;
    /* Build the string in my own variable and copy it into theString - otherwise
       I'll need to NULL out theString in order for strcat to work, and I can't
       do that without knowing its length
    */
    char retVal[45];
    memset(retVal, '\0', 45); /* Fill retVal with NULL so I can use string operations */

    if ((authType & AUTH_TYPE_NONE) == AUTH_TYPE_NONE) {
        strcat(retVal, AUTH_TYPE_NAMES[AUTH_TYPE_NONE]);
    }
    if ((authType & AUTH_TYPE_WEP) == AUTH_TYPE_WEP) {
        if (strlen(retVal) > 0) {
            strcat(retVal, ", ");
        }
        strcat(retVal, AUTH_TYPE_NAMES[AUTH_TYPE_WEP]);
    }
    if ((authType & AUTH_TYPE_WPA) == AUTH_TYPE_WPA) {
        if (strlen(retVal) > 0) {
            strcat(retVal, ", ");
        }
        strcat(retVal, AUTH_TYPE_NAMES[AUTH_TYPE_WPA]);
    }

    strcpy(theString, retVal);

    return err;
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
        #ifdef CONFIG_FLIPPER
            printf("No selected APs\n");
        #else
            ESP_LOGW(TAG, "No selected APs to obtain STAs from");
        #endif
        *staCount = 0;
		return NULL;
	}

	int resCount = 0;
	ScanResultSTA **resPassOne = malloc(sizeof(ScanResultSTA *) * resUpperBound);
	if (resPassOne == NULL) {
		ESP_LOGE(TAG, "Unable to allocate temporary storage for extracting clients of selected APs");
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
			ESP_LOGW(TAG, "Unable to allocate memory to reduce space occupied by Beacon STA list. Sorry.");
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

/* Extract and return the individual components of the specified authType */
/* outputCount should be a pointer to an integer (&myInteger) where the number
   of PROBE_RESPONSE_AUTH_TYPEs extracted from input is recorded */
PROBE_RESPONSE_AUTH_TYPE *unpackAuthType(PROBE_RESPONSE_AUTH_TYPE input, int *outputCount) {
    int count = 0;
    if ((input & AUTH_TYPE_NONE) == AUTH_TYPE_NONE) {
        ++count;
    }
    if ((input & AUTH_TYPE_WEP) == AUTH_TYPE_WEP) {
        ++count;
    }
    if ((input & AUTH_TYPE_WPA) == AUTH_TYPE_WPA) {
        ++count;
    }
    if (count == 0) {
        return NULL;
    }

    /* Initialise the return value */
    PROBE_RESPONSE_AUTH_TYPE *retVal = malloc(sizeof(PROBE_RESPONSE_AUTH_TYPE) * count);
    if (retVal == NULL) {
        #ifdef CONFIG_FLIPPER
            printf("Unable to allocate memory to decompose authTypes\n");
        #else
            ESP_LOGE(TAG, "Unable to allocate memory to decompose an authType into its component PROBE_RESPONSE_AUTH_TYPEs.");
        #endif
        return NULL;
    }

    /* Populate the return value */
    count = 0;
    if ((input & AUTH_TYPE_NONE) == AUTH_TYPE_NONE) {
        retVal[count++] = AUTH_TYPE_NONE;
    }
    if ((input & AUTH_TYPE_WEP) == AUTH_TYPE_WEP) {
        retVal[count++] = AUTH_TYPE_WEP;
    }
    if ((input & AUTH_TYPE_WPA) == AUTH_TYPE_WPA) {
        retVal[count++] = AUTH_TYPE_WPA;
    }

    /* Return it */
    *outputCount = count;
    return retVal;
}


/* Extract and return SSIDs from the specified ScanResultAP array */
// TODO: Is that a memory leak in strcpy?
char **apListToStrings(ScanResultAP **aps, int apsCount) {
    if (apsCount == 0) {
        #ifdef CONFIG_FLIPPER
            printf("WARNING: No selected APs\n");
        #else
            ESP_LOGW(TAG, "No selected APs");
        #endif
        return NULL;
    }
	char **res = malloc(sizeof(char *) * apsCount);
	if (res == NULL) {
		ESP_LOGE(TAG, "Unable to allocate memory to extract AP names");
		return NULL;
	}

	for (int i = 0; i < apsCount; ++i) {
		res[i] = malloc(sizeof(char) * 33);
		if (res[i] == NULL) {
			ESP_LOGE(TAG, "Unable to allocate memory to hold AP %d", i);
			free(res);
			return NULL;
		}
		strcpy(res[i], (char *)aps[i]->espRecord.ssid);
	}
	return res;
}

esp_err_t convert_bytes_to_string(uint8_t *bIn, char *sOut, int maxLen) {
    memset(sOut, '\0', maxLen + 1);
    for (int i = 0; i < maxLen + 1 && bIn[i] != '\0'; ++i) {
        sOut[i] = bIn[i];
    }
    return ESP_OK;
}

/* Convert */
esp_err_t ssid_bytes_to_string(uint8_t *bSsid, char *ssid) {
    return convert_bytes_to_string(bSsid, ssid, 32);
}
