/* Basic console example (esp_console_repl API)

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "gravity.h"

#include "common.h"
#include "dos.h"
#include "beacon.h"
#include "probe.h"
#include "scan.h"
#include "deauth.h"
#include "fuzz.h"
#include "sniff.h"
#include "mana.h"
#include "hop.h"
#include "bluetooth.h"

char **user_ssids = NULL;
char **gravityWordList = NULL;
int user_ssid_count = 0;
long ATTACK_MILLIS = CONFIG_DEFAULT_ATTACK_MILLIS;

bool *attack_status;
bool *hop_defaults;
int *hop_millis_defaults;

#define PROMPT_STR "gravity"

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

/* Functions to manage target-ssids */
int countSsid() {
	return user_ssid_count;
}

char **lsSsid() {
	return user_ssids;
}

int addSsid(char *ssid) {
	#ifdef CONFIG_DEBUG_VERBOSE
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

	#ifdef CONFIG_DEBUG_VERBOSE
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

	#ifdef CONFIG_DEBUG_VERBOSE
		printf("After adding the final item and incrementing length counter newSsids has %d elements. The final item is \"%s\"\n", user_ssid_count, newSsids[user_ssid_count - 1]);
		printf("Pointers are:\tuser_ssids: %p\tnewSsids: %p\n", user_ssids, newSsids);
	#endif
	free(user_ssids);
	user_ssids = newSsids;
	#ifdef CONFIG_DEBUG_VERBOSE
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

/* Run bluetooth test module */
int cmd_bluetooth(int argc, char **argv) {
    #if defined(CONFIG_IDF_TARGET_ESP32)
        testBT();
    #else
        #ifdef CONFIG_FLIPPER
            printf("Bluetooth unsupported in this build because you're using a S2 or C6\n");
        #else
            ESP_LOGW(TAG, "ESP32-Gravity has been built without Bluetooth support. Bluetooth is not supported on this chip.");
        #endif
    #endif
    return ESP_OK;
}

/* Display help information for the specified command.
   Permit the specification of multiple commands, skip any
   that are invalid.
   USage: info <command>+
*/
int cmd_info(int argc, char **argv) {
    if (argc == 1) {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_INFO);
        #else
            ESP_LOGE(TAG, "%s", USAGE_INFO);
        #endif
        return ESP_ERR_INVALID_ARG;
    }

    /* Loop through all arguments */
    for (int i = 1; i < argc; ++i) {
        GravityCommand command = gravityCommandFromString(argv[i]);
        #ifdef CONFIG_DEBUG_VERBOSE
            printf("i: %d command %d\n", i, command);
        #endif
        /* Is it a valid command? */
        if (command == GRAVITY_NONE) {
            #ifdef CONFIG_FLIPPER
                printf("Invalid command \"%s\", skipping\n", argv[i]);
            #else
                ESP_LOGW(TAG, "Invalid command \"%s\", skipping...", argv[i]);
            #endif
        } else {
            printf("%15s:\t%s\n%15s\t%s\n\n", commands[command].command, commands[command].hint, "", commands[command].help);
        }
    }

    return ESP_OK;
}

/* Send various types of incorrect 802.11 packets
   Usage: fuzz OFF | ( ( BEACON | REQ | RESP )+ ( OVERFLOW | MALFORMED ) )
   Overflow: ssid_len has an accurate length, but it's greater than 32. Start with 33 and increment.
   Malformed: ssid_len does not match the SSID's length. Alternate counting down and up.
*/
int cmd_fuzz(int argc, char **argv) {
    esp_err_t err;
    /* Flexible input parameters - loop through them to validate them */
    FuzzPacketType newPacketType = FUZZ_PACKET_NONE;
    FuzzMode newFuzzMode = FUZZ_MODE_NONE;
    /* Skip command name, final argument is required to be 'overflow' or 'malformed' */
    for (int i = 1; i < (argc - 1); ++i) {
        if (!strcasecmp(argv[i], "BEACON")) {
            newPacketType |= FUZZ_PACKET_BEACON;
        } else if (!strcasecmp(argv[i], "REQ")) {
            newPacketType |= FUZZ_PACKET_PROBE_REQ;
        } else if (!strcasecmp(argv[i], "RESP")) {
            newPacketType |= FUZZ_PACKET_PROBE_RESP;
        } else {
            #ifdef CONFIG_FLIPPER
                printf("%s is invalid\nSkipping...\n", argv[i]);
            #else
                ESP_LOGW(FUZZ_TAG, "Invalid packet type \"%s\". Skipping...", argv[i]);
            #endif
            continue;
        }
    }
    /* Don't forget to check the final argument */
    if (!strcasecmp(argv[argc - 1], "OVERFLOW")) {
        newFuzzMode = FUZZ_MODE_SSID_OVERFLOW;
    } else if (!strcasecmp(argv[argc - 1], "MALFORMED")) {
        newFuzzMode = FUZZ_MODE_SSID_MALFORMED;
    } else if (!strcasecmp(argv[argc - 1], "OFF")) {
        newFuzzMode = FUZZ_MODE_OFF;
    } else {
        // This is addressed later
    }

    if (argc == 1) {
        char strFuzzMode[21];
        char strFuzzType[38];
        char strTemp[15];
        err = fuzzModeAsString(strTemp);
        err |= fuzzPacketTypeAsString(strFuzzType);
        if (err != ESP_OK) {
            #ifdef CONFIG_FLIPPER
                printf("Internal failure computing strings\n");
            #else
                ESP_LOGE(FUZZ_TAG, "Internal failure while stringifying Fuzz Mode and Packet Type");
            #endif
            return ESP_ERR_INVALID_RESPONSE;
        }
        sprintf(strFuzzMode, "Mode: %s", strTemp);

        #ifdef CONFIG_FLIPPER
            printf("Fuzz: %s\t%s\t%s\n", (attack_status[ATTACK_FUZZ])?"ON":"OFF", strFuzzType, strFuzzMode);
        #else
            ESP_LOGI(FUZZ_TAG, "Fuzz: %s\tPackets: %s\n%25s\n", (attack_status[ATTACK_FUZZ])?"Enabled":"Disabled", strFuzzType, strFuzzMode);
        #endif
        return ESP_OK;
    }

    /* Now that 'fuzz' with no arguments has returned, enforce the validation */
    /* Specifying packetType can be skipped provided it's already set, specifying mood cannot */
    if ((newPacketType == FUZZ_PACKET_NONE && fuzzPacketType == FUZZ_PACKET_NONE) || newFuzzMode == FUZZ_MODE_NONE || argc > 5) {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_FUZZ);
        #else
            ESP_LOGE(FUZZ_TAG, "Invalid arguments provided.\n%s", USAGE_FUZZ);
        #endif
        return ESP_ERR_INVALID_ARG;
    }

    // Update attack_status[ATTACK_BEACON] appropriately
    attack_status[ATTACK_FUZZ] = strcasecmp(argv[argc - 1], "OFF");

    /* Start/stop hopping task loop as needed */
    hop_millis = dwellTime();
    char *args[] = {"hop","on"};
    if (isHopEnabled()) {
        ESP_ERROR_CHECK(cmd_hop(2, args));
    } else {
        args[1] = "off";
        ESP_ERROR_CHECK(cmd_hop(2, args));
    }

    if (attack_status[ATTACK_FUZZ]) {
        return fuzz_start(newFuzzMode, newPacketType);
    } else {
        return fuzz_stop();
    }
}

