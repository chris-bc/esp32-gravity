| Supported Targets | ESP32-C6 | ESP32 | ESP32-C3 | ESP32-S2 | ESP32-S3 |
| ----------------- | -------- | ----- | -------- | -------- | -------- |

# GRAVITY - ESP32-C6 Wireless Tools
## The unseen force

This project contains an evolving collection of wireless utilities for use on the ESP32-C6.
Initial development will be focused on implementing a core set of 802.11 exploratory tools, with the goal to expand into BLE and 802.15.4 (ZigBee).

## Configuration

Use idf.py menuconfig to configure global options. The section 'Gravity Configuration' contains these options, which include the following:

* DEBUG: Enable additional logging to isolate issues
* DEBUG_VERBOSE: Enable way too much logging
* FLIPPER: Reduce console output as much as possible while retaining utility, to accommodate the Flipper Zero's 20x5 display

## Building & running

All you need to do to build, flash and run Gravity is:
* Install ESP-IDF
* idf.py set-target <chipset>
* idf.py menuconfig
  * Pay particular attention to the section 'Gravity Configuration'
* idf.py build flash monitor

### Using Gravity

To provide a nice, large output screen Gravity was first designed to run as a command-line application. Simply connect your favourite console -
screen, minicom, netcat, putty - to the device's COM port on your computer and explore away!

A Flipper Zero application for Gravity has also been developed, providing a more portable and discreet - if teensy-screened - way of using Gravity.
ESP32-Gravity (when in Flipper mode - see 'Configuration' above) has been heavily customised to make best use of Flipper's small screen, so you
don't lose any functionality on Flipper.
https://github.com/chris-bc/Flipper-Gravity

To connect your Flipper Zero and your ESP32, simply connect RX to TX, TX to RX, GND to GND and 3V3 to 3V3.

## Using Gravity on a console

TODO


## Using Gravity on a Flipper Zero

TODO


## Features Done

* Soft AP
* Command line interface with commands:
    * scan [ ON | OFF ]
      * Scan APs - Fast (API)
      * Scan APs - Continual (SSID + lastSeen when beacons seen)
      * Commands to select/view/remove APs/STAs in scope
      *  Scan STAs - Only include clients of selected AP(s), or all 
      * Fix bug with hidden SSIDs being included in network scan and getting garbled names
      * Update client count when new STAs are found
      * Format timestamps for display
    * set/get SSID_LEN_MIN SSID_LEN_MAX channel hopping MAC channel
      * Options to get/set MAC hopping between frames
      * Options to get/set EXPIRY - When set (value in minutes) Gravity will not use packets of the specified age or older.
    * view: view [ SSID | STA ] - List available targets for the included tools. Each element is prefixed by an identifier for use with the *select* command, with selected items also indicated. "MAC" is a composite set of identifiers consisting of selected stations in addition to MACs for selected SSIDs.
      * VIEW AP selectedSTA - View all APs that are associated with the selected STAs
      * VIEW STA selectedAP - View all STAs that are associated with the selected APs
    * select: select ( SSID | STA ) <specifier>+ - Select/deselect targets for the included tools.
    * beacon: beacon [ RICKROLL | RANDOM [ COUNT ] | INFINITE | USER | OFF ]  - target SSIDs must be specified for USER option. No params returns current status of beacon attack.
      * Beacon spam - Rickroll
      * Beacon spam - User-specified SSIDs
      * Beacon spam - Fuzzing (Random strings)
      * Beacon spam - Infinite (Random strings)
      * Beacon spam - Selected APs
        * Current implementation will include STAs who probed but did not connect to a specified AP. I think that's a good thing?
    * probe: probe [ ANY | SSIDS | AP | OFF ] - Send either directed (requesting a specific SSID) or broadcast probe requests continually, to disrupt legitimate users. SSIDs sourced either from user-specified target-ssids or selected APs from scan results.
    * deauth: deauth [ STA | AP | BROADCAST | OFF ] - Send deauthentication packets to broadcast if no parameters provided, to selected STAs if STA is specified, or to STAs who are or have been associated with selected APs if AP is specified. This attack will have much greater success if specific stations are specified, and greater success still if you adopt the MAC of the access point you are attempting to deauthenticate a device from
    * mana: mana ( ( [ VERBOSE ] [ ON | OFF ] ) | AUTH [ NONE | WEP | WPA ] ) - Enable or disable Mana, its
      verbose output, and set the authentication type it indicates. If not specified returns the current status. 
      * Mana attack - Respond to all probes
      * Loud Mana - Respond with SSIDs from all STAs
    * beacon & probe fuzzing - send buffer-overflowed SSIDs (>32 char, > ssid_len) - FUZZ OFF | ( BEACON | REQ | RESP )+ ( OVERFLOW | MALFORMED )
      * SSID longer/shorter than ssid_len
      * SSID matches ssid_len, is greater than 32 (MAX_SSID_LEN)

