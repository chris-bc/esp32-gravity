#ifndef GRAVITY_H
#define GRAVITY_H

#define GRAVITY_VERSION "0.2.2b"

#include <cmd_nvs.h>
#include <cmd_system.h>
#include <cmd_wifi.h>
#include <esp_console.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_vfs_dev.h>
#include <esp_vfs_fat.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <freertos/portmacro.h>
#include <nvs.h>
#include <nvs_flash.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "usage_const.h"

/* Command usage strings */
extern const char USAGE_BT[];
extern const char USAGE_BEACON[];
extern const char USAGE_TARGET_SSIDS[];
extern const char USAGE_PROBE[];
extern const char USAGE_SNIFF[];
extern const char USAGE_DEAUTH[];
extern const char USAGE_MANA[];
extern const char USAGE_STALK[];
extern const char USAGE_AP_DOS[];
extern const char USAGE_AP_CLONE[];
extern const char USAGE_SCAN[];
extern const char USAGE_HOP[];
extern const char USAGE_SET[];
extern const char USAGE_GET[];
extern const char USAGE_VIEW[];
extern const char USAGE_SELECT[];
extern const char USAGE_SELECTED[];
extern const char USAGE_CLEAR[];
extern const char USAGE_HANDSHAKE[];
extern const char USAGE_COMMANDS[];

/* Command specifications */
esp_err_t cmd_bluetooth(int argc, char **argv);
esp_err_t cmd_beacon(int argc, char **argv);
esp_err_t cmd_probe(int argc, char **argv);
esp_err_t cmd_fuzz(int argc, char **argv);
esp_err_t cmd_sniff(int argc, char **argv);
esp_err_t cmd_deauth(int argc, char **argv);
esp_err_t cmd_mana(int argc, char **argv);
esp_err_t cmd_stalk(int argc, char **argv);
esp_err_t cmd_ap_dos(int argc, char **argv);
esp_err_t cmd_ap_clone(int argc, char **argv);
esp_err_t cmd_scan(int argc, char **argv);
esp_err_t cmd_set(int argc, char **argv);
esp_err_t cmd_get(int argc, char **argv);
esp_err_t cmd_view(int argc, char **argv);
esp_err_t cmd_select(int argc, char **argv);
esp_err_t cmd_selected(int argc, char **argv);
esp_err_t cmd_clear(int argc, char **argv);
esp_err_t cmd_handshake(int argc, char **argv);
esp_err_t cmd_target_ssids(int argc, char **argv);
esp_err_t cmd_commands(int argc, char **argv);
esp_err_t cmd_hop(int argc, char **argv);
esp_err_t cmd_info(int argc, char **argv);

bool gravitySniffActive();

/* Moving attack_status and hop_defaults off the heap */
extern bool *attack_status;
extern bool *hop_defaults;
extern int *hop_millis_defaults;

static bool WIFI_INITIALISED = false;

