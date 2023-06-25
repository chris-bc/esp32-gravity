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

static const char* TAG = "GRAVITY";

char **attack_ssids = NULL;
char **user_ssids = NULL;
int user_ssid_count = 0;

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
        ESP_LOGW(PROBE_TAG, "Syntax: PROBE [ ANY [ COUNT ] | SSIDS [ COUNT ] | OFF ].  SSIDS uses the target-ssids specification.");
        return ESP_ERR_INVALID_ARG;
    }

    probe_attack_t probeType = ATTACK_PROBE_UNDIRECTED; // Default
    int probeCount = DEFAULT_PROBE_COUNT;

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

        // Get probe count
        if (argc == 3) {
            probeCount = atoi(argv[2]);
            if (probeCount <= 0) {
                probeCount = DEFAULT_PROBE_COUNT;
                ESP_LOGW(PROBE_TAG, "Incorrect packet count specified. \"%s\" could not be transformed into a number, using default.", argv[2]);
            }
        }

        char probeNote[100];
        sprintf(probeNote, "Starting a probe flood of %d%s packets%s", probeCount, (probeType == ATTACK_PROBE_UNDIRECTED)?" broadcast":"", (probeType == ATTACK_PROBE_DIRECTED)?" directed to ":"");
        if (probeType == ATTACK_PROBE_DIRECTED) {
            char suffix[16];
            sprintf(suffix, "%d SSIDs", countSsid());
            strcat(probeNote, suffix);
        }

        ESP_LOGI(PROBE_TAG, "%s", probeNote);
        probe_start(probeType, probeCount);
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

int cmd_deauth(int argc, char **argv) {

    return ESP_OK;
}

int cmd_mana(int argc, char **argv) {

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
        ESP_LOGE(TAG, "             MAC | HOP_MILLIS | ATTACK_PKTS | ATTACK_MILLIS");
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
        err = esp_wifi_set_mac(WIFI_IF_MAX, bMac);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set MAC :  %s", esp_err_to_name(err));
            return err;
        }
        ESP_LOGI(TAG, "Set MAC to :  %s", argv[2]);
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
        ESP_LOGE(TAG, "             MAC | HOP_MILLIS | ATTACK_PKTS | ATTACK_MILLIS");
        return ESP_ERR_INVALID_ARG;
    }
    if (!strcasecmp(argv[2], "SSID_LEN_MIN")) {
        ESP_LOGI(TAG, "SSID_LEN_MIN :  %d", SSID_LEN_MIN);
    } else if (!strcasecmp(argv[2], "SSID_LEN_MAX")) {
        ESP_LOGI(TAG, "SSID_LEN_MAX :  %d", SSID_LEN_MAX);
    } else if (!strcasecmp(argv[2], "DEFAULT_SSID_COUNT")) {
        ESP_LOGI(TAG, "DEFAULT_SSID_COUNT :  %d", DEFAULT_SSID_COUNT);
    } else if (!strcasecmp(argv[2], "CHANNEL")) {
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
    } else if (!strcasecmp(argv[2], "MAC")) {
        //
        uint8_t bMac[6];
        char strMac[18];
        esp_err_t err = esp_wifi_get_mac(WIFI_IF_MAX, bMac);
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
    } else if (!strcasecmp(argv[2], "HOP_MILLIS")) {
        //
        ESP_LOGI(TAG, "Not yet implemented");
    } else if (!strcasecmp(argv[2], "ATTACK_PKTS")) {
        //
        ESP_LOGI(TAG, "Not yet implemented");
    } else if (!strcasecmp(argv[2], "ATTACK_MILLIS")) {
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
    initialise_wifi();
    repl_config.history_save_path = HISTORY_PATH;
    ESP_LOGI(TAG, "Command history enabled");
#else
    ESP_LOGI(TAG, "Command history disabled");
#endif

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
    sprintf(strMac, "%02u:%02u:%02u:%02u:%02u:%02u", bMac[0],
            bMac[1], bMac[2], bMac[3], bMac[4], bMac[5]);
    return ESP_OK;
}