| Supported Targets | ESP32-C6 | ESP32 | ESP32-C3 | ESP32-S2 | ESP32-S3 |
| ----------------- | -------- | ----- | -------- | -------- | -------- |

# GRAVITY - ESP32-C6 Wireless Tools

This project contains an evolving collection of wireless utilities for use on the ESP32-C6.
Initial development will be focused on implementing a core set of 802.11 exploratory tools, with the goal to expand into BLE and 802.15.4 (ZigBee).

## Feature List

* **DONE** Soft AP
* Web Server serving a page and various endpoints
    * Since it's more useful for a Flipper Zero implementation, I'll build it with a console API first
    * Once complete can decide whether to go ahead with a web server
* **DONE** Implement console component with commands:
    * **DONE** beacon: beacon [ RICKROLL | RANDOM [ COUNT ] | INFINITE | USER | OFF ]  - target SSIDs must be specified for USER option. No params returns current status of beacon attack.
    * **DONE** probe: probe [ ANY | SSIDS | OFF ] - Send either directed (requesting a specific SSID) or broadcast probe requests continually, to disrupt legitimate users.
    * deauth: deauth [ STA | BROADCAST | OFF ] - Send deauthentication packets to broadcast if STA is not specified, or to selected STAs if it has been specified. This attack will have much greater success if specific stations are specified, and greater success still if you adopt the MAC of the access point you are attempting to deauthenticate a device from
    * **DONE** mana: mana ( ( [ VERBOSE ] [ ON | OFF ] ) | AUTH [ NONE | WEP | WPA ] ) - Enable or disable Mana, its
      verbose output, and set the authentication type it indicates. If not specified returns the current status. 
    * stalk
    * ap-dos
    * ap-clone
    * CLI commands to analyse captured data - stations(ap), ap(station), stations/aps(channel), etc
    * **DONE** scan [ ON | OFF ]
    * **DONE** set/get SSID_LEN_MIN SSID_LEN_MAX channel hopping MAC channel
    * **DONE**view: view [ SSID | STA ] - List available targets for the included tools. Each element is prefixed by an identifier for use with the *select* command, with selected items also indicated. "MAC" is a composite set of identifiers consisting of selected stations in addition to MACs for selected SSIDs.
    * **DONE**select: select ( SSID | STA ) <specifier> - Select/deselect targets for the included tools.
    * handshake