/* Control channel hopping
   Usage: hop [ MILLIS ] [ ON | OFF | DEFAULT | KILL ]
   Not specifying a parameter will report the status. KILL terminates the event loop.
*/
int cmd_hop(int argc, char **argv) {
    if (argc > 3) {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_HOP);
        #else
            ESP_LOGE(HOP_TAG, "%s", USAGE_HOP);
        #endif
        return ESP_ERR_INVALID_ARG;
    }

    if (argc == 1) {
        char hopMsg[39] = "\0";
        switch (hopStatus) {
        case HOP_STATUS_OFF:
            #ifdef CONFIG_FLIPPER
                strcpy(hopMsg, "OFF");
            #else
                strcpy(hopMsg, "is disabled");
            #endif
            break;
        case HOP_STATUS_ON:
            #ifdef CONFIG_FLIPPER
                strcpy(hopMsg, "ON");
            #else
                strcpy(hopMsg, "is enabled");
            #endif
            break;
        case HOP_STATUS_DEFAULT:
        #ifdef CONFIG_FLIPPER
            sprintf(hopMsg, "DEFAULT; %s", (isHopEnabled())?"ON":"OFF");
        #else
            sprintf(hopMsg, "will use defaults; currently %s.", (isHopEnabled())?"enabled":"disabled");
        #endif
            break;
        }
        char tempStr[19];
        hopModeToString(hopMode, tempStr);
        
        #ifdef CONFIG_FLIPPER
            char hopStr[21];
            sprintf(hopStr, "Dwell time %ldms", hop_millis);
            printf("Ch. hop %s\n%20s\nHop Mode: %s\n", hopMsg, hopStr, tempStr);
        #else
            ESP_LOGI(HOP_TAG, "Channel hopping %s; Gravity will dwell on each channel for approximately %ldms\nChannel hopping mode: %s", hopMsg, hop_millis, tempStr);
        #endif
    } else {
        /* argv[1] could be a duration, "on", "default", "off", "sequential" or "random" */
        /* To avoid starting hopping before updating hop_millis we need to check for duration first */
        long duration = atol(argv[1]);
        if (duration > 0) {
            hop_millis = duration;
            #ifdef CONFIG_FLIPPER
                printf("Dwell time: %ldms\n", duration);
            #else
                ESP_LOGI(HOP_TAG, "Gravity will dwell on each channel for %ldms.", duration);
            #endif
        } else if (!strcasecmp(argv[1], "ON") || (argc > 2 && !strcasecmp(argv[2], "ON")) || (argc == 4 && !strcasecmp(argv[3], "ON"))) {
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
            #ifdef CONFIG_FLIPPER
                sprintf(strOutput, "Hop on; %ld ms", hop_millis);
            #else
                ESP_LOGI(HOP_TAG, "%s", strOutput);
            #endif
            if (channelHopTask == NULL) {
                ESP_LOGI(HOP_TAG, "Gravity's channel hopping event task is not running, starting it now.");
                xTaskCreate(&channelHopCallback, "channelHopCallback", 2048, NULL, 5, &channelHopTask);
            }
        } else if (!strcasecmp(argv[1], "OFF") || (argc > 2 && !strcasecmp(argv[2], "OFF")) || (argc == 4 && !strcasecmp(argv[3], "OFF"))) {
            hopStatus = HOP_STATUS_OFF;
            #ifdef CONFIG_FLIPPER
                printf("Ch. hop OFF\n");
            #else
                ESP_LOGI(HOP_TAG, "Channel hopping disabled.");
            #endif
        } else if (!strcasecmp(argv[1], "KILL") || (argc > 2 && !strcasecmp(argv[2], "KILL")) || (argc == 4 && !strcasecmp(argv[3], "KILL"))) {
            hopStatus = HOP_STATUS_OFF;
            if (channelHopTask == NULL) {
                ESP_LOGE(HOP_TAG, "Unable to locate the channel hop task. Is it running?");
                return ESP_ERR_INVALID_ARG;
            } else {
                #ifdef CONFIG_FLIPPER
                    printf("Killing hop task\n");
                #else
                    ESP_LOGI(HOP_TAG, "Killing WiFi channel hopping event task %p...", &channelHopTask);
                #endif
                vTaskDelete(channelHopTask);
                channelHopTask = NULL;
            }
        } else if (!strcasecmp(argv[1], "DEFAULT") || (argc > 2 && !strcasecmp(argv[2], "DEFAULT")) || (argc == 4 && !strcasecmp(argv[3], "DEFAULT"))) {
            hopStatus = HOP_STATUS_DEFAULT;
            hop_millis = dwellForCurrentFeatures();
            #ifdef CONFIG_FLIPPER
                printf("Ch. hop DEFAULT\n");
            #else
                ESP_LOGI(HOP_TAG, "Channel hopping will use feature defaults.");
            #endif
        } else if (!strcasecmp(argv[1], "SEQUENTIAL") || (argc > 2 && !strcasecmp(argv[2], "SEQUENTIAL")) || (argc == 4 && !strcasecmp(argv[3], "SEQUENTIAL"))) {
            hopMode = HOP_MODE_SEQUENTIAL;
        } else if (!strcasecmp(argv[1], "RANDOM") || (argc > 2 && !strcasecmp(argv[2], "RANDOM")) || (argc == 4 && !strcasecmp(argv[3], "RANDOM"))) {
            hopMode = HOP_MODE_RANDOM;
        } else {
            /* Invalid argument */
            #ifdef CONFIG_FLIPPER
                printf("%s\n", SHORT_HOP);
            #else
                ESP_LOGE(HOP_TAG, "Invalid arguments provided: %s", USAGE_HOP);
            #endif
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
        #ifdef CONFIG_FLIPPER
            printf("%s: %s\n", commands[i].command, commands[i].hint);
        #else
            printf("%-13s: %s\n", commands[i].command, commands[i].hint);
        #endif
    }
    return ESP_OK;
}

int cmd_beacon(int argc, char **argv) {
    /* rickroll | random | target-ssids | ap | off | status */
    /* Initially the 'TARGET MAC' argument is unsupported, the attack only supports broadcast beacon frames */
    /* argc must be 1 or 2 - no arguments, or rickroll/random/user/infinite/off */
    if (argc < 1 || argc > 3) {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_BEACON);
        #else
            ESP_LOGE(TAG, "Invalid arguments specified. Expected 0 or 1, received %d.", argc - 1);
        #endif
        return ESP_ERR_INVALID_ARG;
    }
    if (argc == 1) {
        #ifdef CONFIG_FLIPPER
            printf("Beacon %s\n", (attack_status[ATTACK_BEACON])?"Running":"Not Running");
        #else
            ESP_LOGI(TAG, "Beacon Status: %s", attack_status[ATTACK_BEACON]?"Running":"Not Running");
        #endif
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
    } else if (!strcasecmp(argv[1], "target-ssids")) {
        ret = beacon_start(ATTACK_BEACON_USER, 0);
    } else if (!strcasecmp(argv[1], "aps")) {
        ret = beacon_start(ATTACK_BEACON_AP, 0);
    } else if (!strcasecmp(argv[1], "infinite")) {
        ret = beacon_start(ATTACK_BEACON_INFINITE, 0);
    } else if (!strcasecmp(argv[1], "off")) {
        ret = beacon_stop();
    } else {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_BEACON);
        #else
            ESP_LOGE(TAG, "Invalid argument provided to BEACON: \"%s\"", argv[1]);
        #endif
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
            #ifdef CONFIG_FLIPPER
                printf("target-ssids: {}\n");
            #else
                ESP_LOGI(TAG, "target-ssids has no elements.");
            #endif
        } else {
            #ifdef CONFIG_FLIPPER
                printf("%s\n", SHORT_TARGET_SSIDS);
            #else
                ESP_LOGE(TAG, "target-ssids must have either no arguments, to return its current value, or two arguments: ADD/REMOVE <ssid>");
            #endif
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
        #ifdef CONFIG_DEBUG_VERBOSE
            printf("Serialising target SSIDs");
        #endif
        strcpy(strSsids, (lsSsid())[0]);
        #ifdef CONFIG_DEBUG_VERBOSE
            printf("Before serialisation loop returned value is \"%s\"\n", strSsids);
        #endif
        for (int i = 1; i < ssidCount; ++i) {
            sprintf(temp, " , %s", (lsSsid())[i]);
            strcat(strSsids, temp);
            #ifdef CONFIG_DEBUG_VERBOSE
                printf("At the end of iteration %d retVal is \"%s\"\n",i, strSsids);
            #endif
        }
        #ifdef CONFIG_FLIPPER
            printf("SSIDs %s\n", strSsids);
        #else
            printf("Selected SSIDs: %s\n", strSsids);
        #endif
    } else if (!strcasecmp(argv[1], "add")) {
        return addSsid(argv[2]);
    } else if (!strcasecmp(argv[1], "remove")) {
        return rmSsid(argv[2]);
    }

    return ESP_OK;
}

