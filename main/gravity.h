#include <esp_wifi.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
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

/* Command specifications */
int cmd_beacon(int argc, char **argv);
int cmd_probe(int argc, char **argv);
int cmd_deauth(int argc, char **argv);
int cmd_mana(int argc, char **argv);
int cmd_stalk(int argc, char **argv);
int cmd_ap_dos(int argc, char **argv);
int cmd_ap_clone(int argc, char **argv);
int cmd_scan(int argc, char **argv);
int cmd_set(int argc, char **argv);
int cmd_get(int argc, char **argv);
int cmd_view(int argc, char **argv);
int cmd_select(int argc, char **argv);
int cmd_handshake(int argc, char **argv);
int cmd_target_ssids(int argc, char **argv);
int mac_bytes_to_string(uint8_t *bMac, char *strMac);
int mac_string_to_bytes(char *strMac, uint8_t *bMac);

#define CMD_COUNT 14
esp_console_cmd_t commands[CMD_COUNT] = {
    {
        .command = "beacon",
        .hint = "Beacon spam attack. Usage: beacon [ RICKROLL | RANDOM [ COUNT ] | INFINITE | USER | OFF ]",
        .help = "A beacon spam attack continously transmits forged beacon frames. RICKROLL will simulate eight APs named after popular song lyrics. RANDOM will generate COUNT random SSIDs between SSID_LEN_MIN and SSID_LEN_MAX in length. If COUNT is not specified DEFAULT_SSID_COUNT is used. USER will generate SSIDs as specified in target-ssids. INIFINITE will continuously broadcast random APs until it is stopped.",
        .func = cmd_beacon
    }, {
        .command = "target-ssids",
        .hint = "Manage SSID targets. Usage: target-ssids [ ( ADD | REMOVE ) <SSID_NAME> ]",
        .help = "ssid-targets may be specified for several attacks, including BEACON and PROBE.",
        .func = cmd_target_ssids
    }, {
        .command = "probe",
        .hint = "Probe flood attack. Usage: probe [ ANY | SSIDS | OFF ]",
        .help = "A probe flood attack continually transmits probe requests, imposing continual load on target APs.",
        .func = cmd_probe
    }, {
        .command = "deauth",
        .hint = "Deauth attack. Usage: deauth [ STA | BROADCAST | OFF ]",
        .help = "Arguments:   STA: Send deauthentication packets to selected stations.   BROADCAST: Send broadcast deauthentication packets.   OFF: Disable a running deauthentication attack.   No argument: Return the current status of the module.   Deauthentication frames are intended to be issued by an AP to instruct connected STAs to disconnect before the AP makes a change that could affect the connection. This obviously makes it trivial to observe a 4-way handshake and obtain key material, and as a consequence of this many - perhaps even the majority of - wireless devices will disregard a broadcast deauthentication packet. This attack will be much more effective if specific stations are selected as targets. Success will be greater still if you adopt the MAC of the Access Point you are attempting to deauthenticate stations from.",
        .func = cmd_deauth
    } , {
        .command = "mana",
        .hint = "Mana attack. Usage: mana [ ON | OFF ]",
        .help = "Call without arguments to obtain the current status of the module.  The Mana attack is a way to 'trick' stations into connecting to a rogue access point. With Mana enabled the AP will respond to all directed probe requests, impersonating any SSID a STA is searching for. If the STA expects any of these SSIDs to have open (i.e. no) authentication the STA will then establish a connection with the AP. The only criterion for vulnerability is that the station has at least one open/unsecured SSID saved in its WiFi history.",
        .func = cmd_mana
    }, {
        .command = "stalk",
        .hint = "Toggle target tracking/homing. Usage: stalk",
        .help = "Displays signal strength information for selected wireless devices, allowing their location to be rapidly determined.  Where selected, this attack will include both stations and access points in its tracking - while it's unlikely you'll ever need to track STAs and APs simultaneously, it's far from impossible. While selecting multiple devices will improve your accuracy and reliability, it also increases the likelihood that selected devices won't all remain in proximity with each other.",
        .func = cmd_stalk
    }, {
        .command = "ap-dos",
        .hint = "802.11 denial-of-service attack. Usage: ap-dos [ ON | OFF ]",
        .help = "If no argument is provided returns the current state of this module. This attack targets selected SSIDs.  This attack attempts to interrupt all communication and disconnect all stations from selected access points. When a frame addressed to a target AP is observed a deauthentication packet is created as a reply to the sender, specifying the target AP as the sender. TODO: If we can identify STAs that are associated with target APs send directed deauth frames to them. Perhaps this could be 'DOS Mk. II'",
        .func = cmd_ap_dos
    }, {
        .command = "ap-clone",
        .hint = "Clone and attempt takeover of the specified AP. Usage: ap-clone [ <AP MAC> | OFF ]",
        .help = "If no argument is provided returns the current state of the module. To start the attack issue the ap_clone command with the target AP's MAC address as a parameter.  Clone the specified AP and attempt to coerce STAs to connect to the cloned AP instead of the authentic one. The success of this attack is dependent upon being able to generate a more powerful signal than the genuine AP. The attack will set ESP32C6's MAC and SSID to match the AP's, run a deauth attack until there are no associated STAs or a predetermined time has passed, then disable the deauth attack and hope that STAs connect to the rogue AP.",
        .func = cmd_ap_clone
    }, {
        .command = "scan",
        .hint = "Scan for wireless devices. Usage: scan [ FASTAP | AP | STA | ANY | OFF ]",
        .help = "No argument returns scan status.   FastAP: Obtain immediate results from a broadcast probe request.   AP: Initiate a continuous scan for APs.   STA: Initiate a continuous scan for STAs.   ANY: Initiate a continuous scan capturing both AP and STA data.  Scan wireless frequencies to identify access points and stations in range. Most modules in this application require one or more target APs and/or STAs so you will run these commands frequently. FASTAP performs a standard broadcast probe, identifying those APs that would be included in an operating system's wireless network scanner. The other scan types commence an open-ended analysis of received packets, and will continue updating until they are stopped. To assist in identifying contemporary devices these scan types also capture a timestamp of when the device was last seen.",
        .func = cmd_scan
    }, {
        .command = "set",
        .hint = "Set a variable. Usage: set <variable> <value>",
        .help = "Set a variety of variables that affect various components of the application. Usage: set <variable> <value>   <variable>   SSID_LEN_MIN: Minimum length of a generated SSID   SSID_LEN_MAX: Maximum length of a generated SSID   DEFAULT_SSID_COUNT: Number of SSIDs to generate if not specified   CHANNEL: Wireless channel   MAC: ASP32C6's MAC address   HOP_MILLIS: Milliseconds to stay on a channel before hopping to the next (0: Hopping disabled)   ATTACK_PKTS: Number of times to repeat an attack packet when launching an attack (0: Don't stop attacks based on packet count)   ATTACK_MILLIS: Milliseconds to run an attack for when it is launched (0: Don't stop attacks based on duration)   NB: If ATTACK_PKTS and ATTACK_MILLIS are both 0 attacks will not end automatically but will continue until terminated.",
        .func = cmd_set
    }, {
        .command = "get",
        .hint = "Get a variable. Usage: get <variable>",
        .help = "Get a variety of variables that affect various components of the application. Usage: get <variable>   <variable>   SSID_LEN_MIN: Minimum length of a generated SSID   SSID_LEN_MAX: Maximum length of a generated SSID   CHANNEL: Wireless channel   MAC: ASP32C6's MAC address   HOP_MILLIS: Milliseconds to stay on a channel before hopping to the next (0: Hopping disabled)   ATTACK_PKTS: Number of times to repeat an attack packet when launching an attack (0: Don't stop attacks based on packet count)   ATTACK_MILLIS: Milliseconds to run an attack for when it is launched (0: Don't stop attacks based on duration)   NB: If ATTACK_PKTS and ATTACK_MILLIS are both 0 attacks will not end automatically but will continue until terminated.",
        .func = cmd_get
    }, {
        .command = "view",
        .hint = "List available targets. Usage: view ( SSID | STA | MAC )*",
        .help = "VIEW is a fundamental command in this framework, with the typical workflow being Scan-View-Select-Attack. Multiple result sets can be viewed in a single command using, for example, VIEW STA SSID.",
        .func = cmd_view
    }, {
        .command = "select",
        .hint = "Select an element. Usage: select ( SSID | STA ) <elementId>",
        .help = "Select the specified element from the specified scan results. Usage: select ( SSID | STA ) <elementId>.  Selects/deselects item <elementId> from the SSID or STA list.",
        .func = cmd_select
    }, {
        .command = "handshake",
        .hint = "Toggle monitoring for encryption material",
        .help = "Toggle monitoring of 802.11 traffic for a 4-way handshake to obtain key material from. Usage: handshake",
        .func = cmd_handshake
    }
};

/*  Globals to track module status information */
enum {
    ATTACK_BEACON,
    ATTACK_PROBE,
    ATTACK_DEAUTH,
    ATTACK_MANA,
    ATTACK_AP_DOS,
    ATTACK_AP_CLONE,
    ATTACK_SCAN,
    ATTACK_HANDSHAKE,
    ATTACKS_COUNT
};
static bool attack_status[ATTACKS_COUNT] = {false, false, false, false, false, false, false, false};

static bool WIFI_INITIALISED = false;