* **DONE** Beacon spam - Rickroll
* **DONE** Beacon spam - User-specified SSIDs
* **DONE** Beacon spam - Fuzzing (Random strings)
* **DONE** Beacon spam - Infinite (Random strings)
* **ONGOING** Receive and parse 802.11 frames
* **DONE** Commands to Get/Set channel, hopping mode, MAC, etc.
* **DONE** commands to get/set local clock time :( ... or go back to millis since launch (clock())?
* **NOT DOING**Scan APs - Fast (API)
* **DONE**Scan APs - Continual (SSID + lastSeen when beacons seen)
* **DONE**Commands to select/view/remove APs/STAs in scope
* **DONE** Scan STAs - Only include clients of selected AP(s), or all 
  * TODO: additional option to show hidden SSIDs
  * **DONE** Fix bug with hidden SSIDs being included in network scan and getting garbled names
  * **DONE** Update client count when new STAs are found
  * **DONE** Format timestamps for display
* Fix buffer overflow bug in parseChannel()
* **DONE** Probe Flood - broadcast/specific
* Deauth - broadcast/specific
* **DONE** Mana attack - Respond to all probes
* **DONE** Loud Mana - Respond with SSIDs from all STAs
* **DONE** Options to get/set MAC hopping between frames
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

## Scanning implementation

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
STA 4

# TODO

* Mana-Loud
* figure out RSSI
* get/set scan result expiry (lastSeen + x seconds)
* STA channel issues - type/base conversion??
* Display STA's AP
* Display STA vs. AP
* view sta/ap glitches - maybe due to hidden SSIDs?

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


# Basic Console Example (`esp_console_repl`)

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example illustrates the usage of the REPL (Read-Eval-Print Loop) APIs of the [Console Component](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/console.html#console) to create an interactive shell on the ESP chip. The interactive shell running on the ESP chip can then be controlled/interacted with over a serial interface. This example supports UART and USB interfaces.

The interactive shell implemented in this example contains a wide variety of commands, and can act as a basis for applications that require a command-line interface (CLI).

Compared to the [advanced console example](../advanced), this example requires less code to initialize and run the console. `esp_console_repl` API handles most of the details. If you'd like to customize the way console works (for example, process console commands in an existing task), please check the advanced console example.

## How to use example

This example can be used on boards with UART and USB interfaces. The sections below explain how to set up the board and configure the example.

### Using with UART

When UART interface is used, this example should run on any commonly available Espressif development board. UART interface is enabled by default (`CONFIG_ESP_CONSOLE_UART_DEFAULT` option in menuconfig). No extra configuration is required.

### Using with USB_SERIAL_JTAG

On chips with USB_SERIAL_JTAG peripheral, console example can be used over the USB serial port.

* First, connect the USB cable to the USB_SERIAL_JTAG interface.
* Second, run `idf.py menuconfig` and enable `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG` option.

For more details about connecting and configuring USB_SERIAL_JTAG (including pin numbers), see the IDF Programming Guide:
* [ESP32-C3 USB_SERIAL_JTAG](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-guides/usb-serial-jtag-console.html)
* [ESP32-S3 USB_SERIAL_JTAG](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/usb-serial-jtag-console.html)

### Using with USB CDC (USB_OTG peripheral)

USB_OTG peripheral can also provide a USB serial port which works with this example.

* First, connect the USB cable to the USB_OTG peripheral interface.
* Second, run `idf.py menuconfig` and enable `CONFIG_ESP_CONSOLE_USB_CDC` option.

For more details about connecting and configuring USB_OTG (including pin numbers), see the IDF Programming Guide:
* [ESP32-S2 USB_OTG](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s2/api-guides/usb-otg-console.html)
* [ESP32-S3 USB_OTG](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/usb-otg-console.html)

### Other configuration options

This example has an option to store the command history in Flash. This option is enabled by default.

To disable this, run `idf.py menuconfig` and disable `CONFIG_CONSOLE_STORE_HISTORY` option.

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(Replace PORT with the name of the serial port to use.)

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Example Output

Enter the `help` command get a full list of all available commands. The following is a sample session of the Console Example where a variety of commands provided by the Console Example are used.

On ESP32, GPIO15 may be connected to GND to remove the boot log output.

```
This is an example of ESP-IDF console component.
Type 'help' to get the list of commands.
Use UP/DOWN arrows to navigate through command history.
Press TAB when typing command name to auto-complete.
[esp32]> help
help
  Print the list of registered commands

free
  Get the total size of heap memory available

restart
  Restart the program

deep_sleep  [-t <t>] [--io=<n>] [--io_level=<0|1>]
  Enter deep sleep mode. Two wakeup modes are supported: timer and GPIO. If no
  wakeup option is specified, will sleep indefinitely.
  -t, --time=<t>  Wake up time, ms
      --io=<n>  If specified, wakeup using GPIO with given number
  --io_level=<0|1>  GPIO level to trigger wakeup

join  [--timeout=<t>] <ssid> [<pass>]
  Join WiFi AP as a station
  --timeout=<t>  Connection timeout, ms
        <ssid>  SSID of AP
        <pass>  PSK of AP

[esp32]> free
257200
[esp32]> deep_sleep -t 1000
I (146929) deep_sleep: Enabling timer wakeup, timeout=1000000us
I (619) heap_init: Initializing. RAM available for dynamic allocation:
I (620) heap_init: At 3FFAE2A0 len 00001D60 (7 KiB): DRAM
I (626) heap_init: At 3FFB7EA0 len 00028160 (160 KiB): DRAM
I (645) heap_init: At 3FFE0440 len 00003BC0 (14 KiB): D/IRAM
I (664) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (684) heap_init: At 40093EA8 len 0000C158 (48 KiB): IRAM

This is an example of ESP-IDF console component.
Type 'help' to get the list of commands.
Use UP/DOWN arrows to navigate through command history.
Press TAB when typing command name to auto-complete.
[esp32]> join --timeout 10000 test_ap test_password
I (182639) connect: Connecting to 'test_ap'
I (184619) connect: Connected
[esp32]> free
212328
[esp32]> restart
I (205639) restart: Restarting
I (616) heap_init: Initializing. RAM available for dynamic allocation:
I (617) heap_init: At 3FFAE2A0 len 00001D60 (7 KiB): DRAM
I (623) heap_init: At 3FFB7EA0 len 00028160 (160 KiB): DRAM
I (642) heap_init: At 3FFE0440 len 00003BC0 (14 KiB): D/IRAM
I (661) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (681) heap_init: At 40093EA8 len 0000C158 (48 KiB): IRAM

This is an example of ESP-IDF console component.
Type 'help' to get the list of commands.
Use UP/DOWN arrows to navigate through command history.
Press TAB when typing command name to auto-complete.
[esp32]>

```

## Troubleshooting

### Line Endings

The line endings in the Console Example are configured to match particular serial monitors. Therefore, if the following log output appears, consider using a different serial monitor (e.g. Putty for Windows) or modify the example's [UART configuration](#Configuring-UART-and-VFS).

```
This is an example of ESP-IDF console component.
Type 'help' to get the list of commands.
Use UP/DOWN arrows to navigate through command history.
Press TAB when typing command name to auto-complete.
Your terminal application does not support escape sequences.
Line editing and history features are disabled.
On Windows, try using Putty instead.
esp32>
```
