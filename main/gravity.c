/* ********************** ESP32 Gravity ***********************
 * Gravity started life as a way to learn how to assemble and *
 * send wireless packets. I started with the 'RickRoll' beacon*
 * attack, and continued the theme.                           *
 * I like to think that Gravity differentiates itself from    *
 * Marauder in that Marauder feels to me like it's about      *
 * gathering information - scanning, capturing handshakes,    *
 * etc. - whereas Gravity is all about sending stuff down the *
 * wire. I'm fascinated by the possibilities of combining Mana*
 * with other attacks, so my initial drive was to develop a   *
 * Mana process from the ground up.                           *
 *                                                            *
 * Flipper-Gravity, a companion Flipper Zero app, can be      *
 * downloaded from https://github.com/chris-bc/Flipper-Gravity*
 *                                                            *
 * ESP32-Gravity: https://github.com/chris-bc/esp32-gravity   *
 *                                                            *
 * Licensed under the MIT Open Source License.                *
 **************************************************************/

#include "gravity.h"

#include "beacon.h"
#include "bluetooth.h"
#include "common.h"
#include "deauth.h"
#include "dos.h"
#include "esp_err.h"
#include "esp_wifi_types.h"
#include "freertos/portmacro.h"
#include "fuzz.h"
#include "hop.h"
#include "mana.h"
#include "probe.h"
#include "scan.h"
#include "sdkconfig.h"
#include "sniff.h"
#include "stalk.h"
#include "usage_const.h"

char **user_ssids = NULL;
char **gravityWordList = NULL;
int user_ssid_count = 0;
long ATTACK_MILLIS = CONFIG_DEFAULT_ATTACK_MILLIS;
char FLIPPER_SEPARATOR = '0';

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
esp_err_t ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3){
    return ESP_OK;
}

/* Retrieve the scan result separator from menuconfig and initialise it
   (It's #define'd as a string and I want it as a char!)
*/
esp_err_t initFlipperSep() {
    esp_err_t err = ESP_OK;
    if (FLIPPER_SEPARATOR != '0') {
        /* Has already been initialised */
        return err;
    }
    #ifdef CONFIG_FLIPPER_SEPARATOR
        char *temp = malloc(sizeof(char) * (strlen(CONFIG_FLIPPER_SEPARATOR) + 1));
        if (temp == NULL) {
            printf("%sto set separator\n", STRINGS_MALLOC_FAIL);
            return ESP_ERR_NO_MEM;
        }
        strcpy(temp, CONFIG_FLIPPER_SEPARATOR);
        if (temp[0] == '\0') {
            printf("Error: CONFIG_FLIPPER_SEPARATOR (%s) appears to be NULL\n", CONFIG_FLIPPER_SEPARATOR);
            free(temp);
            return ESP_ERR_INVALID_ARG;
        }
        FLIPPER_SEPARATOR = temp[0];
    #else
        /* Arbitrary default */
        FLIPPER_SEPARATOR = '~';
    #endif

    /* Update SELECT usage string with the correct separator */
    SHORT_SELECT[strlen(SHORT_SELECT) - 1] = FLIPPER_SEPARATOR;

    return err;
}

/* Functions to manage target-ssids */
int countSsid() {
	return user_ssid_count;
}

char **lsSsid() {
	return user_ssids;
}