int cmd_probe(int argc, char **argv) {
    // Syntax: PROBE [ ANY | TARGET-SSIDs | APs | OFF ]
    if ((argc > 3) || (argc > 1 && strcasecmp(argv[1], "ANY") && strcasecmp(argv[1], "TARGET-SSIDs") && strcasecmp(argv[1], "APs") && strcasecmp(argv[1], "OFF")) || (argc == 3 && !strcasecmp(argv[1], "OFF"))) {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_PROBE);
        #else
            ESP_LOGW(PROBE_TAG, "%s", USAGE_PROBE);
        #endif
        return ESP_ERR_INVALID_ARG;
    }

    if (argc == 1) {
        #ifdef CONFIG_FLIPPER
            printf("Probe %s\n", (attack_status[ATTACK_PROBE])?"Running":"Not Running");
        #else
            ESP_LOGI(PROBE_TAG, "Probe Status: %s", (attack_status[ATTACK_PROBE])?"Running":"Not Running");
        #endif
        return ESP_OK;
    }

    /* Set attack_status[ATTACK_PROBE] before checking channel hopping or starting/stopping */
    attack_status[ATTACK_PROBE] = strcasecmp(argv[1], "OFF");

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
        #ifdef CONFIG_FLIPPER
            printf("Stopping probes\n");
        #else
            ESP_LOGI(PROBE_TAG, "Stopping Probe Flood ...");
        #endif
        probe_stop();
    } else {
        // Gather parameters for probe_start()
        if (!strcasecmp(argv[1], "TARGET-SSIDS")) {
            probeType = ATTACK_PROBE_DIRECTED_USER;
        } else if (!strcasecmp(argv[1], "APS")) {
            probeType = ATTACK_PROBE_DIRECTED_SCAN;
        }

        char probeNote[100];
        sprintf(probeNote, "Starting a probe flood of %spackets%s", (probeType == ATTACK_PROBE_UNDIRECTED)?"broadcast ":"", (probeType == ATTACK_PROBE_DIRECTED_USER || probeType == ATTACK_PROBE_DIRECTED_SCAN)?" directed to ":"");
        if (probeType == ATTACK_PROBE_DIRECTED_USER || probeType == ATTACK_PROBE_DIRECTED_SCAN) {
            char suffix[25];
            sprintf(suffix, "%d %s SSIDs", countSsid(), (probeType == ATTACK_PROBE_DIRECTED_SCAN)?"scanned":"user-specified");
            strcat(probeNote, suffix);
        }

        #ifdef CONFIG_FLIPPER
            printf("%s probes\n", (probeType == ATTACK_PROBE_UNDIRECTED)?"Broadcast":(probeType == ATTACK_PROBE_DIRECTED_USER)?"User-Specified":"Scan Result");
        #else
            ESP_LOGI(PROBE_TAG, "%s", probeNote);
        #endif
        probe_start(probeType);
    }
    return ESP_OK;
}

int cmd_sniff(int argc, char **argv) {
    // Usage: sniff [ ON | OFF ]
    if (argc > 2) {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_SNIFF);
        #else
            ESP_LOGE(TAG, "%s", USAGE_SNIFF);
        #endif
        return ESP_ERR_INVALID_ARG;
    }

    if (argc == 1) {
        #ifdef CONFIG_FLIPPER
            printf("Sniffing %s\n", (attack_status[ATTACK_SNIFF])?"enabled":"disabled");
        #else
            ESP_LOGI(TAG, "Sniffing is %s", (attack_status[ATTACK_SNIFF])?"enabled":"disabled");
        #endif
        return ESP_OK;
    }

    if (!strcasecmp(argv[1], "on")) {
        attack_status[ATTACK_SNIFF] = true;
    } else if (!strcasecmp(argv[1], "off")) {
        attack_status[ATTACK_SNIFF] = false;
    } else {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_SNIFF);
        #else
            ESP_LOGE(TAG, "%s", USAGE_SNIFF);
        #endif
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
    /* Usage: deauth [ <millis> ] [ FRAME | DEVICE | SPOOF ] [ STA | AP | BROADCAST | OFF ] */
    if (argc > 4 || (argc == 4 && strcasecmp(argv[3], "STA") && strcasecmp(argv[3], "BROADCAST") &&
                strcasecmp(argv[3], "AP") && strcasecmp(argv[3], "OFF")) || 
            (argc == 4 && atol(argv[1]) == 0 && atol(argv[2]) == 0) ||
            (argc == 4 && strcasecmp(argv[1], "FRAME") && strcasecmp(argv[2], "FRAME") &&
                strcasecmp(argv[1], "DEVICE") && strcasecmp(argv[2], "DEVICE") &&
                strcasecmp(argv[1], "SPOOF") && strcasecmp(argv[2], "SPOOF")) ||
            (argc == 3 && strcasecmp(argv[2], "STA") && strcasecmp(argv[2], "BROADCAST") &&
                strcasecmp(argv[2], "AP") && strcasecmp(argv[2], "OFF")) ||
            (argc == 3 && atol(argv[1]) == 0 && strcasecmp(argv[1], "FRAME") &&
                strcasecmp(argv[1], "DEVICE") && strcasecmp(argv[1], "SPOOF")) ||
            (argc == 2 && strcasecmp(argv[1], "STA") && strcasecmp(argv[1], "BROADCAST") &&
                strcasecmp(argv[1], "AP") && strcasecmp(argv[1], "OFF") && (atol(argv[1]) <= 0))) {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_DEAUTH);
        #else
            ESP_LOGE(TAG, "%s", USAGE_DEAUTH);
        #endif
        return ESP_ERR_INVALID_ARG;
    }
    if (argc == 1) {
        #ifdef CONFIG_FLIPPER
            printf("Deauth %s Delay %ldms\n", (attack_status[ATTACK_DEAUTH])?"ON":"OFF", deauth_getDelay());
        #else
            ESP_LOGI(DEAUTH_TAG, "Deauth is %srunning. Waiting %ldms between packets.",
                    (attack_status[ATTACK_DEAUTH])?"":"not ", deauth_getDelay());
        #endif
        return ESP_OK;
    }

    /* Extract parameters */
    long delay = ATTACK_MILLIS;
    DeauthMAC setMAC = DEAUTH_MAC_FRAME;
    DeauthMode dMode = DEAUTH_MODE_OFF;
    switch (argc) {
    case 4:
        /* command must be in [3]*/
        if (!strcasecmp(argv[3], "STA")) {
            dMode = DEAUTH_MODE_STA;
        } else if (!strcasecmp(argv[3], "BROADCAST")) {
            dMode = DEAUTH_MODE_BROADCAST;
        } else if (!strcasecmp(argv[3], "AP")) {
            dMode = DEAUTH_MODE_AP;
        } else if (!strcasecmp(argv[3], "OFF")) {
            dMode = DEAUTH_MODE_OFF;
        }
        delay = atol(argv[1]);
        if (delay == 0) {
            delay = atol(argv[2]);
        }
        if (delay == 0) {
            #ifdef CONFIG_FLIPPER
                printf("Invalid duration, using %ld\n", ATTACK_MILLIS);
            #else
                ESP_LOGW(DEAUTH_TAG, "Invalid duration specified, using ATTACK_MILLIS: %ld", ATTACK_MILLIS);
            #endif
            return delay = ATTACK_MILLIS;
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
        } else if (!strcasecmp(argv[2], "AP")) {
            dMode = DEAUTH_MODE_AP;
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
        } else if (!strcasecmp(argv[1], "AP")) {
            dMode = DEAUTH_MODE_AP;
        } else if (!strcasecmp(argv[1], "OFF")) {
            dMode = DEAUTH_MODE_OFF;
        } else {
            /* Set the time between deauth packets without any other changes */
            return deauth_setDelay(atol(argv[1]));
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
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_MANA);
        #else
            ESP_LOGE(TAG, "%s", USAGE_MANA);
        #endif
        return ESP_ERR_INVALID_ARG;
    }

    bool launchMana = false; /* Not a very elegant way to restructure channel hopping... */

    if (argc == 1) {
        #ifdef CONFIG_FLIPPER
            printf("Mana is %s\n", (attack_status[ATTACK_MANA])?"Enabled":"Disabled");
        #else
            ESP_LOGI(MANA_TAG, "Mana is %s", (attack_status[ATTACK_MANA])?"Enabled":"Disabled");
        #endif
    } else if (!strcasecmp(argv[1], "VERBOSE")) {
        if (argc == 2) {
            #ifdef CONFIG_FLIPPER
                printf("Mana verbose %s\n", (attack_status[ATTACK_MANA])?"ON":"OFF");
            #else
                ESP_LOGI(MANA_TAG, "Mana Verbose Logging is %s", (attack_status[ATTACK_MANA_VERBOSE])?"Enabled":"Disabled");
            #endif
        } else if (argc == 3 && (!strcasecmp(argv[2], "ON") || !strcasecmp(argv[2], "OFF"))) {
            attack_status[ATTACK_MANA_VERBOSE] = strcasecmp(argv[2], "OFF");
        } else {
            #ifdef CONFIG_FLIPPER
                printf("%s\n", SHORT_MANA);
            #else
                ESP_LOGE(MANA_TAG, "%s", USAGE_MANA);
            #endif
            return ESP_ERR_INVALID_ARG;
        }
    } else if (!strcasecmp(argv[1], "OFF") || !strcasecmp(argv[1], "ON")) {
        attack_status[ATTACK_MANA] = strcasecmp(argv[1], "OFF");
    } else if (!strcasecmp(argv[1], "AUTH")) {
        if (argc == 2) {
            // return mana_auth
            #ifdef CONFIG_FLIPPER
                printf("Mana Auth: %s\n", (mana_auth == AUTH_TYPE_NONE)?"OPEN":(mana_auth == AUTH_TYPE_WEP)?"WEP":"WPA");
            #else
                ESP_LOGI(MANA_TAG, "Mana authentication method: %s", (mana_auth==AUTH_TYPE_NONE)?"Open Authentication":(mana_auth==AUTH_TYPE_WEP)?"Wireless Equivalent Privacy":"Wi-Fi Protected Access");
            #endif
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
            #ifdef CONFIG_FLIPPER
                printf("%s\n", SHORT_MANA);
            #else
                ESP_LOGE(MANA_TAG, "%s", USAGE_MANA);
            #endif
            return ESP_ERR_INVALID_ARG;
        }
    } else if (!strcasecmp(argv[1], "LOUD")) {
        if (argc == 2) {
            #ifdef CONFIG_FLIPPER
                printf("Mana %s; Loud %s\n", (attack_status[ATTACK_MANA])?"ON":"OFF", (attack_status[ATTACK_MANA_LOUD])?"ON":"OFF");
            #else
                ESP_LOGI(MANA_TAG, "Mana is %srunning : LOUD-Mana is %s", (attack_status[ATTACK_MANA])?"":"not ", (attack_status[ATTACK_MANA_LOUD])?"Enabled":"Disabled");
            #endif
            return ESP_OK;
        }
        if (!(strcasecmp(argv[2], "ON") && strcasecmp(argv[2], "OFF"))) {
            attack_status[ATTACK_MANA_LOUD] = strcasecmp(argv[2], "OFF");

            if (!attack_status[ATTACK_MANA]) {
                /* Mana isn't running - Start it */
                launchMana = true;
            }
        } else {
            #ifdef CONFIG_FLIPPER
                printf("%s\n", SHORT_MANA);
            #else
                ESP_LOGE(MANA_TAG, "%s", USAGE_MANA);
            #endif
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
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_MANA);
        #else
            ESP_LOGE(MANA_TAG, "%s", USAGE_MANA);
        #endif
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
        #ifdef CONFIG_FLIPPER
            printf("Starting Mana...\n");
        #else
            ESP_LOGI(MANA_TAG, "Mana is not running. Starting ...");
        #endif
        char *manaArgs[2] = { "mana", "ON" };
        attack_status[ATTACK_MANA] = true;
        cmd_mana(2, manaArgs);
    }

    return ESP_OK;
}