char scan_filter_ssid[33] = "\0";
uint8_t scan_filter_ssid_bssid[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

#define CMD_COUNT 22
esp_console_cmd_t commands[CMD_COUNT] = {
    {
        .command = "beacon",
        .hint = SHORT_BEACON,
        .help = "A beacon spam attack continously transmits forged beacon frames. RICKROLL will simulate eight APs named after popular song lyrics. RANDOM will generate COUNT random SSIDs between SSID_LEN_MIN and SSID_LEN_MAX in length. If COUNT is not specified DEFAULT_SSID_COUNT is used. TARGET-SSIDs will generate SSIDs as specified in target-ssids. APs will generate SSIDs based on selected scan results. It is STRONGLY RECOMMENDED that scanning remain active while in this mode.  INIFINITE will continuously broadcast random APs until it is stopped.",
        .func = cmd_beacon
    }, {
        .command = "target-ssids",
        .hint = USAGE_TARGET_SSIDS,
        .help = "ssid-targets may be specified for several attacks, including BEACON and PROBE.",
        .func = cmd_target_ssids
    }, {
        .command = "probe",
        .hint = USAGE_PROBE,
        .help = "A probe flood attack continually transmits probe requests, imposing continual load on target APs.",
        .func = cmd_probe
    }, {
        .command = "fuzz",
        .hint = USAGE_FUZZ,
        .help = "Sends different types of invalid 802.11 packets, to see what happens. Will send either beacon or probe request packets; in OVERFLOW mode will send variously-sized SSIDS that exceed the standard maximum of 32 bytes; in MALFORMED mode will send SSIDs of various sizes that exceed the length specified in ssid_len.",
        .func = cmd_fuzz
    }, {
        .command = "sniff",
        .hint = USAGE_SNIFF,
        .help = "Gravity operates in promiscuous (monitor) mode at all times. This mode displays relevant information about interesting packets as they are received.",
        .func = cmd_sniff
    }, {
        .command = "deauth",
        .hint = USAGE_DEAUTH,
        .help = "Arguments:   <millis>: Time to wait between packets.  FRAME | DEVICE | SPOOF : Deauth packet is sent with the AP's source address, the device's source address, or the device's MAC is changed to match AP.  STA: Send deauthentication packets to selected stations.   AP: Send deauthentication packets to clients of selected APs.  BROADCAST: Send broadcast deauthentication packets.   OFF: Disable a running deauthentication attack.   No argument: Return the current status of the module.   Deauthentication frames are intended to be issued by an AP to instruct connected STAs to disconnect before the AP makes a change that could affect the connection. This obviously makes it trivial to observe a 4-way handshake and obtain key material, and as a consequence of this many - perhaps even the majority of - wireless devices will disregard a broadcast deauthentication packet. This attack will be much more effective if specific stations are selected as targets. Success will be greater still if you adopt the MAC of the Access Point you are attempting to deauthenticate stations from.",
        .func = cmd_deauth
    } , {
        .command = "mana",
        .hint = USAGE_MANA,
        .help = "Call without arguments to obtain the current status of the module.  Including the verbose keyword will enable or disable verbose logging as the attack progresses.  Default authentication type is NONE.  The Mana attack is a way to 'trick' stations into connecting to a rogue access point. With Mana enabled the AP will respond to all directed probe requests, impersonating any SSID a STA is searching for. If the STA expects any of these SSIDs to have open (i.e. no) authentication the STA will then establish a connection with the AP. The only criterion for vulnerability is that the station has at least one open/unsecured SSID saved in its WiFi history.",
        .func = cmd_mana
    }, {
        .command = "stalk",
        .hint = USAGE_STALK,
        .help = "Displays signal strength information for selected wireless devices, allowing their location to be rapidly determined.  Where selected, this attack will include both stations and access points in its tracking - while it's unlikely you'll ever need to track STAs and APs simultaneously, it's far from impossible. While selecting multiple devices will improve your accuracy and reliability, it also increases the likelihood that selected devices won't all remain in proximity with each other.",
        .func = cmd_stalk
    }, {
        .command = "ap-dos",
        .hint = USAGE_AP_DOS,
        .help = "If no argument is provided returns the current state of this module. This attack targets selected SSIDs.  This attack attempts to interrupt all communication and disconnect all stations from selected access points. When a frame addressed to a target AP is observed a deauthentication packet is created as a reply to the sender, specifying the target AP as the sender.",
        .func = cmd_ap_dos
    }, {
        .command = "ap-clone",
        .hint = USAGE_AP_CLONE,
        .help = "If no argument is provided returns the current state of the module. Syntax: ap-clone [ ( ON | OFF ) ( OPEN | WEP | WPA )+ ]  Clone the selected AP(s) and attempt to coerce STAs to connect to the cloned AP instead of the authentic one. The success of this attack will be improved by being able to provide stations a more powerful signal than the genuine AP. The attack will set Gravity's MAC to match the AP's.",
        .func = cmd_ap_clone
    }, {
        .command = "scan",
        .hint = USAGE_SCAN,
        .help = "No argument returns scan status.   ON: Initiate a continuous scan for 802.11 APs and STAs.   Specifying a value for <ssid> will capture only frames from that SSID.  Scan wireless frequencies to identify access points and stations in range. Most modules in this application require one or more target APs and/or STAs so you will run these commands frequently. The scan types commence an open-ended analysis of received packets, and will continue updating until they are stopped. To assist in identifying contemporary devices these scan types also capture a timestamp of when the device was last seen.",
        .func = cmd_scan
    }, {
        .command = "hop",
        .hint = USAGE_HOP,
        .help = "Enable or disable channel hopping, and set the type and frequency of hops. The KILL option terminates the event loop.",
        .func = cmd_hop
    }, {
        .command = "set",
        .hint = USAGE_SET,
        .help = "Set a variety of variables that affect various components of the application. Usage: set <variable> <value>   <variable>   EXPIRY: Age (in minutes) at which packets are no longer included in operations and results.  SCRAMBLE_WORDS: When generating random SSIDs as part of beacon, probe or fuzz, instead of creating SSIDs as a sequence of random words create them as a sequence of random letters.  SSID_LEN_MIN: Minimum length of a generated SSID   SSID_LEN_MAX: Maximum length of a generated SSID   MAC_RAND: Whether to change the device's MAC after each packet (default: ON)   DEFAULT_SSID_COUNT: Number of SSIDs to generate if not specified   CHANNEL: Wireless channel   MAC: ASP32C6's MAC address   HOP_MILLIS: Milliseconds to stay on a channel before hopping to the next (0: Hopping disabled)   ATTACK_PKTS: Number of times to repeat an attack packet when launching an attack (0: Don't stop attacks based on packet count)   ATTACK_MILLIS: Milliseconds to run an attack for when it is launched (0: Don't stop attacks based on duration)   NB: If ATTACK_PKTS and ATTACK_MILLIS are both 0 attacks will not end automatically but will continue until terminated.",
        .func = cmd_set
    }, {
        .command = "get",
        .hint = USAGE_GET,
        .help = "Get a variety of variables that affect various components of the application. Usage: get <variable>   <variable>   EXPIRY: Age (in minutes) at which packets are no longer included in operations and results.  SCRAMBLE_WORDS: When generating random SSIDs as part of beacon, probe or fuzz, instead of creating SSIDs as a sequence of random words create them as a sequence of random letters.  SSID_LEN_MIN: Minimum length of a generated SSID   SSID_LEN_MAX: Maximum length of a generated SSID   MAC_RAND: Whether to change the device's MAC after each packet (default: ON)   CHANNEL: Wireless channel   MAC: ASP32C6's MAC address   HOP_MILLIS: Milliseconds to stay on a channel before hopping to the next (0: Hopping disabled)   ATTACK_PKTS: Number of times to repeat an attack packet when launching an attack (0: Don't stop attacks based on packet count)   ATTACK_MILLIS: Milliseconds to run an attack for when it is launched (0: Don't stop attacks based on duration)   NB: If ATTACK_PKTS and ATTACK_MILLIS are both 0 attacks will not end automatically but will continue until terminated.",
        .func = cmd_get
    }, {
        .command = "view",
        .hint = USAGE_VIEW,
        .help = "VIEW is a fundamental command in this framework, with the typical workflow being Scan-View-Select-Attack. Multiple result sets can be viewed in a single command using, for example, VIEW STA AP or VIEW AP selectedSTA.",
        .func = cmd_view
    }, {
        .command = "select",
        .hint = USAGE_SELECT,
        .help = "Select the specified element from the specified scan results. Usage: select ( AP | STA ) <elementId>.  Selects/deselects item <elementId> from the AP or STA list. Multiple items can be specified separated by spaces.",
        .func = cmd_select
    }, {
        .command = "selected",
        .hint = USAGE_SELECTED,
        .help = "View selected STAs and/or APs from scan results. Usage selected ( AP | STA ). Run with no arguments to display both selected APs and selected STAs.",
        .func = cmd_selected
    }, {
        .command = "clear",
        .hint = USAGE_CLEAR,
        .help = "Clear the specified list.",
        .func = cmd_clear
    }, {
        .command = "handshake",
        .hint = USAGE_HANDSHAKE,
        .help = "Toggle monitoring of 802.11 traffic for a 4-way handshake to obtain key material from. Usage: handshake",
        .func = cmd_handshake
    }, {
        .command = "commands",
        .hint = USAGE_COMMANDS,
        .help = "Display a BRIEF command summary",
        .func = cmd_commands
    }, {
        .command = "info",
        .hint = USAGE_INFO,
        .help = "Display help information for the specified command. Usage: info <command>",
        .func = cmd_info
    }, {
        .command = "bluetooth",
        .hint = USAGE_BT,
        .help = "Test module for bluetooth. Initialises Bluetooth and is working toward device discovery",
        .func = cmd_bluetooth
    }
};

#endif