/* Basic console example (esp_console_repl API)

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_fat.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "cmd_system.h"
#include "cmd_wifi.h"
#include "cmd_nvs.h"

#include "main.h"

static const char* TAG = "WiFi";
#define PROMPT_STR CONFIG_IDF_TARGET

/* Console command history can be stored to and loaded from a file.
 * The easiest way to do this is to use FATFS filesystem on top of
 * wear_levelling library.
 */
#if CONFIG_CONSOLE_STORE_HISTORY

#define MOUNT_PATH "/data"
#define HISTORY_PATH MOUNT_PATH "/history.txt"

/* Command specifications */
#define CMD_COUNT 13
esp_console_cmd_t commands[CMD_COUNT] = {
    {
        .command = "beacon",
        .hint = "Beacon spam attack. Usage: beacon [ ( ( RICKROLL | RANDOM | USER ) [ TARGET MAC ] ) | OFF ]. Beacon without parameters returns the current status of the module, single argument \"OFF\" disables a running beacon attack. User-defined attack requires target-ssids to be set. \"TARGET MAC\" is not currently supported but may be in the future.",
        .help = "A beacon spam attack continously transmits forged beacon frames. RICKROLL will simulate eight APs named after popular song lyrics. RANDOM will generate random SSIDs between SSID_LEN_MIN and SSID_LEN_MAX in length. USER will generate SSIDs as specified in target-ssids.",
        .func = cmd_beacon
    }, {
        .command = "probe",
        .hint = "Probe flood attack. Usage: probe [ ( ( ALL | MACS ) ( ANY | SSIDs ) ) | OFF ]. Send probe requests to either broadcast or specified MACs, requesting information either all or SSIDs specified in target-ssids. 'Probe' without arguments returns the current status of the module, the argument \"OFF\" disables a running probe attack",
        .help = "A probe flood attack continually transmits probe requests, imposing continual load on target APs.",
        .func = cmd_probe
    }, {
        .command = "deauth",
        .hint = "Deauthentication attack. Usage: deauth [ STA | BROADCAST | OFF ]. Arguments:   STA: Send deauthentication packets to selected stations.   BROADCAST: Send broadcast deauthentication packets.   OFF: Disable a running deauthentication attack.   No argument: Return the current status of the module.",
        .help = "Deauthentication frames are intended to be issued by an AP to instruct connected STAs to disconnect before the AP makes a change that could affect the connection. This obviously makes it trivial to observe a 4-way handshake and obtain key material, and as a consequence of this many - perhaps even the majority of - wireless devices will disregard a broadcast deauthentication packet. This attack will be much more effective if specific stations are selected as targets. Success will be greater still if you adopt the MAC of the Access Point you are attempting to deauthenticate stations from.",
        .func = cmd_deauth
    } , {
        .command = "mana",
        .hint = "Mana attack. Usage: mana [ ON | OFF ]. Call without arguments to obtain the current status of the module.",
        .help = "The Mana attack is a way to 'trick' stations into connecting to a rogue access point. With Mana enabled the AP will respond to all directed probe requests, impersonating any SSID a STA is searching for. If the STA expects any of these SSIDs to have open (i.e. no) authentication the STA will then establish a connection with the AP. The only criterion for vulnerability is that the station has at least one open/unsecured SSID saved in its WiFi history.",
        .func = cmd_mana
    }, {
        .command = "stalk",
        .hint = "Toggle the target tracking/homing feature. Usage: stalk. Displays signal strength information for selected wireless devices, allowing their location to be rapidly determined.",
        .help = "Where selected, this attack will include both stations and access points in its tracking - while it's unlikely you'll ever need to track STAs and APs simultaneously, it's far from impossible. While selecting multiple devices will improve your accuracy and reliability, it also increases the likelihood that selected devices won't all remain in proximity with each other.",
        .func = cmd_stalk
    }, {
        .command = "ap-dos",
        .hint = "802.11 denial-of-service attack. Usage: ap-dos [ ON | OFF ]. If no argument is provided returns the current state of this module. This attack targets selected SSIDs.",
        .help = "This attack attempts to interrupt all communication and disconnect all stations from selected access points. When a frame addressed to a target AP is observed a deauthentication packet is created as a reply to the sender, specifying the target AP as the sender. TODO: If we can identify STAs that are associated with target APs send directed deauth frames to them. Perhaps this could be 'DOS Mk. II'",
        .func = cmd_ap_dos
    }, {
        .command = "ap-clone",
        .hint = "Clone and attempt to take over the specified AP. Usage: ap-clone [ <AP MAC> | OFF ]. If no argument is provided returns the current state of the module. To start the attack issue the ap_clone command with the target AP's MAC address as a parameter.",
        .help = "Clone the specified AP and attempt to coerce STAs to connect to the cloned AP instead of the authentic one. The success of this attack is dependent upon being able to generate a more powerful signal than the genuine AP. The attack will set ESP32C6's MAC and SSID to match the AP's, run a deauth attack until there are no associated STAs or a predetermined time has passed, then disable the deauth attack and hope that STAs connect to the rogue AP.",
        .func = cmd_ap_clone
    }, {
        .command = "scan",
        .hint = "Scan for wireless devices (only 802.11 for now). Usage: scan [ FASTAP | AP | STA | ANY | OFF ]. No argument returns scan status.   FastAP: Obtain immediate results from a broadcast probe request.   AP: Initiate a continuous scan for APs.   STA: Initiate a continuous scan for STAs.   ANY: Initiate a continuous scan capturing both AP and STA data.",
        .help = "Scan wireless frequencies to identify access points and stations in range. Most modules in this application require one or more target APs and/or STAs so you will run these commands frequently. FASTAP performs a standard broadcast probe, identifying those APs that would be included in an operating system's wireless network scanner. The other scan types commence an open-ended analysis of received packets, and will continue updating until they are stopped. To assist in identifying contemporary devices these scan types also capture a timestamp of when the device was last seen.",
        .func = cmd_scan
    }, {
        .command = "set",
        .hint = "Set a variety of variables that affect various components of the application. Usage: set <variable> <value>   Accepted values for <variable>: SSID_LEN_MIN, SSID_LEN_MAX, CHANNEL, MAC, HOP_MILLIS, ATTACK_PKTS, ATTACK_MILLIS",
        .help = "Set a variety of variables that affect various components of the application. Usage: set <variable> <value>   <variable>   SSID_LEN_MIN: Minimum length of a generated SSID   SSID_LEN_MAX: Maximum length of a generated SSID   CHANNEL: Wireless channel   MAC: ASP32C6's MAC address   HOP_MILLIS: Milliseconds to stay on a channel before hopping to the next (0: Hopping disabled)   ATTACK_PKTS: Number of times to repeat an attack packet when launching an attack (0: Don't stop attacks based on packet count)   ATTACK_MILLIS: Milliseconds to run an attack for when it is launched (0: Don't stop attacks based on duration)   NB: If ATTACK_PKTS and ATTACK_MILLIS are both 0 attacks will not end automatically but will continue until terminated.",
        .func = cmd_set
    }, {
        .command = "get",
        .hint = "Get a variety of variables that affect various components of the application. Usage: get <variable>   Accepted values for <variable>: SSID_LEN_MIN, SSID_LEN_MAX, CHANNEL, MAC, HOP_MILLIS, ATTACK_PKTS, ATTACK_MILLIS",
        .help = "Get a variety of variables that affect various components of the application. Usage: get <variable>   <variable>   SSID_LEN_MIN: Minimum length of a generated SSID   SSID_LEN_MAX: Maximum length of a generated SSID   CHANNEL: Wireless channel   MAC: ASP32C6's MAC address   HOP_MILLIS: Milliseconds to stay on a channel before hopping to the next (0: Hopping disabled)   ATTACK_PKTS: Number of times to repeat an attack packet when launching an attack (0: Don't stop attacks based on packet count)   ATTACK_MILLIS: Milliseconds to run an attack for when it is launched (0: Don't stop attacks based on duration)   NB: If ATTACK_PKTS and ATTACK_MILLIS are both 0 attacks will not end automatically but will continue until terminated.",
        .func = cmd_get
    }, {
        .command = "view",
        .hint = "List available targets for the included tools. Usage: view ( SSID | STA | MAC )*.  Each result element is prefixed by an identifier to use with the SELECT command, with selected items also indicated. \"MAC\" is a composite set of identifiers consisting of selected stations in addition to MACs for selected SSIDs.",
        .help = "VIEW is a fundamental command in this framework, with the typical workflow being Scan-View-Select-Attack. Multiple result sets can be viewed in a single command using, for example, VIEW STA SSID.",
        .func = cmd_view
    }, {
        .command = "select",
        .hint = "Select the specified element from the specified scan results. Usage: select ( SSID | STA ) <elementId>.  Selects/deselects item <elementId> from the SSID or STA list.",
        .help = "Select the specified element from the specified scan results. Usage: select ( SSID | STA ) <elementId>.  Selects/deselects item <elementId> from the SSID or STA list.",
        .func = cmd_select
    }, {
        .command = "handshake",
        .hint = "Toggle monitoring of 802.11 traffic for a 4-way handshake to obtain key material from. Usage: handshake",
        .help = "Toggle monitoring of 802.11 traffic for a 4-way handshake to obtain key material from. Usage: handshake",
        .func = cmd_handshake
    }
};