int cmd_stalk(int argc, char **argv) {

    return ESP_OK;
}

/* Enable or disable AP Denial-of-service attack mode */
/* Usage ap-dos [ ON | OFF ] */
int cmd_ap_dos(int argc, char **argv) {
    if (argc > 2 || (argc == 2 && strcasecmp(argv[1], "ON") && strcasecmp(argv[1], "OFF"))) {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_AP_DOS);
        #else
            ESP_LOGE(DOS_TAG, "Invalid parameters. Usage: %s", USAGE_AP_DOS);
        #endif
    }
    if (argc == 1) {
        #ifdef CONFIG_FLIPPER
            printf("AP-DOS is %srunning\n", (attack_status[ATTACK_AP_DOS])?"":"not ");
        #else
            ESP_LOGI(DOS_TAG, "AP-DOS is %srunning", (attack_status[ATTACK_AP_DOS])?"":"not ");
        #endif
        return ESP_OK;
    }
    /* Update attack_status[] */
    attack_status[ATTACK_AP_DOS] = strcasecmp(argv[1], "OFF");

    /* Start/Stop hopping task loop if hopping is on by default */
    hop_millis = dwellTime();
    char *args[] = {"hop","on"};
    if (isHopEnabled()) {
        ESP_ERROR_CHECK(cmd_hop(2, args));
    } else {
        args[1] = "off";
        ESP_ERROR_CHECK(cmd_hop(2, args));
    }

    if (gravity_sel_ap_count == 0) {
        #ifdef CONFIG_FLIPPER
            printf("NOTE: You have no selected APs\nAP-DOS will not function until you select some.\n");
        #else
            ESP_LOGI(DOS_TAG, "You have no selected APs. AP-DOS is active, but only operates on selected APs. Please select one or more APs.");
        #endif
    }

    return ESP_OK;
}

int cmd_ap_clone(int argc, char **argv) {
    if (argc > 2 || (argc == 2 && strcasecmp(argv[1], "ON") && strcasecmp(argv[1], "OFF"))) {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_AP_DOS);
        #else
            ESP_LOGE(DOS_TAG, "Invalid arguments: %s", USAGE_AP_DOS);
        #endif
        return ESP_ERR_INVALID_ARG;
    }

    if (argc == 1) {
        #ifdef CONFIG_FLIPPER
            printf("AP-CLONE is %srunning\n", (attack_status[ATTACK_AP_CLONE])?"":"not ");
        #else
            ESP_LOGI(DOS_TAG, "AP-CLONE is %srunning", (attack_status[ATTACK_AP_CLONE])?"":"not ");
        #endif
        return ESP_OK;
    }

    esp_err_t err = cloneStartStop(strcasecmp(argv[1], "OFF"));

    return err;
}

