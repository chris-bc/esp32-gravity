| Supported Targets | ESP32-C6 |
| ----------------- | -------- |

# ESP32-C6 Wireless Tools

This project contains an evolving collection of wireless utilities for use on the ESP32-C6.
Initial development will be focused on implementing a core set of 802.11 exploratory tools, with the goal to expand into BLE and 802.15.4 (ZigBee).

## Feature List

* Soft AP
    * Deferred - None of the sample APs work
    * To implement later
* Web Server serving a page and various endpoints
    * Given a soft AP isn't possible, a console API will be implemented first
    * Once complete can make a decision between implementing an AP with web server, or STA with web server
* Implement console component with commands:
    * beacon: beacon [ ( ( RICKROLL | RANDOM | USER ) [ TARGET MAC ] ) | ON | OFF ]  - target SSIDs must be specified for USER option. No params returns current status of beacon attack.
    * probe: probe [ ( ( ALL | MACS ) ( ANY | SSIDS ) ) | ON | OFF ] - Send probe requests to either broadcast or targetted MACs, requesting information either all or targetted SSIDs.
    * deauth: deauth [ STA | BROADCAST | OFF ] - Send deauthentication packets to broadcast if STA is not specified, or to selected STAs if it has been specified. This attack will have much greater success if specific stations are specified, and greater success still if you adopt the MAC of the access point you are attempting to deauthenticate a device from
    * mana: mana [ ON | OFF ] - Enable or disable Mana. If not specified returns the current status
    * stalk
    * ap-dos
    * ap-clone
    * scan [ FASTAP | AP | STA | ANY | OFF ]
    * set/get SSID_LEN_MIN SSID_LEN_MAX 
    * view: view [ SSID | STA | MAC ] - List available targets for the included tools. Each element is prefixed by an identifier for use with the *select* command, with selected items also indicated. "MAC" is a composite set of identifiers consisting of selected stations in addition to MACs for selected SSIDs.
    * select: select ( SSID | STA ) <specifier> - Select/deselect targets for the included tools.
    * handshake
* Beacon spam - Rickroll
* Beacon spam - User-specified SSIDs
* Beacon spam - Fuzzing (Random strings)
* Receive and parse 802.11 frames
* Commands to Get/Set channel, hopping mode, MAC, etc.
* Scan APs - Fast (API)
* Scan APs - Continual (SSID + lastSeen when beacons seen)
* Commands to select/view/remove APs/STAs in scope
* Scan STAs - Only include clients of selected AP(s), or all
* Probe Flood - broadcast/specific
* Deauth - broadcast/specific
* Mana attack - Respond to all probes
* Homing attack (Focus on RSSI for selected STA(s) or AP)
* Capture authentication frames for cracking
* DOS AP
    * Use target's MAC
    * Respond to frames directed at AP with a deauth packet
* Clone AP
    * Use target's MAC
    * Respond to probe requests with forged beacon frames
    * (Hopefully the SoftAP will handle everything else once a STA initiates a connection)
    * Respond to frames directed at AP - who are not currently connected to ESP - with deauth packet
* Scan 802.15.1 (BLE/BT) devices and types
* Incorporate BLE/BT devices into homing attack
* BLE/BT fuzzer - Attempt to establish a connection with selected/all devices

## Migration notes

#define ATTACK_BEACON 0
#define ATTACK_PROBE 1
#define ATTACK_DEAUTH 2
#define ATTACK_MANA 3
#define ATTACK_AP_DOS 4
#define ATTACK_AP_CLONE 5
#define ATTACK_SCAN 6
#define ATTACK_HANDSHAKE 7

# Wi-Fi SoftAP Example

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example shows how to use the Wi-Fi SoftAP functionality of the Wi-Fi driver of ESP for serving as an Access Point.

## How to use example

SoftAP supports Protected Management Frames(PMF). Necessary configurations can be set using pmf flags. Please refer [Wifi-Security](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi-security.html) for more info.

### Configure the project

Open the project configuration menu (`idf.py menuconfig`).

In the `Example Configuration` menu:

* Set the Wi-Fi configuration.
    * Set `WiFi SSID`.
    * Set `WiFi Password`.

Optional: If you need, change the other options according to your requirements.

### Build and Flash

Build the project and flash it to the board, then run the monitor tool to view the serial output:

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for all the steps to configure and use the ESP-IDF to build projects.

* [ESP-IDF Getting Started Guide on ESP32](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)
* [ESP-IDF Getting Started Guide on ESP32-S2](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/get-started/index.html)
* [ESP-IDF Getting Started Guide on ESP32-C3](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/index.html)

## Example Output

There is the console output for this example:

```
I (917) phy: phy_version: 3960, 5211945, Jul 18 2018, 10:40:07, 0, 0
I (917) wifi: mode : softAP (30:ae:a4:80:45:69)
I (917) wifi softAP: wifi_init_softap finished.SSID:myssid password:mypassword
I (26457) wifi: n:1 0, o:1 0, ap:1 1, sta:255 255, prof:1
I (26457) wifi: station: 70:ef:00:43:96:67 join, AID=1, bg, 20
I (26467) wifi softAP: station:70:ef:00:43:96:67 join, AID=1
I (27657) esp_netif_lwip: DHCP server assigned IP to a station, IP is: 192.168.4.2
```

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-idf/issues) on GitHub. We will get back to you soon.
