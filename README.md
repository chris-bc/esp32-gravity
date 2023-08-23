| Supported Targets | ESP32 | ESP32-C6  | ESP32S2 | ESP32S3 |
| ----------------- | ----- | --------- | ------- | ------- |
| Wireless (802.11) |  Yes  |    Yes    |   Yes   |    No   |
| Bluetooth         |  Yes  |  Not Yet  |   No    |    No   |
| BTLE              |  Yes  |  Maybe?   |   No    |    No   |
| ZigBee/Thread     |  No   |    Yes    |   No    |    No   |
|        (802.15.4) |       |           |         |    No   |

**Flipper WiFi Dev Board uses ESP32S2**

# GRAVITY - ESP32 Wireless Tools
## The unseen force

This project contains an evolving collection of wireless utilities for use on the ESP32.
Initial development will be focused on implementing a core set of 802.11 exploratory tools, with the goal to expand into Bluetooth, BLE and 802.15.4 (ZigBee).

### Flipper Zero App

If you have a Flipper Zero there is a Flipper Zero companion app to Gravity. This is being developed alongside ESP32-Gravity and should be in feature parity most of the time.

https://github.com/chris-bc/Flipper-Gravity

Please note the comments below about compiling ESP32-Gravity for the Flipper Zero.

![Flipper Gravity Stalk Mode](https://github.com/chris-bc/esp32-gravity/blob/main/flip-grav-homing.png)

## What happened to ESP32-C6 Gravity?

ESP-IDF v5.2 beta does not yet properly support many bluetooth features, including
bluetooth scanning. Because of this Gravity now targets the ESP32.

If you would like to use Gravity on an ESP32-C6 or ESP32-S2 (like
the Flipper WiFi Dev Board) just build it as usual. Bluetooth
functionality will automatically be excluded for those two chipsets.

## Version Compatibility

Since Gravity has been included with RogueMaster Firmware I thought I should
make a couple of changes to make it easy for people other than me to know
which esp32-Gravity is compatible with which Flipper-Gravity.

* From here on out versions will be numbered `(major).(minor).(release)`;
* For example `1.2.1`;
* Different **release** versions (such as `1.2.1` and `1.2.9`) will always be compatible with each other, although the addition or significant modification of some features may result in a small number of features not working;
* Changes in minor version, such as `1.2.1` and `1.3.1`, are unlikely to be compatible. A change in minor version represents a noteable change to the platform or a breaking change to the platform;
* Changes in major verson, such as `1.4.9` to `2.0.0`, represent substantial changes to the application and how it runs. Different major versions *will not* be compatible with each other.

## Installation From Source

### Configuration

Use `idf.py` menuconfig to configure global options. The section 'Gravity Configuration' contains these options, which include the following:

* `FLIPPER`: Reduce console output as much as possible while retaining utility, to accommodate the Flipper Zero's smaller display
* `DEBUG`: Enable additional logging to isolate issues
* `DEBUG_VERBOSE`: Enable way too much logging

The following configuration options are also required in order for Gravity to use
Bluetooth and fit within ESP32's smaller memory footprint:
* Components -> FreeRTOS -> Port: `Place FreeRTOS functions into Flash` (enable)
* Components -> ESP Ringbuf: `Place non-ISR ringbuf functions into Flash` (enable)
* Components -> ESP Ringbuf: `Place ISR ringbuf functions into Flash` (enable)
* Components -> BlueTooth:
  * Enable `Bluetooth`
  * Enable `Classic Bluetooth`
  * Enable `A2DP`
  * Enable `SPP`
  * Enable `Bluetooth Low Energy`
  * Enable `Enable BLE multi-connections`
  * Enable `Use Dynamic memory allocation in BT/BLE stack`

### Building & running

All you need to do to build, flash and run Gravity is:
* Install ESP-IDF
* `. /path/to/esp-idf/export.sh` (note the initial 'dot space')
* `idf.py set-target <chipset>` ("esp32", "esp32s2", "esp32c6", etc.)
* `idf.py menuconfig`
  * See the "Configuration" section above
* `idf.py build flash`

To open a console to Gravity:
* `idf.py monitor`

To use Flipper-Gravity:
* Connect 3v3 on the Flipper (pin 9) to 3v3 on the ESP32
* Connect GND on the Flipper (pin 8, 11 or 18) to GND on the ESP32
* Connect TX on the Flipper (pin 13) to RX/RX0 on the ESP32
* Connect RX on the Flipper (pin 14) to TX/TX0 on the ESP32
* Start Gravity on the Flipper

I've been told to avoid powering the ESP32 from Flipper's 3V3 pin, purportedly
because *it does not support hot plugging*.

I have no idea whether that's true or not, or what it would mean if it were true,
but consider yoursef warned. (I use the 3V3 pin myself).

## Installing From Binaries

A number of different binary packages are available with each release.
* **esp32-gravity-<version>**:  Compiled as a console application without debug outputs;
* **esp32-gravity-debug-<version>**: Compiled as a console application with debug outputs;
* **esp32-gravity-flipper-<version>**: Compiled as a Flipper application without debug outputs;
* **esp32-gravity-flipper-debug-<version>**: Compiled as a flipper application with debug outputs.

Download and extract the appropriate archive, preferably on a Linux
VM. On a Linux host the packaged executables will work so the flash
script won't require updating - simply run `flash.sh` and the device should flash.

If it isn't already installed you may need to `python3 -m pip install esptool`.

For Windows and MacOS users you'll just need to take an extra couple of steps:
1. Install `Python` if it is not already installed. Ensure `PIP` is also installed (in most cases either PIP is installed by default or a bootstrap is provided to easily install it).
2. Install `esptool` using `python3 -m pip install esptool`
3. The command `esptool.py` should now be in your path. If not, run esptool.py from the binary package.
4. Edit flash.sh in your favourite text editor
  * If you like you could update the commands to call `esptool.py` directly from the path rather than calling a standalone version via python.
  * Alternatively you could just copy and paste everything that comes after `esptool.py` in the script.

This should have resulted in you flashing three binary files to your esp32: `esp-wireless-tools.bin`, `bootloader.bin` and `partition-table.bin`.


## Using Gravity

To provide a nice, large output screen Gravity was first designed
to run as a command-line application. Simply connect your favourite
console - screen, minicom, netcat, putty - to the device's COM port
on your computer and explore away!

A Flipper Zero application for Gravity has also been developed, providing a more portable and discreet - if teensy-screened - way of using Gravity.
ESP32-Gravity (when in Flipper mode - see 'Configuration' above) has been heavily customised to make best use of Flipper's small screen, so you
don't lose any functionality on Flipper.
https://github.com/chris-bc/Flipper-Gravity
Alternatively you can download a compiled Flipper binary file (FAP) from here:
[![FAP Factory](https://flipc.org/api/v1/cool4uma/UART_Terminal/badge?firmware=unleashed)](https://flipc.org/chris-bc/Flipper-Gravity?firmware=roguemaster)

To connect your Flipper Zero and your ESP32, simply connect RX to TX, TX to RX, GND to GND and 3V3 to 3V3.


### What's Next?

Work on Gravity continues on a few fronts, depending on the mood I'm in at the time:
* Bluetooth and BTLE support
  * Scanning/Discovery
  * Follow by RSSID
  * Fuzzing excessive connections and beacons
  * Attempting connection to target devices
* Expanding the number of ingested packet types
  * Improve efficiency & coverage of identifying STA/AP association by looking at
    * Additional management packets
    * Data packets
    * Control packets
* General improvements/bugfixes as I go


## Using Gravity on a console

### Introduction

Gravity provides a number of offensive and defensive wireless features that may be
useful for testing the robustness of your wireless networks.

Some features do not require any configuration and can be started immediately, for
example the random, infinite and rickroll Beacon Spam. Most features, however,
require one or more targets to be selected. Depending on the feature, the targets
may be real APs, real STAs or user-specified SSIDs.
Real APs and STAs are identified using `scan`; user-specified SSIDs
are managed using `target-ssids`.

Settings that affect the behaviour of Gravity are controlled using `get`, `set`, and
`hop`. One of the most important of these is `hop`, which determines whether channel
hopping is enabled. If this has not been explicitly set Gravity will use the default
channel hop setting for that command, starting and stopping hopping if necessary when
the feature is started and stopped.

All commands accept a variety of parameters. See the *Help* section for information on
using Gravity's help system to obtain syntax and usage information for commands.
The `AP` option in particular confuses some people; try to remember that when
specifying target *stations* as an argument using the `AP` option Gravity will
target all *clients* of the selected access points.


### Help

Gravity uses Espressif's console component, providing tab-completion, command hints and
a built-in help system. To use the built-in help, which provides a list of available
commands and their function, and includes several device-specific commands, run `help`.

Once you're sick of that wall of text, `commands` will provide a list of Gravity
commands and their syntax, and `info <command>` will provide more detailed information
about that command.

Tab completion works as expected, and if you pause after typing a command its syntax
will be displayed on the current line.


### Using User-Specified SSIDs

`target-ssids` is used to manage user-specified (i.e. fake) SSIDs. These can be used
by features such as *Beacon Spam* and *Probe Flood* as the SSIDs broadcast by
Gravity.

```c
Syntax: target-ssids [ ( ADD | REMOVE ) <ssidName> ]
```

As a simple list of strings its use is straightforward:
* `target-ssids` with no arguments displays the list of current targets
* `target-ssids add <ssidName>` adds `<ssidName>` to the list
* `target-ssids remove <ssidName>` removed `<ssidName>` from the list

### Using Scanned APs/STAs

#### SCAN

`scan` controls whether packet scanning is currently active. Scanning parses several
types of wireless packet to identify nearby *stations* (devices) and *access points*.

```c
Syntax: scan [ <ssidName> | ON | OFF ]
```

If `debug` has been enabled in `idf.py menuconfig` you will receive a notification
every time a new STA or AP is discovered.
* `scan` returns the current status of scanning
* `scan on` activates scanning
* `scan off` deactivates scanning

Once Gravity has discovered stations and access points by scanning, you can select
one or more of those objects as targets for your commands. If you leave `scan` running
while you run other commands it will continue to discover new APs and STAs in the
background.

##### SCANNING A SPECIFIC SSID

`scan <ssidName>` will activate scanning, but only for APs advertising
the specified `<ssidName>`, and STAs that are associated with APs
advertising the specified `<ssidName>`.

This feature works by first identifying the MAC of the AP advertising
the specified SSID. Because this requires the AP to send a beacon or
probe response it may take some time before you see any output from
this feature. Rest assured that relevant data is being captured from
packets while this is happening. Because of this, if you `scan` an SSID
that hasn't previously been discovered by `scan` you will typically have
a short delay until a suitable packet is found, followed by a lot of
output from cached data that suddenly becomes relevant once the AP's
MAC is known.


#### VIEW

`view`, you guessed it, allows you to view the access points and stations discovered
by `scan`.

```c
Syntax: view ( ( AP [ selectedSTA ] ) | ( STA [ selectedAP ] ) | SORT ( AGE | RSSI | SSID ) )+
```

`view ap`
lists access points discovered during scanning. The following information is provided
for each AP:
* *ID*: The identifier to use when interacting with this AP in Gravity. For example, `select ap 3`. An asterisk (*) displayed before the ID indicates that the AP has been selected.
* *SSID*: The SSID (name) of the access point.
* *BSSID*: The MAC address of the access point.
* *Cli*: The number of clients of the access point that Gravity has been able to identify. Currently this is based on beacons and probe requests. This may over-estimate the number of clients in uncommon situations, and also fail to identify clients in other situations. Work is planned to improve Gravity's STA/AP association detection.
* *Last Seen*: How long ago the AP was last seen by Gravity.
* *Ch*: The wireless channel the AP was last seen on.
* *WPS*: Indicates whether the AP supports Wireless Protected Setup (WPS)

`view sta`
lists stations discovered during scanning. The following information is provided for each STA:
* *ID*: The identifier to use when interacting with this STA in Gravity. For example, `select sta 7`. An asterisk (*) displayed before the ID indicates that the STA has been selected.
* *MAC*: The MAC address of the station
* *Access Point*: The MAC address of the station's access point, if it has been detected.
* *Ch*: The wireless channel the station was last seen on.
* *Last Seen*: How long ago the STA was last seen by Gravity.

If one or more stations or access points have been selected additional options can be
used to filter your results based on the selected items.

`view ap selectedSTA`
lists access points discovered during scanning that are access points of the currently-selected
stations.

`view sta selectedAP`
lists stations discovered during scanning that are clients of the currently-selected
access points.


Display options can be combined in any way you like, for example `view ap selectedsta ap sta sta selectedap`.

#### CLEAR

```c
Syntax: clear AP | STA | ALL
```

Clears `scan` results of the specified type. `scan` results are kept until the ESP32 is
switched off, with subsequent scans *adding to*, rather than replacing, results.
If you wish to remove these results and start afresh you can run:
* `clear ap` clears cached access points
* `clear sta` clears cached stations
* `clear all` clears both access points and stations

#### SELECT

Selects and deselects stations and access points discovered during scanning. Rather
than specifying whether you want to select or deselect something, `select` will
simply toggle the item's selected status, selecting specified items if they are
not selected and deselecting them if they are selected.

```c
Syntax: select ap <id>+
```

`<id>` refers to the identifiers displayed by `view ap`. Multiple IDs can be specified by
separating them with either a space or a `^` (for Flipper Zero compatibility).
For example `select ap 1 2 3`. If AP 2 had already been selected before running this
command then, after running, APs 1 and 3 will be selected and AP 2 no longer selected.

`select sta <id>+` operates in exactly the same way, except for stations rather than
access points.

#### SELECTED

```c
Syntax: selected [ AP | STA ]+
```

Displays only selected access points and/or stations. These are displayed in the same
format as `view`.
* `selected ap` displays selected access points
* `selected sta` displays selected stations
* `selected`, `selected ap sta` and `selected sta ap` each display both selected access points and selected stations


### Settings & Helper Features

In addition to the configuration options described under *Configuration* a number of
settings can be changed while Gravity is running to change its behaviour. These are
controlled using the commends `get`, `set` and `hop`.

#### GET & SET

These commands are described together because they complement each other, with `get`
displaying the current value of a setting and `set` updating that setting to have the
specified value.

Their use is also very similar,
```c
Syntax:
get <variable>
set <variable> <value>
```

`<variable>` can be one of the following settings.

##### channel

The current wireless channel. This will not disable channel hopping if it is active,
so while setting this *will* change the wireless channel in that situation, it will
continue to hop to other channels.

**TODO: Information on channels supported by the ESP32 and ESP32-C6**

##### mac

The physical identifier for the ESP32. This is displayed, and when setting must be
provided, in the standard colon-separated six octet format, e.g. e0:0a:f6:0f:ca:fe.

The first three octets are referred to as the *Organisationally Unique Identifier* and
identify a specific manufacturer. I hope to be able to add a feature to Gravity to
display manufacturer information for discovered devices, although could have issues
fitting an OUI database on the ESP32.

##### mac_rand

MAC Randomisation. Specifies whether Gravity will change its MAC address after
every packet that is sent.

Valid values to `set` MAC Randomisation are `on` and `off`.

##### expires

The time (in minutes) since a station or access point was last seen when Gravity will
stop including it in operations and results.

Decimal values can be used for this setting if a minute isn't granular-enough control.

##### attack_millis

The length of time for Gravity to wait between sending packets. A value of `0` disables
this delay. The default value for this setting is defined under *Gravity Configuration*
in `esp.py menuconfig`.

This setting was introduced because some firewalls will identify a sudden burst of
packets as an attack.

##### attack_pkts

THIS FEATURE HAS NOT BEEN IMPLEMENTED.

For attacks that don't interact with other
devices, the number of packets to send as part of the attack. Once this number of
packets have been sent the attack will end and Gravity will await your next command.

This setting applies to `beacon`, `probe`, `deauth` and `fuzz`.

##### scramble_words

This setting can override the value set in `idf.py menuconfig`. Several features,
at the time of writing `beacon` and `fuzz`, can generate random SSID names as part
of their functionality.

This setting can be enabled with the value `on`, `yes` or `true` and disabled
with the value `off`, `no` or `false`.

When this setting is disabled (default), SSIDs will be generated by selecting a
random SSID length between SSID_LEN_MIN and SSID_LEN_MAX, and then selecting
words at random from a dictionary of 1,000 words, selecting additional words
until the desired length is reached.

When this setting is enabled SSIDs will be generated by selecting a random SSID
length between SSID_LEN_MIN and SSID_LEN_MAX, and then selecting characters
at random until the desired length is reached.

If you'd like to change the contents of the dictionary it is defined in `words.c`.
If you change the number of items in the dictionary you will need to update the value
of `gravityWordCount`, also defined in `words.c`.

If you'd like to change the characters that are used when this setting is enable
they are held in the array `ssid_chars`, defined in `beacon.h`.

##### SSID_LEN_MEN

The minimum acceptable length for an SSID that is generated by Gravity. This can
be a value between `0` and the system constant `MAX_SSID_LEN`. `MAX_SSID_LEN` should
always have a value of `32`. It also must not be greater than `SSID_LEN_MAX`.

##### SSID_LEN_MAX

The maximum acceptable length for an SSID that is generated by Gravity. This can
be a value between `0` and the system constant `MAX_SSID_LEN`. `MAX_SSID_LEN` should
always have a value of `32`. It also must not be less than `SSID_LEN_MEN`.

##### DEFAULT_SSID_COUNT

The default number of SSIDs to generate for features that generate random SSIDs.

Currently this is only used by `beacon random` (where `count` is not specified).


#### HOP

Channel hopping is a background process that we sometimes want and sometimes don't.
Because of that it has its own command to turn it on and off, but Gravity attempts
to be a little more helpful; if `hop` has not been explicitly turned on or off
and you use a feature that is configured to use channel hopping by default, Gravity
will automatically turn hopping on when the feature is started and off when it is
stopped.

This setting controls the state of Gravity's channel hopping. This can be one of
three values, `OFF`, `ON` and `DEFAULT`. When channel hopping is in the `ON` or
`OFF` state it will remain in that state until you manually change it or the ESP32
is restarted.

```c
Syntax: hop [<millis>] [ ON | OFF | DEFAULT | KILL ] [ SEQUENTIAL | RANDOM ]
```

* If no arguments are provided Gravity will display the current channel hopping state;
* If `<millis>` is provided Gravity will pause on each channel for that number of milliseconds before changing to the next channel;
* `KILL` will terminate the process that controls channel hopping. Normally this remains active once channel hopping has been started for the first time, but this may provide marginally better energy usage through a slight reduction in memory usage and processing when channel hopping is not running;
* `ON` and `OFF` give you manual control of channel hopping; it will remain in that state until you set it again, ignoring features' channel hopping defaults;
* When set to `DEFAULT` channel hopping is not active, but will automatically start if a function is run that is configured to use channel hopping by default. When this occurs channel hopping will also automatically stop when no more functions are using it by default.
* `SEQUENTIAL` and `RANDOM` specify whether channel hopping should occur progressively through the channels, or move around randomly. `RANDOM` hopping can be helpful if trying to evade detection, but `SEQUENTIAL` is the default and typically the best choice.


##### Changing Hop Defaults

If you wish to change the default `hop` state for any feature, they are defined in
a `case` statement in the function `app_main` in `gravity.c`. Default values for *dwell
time* (see below) are also specified in this location.

##### Dwell Time

The *usage* description above states that, when `<millis>` is specified, Gravity will
pause on each wireless channel for that number of milliseconds before changing to
another channel. This may be called the `hop interval`, `hop frequency` or `dwell time`.

If `hop` has been explicitly enabled you are in *'full control'* of channel hopping
features; if you want it to hop at a different frequency you need to change it yourself.
If channel hopping is started without providing a *dwell time* Gravity will determine
the longest *dwell time* required by each enabled feature and use that value.

To set the *dwell time* to 1,000 (1,000ms == 1 second) use `hop 1000`.

If `hop` is in its default mode default values will be used for the dwell time.
Currently all features use one of two dwell times, although this is readily extensible.

These default values are specified in `idf.py menuconfig` by the settings labelled
`default dwell time for most features` and `default dwell time for larger features`.
The latter value is used for all variations of the Mana attack, with all other features
(so far) using the smaller value. The allocation of functions to dwell times occurs in
the function `app_main` in `gravity.c`, where `hop_millis_defaults[]` is populated.

NOTE: While many Gravity features will change the *dwell time* based on their own defaults,
they will not change this back to its original value.


### Gravity Actions

The only reason any of the functionality described above even exists is to provide
the context needed to run the features described in this section (and those coming
soon!) - So with all the above information out of the way let's get into it.

#### BEACON SPAM

Broadcast forged beacon frames, simulating the presence of fake wireless networks.

This function on its own has limited potential beyond practical jokes, although has
some use verifying the absence of buffer overflows when developing modules that
scan for wireless networks.

```c
Syntax: beacon [ RICKROLL | RANDOM [COUNT] | INFINITE | TARGET-SSIDs | APs | OFF ] [ AUTH ( OPEN | WPA )+ ]
```

Run `beacon` with no parameters to display the current status.

`AUTH ( OPEN | WPA )+`

This optional sub-command specifies the type(s) of authentication Gravity should advertise
the APs as having. Functionally, a beacon only advertises whether or not an AP has privacy
enabled, meaning that there is no need to differentiate between `WEP` and `WPA`; both
have the same effect - causing the beacon to advertise itself as *secure* rather than *open*.

Multiple authentication types can be specified by joining them together - `auth open wpa`. In
this case Gravity will send two beacon frames for every one that it would otherwise have
sent - one beacon frame advertising an open network and one advertising a secured network.

As soon as I can come up with a clever way to specify a list of authentication types on the
command line we can allow specifying a different authentication type per AP, but that's a
nut I'm yet to crack.

`RICKROLL`

Broadcasts 8 new SSIDs named with lines from a song we all know and love.

`RANDOM [ COUNT ]`

Generates random SSID names and broadcasts those. If `count` is specified this is
the number of SSIDs that are broadcast; if not specified the value of
`DEFAULT_SSID_COUNT` is used as the number of SSIDs. If this value has not been set
its default value, defined in `idf.py menuconfig`, is used.

`INFINITE`

Instead of repeatedly broadcasting frames advertising the same set of SSIDs, this
mode broadcasts a different frame, advertising a different SSID, every time. This
was an effective way to find and remove buffer overflows in one of my wireless
applications.

`TARGET-SSIDs`

This mode should probably be higher up the list because it's probably the most popular
of this class of attack; broadcast SSID names that you specify yourself.

Use the `target-ssids` command to build a list of SSIDs, and then run
`beacon target-ssids` to begin broadcasting the SSIDs you have defined.

`APs`

Broadcast beacon frames for the SSIDs used by the selected access points. Access
Points must have been `scan`ned and `select`ed prior to starting this mode.

#### PROBE REQUEST FLOOD

Broadcast forged probe request frames, sending requests for specified SSIDs.

Probe requests are the mechanism that allows a station (like a phone) to
automatically connect to an access point (like a wifi router) when they are
in range and powered on. Stations periodically send probe requests, which
may signal nearby access points to send a response to the station with information
about their SSIDs. A probe request may either be a *wildcard request*, asking all
access points to respond with their SSID information, or it could be a
*directed request*, asking nearby access points if they provide a specific SSID.

```c
Syntax: probe [ ANY | TARGET-SSIDs | APs | OFF ]
```

Run `probe` with no parameters to display its current status.

`ANY` transmits *wildcard* probe requests.

`TARGET-SSIDs` sends probe requests for the SSIDs specified by `target-ssids`.

`APs` sends probe requests for the SSIDs of the selected access points.

#### DEAUTHENTICATION ATTACK

Deauthentication frames are a valid part of the wireless protocol which, for a
variety of reasons, alert a connected station that it is being deauthenticated
from its access point. This provides the station with an opportunity to stop what
it's doing and disconnect from the access point of its own accord.

This is clearly a feature that could be disruptive if misused, and in the
distant past it was indeed possible to send a broadcast deauthentication packet
and have most stations disconnect from a network. Things are a little more robust
these days and you won't often be able to disconnect stations with a broadcast
packet. Target them individually, however, and your success rate will be high.

```c
Syntax: deauth [<millis>]  [ FRAME | DEVICE | SPOOF ] [ STA | AP | BROADCAST | OFF ]
```

`<millis>` provides an optional way to set `ATTACK_MILLIS` while starting `deauth`; specifies the time between deauthentication packets.

`[ FRAME | DEVICE | SPOOF ]`

Specifies what, if any, MAC spoofing occurs.
* `FRAME` modifies the sender MAC in the wireless frame that is transmitted, but does not change the device's MAC. The sender MAC address that is used, except in `BROADCAST` mode, is the MAC address of the AP the target STA is associated with.
* `DEVICE` replaces the sender MAC address in the frame template with the ESP32's MAC address.
* `SPOOF` modifies the sender MAC address in the same way as `FRAME`, and also modifies the ESP32's MAC address to match the address of the target STA's AP.

`[ STA | AP | BROADCAST | OFF ]`

Specifies the target(s) of the deauthentication attack.

##### BROADCAST DEAUTH

While a broadcast packet is widest-reaching, it's also least effective and only
causes disassociation on a small number of modern devices.

Broadcast mode uses the broadcast address (FF:FF:FF:FF:FF:FF) as the destination
and the device's MAC as the sender.

##### STATION DEAUTH

The deauthentication attack targets selected stations (i.e. stations `select`ed
after `scan`ning).

##### AP DEAUTH

The `AP` deauth mode targets all stations that are associated with selected
access points. In other words, after you `scan` and `select` some access points,
this mode will target stations connected to those access points.


#### Mana

```c
Syntax: mana ( CLEAR | ( [ VERBOSE ] [ ON | OFF ] ) | ( AUTH [ NONE | WEP | WPA ] ) | ( LOUD [ ON | OFF ] ) )
```

Mana is a neat little trick you can use to make almost any wireless device connect
to your access point.
* Your phone sends a probe request looking for `Home WiFi`
* Gravity sends a probe response claiming to be `Home WiFi`
* Your phone attempts to connect to Gravity

That's called the `Karma attack`. If only life were that simple! It used to be, until manufacturers closed that loophole by requiring APs to have previously responded to
a wildcard probe request.
* Your phone sends a probe request looking for `Home WiFi`
* Gravity associates `Home WiFi` with your phone's MAC address, and sends a probe response claiming to be `Home WiFi` (although it won't do anything unless your phone is vulnerable to Karma)
* Your phone ignores Gravity's probe response because it hasn't seen that MAC address before
* Your phone sends a wildcard probe request
* Gravity sees that `Home WiFi` is associated with your phone's MAC and sends a probe response claimining to be `Home WiFi`
* Your phone may now attempt to connect to Gravity (although wildcard probes are typically used to discover nearby networks rather than connect to one)
* Your phone sends a directed probe request looking for `Home WiFi`
* Gravity sends a probe response claiming to be `Home WiFi`
* Your phone has previously seen Gravity identify as `Home WiFi` in response to a wildcard probe request, so it's OK to trust Gravity

This variation is the `Mana attack`. Gravity starts an access point using
Open Authentication (i.e. no password) on launch, which handles the final
piece of a simple Mana-based attack chain:
* If the authentication type presented in the probe response does not match the STA's expected authentication type, for example you specify open authentication and WPA2 is expected, in the case of a phone or laptop the user will typically be alerted that the authentication type has changed and warned that proceeding could be dangerous (although they still might). In the case of automated connections (such as IoT devices) association will almost certainly **NOT** occur.
* If the correct authentication type is specified the STA and AP will proceed through association and authentication before establishing a connection. In addition to running an AP, a DHCP server is running to assign IP addresses to connected STAs.
  * Being able to bridge Gravity's network connection to the Internet, so that connected STAs can obtain an Internet connection and thus be none-the-wiser about Gravity being in the middle, isn't currently possible with Gravity but is on my research list.
This means that a STA which sends probe requests for an AP it expects to be open can be tricked into connecting to our rogue AP.

A STA will, over timee, send a directed probe request for every network in
its Preferred Network List (PNL).

In practice this means that, for a consumer device such as a phone, tablet
or laptop, if that device has *ever connected to* an open wireless network
before - as is often found at coffee shops, restaurants, airports, etc. -
and the device hasn't been told to forget that network, then it can be tricked
into connecting to our rogue AP.

Until I'm able to include bridging to the Internet there's limited use to
this - most phones will quickly disconnect once they decide they can't get
an Internet connection - but I think it's a very interesting technique nonetheless.

A connected device will send all network requests to Gravity - For example if
it tries to open the web page http://google.com it will first ask Gravity for
the IP of google.com. Unfortunately it will stop there at the moment because
Gravity is not Internet-connected. However, if you set that aside as a problem
to be solved, once it is Gravity will see everything that the victim sends over
the network. While most communication is encrypted today some is not, and some
that is will fall back to unencrypted if Gravity refuses to connect the victim
to encrypted ports.

##### Loud Mana

`Loud Mana` is a variation on the `Mana` attack that attempts to successfully
target devices that are not broadcasting probe requests for open networks
(for whatever reason) by sending them probe responses for networks we think
they *may have* seen.

* Suppose devices A and B are both sending directed probe requests for `Johnsons Home WiFi`;
* Device B is also sending a directed probe request for `My Cafe Free WiFi`, an open network;
* Sending a probe response for `My Cafe Free WiFi` to **device A** has a high likelihood of succeeding; if the devices are owned by family members they are likely to both have used the free WiFi at the same cafe.

Because this potentially results in Gravity sending a probe response for a
large number of SSIDs it generates much more network traffic than a regular
`Mana` attack, hence **Loud** Mana.

##### Running Mana

**NOTE:** Because Mana's purpose is to have STAs connect to your rogue AP
it is important that channel hopping and MAC randomisation both be disabled
when running `Mana`.

```c
Syntax: mana ( CLEAR | ( [ VERBOSE ] [ ON | OFF ] ) | ( AUTH [ NONE | WEP | WPA ] ) | ( LOUD [ ON | OFF ] ) )
```

`mana clear`

Because `Mana` compiles Preferred Network Lists for all stations it observes, you may like to clear out the cached PNLs. That's what `mana clear` does.

`mana [ VERBOSE ] [ ON | OFF ]`

This is the primary command that turns Mana on or Off. Running `mana`
with no parameters displays its current status. Starting mana using
`mana verbose on` will generate more console output - there's a lot
of useful information to monitor while this is in development. Once
everything is ironed out and there's a clever way to bridge victims
to the Internet this can become a simple `pwn [ ON | OFF ]` feature
with everything automated, but for now extra information is good.

`mana auth [ NONE | WEP | WPA ]`

Configure the authentication type advertised by `Mana` in probe responses.

The default authentication type is `NONE` because I don't believe the
other types offer any value in the `Mana` attack - In order to establish
an encrypted connection with a STA the AP also needs to know the encryption
key. And if we know the key it would probably be easier to just connect to
the network and find your way into the device, rather than convincing it
to connect to you.

At least, I hope that would be easier, because right now changing the built-in
AP's authentication type and password requires a recompile.

`mana loud [ ON | OFF ]`

Enable or disable `Loud Mana`, described above. If `Mana` is not running when
`mana loud on` is run it will be automatically started.


#### FUZZ

Sends a variety of invalid packets to see how different devices respond to
different types of invalid packets.

This feature can currently operate on beacon frames, probe requests and probe
responses. Two models of invalid packet have been developed so far:
* **Overflow:** The SSID specified in the packet is longer than permitted by the 802.11 std.
* **Malformed:** The length of the SSID in the packet doesn't match the SSID_LEN specified in the packet.

```c
Syntax: fuzz OFF | ( ( BEACON | REQ | RESP )+ ( OVERFLOW | MALFORMED ) )
```

`( BEACON | REQ | RESP )+`

The type of packet to send. Multiple packet types can be specified, although
thinking about it that has never been tested so ... you know.
* `BEACON` will send a beacon frame
* `REQ` will send a probe request
* `RESP` will send a probe response
* Additional frame types will be added as additional frame types can be parsed by Gravity (which will also improve `scan` results)

`OVERFLOW`

In this mode the transmitted wireless packets are well-formed - they *would* be
correct, except that their SSID is longer than an SSID is allowed to be.

This mode will begin by sending packets with length `MAX_SSID_LEN + 1`. `MAX_SSID_LEN`
is (theoretically) platform-specific, but is 32. So we start with an SSID length
of 33. Then increase it to 34, then 35, and so on until the function is stopped.

`MALFORMED`

In this mode the packets are **not** well-formed - The SSID is either longer or
shorter than specified by SSID_LEN. This function alters SSID_LEN in invalid ways
(without changing the length of the SSID) to see what happens.

This function begins by generating a valid SSID with a length as specified by the
`MALFORMED_FROM` setting in `idf.py menuconfig` (default 16). If the default value
of 16 is used as the (real) SSID length, this function will transmit a packet
specifying an SSID_LEN of 17, then 15, 18, 14, 19, 13, 20, 12, etc.


#### SNIFF

`sniff` doesn't offer any really compelling functionality and started life as a
debugging flag during the very early stages of Gravity's development, when I
was learning how to parse wireless frames.

The intent of `sniff` is to display information about interesting packets that are
observed. The sheer volume of packets, coupled with limitations of a serial console,
make finding the balance where enough packets are displayed to be useful, but not so many as to make them incomprehensible.

All that is by way of saying that `sniff`'s functionality may vary considerably over
time. What won't change, however, is its principle - `sniff` displays interested
information about interesting packets to the screen. It doesn't store them or do
anything else with them, so in a lot of ways it is a less capable version of `scan`.

```c
Syntax: sniff [ ON | AFF ]
```


#### STALK

NOT YET IMPLEMENTED.

The intent of this feature is to generate a composite RSSI based on selected
wireless and bluetooth devices, and create a sort of 'homing' feature, attempting
to make it simple to follow the RSSI of several devices to locate the person carrying
them all. 

#### AP-DOS

This feature is intended to simulate a denial-of-service (DOS) attack on the
selected AP(s) by
1. When a station sends or receives a packet from a target AP
  * Adopting the target AP's MAC and sending a deauthentication packet to the STA
  * Adopting the STA's MAC and sending a disassociation packet to the AP
2. When a station that is known to be associated with a target AP sends or receives a packet from another station
  * Adopting the AP's MAC and sending a deauthentication packet to both stations

`AP-DOS` operates on `selectedAPs` - that is, the Access Points that were
identified using `scan` and selected using `select ap`.

```c
Syntax: ap-dos [ ON | OFF ]
```


#### AP-CLONE

This feature is intended to simulate a 'clone-and-takeover' attack on the selected
AP. At a general level you could say that it combines `AP-DOS`'s `deauthentication`
and `disassociation` messages, `beacon`'s forged beacon frames, and `Mana`'s
ability to sneak its way into a station's Preferred Network List (PNL). The
hoped-for result is to trick stations to disconnect from their existing access
point and, not necessarily connect to Gravity, but **NOT** reconnect to their original AP.

`AP-CLONE` operates on `selectedAPs` - that is, the Access Points that were
identified using `scan` and selected using `select ap`.

Any number of authentication types can be specified. Where multiple authentication
types are provided Gravity will send a packet for each authentication type. This
may be detected by some routers as an attack.

```c
Syntax: ap-clone [ ( ON | OFF ) ( OPEN | WEP | WPA )+ ]
```


#### HANDSHAKE

NOT YET IMPLEMENTED.

This is a placeholder for the standard handshake/pmkid capture that all wireless
software needs to have as a feature.

Actually though, given how many new features have been added to `ESP32 Marauder`
over the past six months, I think I might leave it as my scanning and
handshake-capturing tool of choice while I focus on different kinds of features
like `Mana`, `Fuzz` and `AP-DOS`/`AP-CLONE`.

At least in the short term, then, this feature probably won't be built.



## Using Gravity on a Flipper Zero

TODO

![Flipper-Gravity Main Menu](https://github.com/chris-bc/flipper-gravity/blob/main/assets/flip-grav-mainmenu.png)

![Flipper-Gravity Manna Attack](https://github.com/chris-bc/flipper-gravity/blob/main/assets/flip-grav-mana.png)

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



* **ONGOING** Receive and parse 802.11 frames

## Features TODO

* Web Server serving a page and various endpoints
    * Since it's more useful for a Flipper Zero implementation, I'll build it with a console API first
    * Once complete can decide whether to go ahead with a web server
* TODO: additional option to show hidden SSIDs
* improvements to stalk
  * Average
  * Median (or trimmed mean)
  * Bluetooth
* CLI commands to analyse captured data - stations/aps(channel), etc
* handshake
* Capture authentication frames for cracking
* Scan 802.15.1 (BLE/BT) devices and types
  * Currently runs discovery-based scanning, building device list
  * Refactor code to retrieve service information from discovered devices
  * Command view bt
  * Command select bt
  * Refactor selected
  * Bluetooth stalk
  * Sniff-based scanning
  * BLE
* Incorporate BLE/BT devices into homing attack
* BLE/BT fuzzer - Attempt to establish a connection with selected/all devices

## Bugs / Todo

* Add BT-Discovery to scan and build out from there
* Don't automatically get all services as part of Gravity - additional command to inspect a device
  * Move services callback handler into its own function
  * Manage a data model for the services rather than displaying them
  * Don't bother doing that until confirming they can provide useful information
* Add BT results to view
* Add BT results to select
* Add BT results to selected
* Add BT results to stalk
* Add BT sniffing
* Add active scanning - connections
* Retrieve services for all devices
* Interpret services based on their UUIDs
* BT discovery Progress indicator - seconds remaining?
* add_bt_device() - Add a scan method device was discovered with - BT_SCAN_DISCOVERY BT_SCAN_SNIFF BT_SCAN_ACTIVE
* Better formatting of remaining UIs
* Occasionally selecting a STA will give the STA a BIG_INT-like value
  * deselecting and reselecting fixes it.
  * Problem *likely* just with display...
  * Haven't seen this for a week now
* Sorting APs not working. Looks like it should :(
* MAC changing problems on ESP32
  * Dropped packets after setting MAC
    * Which bits to re-init?
    * Currently disabled MAC randomisation
  * Fix MAC randomisation for Probe
  * Implement MAC randomisation for beacon
  * Re-implement MAC spoofing for deauth
  * Re-implement MAC spoofing for DOS (Used in 5 places)
* parse additional packet types
* Decode OUI when displaying MACs (in the same way Wireshark does)
* Test using fuzz with multiple packet types
* Add non-broadcast targets to fuzz
* Mana "Scream" - Broadcast known APs in beacon frames
* Better support unicode SSIDs (captured, stored & printed correctly but messes up spacing in AP table - 1 japanese kanji takes 2 bytes.)
* Improve sniff implementation
* Eventually deauth triggers "wifi:max connection, deauth!"
    
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
Building & installation follows the same paradigm as other ESP-IDF projects, using idf.py build flash monitor to build the application, flash a chip, and start a serial connection with the device.

Gravity currently uses a serial console as its user interface, with text-based commands and feedback.
Tools such as Putty, Screen, Minicom and the Arduino IDE can be used to establish a serial connection over USB to interact with the device. Since it will already be installed for building the application, I suggest using ESP-IDF as your serial monitor. To start the ESP-IDF serial monitor and have it connect to a device run idf.py monitor. This can be combined alongside building and flashing with the command "idf.py build flash monitor".

Finding the serial port used by Gravity varies depending on operating system:
* On MacOS I forget
* On Windows I never knew
* On Linux, run ls /dev/tty* before plugging the device in, and again with the device connected. The new file is the serial port in use, and will typically be named ttyUSB0 or ttyACM0.