int cmd_scan(int argc, char **argv) {
    if (argc > 2 || (argc == 2 && strcasecmp(argv[1], "ON") && strcasecmp(argv[1], "OFF") && strlen(argv[1]) > 32)) {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_SCAN);
        #else
            ESP_LOGE(TAG, "%s", USAGE_SCAN);
        #endif
        return ESP_ERR_INVALID_ARG;
    }

    if (argc == 1) {
        char strMsg[128];
        char working[64];
        #ifdef CONFIG_FLIPPER
            sprintf(strMsg, "Scan %s; ", (attack_status[ATTACK_SCAN])?"ON":"OFF");
        #else
            sprintf(strMsg, "Scanning is %s. Network selection is configured to sniff ", (attack_status[ATTACK_SCAN])?"Active":"Inactive");
        #endif
        if (strlen(scan_filter_ssid) == 0) {
            #ifdef CONFIG_FLIPPER
                strcat(strMsg, "all nets");
            #else
                strcat(strMsg, "packets from all networks.");
            #endif
        } else {
            #ifdef CONFIG_FLIPPER
                strcat(strMsg, "user SSID");
            #else
                sprintf(working, "packets from the SSID \"%s\"", scan_filter_ssid);
                strcat(strMsg, working);
            #endif
        }
        #ifdef CONFIG_FLIPPER
            printf("%s\n", strMsg);
        #else
            ESP_LOGI(TAG, "%s", strMsg);
        #endif
        return ESP_OK;
    }

    /* Zero out SSID filter */
    memset(scan_filter_ssid, '\0', 33);
    memset(scan_filter_ssid_bssid, 0x00, 6);

    if (!strcasecmp(argv[1], "ON")) {
        attack_status[ATTACK_SCAN] = true;
    } else if (!strcasecmp(argv[1], "OFF")) {
        attack_status[ATTACK_SCAN] = false;
    } else {
        attack_status[ATTACK_SCAN] = true;
        /* Use argv[1] as an SSID filter */
        strncpy(scan_filter_ssid, argv[1], 32); /* SSID max 32 chars */
        /* See if we've already seen the AP */
        int i;
        for (i = 0; i < gravity_ap_count && strcasecmp(scan_filter_ssid,
                                (char *)gravity_aps[i].espRecord.ssid); ++i) { }
        if (i < gravity_ap_count) {
            /* Found the SSID in cached scan results */
            #ifdef CONFIG_DEBUG
                char strMac[18] = "\0";
                ESP_ERROR_CHECK(mac_bytes_to_string(scan_filter_ssid_bssid, strMac));
                #ifndef CONFIG_FLIPPER
                    ESP_LOGI(SCAN_TAG, "Have already seen BSSID %s for AP \"%s\"", strMac, scan_filter_ssid);
                #endif
            #endif
        }
    }

    #ifdef CONFIG_FLIPPER
        printf("Scanning %s\n", (attack_status[ATTACK_SCAN])?"ON":"OFF");
        if (strlen(scan_filter_ssid) > 0) {
            char truncSsid[33];
            strcpy(truncSsid, scan_filter_ssid);
            if (strlen(truncSsid) > 23) {
                if (truncSsid[20] == ' ') {
                    memcpy(&truncSsid[20], "...\0", 4);
                } else {
                    memcpy(&truncSsid[21], "..\0", 3);
                }
            }
            printf("> %25s\n",truncSsid);
        }
    #else
        char strMsg[16];
        char ssidMsg[43] = "\0";
        sprintf(strMsg, "Scanning is %s", (attack_status[ATTACK_SCAN])?"ON":"OFF");
        if (strlen(scan_filter_ssid) > 0) {
            sprintf(ssidMsg, " for SSID %s", scan_filter_ssid);
        }
            ESP_LOGI(SCAN_TAG, "%s%s\n", strMsg, ssidMsg);
    #endif

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
      SCRAMBLE_WORDS, SSID_LEN_MIN, SSID_LEN_MAX, DEFAULT_SSID_COUNT, CHANNEL,
      MAC, ATTACK_PKTS, ATTACK_MILLIS, MAC_RAND, EXPIRY | HOP_MODE */
