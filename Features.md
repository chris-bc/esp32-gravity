## Features Done

* Automatically include/exclude bluetooth based on target chipset
  * ESP32 is currently the only device with Bluetooth support
  * Hopefully ESP32-C6 will join it soon!
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
      * RSSI, channel, secondary channel and WPS now being displayed reliably
    * set/get SSID_LEN_MIN SSID_LEN_MAX channel hopping MAC channel
      * Options to get/set MAC hopping between frames
      * Options to get/set EXPIRY - When set (value in minutes) Gravity will not use packets of the specified age or older.
    * view: view [ SSID | STA ] - List available targets for the included tools. Each element is prefixed by an identifier for use with the *select* command, with selected items also indicated. "MAC" is a composite set of identifiers consisting of selected stations in addition to MACs for selected SSIDs.
      * VIEW AP selectedSTA - View all APs that are associated with the selected STAs
      * VIEW STA selectedAP - View all STAs that are associated with the selected APs
    * select: select ( SSID | STA ) &lt;specifier&gt;+ - Select/deselect targets for the included tools.
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
    * ap-dos
      * Respond to frames to/from a selectedAP with
        * deauth to STA using AP's MAC
        * disassoc to AP using STA's MAC
      * Respond to frames between STAs, where one STA is known to be associated with a selectedAP
        * deauth to both STAs using AP's MAC
    * ap-clone
      * composite attack
      * AP-DOS: Clone AP - Use target's MAC, respond to messages with deauth and APs with disassoc
      * Beacon: Send forged beacon frames
      * Mana (almost): Send forged probe responses
      * (Hopefully the SoftAP will handle everything else once a STA initiates a connection)
      * Ability to select security of broadcast AP - open so you can get connections, or matching the target so you get assoc requests not them?
        * No way to get the target's configured auth type
        * Command line parameter - can specify multiple
    * stalk
      * Homing attack (Focus on RSSI for selected STA(s) or AP)
      * Individual wireless devices selected (selected AP, STA, BT, BTLE, ...)
  * Configurable behaviour on BLE out of memory
    * Truncate all non-selected BT devices and continue
    * Truncate all unnamed BT devices and continue
    * Truncate unnamed and unselected BT devices and continue
    * Truncate the oldest BT devices and continue
    * Truncate lowest RSSI (could even write an incremental cutoff)
    * Halt scanning - esp_ble_gap_stop_scanning() - needs config option
  * Move max RSSI and min AGE into menuconfig and CLI parameters
  * Fuzz now supports a variety of targets: Broadcast, Random, Target-SSIDs, selectedSTA & selectedAP

## All Operations by Command

* BEACON
  * Status
  * RickRoll
  * Random [Count]
  * Infinite
  * Target-SSIDs
  * APs
  * AUTH ( OPEN | WPA )+
* TARGET-SSIDs
  * Add
  * Remove
  * List
* PROBE
  * Target-SSIDs
  * APs
  * Status
* FUZZ
  * Packet Types
    * Beacon
    * Probe REQuest
    * Probe RESPonse
  * Fuzz Type
    * Overflow
    * Malformed
  * Target
    * Broadcast
    * Target-SSIDs
    * SelectedSTA
    * SelectedAP
    * Random
* SNIFF
  * ON | OFF
* DEAUTH
  * Source MAC
    * Frame: As seen in observed packets
    * Device: Gravity device's MAC
    * Spoof: Change the device MAC to the observed MAC
  * Destination MAC
    * STA: Selected Stations (Devices)
    * AP: Selected APs
    * Broadcast
* MANA
  * Clear | On | Off
  * Authentication
    * None
    * WEP
    * WPA
  * MANA Loud
* STALK
  * ON | OFF
* AP-DOS
  * ON | OFF
* AP-Clone
  * ON | OFF
  * Authentication
    * Open
    * WEP
    * WPA
* SCAN
  * WiFi
  * Bluetooth Classic
    * Device Discovery
    * Service Discovery
  * BLE (Bluetooth Low-Energy)
    * Device Discovery
    * Purge Strategy:
      * RSSI [Max RSSI]
      * Age [Min Age]
      * Unnamed Devices
      * Unselected Devices
* HOP [ <millis> ] [ ON | OFF | DEFAULT | KILL ]
* SET <variable> <value>
* GET <variable> <value>
* VIEW
  * Access Points
    * Filter by selected Stations
  * Stations
    * Filter by selected Access Points
  * Bluetooth Devices (Classic + BLE)
  * Bluetooth Services
    * For Selected/All Bluetooth Devices
    * Show only services whose UUID can be matched in Gravity's dictionary
    * Show only services whose UUID canNOT be matched in Gravity's dictionary
  * Sort
    * Age
    * RSSI
    * SSID
    * CURRENTLY NOT WORKING