int cmd_beacon(int argc, char **argv) {
    /* rickroll | random | user | off | status */
    /* Initially the 'TARGET MAC' argument is unsupported, the attack only supports broadcast beacon frames */
    /* argc must be 1 or 2 - no arguments, or rickroll/random/user/off */
    if (argc < 1 || argc > 2) {
        ESP_LOGE(TAG, "Invalid arguments specified. Expected 0 or 1, received %d.", argc - 1);
        return ESP_ERR_INVALID_ARG;
    }
    if (argc == 1) {
        ESP_LOGI(TAG, "Beacon Status: %s", attack_status[ATTACK_BEACON]?"Running":"Not Running");
        return ESP_OK;
    }

    /* Handle argument to beacon */
    if (!strcasecmp(argv[1], "rickroll")) {
        return beacon_start(ATTACK_BEACON_RICKROLL);
    } else if (!strcasecmp(argv[1], "random")) {
        if (SSID_LEN_MIN == 0) {
            SSID_LEN_MIN = 8;
        }
        if (SSID_LEN_MAX == 0) {
            SSID_LEN_MAX = 32;
        }
        return beacon_start(ATTACK_BEACON_RANDOM);
    } else if (!strcasecmp(argv[1], "user")) {
        // Need a strategy to build user-specified list
        return beacon_start(ATTACK_BEACON_USER);
    } else if (!strcasecmp(argv[1], "off")) {
        return beacon_stop();
    } else {
        ESP_LOGE(TAG, "Invalid argument provided to BEACON: \"%s\"", argv[1]);
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

int cmd_probe(int argc, char **argv) {

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

int cmd_set(int argc, char **argv) {

    return ESP_OK;
}

int cmd_get(int argc, char **argv) {

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

static int register_console_commands() {
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

    /* Initialise console */
    esp_console_config_t *config;
    config = malloc(sizeof(esp_console_config_t));
    if (config == NULL) {
        ESP_LOGE(TAG, "Unable to allocate memory for console configuration. PANIC");
        return;
    }
    config->hint_bold = 1;
    config->hint_color = atoi(LOG_COLOR_GREEN);
    config->max_cmdline_args = 3;
    config->max_cmdline_length = 64;
    esp_console_init(config);

    /* Register console commands */
    register_console_commands();
    esp_console_register_help_command();
    register_system_common();
#ifndef CONFIG_IDF_TARGET_ESP32H2  // needs deep sleep support, IDF-6268
    register_system_sleep();
#endif
#if SOC_WIFI_SUPPORTED
    register_wifi();
#endif
    register_nvs();

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