/* Channel hopping is not catered for in this feature */
int cmd_set(int argc, char **argv) {
    if (argc != 3) {
        #ifdef CONFIG_FLIPPER
            printf("%s\nSCRAMBLE_WORDS,\nSSID_LEN_MIN,\nSSID_LEN_MAX,\nDEFAULT_SSID_COUNT,\nCHANNEL,ATTACK_PKTS,\nATTACK_MILLIS,MAC,\nMAC_RAND,EXPIRY\nHOP_MODE\n", SHORT_SET);
        #else
            ESP_LOGE(TAG, "%s", USAGE_SET);
            ESP_LOGE(TAG, "<variable> : SSID_LEN_MIN | SSID_LEN_MAX | DEFAULT_SSID_COUNT | CHANNEL | HOP_MODE |");
            ESP_LOGE(TAG, "             MAC | ATTACK_PKTS | ATTACK_MILLIS | MAC_RAND | EXPIRY | SCRAMBLE_WORDS");
        #endif
        return ESP_ERR_INVALID_ARG;
    }
    if (!strcasecmp(argv[1], "SCRAMBLE_WORDS")) {
        if (!strcasecmp(argv[2], "TRUE") || !strcasecmp(argv[2], "YES") || !strcasecmp(argv[2], "ON")) {
            scrambledWords = true;
        } else if (!strcasecmp(argv[2], "FALSE") || !strcasecmp(argv[2], "NO") || !strcasecmp(argv[2], "OFF")) {
            scrambledWords = false;
        } else {
            #ifdef CONFIG_FLIPPER
                printf("Usage: set SCRAMBLE WORDS <value>\n<value> can be true/false, yes/no or on/off\n");
            #else
                ESP_LOGE(TAG, "Incorrect state specified for SCRAMBLE_WORDS. Usage: set SCRAMBLE_WORDS <value>\n<value> ==> True/False, Yes/No, On/Off");
            #endif
            return ESP_ERR_INVALID_ARG;
        }
        #ifdef CONFIG_FLIPPER
            printf("SCRAMBLE_WORDS: %s\n", (scrambledWords)?"ON":"OFF");
        #else
            ESP_LOGI(TAG, "SCRAMBLE_WORDS is %s", (scrambledWords)?"Enabled":"Disabled");
        #endif
        return ESP_OK;
    } else if (!strcasecmp(argv[1], "HOP_MODE")) {
        /* Check that argv[2] is valid along the way */
        if (!strcasecmp(argv[2], "SEQUENTIAL")) {
            hopMode = HOP_MODE_SEQUENTIAL;
        } else if (!strcasecmp(argv[2], "RANDOM")) {
            hopMode = HOP_MODE_RANDOM;
        } else {
            #ifdef CONFIG_FLIPPER
                printf("SET HOP_MODE ( SEQUENTIAL | RANDOM )\n");
            #else
                ESP_LOGE(HOP_TAG, "Invalid mode specified. Please use one of ( SEQUENTIAL , RANDOM )");
            #endif
            return ESP_ERR_INVALID_ARG;
        }
    } else if (!strcasecmp(argv[1], "SSID_LEN_MIN")) {
        //
        int iVal = atoi(argv[2]);
        if (iVal < 0 || iVal > SSID_LEN_MAX) {
            ESP_LOGE(TAG, "Invalid value specified. SSID_LEN_MIN must be between 0 and SSID_LEN_MAX (%d).", SSID_LEN_MAX);
            return ESP_ERR_INVALID_ARG;
        }
        SSID_LEN_MIN = iVal;
        #ifdef CONFIG_FLIPPER
            printf("SSID_LEN_MIN: %d\n", SSID_LEN_MIN);
        #else
            ESP_LOGI(TAG, "SSID_LEN_MIN is now %d", SSID_LEN_MIN);
        #endif
        return ESP_OK;
    } else if (!strcasecmp(argv[1], "SSID_LEN_MAX")) {
        //
        int iVal = atoi(argv[2]);
        if (iVal < SSID_LEN_MIN) {
            ESP_LOGE(TAG, "Invalid value specified. SSID_LEN_MAX must be greater than SSID_LEN_MIN (%d)", SSID_LEN_MIN);
            return ESP_ERR_INVALID_ARG;
        }
        SSID_LEN_MAX = iVal;
        #ifdef CONFIG_FLIPPER
            printf("SSID_LEN_MAX: %d\n", SSID_LEN_MAX);
        #else
            ESP_LOGI(TAG, "SSID_LEN_MAX is now %d", SSID_LEN_MAX);
        #endif
        return ESP_OK;
    } else if (!strcasecmp(argv[1], "DEFAULT_SSID_COUNT")) {
        //
        int iVal = atoi(argv[2]);
        if (iVal <= 0) {
            ESP_LOGE(TAG, "Invalid value specified. DEFAULT_SSID_COUNT must be a positive integer. Received %d", iVal);
            return ESP_ERR_INVALID_ARG;
        }
        DEFAULT_SSID_COUNT = iVal;
        #ifdef CONFIG_FLIPPER
            printf("DEF_SSID_COUNT: %d\n", DEFAULT_SSID_COUNT);
        #else
            ESP_LOGI(TAG, "DEFAULT_SSID_COUNT is now %d", DEFAULT_SSID_COUNT);
        #endif
        return ESP_OK;
    } else if (!strcasecmp(argv[1], "CHANNEL")) {
        //
        uint8_t channel = atoi(argv[2]);
        wifi_second_chan_t chan2 = WIFI_SECOND_CHAN_ABOVE;
        esp_err_t err = esp_wifi_set_channel(channel, chan2);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error while setting channel: %s", esp_err_to_name(err));
        } else {
            #ifdef CONFIG_FLIPPER
                printf("Channel %u\n", channel);
            #else
                ESP_LOGI(TAG, "Successfully changed to channel %u", channel);
            #endif
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
        #ifdef CONFIG_FLIPPER
            printf("MAC: %02x%02x:%02x%02x:%02x%02x\n", bMac[0], bMac[1], bMac[2],
                    bMac[3], bMac[4], bMac[5]);
        #else
            ESP_LOGI(TAG, "Set MAC to :  %s", argv[2]);
        #endif
        return ESP_OK;
    } else if (!strcasecmp(argv[1], "MAC_RAND")) {
        if (!strcasecmp(argv[2], "ON")) {
            attack_status[ATTACK_RANDOMISE_MAC] = true;
        } else if (!strcasecmp(argv[2], "OFF")) {
            attack_status[ATTACK_RANDOMISE_MAC] = false;
        } else {
            #ifdef CONFIG_FLIPPER
                printf("set MAC_RAND ON|OFF\n");
            #else
                ESP_LOGE(TAG, "Usage: set MAC_RAND [ ON | OFF ]");
            #endif
            return ESP_ERR_INVALID_ARG;
        }
        #ifdef CONFIG_FLIPPER
            printf("MAC Rand : %s\n", (attack_status[ATTACK_RANDOMISE_MAC])?"ON":"OFF");
        #else
            ESP_LOGI(TAG, "MAC randomisation :  %s", (attack_status[ATTACK_RANDOMISE_MAC])?"ON":"OFF");
        #endif
        return ESP_OK;
    } else if (!strcasecmp(argv[1], "EXPIRY")) {
        /* Parameter check */
        if (argc != 3 || strtod(argv[2], NULL) == 0) {
            #ifdef CONFIG_FLIPPER
                printf("%s\n", SHORT_SET);
            #else
                ESP_LOGE(TAG, "%s", USAGE_SET);
            #endif
            return ESP_ERR_INVALID_ARG;
        }
        scanResultExpiry = strtod(argv[2], NULL);
        /* Display its new status */
        char *cmd[] = { "GET", "EXPIRY"};
        cmd_get(2, cmd);
        return ESP_OK;
    } else if (!strcasecmp(argv[1], "ATTACK_PKTS")) {
        #ifdef CONFIG_FLIPPER
            printf("Not implemented\n");
        #else
            ESP_LOGI(TAG, "This command has not been implemented.");
        #endif
    } else if (!strcasecmp(argv[1], "ATTACK_MILLIS")) {
        long newMillis = atof(argv[1]);
        if (newMillis == 0) {
            #ifdef CONFIG_FLIPPER
                printf("%s\n", SHORT_SET);
            #else
                ESP_LOGI(TAG, "%s", USAGE_SET);
            #endif
            return ESP_ERR_INVALID_ARG;
        }
        ATTACK_MILLIS = newMillis;
        #ifdef CONFIG_FLIPPER
            printf("ATTACK_MILLIS is %ld\n", ATTACK_MILLIS);
        #else
            ESP_LOGI(TAG, "ATTACK_MILLIS is %ld\n", ATTACK_MILLIS);
        #endif
    } else {
        #ifdef CONFIG_FLIPPER
            printf("%s\nSSID_LEN_MIN,\nSSID_LEN_MAX,\nDEFAULT_SSID_COUNT,\nCHANNEL,ATTACK_PKTS,\nATTACK_MILLIS,MAC,\nMAC_RAND,EXPIRY\nHOP_MODE\n", SHORT_SET);
        #else
            ESP_LOGE(TAG, "Invalid variable specified. %s", USAGE_SET);
            ESP_LOGE(TAG, "<variable> : SSID_LEN_MIN | SSID_LEN_MAX | DEFAULT_SSID_COUNT | CHANNEL |");
            ESP_LOGE(TAG, "             MAC | ATTACK_PKTS | ATTACK_MILLIS | MAC_RAND | EXPIRY | HOP_MODE");
        #endif
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

/* Get application configuration items */
/* Usage: set <variable> <value>
   Allowed values for <variable> are:
      SSID_LEN_MIN, SSID_LEN_MAX, DEFAULT_SSID_COUNT, CHANNEL, HOP_MODE
      MAC, EXPIRY, MAC_RAND, ATTACK_PKTS (unused), ATTACK_MILLIS */
/* Channel hopping is not catered for in this feature */
int cmd_get(int argc, char **argv) {
    if (argc != 2) {
        #ifdef CONFIG_FLIPPER
            printf("%s\nSCRAMBLE_WORDS,\nSSID_LEN_MIN,\nSSID_LEN_MAX,\nDEFAULT_SSID_COUNT,\nCHANNEL,ATTACK_PKTS,\nATTACK_MILLIS,MAC,\nMAC_RAND,EXPIRY\nHOP_MODE\n", SHORT_GET);
        #else
            ESP_LOGE(TAG, "%s", USAGE_GET);
            ESP_LOGE(TAG, "<variable> : SSID_LEN_MIN | SSID_LEN_MAX | DEFAULT_SSID_COUNT | CHANNEL | HOP_MODE");
            ESP_LOGE(TAG, "             MAC | ATTACK_PKTS | ATTACK_MILLIS | MAC_RAND | EXPIRY | SCRAMBLE_WORDS");
        #endif
        return ESP_ERR_INVALID_ARG;
    }
    if (!strcasecmp(argv[1], "SCRAMBLE_WORDS")) {
        #ifdef CONFIG_FLIPPER
            printf("SCRAMBLE_WORDS: %s\n", (scrambledWords)?"On":"Off");
        #else
            ESP_LOGI(TAG, "SCRAMBLE_WORDS: %s", (scrambledWords)?"Enabled":"Disabled");
        #endif
    } else if (!strcasecmp(argv[1], "HOP_MODE")) {
        char mode[19] = "";
        if (hopModeToString(hopMode, mode) != ESP_OK) {
            mode[0] = '\0';
        }
        #ifdef CONFIG_FLIPPER
            printf("HOP_MODE: %s (%d)\n", (strlen(mode) > 0)?mode:"Unknown", hopMode);
        #else
            ESP_LOGI(HOP_TAG, "HOP_MODE :  %s (%d)", (strlen(mode) > 0)?mode:"Unknown", hopMode);
        #endif
    } else if (!strcasecmp(argv[1], "SSID_LEN_MIN")) {
        #ifdef CONFIG_FLIPPER
            printf("SSID_LEN_MIN: %d\n", SSID_LEN_MIN);
        #else
            ESP_LOGI(TAG, "SSID_LEN_MIN :  %d", SSID_LEN_MIN);
        #endif
    } else if (!strcasecmp(argv[1], "SSID_LEN_MAX")) {
        #ifdef CONFIG_FLIPPER
            printf("SSID_LEN_MAX: %d\n", SSID_LEN_MAX);
        #else
            ESP_LOGI(TAG, "SSID_LEN_MAX :  %d", SSID_LEN_MAX);
        #endif
    } else if (!strcasecmp(argv[1], "DEFAULT_SSID_COUNT")) {
        #ifdef CONFIG_FLIPPER
            printf("DEF_SSID_COUNT: %d\n", DEFAULT_SSID_COUNT);
        #else
            ESP_LOGI(TAG, "DEFAULT_SSID_COUNT :  %d", DEFAULT_SSID_COUNT);
        #endif
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
        #ifdef CONFIG_FLIPPER
            printf("Channel: %u\n", channel);
        #else
            ESP_LOGI(TAG, "Channel: %u   Secondary: %s", channel, secondary);
        #endif
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
        #ifdef CONFIG_FLIPPER
            printf("%s\n", strMac);
        #else
            ESP_LOGI(TAG, "Current MAC :  %s", strMac);
        #endif
        return ESP_OK;
    } else if (!strcasecmp(argv[1], "MAC_RAND")) {
        #ifdef CONFIG_FLIPPER
            printf("MAC Rand: %s\n", (attack_status[ATTACK_RANDOMISE_MAC])?"ON":"OFF");
        #else
            ESP_LOGI(TAG, "MAC Randomisation is :  %s", (attack_status[ATTACK_RANDOMISE_MAC])?"ON":"OFF");
        #endif
        return ESP_OK;
    } else if (!strcasecmp(argv[1], "EXPIRY")) {
        /* Check whether expiry is disabled */
        char resultStr[17] = "";
        if (scanResultExpiry == 0) {
            strcpy(resultStr, "Disabled");
        } else {
            #ifdef CONFIG_FLIPPER
                sprintf(resultStr, "%fmin", scanResultExpiry);
            #else
                sprintf(resultStr, "%f minutes", scanResultExpiry);
            #endif
        }
        #ifdef CONFIG_FLIPPER
            printf("Packet Expiry: %s\n", resultStr);
        #else
            ESP_LOGI(TAG, "Packet Expiry: %s", resultStr);
        #endif
    } else if (!strcasecmp(argv[1], "ATTACK_PKTS")) {
        //
        #ifdef CONFIG_FLIPPER
            printf("Not Implemented\n");
        #else
            ESP_LOGI(TAG, "Not yet implemented");
        #endif
    } else if (!strcasecmp(argv[1], "ATTACK_MILLIS")) {
        //
        #ifdef CONFIG_FLIPPER
            printf("Not Implemented\n");
        #else
            ESP_LOGI(TAG, "Not yet implemented");
        #endif
    } else {
        #ifdef CONFIG_FLIPPER
            printf("%s\nSSID_LEN_MIN,\nSSID_LEN_MAX,\nDEFAULT_SSID_COUNT,\nCHANNEL,ATTACK_PKTS,\nATTACK_MILLIS,MAC,\nMAC_RAND,EXPIRY\nHOP_MODE\n", SHORT_GET);
        #else
            ESP_LOGE(TAG, "Invalid variable specified. %s", USAGE_GET);
            ESP_LOGE(TAG, "<variable> : SSID_LEN_MIN | SSID_LEN_MAX | DEFAULT_SSID_COUNT | CHANNEL |");
            ESP_LOGE(TAG, "             MAC | ATTACK_PKTS | ATTACK_MILLIS | MAC_RAND | EXPIRY | HOP_MODE");
        #endif
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

/* Channel hopping is not catered for in this feature */
int cmd_view(int argc, char **argv) {
    if (argc < 2 || argc > 5) {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_VIEW);
        #else
            ESP_LOGE(TAG, "%s", USAGE_VIEW);
        #endif
        return ESP_ERR_INVALID_ARG;
    }
    bool success = true;
    for (int i=1; i < argc; ++i) {
        /* Hide expired packets for display if scanResultExpiry has been set */
        if (!strcasecmp(argv[i], "AP")) {
            /* Is this looking for APs, or for APs associated with selected STAs? */
            if (argc > (i + 1) && !strcasecmp(argv[i + 1], "selectedSTA")) {
                /* Collate all APs that are associated with the selected STAs */
                int apCount = 0;
                ScanResultAP **selectedAPs = collateAPsOfSelectedSTAs(&apCount);

                success = (success && (gravity_list_ap(selectedAPs, apCount, (scanResultExpiry != 0)) == ESP_OK));

                free(selectedAPs);
                ++i;
            } else {
                success = (success && (gravity_list_all_aps((scanResultExpiry != 0)) == ESP_OK));
            }
        } else if (!strcasecmp(argv[i], "STA")) {
            /* Are we looking for all STAs, or STAs associated with select APs? */
            if (argc > (i + 1) && !strcasecmp(argv[i + 1], "selectedAP")) {
                /* Collate all STAs that are associated with the selected APs */
                int staCount = 0;
                ScanResultSTA **selectedSTAs = collateClientsOfSelectedAPs(&staCount);
                success = (success && (gravity_list_sta(selectedSTAs, staCount, (scanResultExpiry != 0)) == ESP_OK));
                free(selectedSTAs);
                ++i;
            } else {
                success = (success && (gravity_list_all_stas((scanResultExpiry != 0)) == ESP_OK));
            }
        } else {
            #ifdef CONFIG_FLIPPER
                printf("%s\n", SHORT_VIEW);
            #else
                ESP_LOGE(TAG, "%s", USAGE_VIEW);
            #endif
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
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_SELECT);
        #else
            ESP_LOGE(TAG, "%s", USAGE_SELECT);
        #endif
        return ESP_ERR_INVALID_ARG;
    }

    /* Flipper Zero's keyboard doesn't have a space, so support ^-separated selectors */
    if (argc == 3 && strstr(argv[2], "^") != NULL) {
        for (int i = 0; i < strlen(argv[2]); ++i) {
            if (argv[2][i] == '^') {
                argv[2][i] = ' ';
            }
        }
    }

    esp_err_t err = ESP_OK;;
    if (!strcasecmp(argv[1], "AP")) {
        for (int i = 2; i < argc; ++i) {
            err = gravity_select_ap(atoi(argv[i]));
            #ifdef CONFIG_FLIPPER
                printf("AP %d %sselected\n", atoi(argv[i]), (gravity_ap_isSelected(atoi(argv[i])))?"":"not ");
            #else
                ESP_LOGI(TAG, "AP element %d is %sselected", atoi(argv[i]), (gravity_ap_isSelected(atoi(argv[i])))?"":"not ");
            #endif
        }
    } else if (!strcasecmp(argv[1], "STA")) {
        for (int i = 2; i < argc; ++i) {
            err = gravity_select_sta(atoi(argv[i]));
            #ifdef CONFIG_FLIPPER
                printf("STA %d %sselected\n", atoi(argv[i]), (gravity_sta_isSelected(atoi(argv[i])))?"":"not ");
            #else
                ESP_LOGI(TAG, "STA element %d is %sselected", atoi(argv[i]), (gravity_sta_isSelected(atoi(argv[i])))?"":"not ");
            #endif
        }
    }
    return err;
}

/* Display selected STAs and/or APs. Usage: selected ( AP | STA ). Call with no arguments to display both */
/* Adding selected AP STA as well as no args because I keep forgetting I can use no args */
int cmd_selected(int argc, char **argv) {
    int retVal = ESP_OK;
    int retVal2 = ESP_OK;

    if (argc > 3 || (argc > 1 && strcasecmp(argv[1], "STA") && strcasecmp(argv[1], "AP")) ||
            (argc == 3 && strcasecmp(argv[2], "STA") && strcasecmp(argv[2], "AP"))) {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_SELECTED);
        #else
            ESP_LOGE(TAG, "%s", USAGE_SELECTED);
        #endif
        return ESP_ERR_INVALID_ARG;
    }

    /* Print APs if no args or "AP" */
    /* Hide expired packets only if scanResultExpiry has been set */
    if (argc == 1 || (argc > 1 && !strcasecmp(argv[1], "AP")) || (argc == 3 && !strcasecmp(argv[2], "AP"))) {
        retVal = gravity_list_ap(gravity_selected_aps, gravity_sel_ap_count, (scanResultExpiry != 0));
    }
    if (argc == 1 || (argc > 1 && !strcasecmp(argv[1], "STA")) || (argc == 3 && !strcasecmp(argv[2], "STA"))) {
        retVal2 = gravity_list_sta(gravity_selected_stas, gravity_sel_sta_count, (scanResultExpiry != 0));
    }

    if (retVal != ESP_OK) {
        return retVal;
    } else if (retVal2 != ESP_OK) {
        return retVal2;
    }
    return ESP_OK;
}

/* Channel hopping is not catered for in this feature */
int cmd_clear(int argc, char **argv) {
    if (argc != 2 && argc != 3) {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_CLEAR);
        #else
            ESP_LOGE(TAG, "%s", USAGE_CLEAR);
        #endif
        return ESP_ERR_INVALID_ARG;
    }
    for (int i=1; i < argc; ++i) {
        if (strcasecmp(argv[i], "AP") && strcasecmp(argv[i], "STA") && strcasecmp(argv[i], "ALL")) {
            #ifdef CONFIG_FLIPPER
                printf("%s\n", SHORT_CLEAR);
            #else
                ESP_LOGE(TAG, "%s", USAGE_CLEAR);
            #endif
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
        ESP_LOGE(MANA_TAG, "Unrecognised authentication type: %d\n", authType);
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

/* Does Gravity need to monitor frames as they arrive?
   This is just a simple IF statement, but it keeps getting longer
   and more complicated; I figured it could do with having its
   own function
*/
bool gravitySniffActive() {
    //

    return (attack_status[ATTACK_AP_DOS] || attack_status[ATTACK_AP_CLONE] ||
        attack_status[ATTACK_SNIFF] || attack_status[ATTACK_MANA] || attack_status[ATTACK_SCAN] ||
        attack_status[ATTACK_MANA_LOUD]);
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

    if (!gravitySniffActive()) {
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
    /* Ditto for the sniffer */
    if (attack_status[ATTACK_SNIFF]) {
        esp_err_t err;
        err = sniffPacket(payload);
        /* Report the error, but continue */
        if (err != ESP_OK) {
            #ifdef CONFIG_FLIPPER
                printf("Packet sniffer returned %s\n", esp_err_to_name(err));
            #else
                ESP_LOGW(SNIFF_TAG, "Packet sniffer returned an error: %s", esp_err_to_name(err));
            #endif
        }
    }
    /* DOS payload */
    if (attack_status[ATTACK_AP_DOS]) {
        esp_err_t err = dosParseFrame(payload);
        if (err != ESP_OK) {
            #ifdef CONFIG_FLIPPER
                printf("DOS returned %s\n", esp_err_to_name(err));
            #else
                ESP_LOGW(DOS_TAG, "DOS returned %s", esp_err_to_name(err));
            #endif
        }
    }
    /* AP-CLONE - More like DOS+ than "clone", but ... */
    if (attack_status[ATTACK_AP_CLONE]) {
        // dosParseFrame() or cloneParseFrame() ?
    }
    if (payload[0] == WIFI_FRAME_PROBE_REQ) {
        //printf("W00T! Got a probe request!\n");
        int ssid_len = payload[PROBE_SSID_OFFSET - 1];
        char *ssid = malloc(sizeof(char) * (ssid_len + 1));
        if (ssid == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory to hold probe request's SSID");
            return;
        }
        strncpy(ssid, (char *)&payload[PROBE_SSID_OFFSET], ssid_len);
        ssid[ssid_len] = '\0';
        
        #ifdef CONFIG_DEBUG_VERBOSE
            char srcMac[18];
            esp_err_t err = mac_bytes_to_string(&payload[PROBE_SRCADDR_OFFSET], srcMac);
            ESP_LOGI(TAG, "Probe for \"%s\" from %s", ssid, srcMac);
        #endif
        if (attack_status[ATTACK_MANA]) {
            mana_handleProbeRequest(payload, ssid, ssid_len);
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
            #ifndef CONFIG_FLIPPER
                ESP_LOGI(TAG, "Registered command \"%s\"...", commands[i].command);
            #endif
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
            case ATTACK_FUZZ:
            case ATTACK_SNIFF:
            case ATTACK_DEAUTH:
            case ATTACK_SCAN:
            case ATTACK_MANA:
            case ATTACK_MANA_VERBOSE:
            case ATTACK_MANA_LOUD:
            case ATTACK_AP_DOS:
            case ATTACK_AP_CLONE:
            case ATTACK_HANDSHAKE:
            case ATTACK_BT:
                attack_status[i] = false;
                break;
            case ATTACK_RANDOMISE_MAC:
                attack_status[i] = true;
                break;
            default:
                ESP_LOGE(TAG, "ATTACKS_COUNT has incorrect length. Unexpected value %d", i);
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
            case ATTACK_FUZZ:
            case ATTACK_SNIFF:
            case ATTACK_SCAN:
            case ATTACK_MANA:
            case ATTACK_AP_DOS:
            case ATTACK_AP_CLONE:
                hop_defaults[i] = true;
                break;
            case ATTACK_DEAUTH:
            case ATTACK_MANA_VERBOSE:
            case ATTACK_MANA_LOUD:
            case ATTACK_HANDSHAKE:
            case ATTACK_RANDOMISE_MAC:
            case ATTACK_BT:
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
                hop_millis_defaults[i] = CONFIG_DEFAULT_MANA_HOP_MILLIS;
                break;
            case ATTACK_BEACON:
            case ATTACK_PROBE:
            case ATTACK_FUZZ:
            case ATTACK_SNIFF:
            case ATTACK_SCAN:
            case ATTACK_DEAUTH:
            case ATTACK_AP_DOS:                                     /* where hopping doesn't */
            case ATTACK_AP_CLONE:                                   /* make sense be */
            case ATTACK_RANDOMISE_MAC: 
            case ATTACK_BT:                             /* treated differently somehow? */
                hop_millis_defaults[i] = CONFIG_DEFAULT_HOP_MILLIS;
                break;
            default:
                ESP_LOGE(TAG, "ATTACKS_COUNT has incorrect length");
                free(attack_status);
                free(hop_defaults);
                free(hop_millis_defaults);
                return;
        }
    }
    /* BEACON, PROBE, FUZZ, SNIFF, DEAUTH, MANA, MANA_VERBOSE, AP_DOS, AP_CLONE, SCAN, HANDSHAKE, RAND_MAC */
/*    bool attack_status[ATTACKS_COUNT] = {false, false, false, false, false, false, false, false, false, false, false, false, true};
    bool  hop_defaults[ATTACKS_COUNT] = {true, true, true, true, true, false, false, false, false, false, true, false, false };
*/

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = CONFIG_CONSOLE_MAX_COMMAND_LINE_LENGTH;

    initialize_nvs();

    esp_log_level_set("wifi", ESP_LOG_ERROR); /* YAGNI: Consider reducing these to ESP_LOG_WARN */
    esp_log_level_set("esp_netif_lwip", ESP_LOG_ERROR);

    /* In Flipper mode remove all use of ESP logging except for errors (the line prefix is too long) */
    #ifdef CONFIG_FLIPPER
        esp_log_level_set(BEACON_TAG, ESP_LOG_ERROR);
        esp_log_level_set(TAG, ESP_LOG_ERROR);
        esp_log_level_set(MANA_TAG, ESP_LOG_ERROR);
        esp_log_level_set(HOP_TAG, ESP_LOG_ERROR);
        esp_log_level_set(PROBE_TAG, ESP_LOG_ERROR);
        esp_log_level_set(SCAN_TAG, ESP_LOG_ERROR);
    #endif


#if CONFIG_CONSOLE_STORE_HISTORY
    initialize_filesystem();
    repl_config.history_save_path = HISTORY_PATH;
    #ifndef CONFIG_FLIPPER
        ESP_LOGI(TAG, "Command history enabled");
    #endif
#else
    #ifndef CONFIG_FLIPPER
        ESP_LOGI(TAG, "Command history disabled");
    #endif
#endif

    initialise_wifi();
    /* Register commands */
    esp_console_register_help_command();
    register_system();
    register_wifi();
    register_gravity_commands();
    /*register_nvs();*/

    /* Display the Gravity version */
    #ifdef CONFIG_FLIPPER
        printf("Started Gravity v%s\n\n", GRAVITY_VERSION);
    #else
        ESP_LOGI(TAG, "Started Gravity v%s\n", GRAVITY_VERSION);
    #endif

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