/* The prototype for these functions are in beacon.h because if they're in any
   other file beacon.h won't find them.
*/
esp_err_t addSsid(char *ssid) {
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

esp_err_t rmSsid(char *ssid) {
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
/* The bluetooth command may not be needed for quite some time now it's done as a PoC
   Future expansion of the BT module to allow interactive exploration of the airspace would be neat.
*/
esp_err_t cmd_bluetooth(int argc, char **argv) {
    esp_err_t err = ESP_OK;
    #if defined(CONFIG_IDF_TARGET_ESP32)
        //err |= gravity_ble_test();
        printf("BT Test harness currently inactive.\n");
    #else
        displayBluetoothUnsupported();
    #endif
    return err;
}

/* Manually execute purge methods
   Usage: purge [ AP | STA | BT | BLE ]+ [ RSSI [ <maxRSSI> ] | AGE [ <minAge> ] | UNNAMED | UNSELECTED | NONE ]+
*/
esp_err_t cmd_purge(int argc, char **argv) {
    #if defined(CONFIG_IDF_TARGET_ESP32)
        const char PURGE_TAG[] = "purge@GRAVITY";
        /* Parameter validation */
        if (argc == 1) {
            /* Return purge settings */
            char strStrat[56];
            purgeStrategyToString(purgeStrategy, strStrat);
            #ifdef CONFIG_FLIPPER
                printf("Purge Configuration\nMin Age: %u\nMax RSSI: %ld\nPurge Strategies:\n%s\n", PURGE_MIN_AGE, PURGE_MAX_RSSI, strStrat);
            #else
                ESP_LOGI(PURGE_TAG, "Purge Minimum Age: %u\tPurge Maximum RSSI: %ld", PURGE_MIN_AGE, PURGE_MAX_RSSI);
                ESP_LOGI(PURGE_TAG, "Active Purge Strategies:");
                ESP_LOGI(PURGE_TAG, "%s", strStrat);
            #endif
            return ESP_OK;
        }
        if (argc > 10) {
            #ifdef CONFIG_FLIPPER
                printf("%s\n", SHORT_PURGE);
            #else
                ESP_LOGE(PURGE_TAG, "%s", SHORT_PURGE);
            #endif
            return ESP_ERR_INVALID_ARG;
        }
        GravityDeviceType devices = 0;
        /* Process device arguments */
        uint8_t argIdx = 1;
        while (argIdx < argc && !(strcasecmp(argv[argIdx], "AP") && strcasecmp(argv[argIdx], "STA") &&
                strcasecmp(argv[argIdx], "BT") && strcasecmp(argv[argIdx], "BLE"))) {
            if (!strcasecmp(argv[argIdx], "AP")) {
                devices = devices | GRAVITY_DEV_AP;
                #ifdef CONFIG_DEBUG
                    #ifdef CONFIG_FLIPPER
                        printf("Purge: Selected AP\n");
                    #else
                        ESP_LOGI(PURGE_TAG, "Purge: Selected AP");
                    #endif
                #endif
            } else if (!strcasecmp(argv[argIdx], "STA")) {
                devices = devices | GRAVITY_DEV_STA;
                #ifdef CONFIG_DEBUG
                    #ifdef CONFIG_FLIPPER
                        printf("Purge: Selected STA\n");
                    #else
                        ESP_LOGI(PURGE_TAG, "Purge: Selected STA");
                    #endif
                #endif
            } else if (!strcasecmp(argv[argIdx], "BT")) {
                devices = devices | GRAVITY_DEV_BT;
                #ifdef CONFIG_DEBUG
                    #ifdef CONFIG_FLIPPER
                        printf("Purge: Selected Bluetooth\n");
                    #else
                        ESP_LOGI(PURGE_TAG, "Purge: Selected Bluetooth");
                    #endif
                #endif
            } else if (!strcasecmp(argv[argIdx], "BLE")) {
                devices = devices | GRAVITY_DEV_BLE;
                #ifdef CONFIG_DEBUG
                    #ifdef CONFIG_FLIPPER
                        printf("Purge: Selected BLE\n");
                    #else
                        ESP_LOGI(PURGE_TAG, "Purge: Selected Bluetooth Low Energy");
                    #endif
                #endif
            }
            ++argIdx;
        }
        if (argIdx == 1) {
            /* No device parameters were provided */
            #ifdef CONFIG_FLIPPER
                printf("No devices specified.\nSelecting all devices.\n");
            #else
                ESP_LOGI(PURGE_TAG, "No devices specified. Selecting all devices.");
            #endif
            devices = GRAVITY_DEV_AP | GRAVITY_DEV_STA | GRAVITY_DEV_BT | GRAVITY_DEV_BLE;
        }
        /* Process purge method arguments */
        gravity_bt_purge_strategy_t strat = GRAVITY_BLE_PURGE_IDLE;
        int8_t rssiSpec = PURGE_MAX_RSSI;
        unsigned long ageSpec = PURGE_MIN_AGE;
        while (argIdx < argc && !(strcasecmp(argv[argIdx], "RSSI") && strcasecmp(argv[argIdx], "AGE") &&
                strcasecmp(argv[argIdx], "UNNAMED") && strcasecmp(argv[argIdx], "UNSELECTED") && strcasecmp(argv[argIdx], "NONE"))) {
            if (!strcasecmp(argv[argIdx], "RSSI")) {
                strat = strat | GRAVITY_BLE_PURGE_RSSI;
                if (atol(argv[argIdx + 1]) != 0) {
                    /* RSSI arg was provided */
                    rssiSpec = atol(argv[argIdx + 1]);
                    ++argIdx;
                }
            } else if (!strcasecmp(argv[argIdx], "AGE")) {
                strat = strat | GRAVITY_BLE_PURGE_AGE;
                if (atol(argv[argIdx + 1]) != 0) {
                    /* AGE arg was provided */
                    ageSpec = atol(argv[argIdx + 1]);
                    ++argIdx;
                }
            } else if (!strcasecmp(argv[argIdx], "UNNAMED")) {
                strat = strat | GRAVITY_BLE_PURGE_UNNAMED;
            } else if (!strcasecmp(argv[argIdx], "UNSELECTED")) {
                strat = strat | GRAVITY_BLE_PURGE_UNSELECTED;
            } else if (!strcasecmp(argv[argIdx], "NONE")) {
                strat = strat | GRAVITY_BLE_PURGE_NONE;
            } else {
                #ifdef CONFIG_FLIPPER
                    printf("Invalid Purge Method:\n%30s\n", argv[argIdx]);
                #else
                    ESP_LOGE(PURGE_TAG, "Invalid Purge Method specified: \"%s\"", argv[argIdx]);
                #endif
                return ESP_ERR_INVALID_ARG;
            }
            ++argIdx;
        }
        /* Note we're checking equality here, not & - IDLE is 0 */
        if (strat == GRAVITY_BLE_PURGE_IDLE) {
            /* No purge methods specified - Is a default one specified? */
            strat = purgeStrategy;
            if (strat == GRAVITY_BLE_PURGE_IDLE || strat == GRAVITY_BLE_PURGE_NONE) {
                #ifdef CONFIG_FLIPPER
                    printf("No Purge Methods specified.\nNo default purge methods.\n");
                #else
                    ESP_LOGE(PURGE_TAG, "No Purge Methods were specified and Gravity has not been configured with default Purge Methods.");
                #endif
                return ESP_ERR_INVALID_ARG;
            } else {
                char strStrat[56];
                purgeStrategyToString(strat, strStrat);
                #ifdef CONFIG_FLIPPER
                    printf("No Purge Methods specified.\nUsing defaults:\n%s\n", strStrat);
                #else
                    ESP_LOGI(PURGE_TAG, "No Purge Methods specified, using Gravity default Purge Methods:");
                    ESP_LOGI(PURGE_TAG, "%s", strStrat);
                #endif
            }
        } else {
            char strStrat[56];
            purgeStrategyToString(strat, strStrat);
            #ifdef CONFIG_FLIPPER
                printf("Using Purge Methods:\n%s\n", strStrat);
            #else
                ESP_LOGI(PURGE_TAG, "Using Purge Methods: %s", strStrat);
            #endif
        }
        /* Now we know the devices and purge methods to use, get on with the purging! */
        esp_err_t err = ESP_OK;
        if ((devices & GRAVITY_DEV_AP) == GRAVITY_DEV_AP) {
            /* Purge Wireless Access Points */
            err |= purgeAP(strat, ageSpec, rssiSpec);
        }
        if ((devices & GRAVITY_DEV_STA) == GRAVITY_DEV_STA) {
            /* Purge wireless STAtions */
            err |= purgeSTA(strat, ageSpec, rssiSpec);
        }
        if ((devices & GRAVITY_DEV_BT) == GRAVITY_DEV_BT) {
            err |= purgeBT(strat, ageSpec, rssiSpec);
        }
        if ((devices & GRAVITY_DEV_BLE) == GRAVITY_DEV_BLE) {
            err |= purgeBLE(strat, ageSpec, rssiSpec);
        }
        return err;
    #else
        displayBluetoothUnsupported();
        return ESP_ERR_NOT_SUPPORTED;
    #endif
}

/* Display version info for esp32-Gravity */
esp_err_t cmd_version(int argc, char **argv) {
    esp_err_t err = ESP_OK;

    #ifdef CONFIG_FLIPPER
        printf("esp32-Gravity v%s\n", GRAVITY_VERSION);
    #else
        ESP_LOGI(TAG, "esp32-Gravity v%s\n", GRAVITY_VERSION);
    #endif
    return err;
}

/* Display help information for the specified command.
   Permit the specification of multiple commands, skip any
   that are invalid.
   USage: info <command>+
*/
esp_err_t cmd_info(int argc, char **argv) {
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
esp_err_t cmd_fuzz(int argc, char **argv) {
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
    /* Specifying packetType can be skipped provided it's already set, specifying mode cannot */
    if ((newPacketType == FUZZ_PACKET_NONE && fuzzPacketType == FUZZ_PACKET_NONE) ||
            newFuzzMode == FUZZ_MODE_NONE || argc > 5) {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_FUZZ);
        #else
            ESP_LOGE(FUZZ_TAG, "Invalid arguments provided.\n%s", USAGE_FUZZ);
        #endif
        return ESP_ERR_INVALID_ARG;
    }

    // Update attack_status[ATTACK_BEACON] appropriately
    attack_status[ATTACK_FUZZ] = strcasecmp(argv[argc - 1], "OFF");

    /* Start/stop hopping as necessary */
    err = setHopForNewCommand();
    if (err != ESP_OK) {
        #ifdef CONFIG_FLIPPER
            printf("%s%s\n",STRINGS_HOP_STATE_FAIL, esp_err_to_name(err));
        #else
            ESP_LOGW(HOP_TAG, "%s%s", STRINGS_HOP_STATE_FAIL, esp_err_to_name(err));
        #endif
    }

    if (attack_status[ATTACK_FUZZ]) {
        return fuzz_start(newFuzzMode, newPacketType);
    } else {
        return fuzz_stop();
    }
}

/* Control channel hopping
   Usage: hop [ MILLIS ] [ ON | OFF | DEFAULT | KILL ] [ SEQUENTIAL | RANDOM ]
   Not specifying a parameter will report the status. KILL terminates the event loop.
*/
esp_err_t cmd_hop(int argc, char **argv) {
    if (argc > 4) {
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
        /* argv[1] could be a duration, "on", "default", "off", "kill", "sequential" or "random" */
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
            createHopTaskIfNeeded();
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
            createHopTaskIfNeeded();
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

esp_err_t cmd_commands(int argc, char **argv) {
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

esp_err_t cmd_beacon(int argc, char **argv) {
    /* Usage beacon [ rickroll | random [ count ] | infinite | target-ssids | aps | off ] [ AUTH ( OPEN | WPA )+ ] */
    /* Initially the 'TARGET MAC' argument is unsupported, the attack only supports broadcast beacon frames */
    
    /* Validate arguments:
       * 1: Status
       * 2 onwards: loop through looking for specified keywords.
          * If RANDOM is found then look for and handle COUNT at the same time
          * If AUTH is found then look for and handle authType at the same time
       * Absolute maximum argc: beacon random 42 auth open wpa 
    */
    if (argc > 6) {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_BEACON);
        #else
            ESP_LOGE(TAG, "Invalid arguments specified. Expected fewer than six, received %d.", argc - 1);
        #endif
        return ESP_ERR_INVALID_ARG;
    }
    if (argc == 1) {
        return beacon_status();
    }

    esp_err_t ret = ESP_OK;
    // Update attack_status[ATTACK_BEACON] appropriately
    if (!strcasecmp(argv[1], "off")) {
        attack_status[ATTACK_BEACON] = false;
    } else {
        attack_status[ATTACK_BEACON] = true;
    }

    /* Start/stop hopping task loop as needed */
    esp_err_t err = setHopForNewCommand();
    if (err != ESP_OK) {
        #ifdef CONFIG_FLIPPER
            printf("%s%s\n", STRINGS_HOP_STATE_FAIL, esp_err_to_name(err));
        #else
            ESP_LOGW(HOP_TAG, "%s%s", STRINGS_HOP_STATE_FAIL, esp_err_to_name(err));
        #endif
    }

    /* What auth type(s) are we using? */
    PROBE_RESPONSE_AUTH_TYPE specAuthType = 0;
    int authIdx = 0;
    for (authIdx = 1; authIdx < (argc - 1) && strcasecmp(argv[authIdx], "AUTH"); ++authIdx) { }
    if (authIdx == (argc - 1)) {
        /* Not present - Use default */
        specAuthType = AUTH_TYPE_NONE;
    } else {
        /* Found "AUTH" in argument authIdx - Loop through subsequent arguments collecting valid authTypes */
        int typeIdx = authIdx + 1;
        while (typeIdx < argc) {
            if (!strcasecmp(argv[typeIdx], "OPEN")) {
                /* Look for open networks */
                specAuthType |= AUTH_TYPE_NONE;
            } else if (!strcasecmp(argv[typeIdx], "WPA")) {
                /* Look for secured networks */
                specAuthType |= AUTH_TYPE_WPA;
            } else if (specAuthType == 0) {
                #ifdef CONFIG_FLIPPER
                    printf("Invalid argument \"%s\"\n", argv[typeIdx]);
                #else
                    ESP_LOGE(BEACON_TAG, "Invalid argument \"%s\"", argv[typeIdx]);
                #endif
                return ESP_ERR_INVALID_ARG;
            }
            ++typeIdx;
        }
    }
    /* specAuthType should now contain a valid authType based on user input */

    /* Handle arguments to beacon */
    int ssidCount = DEFAULT_SSID_COUNT;
    if (!strcasecmp(argv[1], "rickroll")) {
        ret = beacon_start(ATTACK_BEACON_RICKROLL, &specAuthType, 1, 0);
    } else if (!strcasecmp(argv[1], "random")) {
        if (SSID_LEN_MIN == 0) {
            SSID_LEN_MIN = CONFIG_SSID_LEN_MIN;
        }
        if (SSID_LEN_MAX == 0) {
            SSID_LEN_MAX = CONFIG_SSID_LEN_MAX;
        }
        if (argc >= 3) {
            ssidCount = atoi(argv[2]);
            if (ssidCount == 0) {
                ssidCount = DEFAULT_SSID_COUNT;
            }
        }
        ret = beacon_start(ATTACK_BEACON_RANDOM, &specAuthType, 1, ssidCount);
    } else if (!strcasecmp(argv[1], "target-ssids")) {
        ret = beacon_start(ATTACK_BEACON_USER, &specAuthType, 1, 0);
    } else if (!strcasecmp(argv[1], "aps")) {
        ret = beacon_start(ATTACK_BEACON_AP, &specAuthType, 1, 0);
    } else if (!strcasecmp(argv[1], "infinite")) {
        ret = beacon_start(ATTACK_BEACON_INFINITE, &specAuthType, 1, 0);
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
esp_err_t cmd_target_ssids(int argc, char **argv) {
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

esp_err_t cmd_probe(int argc, char **argv) {
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
        return display_probe_status();
    }

    /* Set attack_status[ATTACK_PROBE] before checking channel hopping or starting/stopping */
    attack_status[ATTACK_PROBE] = strcasecmp(argv[1], "OFF");

    /* Start hopping task loop if hopping is on by default */
    esp_err_t err = setHopForNewCommand();
    if (err != ESP_OK) {
        #ifdef CONFIG_FLIPPER
            printf("%s%s\n", STRINGS_HOP_STATE_FAIL, esp_err_to_name(err));
        #else
            ESP_LOGW(HOP_TAG, "%s%s", STRINGS_HOP_STATE_FAIL, esp_err_to_name(err));
        #endif
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

esp_err_t cmd_sniff(int argc, char **argv) {
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
    esp_err_t err = setHopForNewCommand();
    if (err != ESP_OK) {
        #ifdef CONFIG_FLIPPER
            printf("%s%s\n", STRINGS_HOP_STATE_FAIL, esp_err_to_name(err));
        #else
            ESP_LOGW(HOP_TAG, "%s%s", STRINGS_HOP_STATE_FAIL, esp_err_to_name(err));
        #endif
    }
    return ESP_OK;
}

esp_err_t cmd_deauth(int argc, char **argv) {
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
        display_deauth_status();
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
    esp_err_t err = setHopForNewCommand();
    if (err != ESP_OK) {
        #ifdef CONFIG_FLIPPER
            printf("%s%s\n", STRINGS_HOP_STATE_FAIL, esp_err_to_name(err));
        #else
            ESP_LOGW(HOP_TAG, "%s%s", STRINGS_HOP_STATE_FAIL, esp_err_to_name(err));
        #endif
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
   Usage: mana ( CLEAR | ( [ VERBOSE ] [ ON | OFF ] ) | ( AUTH [ NONE | WEP | WPA ] ) | ( LOUD [ ON | OFF ] ) )
   CLEAR   :  Erase Preferred Network Lists (PNLs) cached by Mana
   VERBOSE :  Display messages as packets are sent and received,
              providing attack status
   ON | OFF:  Start or stop either the Mana attack or verbose logging
   AUTH    :  Set or display auth method
   LOUD    :  Enable/Disable the "Loud Mana" variant of this attack.

   TODO :  Display status of attack - Number of responses sent,
           number of association attempts, number of successful
           associations.

 */
esp_err_t cmd_mana(int argc, char **argv) {
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
        mana_display_status();
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
    esp_err_t err = setHopForNewCommand();
    if (err != ESP_OK) {
        #ifdef CONFIG_FLIPPER
            printf("%s%s\n", STRINGS_HOP_STATE_FAIL, esp_err_to_name(err));
        #else
            ESP_LOGW(HOP_TAG, "%s%s", STRINGS_HOP_STATE_FAIL, esp_err_to_name(err));
        #endif
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

/* Start/Stop the homing feature (stalking is such an emotive word) */
/* Usage : stalk [ ON | OFF ]
   This feature monitors the RSSI of selected wireless radios, presenting both unit
   and aggregate data in an effort to simplify following a set of signals to their source
*/
esp_err_t cmd_stalk(int argc, char **argv) {
    if (argc > 2 || (argc == 2 && strcasecmp(argv[1], "ON") && strcasecmp(argv[1], "OFF"))) {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_STALK);
        #else
            ESP_LOGE(STALK_TAG, "%s", USAGE_STALK);
        #endif
        return ESP_ERR_INVALID_ARG;
    }
    if (argc == 1) {
        #ifdef CONFIG_FLIPPER
            printf("Gravity-Homing is %sRunning\n", attack_status[ATTACK_STALK]?"":"Not ");
        #else
            ESP_LOGI(STALK_TAG, "Gravity-Homing is %sRunning", attack_status[ATTACK_STALK]?"":"Not ");
        #endif
        return ESP_OK;
    }
    /* If we reach this point argv == 2 */
    /* Only include variables from the Bluetooth module if Bluetooth is supported */
    #if defined(CONFIG_IDF_TARGET_ESP32)
        if (gravity_sel_ap_count == 0 && gravity_sel_sta_count == 0 && gravity_sel_bt_count == 0) {
    #else
        if (gravity_sel_ap_count == 0 && gravity_sel_sta_count == 0) {
    #endif
        #ifdef CONFIG_FLIPPER
            printf("You don't have any selected APs or STAs, you probably don't want to stalk those nonexistent things.");
        #else
            ESP_LOGI(TAG, "No selectedAPs or selectedSTAs, you need some of those to do some stalking.");
        #endif
    }
    attack_status[ATTACK_STALK] = strcasecmp(argv[1], "OFF");
    if (attack_status[ATTACK_STALK]) {
        stalk_begin();
    } else {
        /* Pause for 500 ms */
        vTaskDelay(500/ portTICK_PERIOD_MS);
        stalk_end();
    }

    return ESP_OK;
}

/* Enable or disable AP Denial-of-service attack mode */
/* Usage ap-dos [ ON | OFF ] */
esp_err_t cmd_ap_dos(int argc, char **argv) {
    if (argc > 2 || (argc == 2 && strcasecmp(argv[1], "ON") && strcasecmp(argv[1], "OFF"))) {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_AP_DOS);
        #else
            ESP_LOGE(DOS_TAG, "Invalid parameters. Usage: %s", USAGE_AP_DOS);
        #endif
    }
    if (argc == 1) {
        return dos_display_status();
    }
    /* Update attack_status[] */
    attack_status[ATTACK_AP_DOS] = strcasecmp(argv[1], "OFF");

    /* Start/Stop hopping task loop if hopping is on by default */
    esp_err_t err = setHopForNewCommand();
    if (err != ESP_OK) {
        #ifdef CONFIG_FLIPPER
            printf("%s%s\n", STRINGS_HOP_STATE_FAIL, esp_err_to_name(err));
        #else
            ESP_LOGW(HOP_TAG, "%s%s", STRINGS_HOP_STATE_FAIL, esp_err_to_name(err));
        #endif
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

/* AP-Clone :  Composite denial of service attack
   USAGE    :  ap-clone [ ( ON | OFF ) ( OPEN | WEP | WPA )+ ]
*/
esp_err_t cmd_ap_clone(int argc, char **argv) {
    if (argc > 5 || (argc >= 2 && strcasecmp(argv[1], "ON") && strcasecmp(argv[1], "OFF")) ||
            (argc >= 3 && strcasecmp(argv[2], "OPEN") && strcasecmp(argv[2], "WEP") &&
                strcasecmp(argv[2], "WPA")) || (argc >= 4 && strcasecmp(argv[3], "OPEN") &&
                strcasecmp(argv[3], "WEP") && strcasecmp(argv[3], "WPA")) || (argc == 5 &&
                strcasecmp(argv[4], "OPEN") && strcasecmp(argv[4], "WEP") && strcasecmp(argv[4], "WPA"))) {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_AP_CLONE);
        #else
            ESP_LOGE(DOS_TAG, "Invalid arguments: %s", USAGE_AP_DOS);
        #endif
        return ESP_ERR_INVALID_ARG;
    }

    if (argc == 1) {
        return clone_display_status();
    }
    PROBE_RESPONSE_AUTH_TYPE cloneAuthType = 0;
    for (int i = 2; i < argc; ++i) {
        if (!strcasecmp(argv[i], "OPEN")) {
            cloneAuthType |= AUTH_TYPE_NONE;
        } else if (!strcasecmp(argv[i], "WEP")) {
            cloneAuthType |= AUTH_TYPE_WEP;
        } else if (!strcasecmp(argv[i], "WPA")) {
            cloneAuthType |= AUTH_TYPE_WPA;
        } else {
            #ifdef CONFIG_FLIPPER
                printf("Invalid argument \"%s\"\n%s\n", argv[i], SHORT_AP_CLONE);
            #else
                ESP_LOGE(DOS_TAG, "Invalid argument provided: \"%s\"\n%s", argv[i], USAGE_AP_CLONE);
            #endif
            return ESP_ERR_INVALID_ARG;
        }
    }
    if (cloneAuthType == 0 && strcasecmp(argv[1], "OFF")) {
        #ifdef CONFIG_FLIPPER
            printf("Error: No auth type specified.\n%s\n", SHORT_AP_CLONE);
        #else
            ESP_LOGE(DOS_TAG, "No auth type specified.\n%s", USAGE_AP_CLONE);
        #endif
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = cloneStartStop(strcasecmp(argv[1], "OFF"), cloneAuthType);

    return err;
}

/* Usage: SCAN [ ( [ <ssid> ] WIFI ) | BT | BLE [ PURGE ( RSSI [ <rssiSpec> ] | AGE [ <ageSpec> ] | UNNAMED | UNSELECTED | NONE )+ ] | OFF ]*/
esp_err_t cmd_scan(int argc, char **argv) {
    if (argc > 9 || (argc == 2 && strcasecmp(argv[1], "WIFI") && strcasecmp(argv[1], "OFF") &&
    /* The following addition validates Bluetooth Classic scanning arguments i.e. scan BT ( DISCOVER | SNIFF | PROBE ) */
                strcasecmp(argv[1], "BT") && strcasecmp(argv[1], "BLE") && strlen(argv[1]) > 32) ||
                // TODO: The following lines have been remediated to support SCAN BT SERVICES [selected]
                // Validate them
                // argc=3 && (!wifi && (BT && !services)) orr (argc>3 & ( (!BLE | !PURGE) & (!BT | !SERVICES | !selected))
            (argc == 3 && ( (!strcasecmp(argv[1], "BT") && strcasecmp(argv[2], "SERVICES")) || (strcasecmp(argv[1], "BT") && strcasecmp(argv[2], "WIFI")))) ||
            (argc > 3 && ( (!strcasecmp(argv[1], "BLE") && strcasecmp(argv[2], "PURGE")) || (!strcasecmp(argv[1], "BT") && (strcasecmp(argv[2], "SERVICES") || strcasecmp(argv[3], "SELECTED")))))) {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_SCAN);
        #else
            ESP_LOGE(TAG, "%s", USAGE_SCAN);
        #endif
        return ESP_ERR_INVALID_ARG;
    }
    /* Now validate BLE purge parameters */
    if (argc > 3 && !strcasecmp(argv[2], "PURGE")) {
        uint8_t argCount = 3;
        for ( ; argCount < argc && !(strcasecmp(argv[argCount], "RSSI") && strcasecmp(argv[argCount], "AGE") && atol(argv[argCount]) == 0 &&
                strcasecmp(argv[argCount], "UNNAMED") && strcasecmp(argv[argCount], "UNSELECTED") && strcasecmp(argv[argCount], "NONE")); ++argCount) { }
        if (argCount < argc) {
            /* Dropped out of the loop before processing all args - Invalid arg */
            #ifdef CONFIG_FLIPPER
                printf("Invalid argument: %s\nUsage: %s\n", argv[argCount], SHORT_SCAN);
            #else
                ESP_LOGE(SCAN_TAG, "Invalid argument %s", argv[argCount]);
                ESP_LOGE(SCAN_TAG, "Usage: %s", SHORT_SCAN);
            #endif
            return ESP_ERR_INVALID_ARG;
        }
    }

    esp_err_t err = ESP_OK;
    if (argc == 1) {
        err = scan_display_status();
        #if defined(CONFIG_IDF_TARGET_ESP32)
            err = gravity_bt_scan_display_status();
        #endif
        /* Also display usage just in case */
        printf("\n");
        #ifdef CONFIG_FLIPPER
            printf("Usage: %s\n", SHORT_SCAN);
        #else
            ESP_LOGI(SCAN_TAG, "Usage: %s", SHORT_SCAN);
        #endif
        return err;
    }

    /* Zero out SSID filter */
    memset(scan_filter_ssid, '\0', 33);
    memset(scan_filter_ssid_bssid, 0x00, 6);

    if (!strcasecmp(argv[1], "WIFI")) {
        attack_status[ATTACK_SCAN] = true;
    } else if (!strcasecmp(argv[1], "OFF")) {
        #if defined(CONFIG_IDF_TARGET_ESP32)
            err = gravity_bt_disable_scan();
            if (err != ESP_OK) {
                #ifdef CONFIG_FLIPPER
                    printf("Error disabling BT scan: %s.\n", esp_err_to_name(err));
                #else
                    ESP_LOGW(BT_TAG, "Bluetooth scanning could not be cancelled, awaiting timeout: %s.", esp_err_to_name(err));
                #endif
            }
        #endif
        attack_status[ATTACK_SCAN] = false;
        attack_status[ATTACK_SCAN_BT_CLASSIC] = false;
        attack_status[ATTACK_SCAN_BLE] = false;
    } else if (!strcasecmp(argv[1], "BT")) {
        /* Initialise BT Classic mode and start discovery */
        #if defined(CONFIG_IDF_TARGET_ESP32)
            /* Scanning for devices or services? */
            if (argc >= 3 && !strcasecmp(argv[2], "SERVICES")) {
                bool selected = (argc > 3 && !strcasecmp(argv[3], "SELECTED"));
                if (selected) {
                    err |= gravity_bt_discover_selected_services();
                } else {
                    err |= gravity_bt_discover_all_services();
                }
            } else {
                err |= gravity_bt_gap_start();
                attack_status[ATTACK_SCAN_BT_CLASSIC] = true;
            }
        #else
            displayBluetoothUnsupported();
        #endif
    } else if (!strcasecmp(argv[1], "BLE")) {
        /* Initialise BT monitor mode and identify devices */
        #if defined(CONFIG_IDF_TARGET_ESP32)
            /* Do we have purge arguments? */
            gravity_bt_purge_strategy_t purgeStrat = 0;
            if (argc > 3 && !strcasecmp(argv[1], "BLE") && !strcasecmp(argv[2], "PURGE")) {
                for (int i = 3; i < argc; ++i) {
                    if (!strcasecmp(argv[i], "RSSI")) {
                        purgeStrat += GRAVITY_BLE_PURGE_RSSI;
                        if (atol(argv[i + 1]) != 0) {
                            PURGE_MAX_RSSI = atol(argv[i + 1]);
                            ++i;
                        }
                    } else if (!strcasecmp(argv[i], "AGE")) {
                        purgeStrat += GRAVITY_BLE_PURGE_AGE;
                        if (argc > (i + 1) && atol(argv[i + 1]) != 0) {
                            PURGE_MIN_AGE = atol(argv[i + 1]);
                            ++i;
                        }
                    } else if (!strcasecmp(argv[i], "UNNAMED")) {
                        purgeStrat += GRAVITY_BLE_PURGE_UNNAMED;
                    } else if (!strcasecmp(argv[i], "UNSELECTED")) {
                        purgeStrat += GRAVITY_BLE_PURGE_UNSELECTED;
                    } else if (!strcasecmp(argv[i], "NONE")) {
                        purgeStrat += GRAVITY_BLE_PURGE_NONE;
                    }
                }
                if (purgeStrat != GRAVITY_BLE_PURGE_NONE && ((purgeStrat & GRAVITY_BLE_PURGE_NONE) == GRAVITY_BLE_PURGE_NONE)) {
                    /* Purge strategy NONE was selected, along with at least one other. */
                    #ifdef CONFIG_FLIPPER
                        printf("Cannot combine \"NONE\" purge strategy with another purge strategy.\n");
                    #else
                        ESP_LOGE(SCAN_TAG, "You have specified the \"NONE\" purge strategy in addition to at least one other purge strategy. Doing something and nothing at the same time doesn't make sense to me, sorry.");
                    #endif
                    return ESP_ERR_INVALID_ARG;
                }
            }
            if (purgeStrat == 0) {
                if (purgeStrategy != 0) {
                    purgeStrat = purgeStrategy;
                } else {
                    purgeStrat = GRAVITY_BLE_PURGE_NONE;
                }
            }
            char strStrat[56];
            purgeStrategyToString(purgeStrat, strStrat);
            #ifdef CONFIG_FLIPPER
                printf("Starting BLE scanning.\nOut-of-memory purge methods:\n%s\n", strStrat);
            #else
                ESP_LOGI(BT_TAG, "Starting BLE scanning. Purge Methods if out of memory:");
                ESP_LOGI(BT_TAG, "%s", strStrat);
            #endif
            /* Now start BLE scanning */
            attack_status[ATTACK_SCAN_BLE] = true;
            err |= gravity_ble_scan_start(purgeStrat);
        #else
            displayBluetoothUnsupported();
        #endif
    } else {
        /* Use argv[1] as an SSID filter */
        attack_status[ATTACK_SCAN] = true;
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
    err |= setHopForNewCommand();
    if (err != ESP_OK) {
        #ifdef CONFIG_FLIPPER
            printf("%s%s\n", STRINGS_HOP_STATE_FAIL, esp_err_to_name(err));
        #else
            ESP_LOGW(HOP_TAG, "%s%s", STRINGS_HOP_STATE_FAIL, esp_err_to_name(err));
        #endif
    }

    return ESP_OK;
}

/* Set application configuration variables */
/* At the moment these config settings are not retained between
   Gravity instances. Many of them are initialised in beacon.h.
   Usage: set <variable> <value>
   Allowed values for <variable> are:
      SCRAMBLE_WORDS, SSID_LEN_MIN, SSID_LEN_MAX, DEFAULT_SSID_COUNT, CHANNEL,
      MAC, ATTACK_PKTS, ATTACK_MILLIS, MAC_RAND, EXPIRY, HOP_MODE, SCRAMBLE_WORDS,
      BLE_PURGE_STRAT, BLE_PURGE_MAX_RSSI, BLE_PURGE_MIN_AGE */
/* Channel hopping is not catered for in this feature */
esp_err_t cmd_set(int argc, char **argv) {
    if (argc != 3) {
        #ifdef CONFIG_FLIPPER
            printf("%s\nSCRAMBLE_WORDS,\nSSID_LEN_MIN,\nSSID_LEN_MAX,\nDEFAULT_SSID_COUNT,\nCHANNEL,ATTACK_PKTS,\nATTACK_MILLIS,MAC,\nMAC_RAND,EXPIRY,\nHOP_MODE,\nSCRAMBLE_WORDS,\nBLE_PURGE_STRAT,\nBLE_PURGE_MAX_RSSI,\nBLE_PURGE_MIN_AGE\n", SHORT_SET);
        #else
            ESP_LOGE(TAG, "%s", USAGE_SET);
            ESP_LOGE(TAG, "<variable> : SSID_LEN_MIN | SSID_LEN_MAX | DEFAULT_SSID_COUNT | CHANNEL | HOP_MODE |");
            ESP_LOGE(TAG, "             MAC | ATTACK_PKTS | ATTACK_MILLIS | MAC_RAND | EXPIRY | SCRAMBLE_WORDS |");
            ESP_LOGE(TAG, "             BLE_PURGE_STRAT | BLE_PURGE_MAX_RSSI | BLE_PURGE_MIN_AGE");
        #endif
        return ESP_ERR_INVALID_ARG;
    }
    if (!strcasecmp(argv[1], "WIFI")) {
        initPromiscuous();
    } else if (!strcasecmp(argv[1], "SCRAMBLE_WORDS")) {
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
        wifi_second_chan_t chan2 = WIFI_SECOND_CHAN_NONE;
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
                printf("\n\nNOTE: Mac Randomisation may be causing packet loss\non ESP32 devices. Currently being investigated.\n");
            #else
                ESP_LOGE(TAG, "Usage: set MAC_RAND [ ON | OFF ]");
                printf("\n\n");
                ESP_LOGW(TAG, "NOTE: MAC randomisation may be causing packet loss on ESP32 devices. Currently being looked into.");
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
    } else if (!strcasecmp(argv[1], "BLE_PURGE_STRAT")) {
        /* Set the purge strategy in a way that's not a nightmare for users
            RSSI = 1,
            AGE = 2,
            UNNAMED = 4,
            UNSELECTED = 8,
            NONE = 16
           Allow either a set of strings or a number to be specified
           Or SET BLE_PURGE_STRAT RSSI UNNAMED AGE UNSELECTED
        */
        #if defined(CONFIG_IDF_TARGET_ESP32)
            if (argc < 3) {
                #ifdef CONFIG_FLIPPER
                    printf("%s\n", SHORT_BT_STRAT);
                #else
                    ESP_LOGE(BT_TAG, "%s", USAGE_BT_STRAT);
                #endif
                return ESP_ERR_INVALID_ARG;
            }
            /* If argv[2] is a number then there can only be 3 arguments */
            int stratSpec = atol(argv[2]);
            if (stratSpec == 0) {
                /* Build stratSpec from string arguments */
                for (int i = 2; i < argc; ++i) {
                    if (!strcasecmp(argv[i], "RSSI")) {
                        stratSpec = stratSpec | GRAVITY_BLE_PURGE_RSSI;
                    } else if (!strcasecmp(argv[i], "AGE")) {
                        stratSpec = stratSpec | GRAVITY_BLE_PURGE_AGE;
                    } else if (!strcasecmp(argv[i], "UNNAMED")) {
                        stratSpec = stratSpec | GRAVITY_BLE_PURGE_UNNAMED;
                    } else if (!strcasecmp(argv[i], "UNSELECTED")) {
                        stratSpec = stratSpec | GRAVITY_BLE_PURGE_UNSELECTED;
                    } else if (!strcasecmp(argv[i], "NONE")) {
                        stratSpec = stratSpec | GRAVITY_BLE_PURGE_NONE;
                    } else {
                        #ifdef CONFIG_FLIPPER
                            printf("Invalid purge method:\n%35s\nValid: RSSI, AGE, UNNAMED, UNSELECTED, NONE\n", argv[i]);
                        #else
                            ESP_LOGE(BT_TAG, "Invalid purge method \"%s\" specified.", argv[i]);
                        #endif
                        return ESP_ERR_INVALID_ARG;
                    }
                }
                /* No need to do any further validation - It's done below */
            } else if (stratSpec < 0) {
                #ifdef CONFIG_FLIPPER
                    printf("That's a silly argument.\nDon't be silly.\n");
                #else
                    ESP_LOGE(BT_TAG, "That's a silly argument. Don't be silly. :P");
                #endif
            }
            if (stratSpec > 16) {
                /* NONE cannot be combined with anything else */
                #ifdef CONFIG_FLIPPER
                    printf("Cannot combine NONE with\nother purge types.\n");
                #else
                    ESP_LOGE(BT_TAG, "Cannot combine purge type NONE with other purge types.");
                #endif
                return ESP_ERR_INVALID_ARG;
            }
            char strStrat[56];
            purgeStrategyToString(stratSpec, strStrat);
            #ifdef CONFIG_FLIPPER
                printf("Setting Purge Methods:\n%s\n", strStrat);
            #else
                ESP_LOGI(BT_TAG, "Setting Purge Methods:");
                ESP_LOGI(BT_TAG, "%s", strStrat);
            #endif
            purgeStrategy = stratSpec;
        #else
            displayBluetoothUnsupported();
            return ESP_ERR_NOT_SUPPORTED;
        #endif
    } else if (!strcasecmp(argv[1], "BLE_PURGE_MAX_RSSI")) {
        #if defined(CONFIG_IDF_TARGET_ESP32)
            if (argc != 3) {
                #ifdef CONFIG_FLIPPER
                    printf("Value required for SET.\n");
                #else
                    ESP_LOGE(TAG, "Maximum Purge RSSI (in dBm) is required.");
                #endif
                return ESP_ERR_INVALID_ARG;
            }
            int rssiSpec = atol(argv[2]);
            if (rssiSpec == 0) {
                #ifdef CONFIG_FLIPPER
                    printf("Invalid RSSI: \"%s.\n", argv[2]);
                #else
                    ESP_LOGE(TAG, "Invalid RSSI speified: \"%s\".", argv[2]);
                #endif
                return ESP_ERR_INVALID_ARG;
            }
            PURGE_MAX_RSSI = rssiSpec;
            #ifdef CONFIG_DEBUG
                #ifdef CONFIG_FLIPPER
                    printf("Max purge RSSI: %d\n", rssiSpec);
                #else
                    ESP_LOGI(TAG, "Maximum BLE purge RSSI set to %d dbM.", rssiSpec);
                #endif
            #endif
        #else
            displayBluetoothUnsupported();
            return ESP_ERR_NOT_SUPPORTED;
        #endif
    } else if (!strcasecmp(argv[1], "BLE_PURGE_MIN_AGE")) {
        #if defined(CONFIG_IDF_TARGET_ESP32)
            if (argc != 3) {
                #ifdef CONFIG_FLIPPER
                    printf("Value required for SET.\n");
                #else
                    ESP_LOGE(TAG, "Minimum Purge Age (seconds) is required.");
                #endif
                return ESP_ERR_INVALID_ARG;
            }
            int ageSpec = atol(argv[2]);
            if (ageSpec == 0) {
                #ifdef CONFIG_FLIPPER
                    printf("Invalid age: \"%s\".\n", argv[2]);
                #else
                    ESP_LOGE(TAG, "Invalid Age specified: \"%s\".", argv[2]);
                #endif

                return ESP_ERR_INVALID_ARG;
            }
            if (ageSpec > MAX_16_BIT) {
                ageSpec = MAX_16_BIT;
                #ifdef CONFIG_DEBUG
                    #ifdef CONFIG_FLIPPER
                        printf("Rcvd age %ld\nTrunc to %d (16bit)\n", atol(argv[2]), MAX_16_BIT);
                    #else
                        ESP_LOGI(BT_TAG, "Received age %ld. Truncating to %d for 16-bit container.", atol(argv[2]), MAX_16_BIT);
                    #endif
                #endif
            }
            PURGE_MIN_AGE = ageSpec;
            #ifdef CONFIG_DEBUG
                #ifdef CONFIG_FLIPPER
                    printf("Min purge age: %d\n", ageSpec);
                #else
                    ESP_LOGI(TAG, "Minimum BLE purge age set to %d seconds.", ageSpec);
                #endif
            #endif
        #else
            displayBluetoothUnsupported();
            return ESP_ERR_NOT_SUPPORTED;
        #endif
    } else {
        #ifdef CONFIG_FLIPPER
            printf("%s\nSSID_LEN_MIN,\nSSID_LEN_MAX,\nDEFAULT_SSID_COUNT,\nCHANNEL,ATTACK_PKTS,\nATTACK_MILLIS,MAC,\nMAC_RAND,EXPIRY,\nHOP_MODE,\nSCRAMBLE_WORDS,\nBLE_PURGE_STRAT,\nBLE_PURGE_MAX_RSSI,\nBLE_PURGE_MIN_AGE\n", SHORT_SET);
        #else
            ESP_LOGE(TAG, "Invalid variable specified. %s", USAGE_SET);
            ESP_LOGE(TAG, "<variable> : SSID_LEN_MIN | SSID_LEN_MAX | DEFAULT_SSID_COUNT | CHANNEL |");
            ESP_LOGE(TAG, "             MAC | ATTACK_PKTS | ATTACK_MILLIS | MAC_RAND | EXPIRY | HOP_MODE");
            ESP_LOGE(TAG, "             SCRAMBLE_WORDS | BLE_PURGE_STRAT | BLE_PURGE_MAX_RSSI | BLE_PURGE_MIN_AGE");
        #endif
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

/* Get application configuration items */
/* Usage: set <variable> <value>
   Allowed values for <variable> are:
      SSID_LEN_MIN, SSID_LEN_MAX, DEFAULT_SSID_COUNT, CHANNEL, HOP_MODE
      MAC, EXPIRY, MAC_RAND, ATTACK_PKTS (unused), ATTACK_MILLIS, BLE_PURGE_STRAT
      BLE_PURGE_MAX_RSSI, BLE_PURGE_MIN_AGE */
/* Channel hopping is not catered for in this feature */
esp_err_t cmd_get(int argc, char **argv) {
    if (argc != 2) {
        #ifdef CONFIG_FLIPPER
            printf("%s\nSCRAMBLE_WORDS,\nSSID_LEN_MIN,\nSSID_LEN_MAX,\nDEFAULT_SSID_COUNT,\nCHANNEL,ATTACK_PKTS,\nATTACK_MILLIS,MAC,\nMAC_RAND,EXPIRY,\nHOP_MODE,\nBLE_PURGE_STRAT,\nBLE_PURGE_MAX_RSSI,\nBLE_PURGE_MIN_AGE\n", SHORT_GET);
        #else
            ESP_LOGE(TAG, "%s", USAGE_GET);
            ESP_LOGE(TAG, "<variable> : SSID_LEN_MIN | SSID_LEN_MAX | DEFAULT_SSID_COUNT | CHANNEL | HOP_MODE | MAC");
            ESP_LOGE(TAG, "             ATTACK_PKTS | ATTACK_MILLIS | MAC_RAND | EXPIRY | SCRAMBLE_WORDS | BLE_PURGE_STRAT");
            ESP_LOGE(TAG, "             BLE_PURGE_MAX_RSSI | BLE_PURGE_MIN_AGE");
        #endif
        return ESP_ERR_INVALID_ARG;
    }
    if (!strcasecmp(argv[1], "SCRAMBLE_WORDS")) {
        #ifdef CONFIG_FLIPPER
            printf("SCRAMBLE_WORDS: %s\n", (scrambledWords)?"On":"Off");
        #else
            ESP_LOGI(TAG, "SCRAMBLE_WORDS: %s", (scrambledWords)?"Enabled":"Disabled");
        #endif
    } else if (!strcasecmp(argv[1], "BLE_PURGE_STRAT")) {
        #if defined(CONFIG_IDF_TARGET_ESP32)
            char strStrat[56] = "";
            purgeStrategyToString(purgeStrategy, strStrat);
            #ifdef CONFIG_FLIPPER
                printf("Active Purge Methods:\n%s\n", strStrat);
            #else
                ESP_LOGI(BT_TAG, "Active Purge Methods:");
                ESP_LOGI(BT_TAG, "%s", strStrat);
            #endif
        #else
            displayBluetoothUnsupported();
            return ESP_ERR_NOT_SUPPORTED;
        #endif
    } else if (!strcasecmp(argv[1], "BLE_PURGE_MAX_RSSI")) {
        #if defined(CONFIG_IDF_TARGET_ESP32)
            #ifdef CONFIG_FLIPPER
                printf("Max Purge RSSI: %lddBm\n", PURGE_MAX_RSSI);
            #else
                ESP_LOGI(BT_TAG, "BLE devices with RSSI greater than %ld dBm will not be automatically purged.", PURGE_MAX_RSSI);
            #endif
        #else
            displayBluetoothUnsupported();
            return ESP_ERR_NOT_SUPPORTED;
        #endif
    } else if (!strcasecmp(argv[1], "BLE_PURGE_MIN_AGE")) {
        #if defined(CONFIG_IDF_TARGET_ESP32)
            #ifdef CONFIG_FLIPPER
                printf("Min Purge Age: %us\n", PURGE_MIN_AGE);
            #else
                ESP_LOGI(BT_TAG, "BLE devices seen within the last %u seconds will not be automatically purged.", PURGE_MIN_AGE);
            #endif
        #else
            displayBluetoothUnsupported();
            return ESP_ERR_NOT_SUPPORTED;
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
    } else if (!strcasecmp(argv[1], "BLE_PURGE_STRAT")) {
        /* Syntax GET BLE_PURGE_STRAT */
        #if defined(CONFIG_IDF_TARGET_ESP32)
            char stratStr[56];
            purgeStrategyToString(purgeStrategy, stratStr);
            #ifdef CONFIG_FLIPPER
                printf("Purge Strategy: %s\n", stratStr);
            #else
                ESP_LOGI(BT_TAG, "Bluetooth Strategy: %s", stratStr);
            #endif
        #else
            #ifdef CONFIG_FLIPPER
                printf("Device doesn't support Bluetooth\n");
            #else
                ESP_LOGW(TAG, "Bluetooth unsupported by this device.");
            #endif
        #endif
    } else if (!strcasecmp(argv[1], "BLE_PURGE_MAX_RSSI")) {
        /* Syntax: GET BLE_PURGE_MAX_RSSI */
        #if defined(CONFIG_IDF_TARGET_ESP32)
            #ifdef CONFIG_FLIPPER
                printf("Max Purged RSSI %ld\n", PURGE_MAX_RSSI);
            #else
                ESP_LOGI(BT_TAG, "BLE Maximum Purge RSSI: %ld.", PURGE_MAX_RSSI);
            #endif
        #else
            #ifdef CONFIG_FLIPPER
                printf("Device doesn't support Bluetooth\n");
            #else
                ESP_LOGW(TAG, "Bluetooth unsupported by this device.");
            #endif
        #endif
    } else if (!strcasecmp(argv[1], "BLE_PURGE_MIN_AGE")) {
        /* Syntax: GET BLE_PURGE_MAX_AGE */
        #if defined(CONFIG_IDF_TARGET_ESP32)
            #ifdef CONFIG_FLIPPER
                printf("Min Purged Age %ds\n", PURGE_MIN_AGE);
            #else
                ESP_LOGI(BT_TAG, "BLE Minimum Purge Age: %ds", PURGE_MIN_AGE);
            #endif
        #else
            #ifdef CONFIG_FLIPPER
                printf("Device doesn't support Bluetooth\n");
            #else
                ESP_LOGW(TAG, "Bluetooth unsupported by this device.");
            #endif
        #endif
    } else {
        #ifdef CONFIG_FLIPPER
            printf("%s\nSSID_LEN_MIN,\nSSID_LEN_MAX,\nDEFAULT_SSID_COUNT,\nCHANNEL,ATTACK_PKTS,\nATTACK_MILLIS,MAC,\nMAC_RAND,EXPIRY,\nHOP_MODE,\nSCRAMBLE_WORDS,\nBLE_PURGE_STRAT,\nBLE_PURGE_MAX_RSSI,\nBLE_PURGE_MIN_AGE\n", SHORT_GET);
        #else
            ESP_LOGE(TAG, "Invalid variable specified. %s", USAGE_GET);
            ESP_LOGE(TAG, "<variable> : SSID_LEN_MIN | SSID_LEN_MAX | DEFAULT_SSID_COUNT | CHANNEL |");
            ESP_LOGE(TAG, "             MAC | ATTACK_PKTS | ATTACK_MILLIS | MAC_RAND | EXPIRY | HOP_MODE");
            ESP_LOGE(TAG, "             SCRAMBLE_WORDS | BLE_PURGE_STRAT | BLE_PURGE_MAX_RSSI | BLE_PURGE_MIN_AGE");
        #endif
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

/* Channel hopping is not catered for in this feature */
/* Usage: view ( ( AP [selectedSTA] ) | ( STA [selectedAP] ) | BT [ SERVICES [ SELECTED | KNOWN | UNKNOWN ] ] | SORT ( AGE | RSSI | SSID ) )+
*/
esp_err_t cmd_view(int argc, char **argv) {
    if (argc < 2 || argc > 12) {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_VIEW);
        #else
            ESP_LOGE(TAG, "%s", USAGE_VIEW);
        #endif
        return ESP_ERR_INVALID_ARG;
    }
    /* Scan arguments to see if sort criteria have been specified */
    int i = 1;
    GRAVITY_SORT_TYPE currentSortTypes[] = { GRAVITY_SORT_NONE, GRAVITY_SORT_NONE, GRAVITY_SORT_NONE };
    int currentSortCount = 0;
    while (i < argc) {
        for (; i < argc && strcasecmp(argv[i], "SORT"); ++i) { }
        /* Here i is either argc and we're done, or not */
        if (i < argc) {
            /* argv[i+1] should contain AGE, RSSI or SSID */
            if (!strcasecmp(argv[i + 1], "AGE")) {
                currentSortTypes[currentSortCount++] = GRAVITY_SORT_AGE;
            } else if (!strcasecmp(argv[i + 1], "RSSI")) {
                currentSortTypes[currentSortCount++] = GRAVITY_SORT_RSSI;
            } else if (!strcasecmp(argv[i + 1], "SSID") || !strcasecmp(argv[i + 1], "NAME")) {
                currentSortTypes[currentSortCount++] = GRAVITY_SORT_SSID;
            } else {
                #ifdef CONFIG_FLIPPER
                    printf("Invalid sort specifier: \"%s\"\n", argv[i + 1]);
                #else
                    ESP_LOGE(TAG, "Invalid sort specifier: \"%s\"", argv[i + 1]);
                #endif
                return ESP_ERR_INVALID_ARG;
            }
        }
        i += 2; /* Jump past "SORT xxx" */
    }
    /* Have now captured all sort criteria */
    sortCount = currentSortCount;
    sortResults[0] = currentSortTypes[0];
    sortResults[1] = currentSortTypes[1];
    sortResults[2] = currentSortTypes[2];

    #ifdef CONFIG_DEBUG
        if (currentSortCount > 0) {
            char sortCriteria[25] = "";
            strcpy(sortCriteria, "Sort by: ");
            for (int j = 0; j < currentSortCount; ++j) {
                switch (currentSortTypes[j]) {
                    case GRAVITY_SORT_NONE:
                        if (j > 0) {
                            strcat(sortCriteria, ", ");
                        }
                        strcat(sortCriteria, "NONE");
                        break;
                    case GRAVITY_SORT_AGE:
                        if (j > 0) {
                            strcat(sortCriteria, ", ");
                        }
                        strcat(sortCriteria, "AGE");
                        break;
                    case GRAVITY_SORT_RSSI:
                        if (j > 0) {
                            strcat(sortCriteria, ", ");
                        }
                        strcat(sortCriteria, "RSSI");
                        break;
                    case GRAVITY_SORT_SSID:
                        if (j > 0) {
                            strcat(sortCriteria, ", ");
                        }
                        strcat(sortCriteria, "SSID");
                        break;
                    default:
                        #ifdef CONFIG_FLIPPER
                            printf("Unknown sort criterion '%d'\n", currentSortTypes[j]);
                        #else
                            ESP_LOGE(TAG, "Unknown sort criterion '%d'", currentSortTypes[j]);
                        #endif
                        return ESP_ERR_INVALID_ARG;
                }
            }
            #ifdef CONFIG_FLIPPER
                printf("%s\n", sortCriteria);
            #else
                ESP_LOGI(TAG, "%s", sortCriteria);
            #endif
        }
    #endif

    bool success = true;
    for (i=1; i < argc; ++i) {
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
        } else if (!strcasecmp(argv[i], "BT")) {
            #if defined(CONFIG_IDF_TARGET_ESP32)
                /* Are we looking for services or devices? */
                /* This is gonna get crazy. Sorry for constantly changing i */
                if (argc > i + 1 && !strcasecmp(argv[i  + 1], "SERVICES")) {
                    ++i;
                    /* All, selected, known or unknown devices? */
                    if (argc > i + 1 && !strcasecmp(argv[i + 1], "SELECTED")) {
                        ++i;
                        // display services for selected HCIs
                        return bt_listAllServicesFor(gravity_selected_bt, gravity_sel_bt_count);
                    } else if (argc > i + 1 && !strcasecmp(argv[i + 1], "KNOWN")) {
                        ++i;
                        return listKnownServices(gravity_bt_devices, gravity_bt_dev_count);
                    } else if (argc > i + 1 && !strcasecmp(argv[i + 1], "UNKNOWN")) {
                        ++i;
                        return listUnknownServices();
                    } else {
                        // Display all services
                        bt_listAllServices();
                    }
                } else {
                    success = (success && gravity_bt_list_all_devices((scanResultExpiry != 0)) == ESP_OK);
                }
            #else
                displayBluetoothUnsupported();
            #endif
        } else if (!strcasecmp(argv[i], "SORT")) {
            ++i; /* Skip "SORT" and specifier */
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
esp_err_t cmd_select(int argc, char **argv) {
    if (argc < 3 || (strcasecmp(argv[1], "AP") && strcasecmp(argv[1], "STA") && strcasecmp(argv[1], "BT"))) {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_SELECT);
        #else
            ESP_LOGE(TAG, "%s", USAGE_SELECT);
        #endif
        return ESP_ERR_INVALID_ARG;
    }

    /* Vars to support using a non-space separator to select multiple scan elements from a Flipper */
    int begin = 0;
    int hatCount = 0;
    int origLen = strlen(argv[2]);

    /* Initialise the configured separator because we need a string */
    char tmp[2];
    sprintf(tmp, "%c", FLIPPER_SEPARATOR);

    /* Flipper Zero's keyboard doesn't have a space, so support ^-separated selectors */
    if (argc == 3 && strstr(argv[2], tmp) != NULL) {
        /* New approach to this: replace argc and argv */
        /* Turning hats into spaces doesn't create new array elements in argv. D'oh! */
        /* Count the number of hats */
        int numHats = 0;
        for (int i = 0; i < strlen(argv); ++i) {
            if (argv[2][i] == FLIPPER_SEPARATOR) {
                ++numHats;
            }
        }
        int newArgc = argc + numHats;
        char **newArgv = malloc(sizeof(char *) * newArgc);
        if (newArgv == NULL) {
            printf("Out of memory to separate select list\n");
            /* Don't do anything else - Let this fall through to at least select the first item */
        } else {
            /* Copy the first two arguments */
            for (int i = 0; i < (argc - 1); ++i) {
                newArgv[i] = argv[i];
            }
            /* Now add numHats elements from argv[2] */
            for (int i = 0; i < origLen; ++i) {
                #ifdef CONFIG_DEBUG_VERBOSE
                    printf("start loop, i is %d char is %c argv (len %d) is %s\n", i, argv[2][i], strlen(argv[2]), argv[2]);
                #endif
                if (argv[2][i] == FLIPPER_SEPARATOR) {
                    /* Debug outputs - something strange is happening */
                    #ifdef CONFIG_DEBUG_VERBOSE
                        printf("Found hat at idx %d. begin: %d  hatCount: %d  argc: %d  argv[2]: \"%s\"\n", i, begin, hatCount, argc, argv[2]);
                    #endif

                    /* Add the next argument */
                    argv[2][i] = '\0';
                    newArgv[argc + hatCount - 1] = &(argv[2][begin]);
                    ++hatCount;
                    begin = i + 1;

                    #ifdef CONFIG_DEBUG_VERBOSE
                        printf("After setting hat to NULL, &argv[2][begin] should contain the element id\nnewArgv[argc + hatCount]: \"%s\"\n", newArgv[argc + hatCount - 1]);
                    #endif
                }
            }
            #ifdef CONFIG_DEBUG_VERBOSE
                printf("Loop is done, newArgc %d  hatCount %d  begin %d\n", newArgc, hatCount, begin);
            #endif
            /* Add the final element */
            newArgv[argc + hatCount - 1] = &(argv[2][begin]);
            /* And replace argc & argv */
            argc = newArgc;
            argv = newArgv;

            #ifdef CONFIG_DEBUG_VERBOSE
                printf("After processing hats Gravity has generated the following command line with argc %d:\n", argc);
                for (int i = 0; i < argc; ++i) {
                    printf("newArgv[%d]: \"%s\"\n", i, newArgv[i]);
                }
            #endif
        }
    }

    /* Check whether 'ALL' is specified anywhere in the arguments */
    bool selectAll = false;
    for (int i = 2; i < argc && !selectAll; ++i) {
        if (!strcasecmp(argv[i], "ALL")) {
            selectAll = true;
        }
    }

    esp_err_t err = ESP_OK;
    if (!strcasecmp(argv[1], "AP")) {
        /* All or some */
        if (selectAll) {
            /* Toggle select status of all APs */
            for (int i = 0; i < gravity_ap_count; ++i) {
                err |= gravity_select_ap(gravity_aps[i].index);
                #ifdef CONFIG_FLIPPER
                    printf("AP %d %sselected\n", gravity_aps[i].index, (gravity_ap_isSelected(gravity_aps[i].index))?"":"not ");
                #else
                    ESP_LOGI(TAG, "AP element %d is %sselected", gravity_aps[i].index, (gravity_ap_isSelected(gravity_aps[i].index))?"":"not ");
                #endif
            }
        } else {
            for (int i = 2; i < argc; ++i) {
                err |= gravity_select_ap(atoi(argv[i]));
                #ifdef CONFIG_FLIPPER
                    printf("AP %d %sselected\n", atoi(argv[i]), (gravity_ap_isSelected(atoi(argv[i])))?"":"not ");
                #else
                    ESP_LOGI(TAG, "AP element %d is %sselected", atoi(argv[i]), (gravity_ap_isSelected(atoi(argv[i])))?"":"not ");
                #endif
            }
        }
    } else if (!strcasecmp(argv[1], "STA")) {
        /* All or some */
        if (selectAll) {
            /* Toggle select status of all STAs */
            for (int i = 0; i < gravity_sta_count; ++i) {
                err |= gravity_select_sta(gravity_stas[i].index);
                #ifdef CONFIG_FLIPPER
                    printf("STA %d %sselected\n", gravity_stas[i].index, (gravity_sta_isSelected(gravity_stas[i].index))?"":"not ");
                #else
                    ESP_LOGI(TAG, "STA element %d is %sselected", gravity_stas[i].index, (gravity_sta_isSelected(gravity_stas[i].index))?"":"not ");
                #endif
            }
        } else {
            for (int i = 2; i < argc; ++i) {
                err |= gravity_select_sta(atoi(argv[i]));
                #ifdef CONFIG_FLIPPER
                    printf("STA %d %sselected\n", atoi(argv[i]), (gravity_sta_isSelected(atoi(argv[i])))?"":"not ");
                #else
                    ESP_LOGI(TAG, "STA element %d is %sselected", atoi(argv[i]), (gravity_sta_isSelected(atoi(argv[i])))?"":"not ");
                #endif
            }
        }
    } else if (!strcasecmp(argv[1], "BT")) {
        #if defined(CONFIG_IDF_TARGET_ESP32)
            /* All or some */
            if (selectAll) {
                /* Toggle select status of all BT devices */
                for (int i = 0; i < gravity_bt_dev_count; ++i) {
                    err |= gravity_select_bt(gravity_bt_devices[i]->index);
                    #ifdef CONFIG_FLIPPER
                        printf(" BT %d %sselected\n", gravity_bt_devices[i]->index, (gravity_bt_isSelected(gravity_bt_devices[i]->index))?"":"not ");
                    #else
                        ESP_LOGI(TAG, " BT element %d is %sselected", gravity_bt_devices[i]->index, (gravity_bt_isSelected(gravity_bt_devices[i]->index))?"":"not ");
                    #endif
                }
            } else {
                for (int i = 2; i < argc; ++i) {
                    err |= gravity_select_bt(atoi(argv[i]));
                    #ifdef CONFIG_FLIPPER
                        printf(" BT %d %sselected\n", atoi(argv[i]), (gravity_bt_isSelected(atoi(argv[i])))?"":"not ");
                    #else
                        ESP_LOGI(TAG, " BT element %d is %sselected", atoi(argv[i]), gravity_bt_isSelected(atoi(argv[i]))?"":"not ");
                    #endif
                }
            }
        #else
            displayBluetoothUnsupported();
        #endif
    }
    return err;
}

/* Display selected STAs and/or APs. Usage: selected ( AP | STA | BT ). Call with no arguments to display both */
/* Adding selected AP STA as well as no args because I keep forgetting I can use no args */
esp_err_t cmd_selected(int argc, char **argv) {
    int retVal = ESP_OK;

    if (argc > 4 || (argc > 1 && strcasecmp(argv[1], "STA") && strcasecmp(argv[1], "AP") && strcasecmp(argv[1], "BT")) ||
            (argc > 2 && strcasecmp(argv[2], "STA") && strcasecmp(argv[2], "AP") && strcasecmp(argv[2], "BT")) ||
            (argc == 4 && strcasecmp(argv[3], "STA") && strcasecmp(argv[3], "AP") && strcasecmp(argv[3], "BT"))) {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_SELECTED);
        #else
            ESP_LOGE(TAG, "%s", USAGE_SELECTED);
        #endif
        return ESP_ERR_INVALID_ARG;
    }

    /* Print APs if no args or "AP" */
    /* Hide expired packets only if scanResultExpiry has been set */
    if (argc == 1 || (argc > 1 && !strcasecmp(argv[1], "AP")) || (argc > 2 && !strcasecmp(argv[2], "AP")) || (argc == 4 && !strcasecmp(argv[3], "AP"))) {
        retVal |= gravity_list_ap(gravity_selected_aps, gravity_sel_ap_count, (scanResultExpiry != 0));
        printf("\n");
    }
    if (argc == 1 || (argc > 1 && !strcasecmp(argv[1], "STA")) || (argc > 2 && !strcasecmp(argv[2], "STA")) || (argc == 4 && !strcasecmp(argv[3], "STA"))) {
        retVal |= gravity_list_sta(gravity_selected_stas, gravity_sel_sta_count, (scanResultExpiry != 0));
        printf("\n");
    }
    if (argc == 1 || (argc > 1 && !strcasecmp(argv[1], "BT")) || (argc > 2 && !strcasecmp(argv[2], "BT")) || (argc == 4  && !strcasecmp(argv[3], "BT"))) {
        #if defined(CONFIG_IDF_TARGET_ESP32)
            retVal |= gravity_bt_list_devices(gravity_selected_bt, gravity_sel_bt_count, (scanResultExpiry != 0));
        #else
            displayBluetoothUnsupported();
        #endif
    }

    return retVal;
}

/* Channel hopping is not catered for in this feature */
esp_err_t cmd_clear(int argc, char **argv) {
    esp_err_t err = ESP_OK;
    if (argc != 2 && argc != 3) {
        #ifdef CONFIG_FLIPPER
            printf("%s\n", SHORT_CLEAR);
        #else
            ESP_LOGE(TAG, "%s", USAGE_CLEAR);
        #endif
        return ESP_ERR_INVALID_ARG;
    }
    for (int i=1; i < argc; ++i) {
        if (strcasecmp(argv[i], "AP") && strcasecmp(argv[i], "STA") && strcasecmp(argv[i], "BT") && strcasecmp(argv[i], "SERVICES") && strcasecmp(argv[i], "ALL")) {
            #ifdef CONFIG_FLIPPER
                printf("%s\n", SHORT_CLEAR);
            #else
                ESP_LOGE(TAG, "%s", USAGE_CLEAR);
            #endif
            return ESP_ERR_INVALID_ARG;
        }
        if (!(strcasecmp(argv[i], "AP") && strcasecmp(argv[i], "ALL"))) {
            err |= gravity_clear_ap();
        }
        if (!(strcasecmp(argv[i], "STA") && strcasecmp(argv[i], "ALL"))) {
            err |= gravity_clear_sta();
        }
        /* Remove services whether it's BT or BT SERVICES */
        if (!(strcasecmp(argv[i], "BT") && strcasecmp(argv[i], "ALL"))) {
            /* Clear BT Services */
            #if defined(CONFIG_IDF_TARGET_ESP32)
                err |= bt_service_rm_all();
            #else
                displayBluetoothUnsupported();
            #endif
        }
        /* If we have BT and we don't have SERVICES then remove the BT devices as well */
        if (((!strcasecmp(argv[i], "BT")) && (argc <= i + 1 || strcasecmp(argv[i + 1], "SERVICES"))) ||
                !strcasecmp(argv[i], "ALL")) {
            #if defined(CONFIG_IDF_TARGET_ESP32)
                err |= gravity_clear_bt();
            #else
                displayBluetoothUnsupported();
            #endif
        }
    }
    return err;
}

esp_err_t cmd_handshake(int argc, char **argv) {
    /* TODO: Set attack_status appropriately */

    /* Start hopping task loop if hopping is on by default */
    esp_err_t err = setHopForNewCommand();
    if (err != ESP_OK) {
        #ifdef CONFIG_FLIPPER
            printf("%s%s\n", STRINGS_HOP_STATE_FAIL, esp_err_to_name(err));
        #else
            ESP_LOGW(HOP_TAG, "%s%s", STRINGS_HOP_STATE_FAIL, esp_err_to_name(err));
        #endif
    }

    return ESP_OK;
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
        attack_status[ATTACK_MANA_LOUD] || attack_status[ATTACK_STALK]);
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
        scan_wifi_parse_frame(payload, data->rx_ctrl);
    }
    if (attack_status[ATTACK_STALK]) {
        stalk_frame(payload, data->rx_ctrl);
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

        // Set up promiscuous mode and packet callback
        initPromiscuous();
        WIFI_INITIALISED = true;
    }
    return ESP_OK;
}

void initPromiscuous() {
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

    wifi_promiscuous_filter_t filter = { .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_CTRL | WIFI_PROMIS_FILTER_MASK_DATA };
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(wifi_pkt_rcvd);
    esp_wifi_set_promiscuous(true);
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
            case ATTACK_SCAN_BT_CLASSIC:
            case ATTACK_SCAN_BLE:
            case ATTACK_MANA:
            case ATTACK_MANA_VERBOSE:
            case ATTACK_MANA_LOUD:
            case ATTACK_AP_DOS:
            case ATTACK_AP_CLONE:
            case ATTACK_HANDSHAKE:
            case ATTACK_BT:
            case ATTACK_STALK:
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
            case ATTACK_SCAN_BT_CLASSIC:
            case ATTACK_SCAN_BLE:
            case ATTACK_MANA:
            case ATTACK_AP_DOS:
            case ATTACK_AP_CLONE:
            case ATTACK_STALK:
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
            case ATTACK_SCAN_BT_CLASSIC:
            case ATTACK_SCAN_BLE:
            case ATTACK_DEAUTH:
            case ATTACK_AP_DOS:                                     /* where hopping doesn't */
            case ATTACK_AP_CLONE:                                   /* make sense be */
            case ATTACK_RANDOMISE_MAC: 
            case ATTACK_BT:                             /* treated differently somehow? */
                hop_millis_defaults[i] = CONFIG_DEFAULT_HOP_MILLIS;
                break;
            case ATTACK_STALK:
                hop_millis_defaults[i] = 100; // TODO: make a variable
                break;
            default:
                ESP_LOGE(TAG, "ATTACKS_COUNT has incorrect length");
                free(attack_status);
                free(hop_defaults);
                free(hop_millis_defaults);
                return;
        }
    }

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
        #if defined(CONFIG_IDF_TARGET_ESP32)
            esp_log_level_set(BT_TAG, ESP_LOG_ERROR);
        #endif
        esp_log_level_set(DEAUTH_TAG, ESP_LOG_ERROR);
        esp_log_level_set(DOS_TAG, ESP_LOG_ERROR);
        esp_log_level_set(FUZZ_TAG, ESP_LOG_ERROR);
        esp_log_level_set(TAG, ESP_LOG_ERROR);
        esp_log_level_set(MANA_TAG, ESP_LOG_ERROR);
        esp_log_level_set(HOP_TAG, ESP_LOG_ERROR);
        esp_log_level_set(PROBE_TAG, ESP_LOG_ERROR);
        esp_log_level_set(SCAN_TAG, ESP_LOG_ERROR);
        esp_log_level_set(SNIFF_TAG, ESP_LOG_ERROR);
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

    /* Initialise the configured separator when selecting results from Flipper Zero */
    initFlipperSep();

}