* **ONGOING** Receive and parse 802.11 frames

## Features TODO

* Web Server serving a page and various endpoints
    * Since it's more useful for a Flipper Zero implementation, I'll build it with a console API first
    * Once complete can decide whether to go ahead with a web server
* TODO: additional option to show hidden SSIDs
* stalk
  * Homing attack (Focus on RSSI for selected STA(s) or AP)
* ap-dos
  * DOS AP
  * Use target's MAC
  * Respond to frames directed at AP with a deauth packet
* ap-clone
  * Clone AP
  * Use target's MAC
  * Respond to probe requests with forged beacon frames
  * (Hopefully the SoftAP will handle everything else once a STA initiates a connection)
  * Respond to frames directed at AP - who are not currently connected to ESP - with deauth packet
* CLI commands to analyse captured data - stations(ap), ap(station), stations/aps(channel), etc
* handshake
* Capture authentication frames for cracking
* Scan 802.15.1 (BLE/BT) devices and types
* Incorporate BLE/BT devices into homing attack
* BLE/BT fuzzer - Attempt to establish a connection with selected/all devices

## Bugs / Todo

* Add non-broadcast targets to fuzz
* Mana "Scream" - Broadcast known APs
* Add "scrambleWords" config option that applies to beacon, probe, and fuzz SSID generation
* Migrate beacon's random SSIDs to use words
* Better support unicode SSIDs (captured, stored & printed correctly but messes up spacing in AP table - 1 japanese kanji takes 2 bytes.)
* STA channels not recorded
* figure out RSSI
* STA channel issues - can't set channel >= 10 - type/base conversion??


## Testing / Packet verification

| Feature              | Broadcast | selectedSTA (1) | selectedSTA (N>1) | target-SSIDs |
|----------------------|-----------|-----------------|-------------------|--------------|
| Beacon - Random MAC  |  Pass     |  N/A            |  N/A              |  Pass        |
| Beacon - Device MAC  |  Pass     |  N/A            |  N/A              |  Pass        |
| Probe - Random MAC   |  Pass     |  N/A            |  N/A              |  Pass        |
| Probe - Device MAC   |  Pass     |  N/A            |  N/A              |  Pass        |
| Deauth - Frame Src   |  Pass     |  Pass   | Pass  |  Pass             |  N/A         |
| Deauth - Device Src  |  Pass     |  Pass   | Pass  |  Pass             |  N/A         |
| Deauth - Spoof Src   |  N/A      |  Pass   | Pass  |  Pass             |  N/A         |
| Mana - Open Auth     |           |                 |                   |              |
| Mana - Open - Loud   |           |                 |                   |              |
| Mana - WEP           |           |                 |                   |              |
| Mana - WPA           |           |                 |                   |              |
|----------------------|-----------|-----------------|-------------------|--------------|


## Packet types

802.11 type/subtypes
0x40 Probe request
0x50 Probe response
0x80 Beacon
0xB4 RTS
0xC4 CTS
0xD4 ACK
0x1A PS-Poll
0x1E CF-End
0x1F CF-End+CF-Ack
0x08 Data
0x88 QoS Data

QoS data & data both have STA and BSSID
DATA:
BSSID 10
STA 4
QoS DATA:
BSSID 10

STA 431:83:40:14:00:00

TelstraB20819 BC:30:D9:B2:08:1B


# Installation notes

Debugging statements written during development have not been removed from the code; they've been put behind #ifdef directives and can be enabled by adding #define DEBUG to one of the header files to enable them.

Gravity is written using Espressif's ESP-IDF development platform.
Building & installation follows the same paradigm as other ESP-IDF projects, using idf.py build flash monitor to build the application, flash a ESP32-C6, and start a serial connection with the device.

Gravity currently uses a serial console as its user interface, with text-based commands and feedback.
Tools such as Putty, Screen, Minicom and the Arduino IDE can be used to establish a serial connection over USB to interact with the device. Since it will already be installed for building the application, I suggest using ESP-IDF as your serial monitor. To start the ESP-IDF serial monitor and have it connect to a device run idf.py monitor. This can be combined alongside building and flashing with the command "idf.py build flash monitor".

Finding the serial port used by Gravity varies depending on operating system:
* On MacOS I forget
* On Windows I never knew
* On Linux, run ls /dev/tty* before plugging the device in, and again with the device connected. The new file is the serial port in use, and will typically be named ttyUSB0 or ttyACM0.