* SELECT :  select ( AP | STA | BT ) <elementId>+
  * AP: Access Point
  * STA: Station
  * BT: Bluetooth Device
  * All or Any of the above
* SELECTED : SELECTED [ AP | STA | BT ]
  * AP: Access Point
  * STA: Station
  * BT: Bluetooth Device
* CLEAR :  clear ( AP [ SELECTED ] | STA [ SELECTED ] | BT [ SERVICES | SELECTED | ALL ] )
  * Access Points: Selected or All
  * Stations: Selected or All
  * Bluetooth
    * All Bluetooth Devices
    * Selected Bluetooth Devices
    * All Bluetooth Services
* PURGE :  purge [ AP | STA | BT | BLE ]+ [ RSSI [ <maxRSSI> ] | AGE [ <minAge> ] | UNNAMED | UNSELECTED | NONE ]+
  * Object of Purge:
    * Access Points
    * Wireless Stations
    * Bluetooth Classic Devices
    * Bluetooth Low-Energy Devices
  * Purge Strategy
    * Lowest Signal (RSSI) [ <maxRSSI> ]
    * Highest Age [ <minAge> ]
    * Unnamed Devices
    * Unselected Devices
* GRAVITY-VERSION
* INFO <command>


* **ONGOING** Receive and parse 802.11 frames

## Features TODO

* Web Server serving a page and various endpoints
    * Since it's more useful for a Flipper Zero implementation, I'll build it with a console API first
    * Once complete can decide whether to go ahead with a web server
* TODO: additional option to show hidden SSIDs
* improvements to stalk
  * Average
  * Median (or trimmed mean)
* CLI commands to analyse captured data - stations/aps(channel), etc
* handshake
* Capture authentication frames for cracking
* Scan 802.15.1 (BLE/BT) devices and types
  * Currently runs discovery-based scanning, building device list
  * Refactor code to retrieve service information from discovered devices
  * Sniff-based scanning
  * BLE
* BLE/BT fuzzer - Attempt to establish a connection with selected/all devices

## Bugs / Todo

* Save/Load data (entire application state)
  * raw-data [set]
  * Set UART callback to new function in Flipper app
  * Function collects data in a global buffer until all retrieved
    * Similar process/logic to sync
  * Load operates in much the same way, but in reverse
  * Content to save when saving state to Flipper:
    * bool *attack_status
    * char scan_filter_ssid[33]
    * uint8_t scan_filter_ssid_bssid[6]
    * char **user_ssids
    * int user_ssid_count
    * int gravity_ap_count
    * int gravity_sel_ap_count
    * int gravity_sta_count
    * int gravity_sel_sta_count
    * ScanResultAP *gravity_aps
    * ScanResultSTA *gravity_stas
    * ScanResultAP **gravity_selected_aps
    * ScanResultSTA **gravity_selected_stas
    * app_gap_cb_t **gravity_bt_devices
    * uint8_t gravity_bt_dev_count
    * app_gab_cb_t **gravity_selected_bt
    * uint8_t gravity_sel_bt_count
* Fuzz claims to support multiple packet types, but fuzz_overflow_callback only calls fuzz_overflow_pkt once, passing the aggregate packet type (same for malformed). Modify callbacks to iteratively call fuzz_overflow_pkt when multiple packet types are selected
* purge strategy - ability to get & set as a whole rather than age, rssi & strategy separately
    * Specifically for Flipper use case
    * Just confirm the current approach works. YAGNI
* scan <ssid> may be broken - got a freeze after scan bn scan off
* Very long usage strings display pretty well, but may be truncated in non-flipper mode - "scan" line 3: "UNNAMED | UNSE"
  * Add an ANSI cursor up for each line so the cursor stays in place
  * `0112[03L01 sorta thing ... used in stalk mode.
* Add active BT scanning - connections
* Sorting not working. Looks like it should :(
* MAC changing problems on ESP32
  * It's probably fixed as a result of changes to support a new Flipper UI - TEST IT
  * Dropped packets after setting MAC
    * Which bits to re-init?
    * Currently disabled MAC randomisation
  * Fix MAC randomisation for Probe
  * Implement MAC randomisation for beacon
  * Re-implement MAC spoofing for deauth
  * Re-implement MAC spoofing for DOS (Used in 5 places)
* parse additional packet types
* Decode OUI when displaying MACs (in the same way Wireshark does)
* Mana "Scream" - Broadcast known APs in beacon frames
* Better support unicode SSIDs (captured, stored & printed correctly but messes up spacing in AP table - 1 japanese kanji takes 2 bytes.)
* Improve sniff implementation

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
