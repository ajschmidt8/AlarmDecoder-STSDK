# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

## [1.0.6 P5] - 2021-01-10
- [X] CORE: Improve names of components in menu config build.
- [X] STSTK: DSC panels have no special command to disarm just the code is used. Sending the code while armed disarms and while disarmed will arm. To deal with this quirk on DSC if the panel is already DISARMED and the ST Disarm button is pushed the code will not send anything to the panel.
## [Unreleased] WIP
- [ ] CORE: FIXME: Setting HOST NAME when using static IP over ethernet not working.
- [ ] CORE: FIXME: reboot of esp32 causes connected ser2sock clients to hang. So far various attempts to fix have been unsuccessful. Will need to do some network captures to determine the problem.
- [ ] CORE: HUP/RESTART needs to be centralized so cleanup ex(_fifo_destroy) can happen first. How to connect with STSDK having its own restart calls.
- [ ] CORE: TODO: Find way to set IOT_PUB_QUEUE_LENGTH & IOT_QUEUE_LENGTH from 10 to 20 during build.
- [ ] CORE: Noted coredump when doing oil change check and a twilio message goes out. Both are mbedtls web requests. Will need to investigate and possibly serialize web requests.
- [ ] CORE: Need a vacuum maintenance routine for nv storage to remove dead values or format partition to factory.
- [ ] API: Add Zone tracking algorithm event triggers to AD2EventSearch class.
- [ ] API: Add countdown tracking for DSC/Ademco exit mode
- [ ] CORE: Improve: Finish wiring Virtual Switch A & B and Button A & B.
- [ ] STSDK: Improve: Connect Component OutputA & OutputB with switch capabilities tied to hal_
- [ ] CORE: TODO: Ethernet hardware enable.
- [ ] CORE: Wishlist: Compile for bare metal BeagleBone Black and Raspberry Pi. https://forums.freertos.org/t/freertos-porting-to-raspberry-pi-3/6686/5. Alternatively run inside an ESP32 Virtual machine on a Pi?
- [ ] CORE: TODO: better hardware abstraction. Need to remove _esp_ specific code to make it easier to port to other hardware. Trying to keep the code as POSIX as possible with the limited resources I have.
- [ ] CORE: TODO: ```'ping'``` command could come in handy. Again today needed this with ST MQTT servers seeming to be down.
- [ ] STSDK: TODO: Add SmartThings Zone devices.
## [1.0.6 P4] - 2021-01-08
- [ ] CORE: ad2term arg to reset AD2 device and then start the terminal. Any argument string will send hold the reset line on the AD2pHat board low for 1 second as the terminal is started causing the AD2* to hard reset. Useful when firmware on the AD2* gets corrupted and its hard to get into the boot loader.
## [1.0.6 P3] - 2021-01-08
- [X] CORE: Consolidate some common functions from Twilio and Pushover clients into ad2_utils functions.
- [X] PUSHOVER: New notification component Pushover.net. A simple HTTPS request much like SendGrid or Twilio but I prefer this command format and will be changing other commands to match soon.
## [1.0.6 P2] - 2020-12-29
- [X] CORE: refactor where serial starts to allow to enter ad2term mode early before any other  code starts. This allows to update firmware over the USB cable in ad2term mode using the GUI app or other AD2* firmware update tool.
## [1.0.6 P1] - 2020-12-21
- [X] OTA: Some change in mbedtls happend at some point causing OTA to now fail. Releaseing P1 to fix.
## [1.0.6] - 2020-12-19
- [X] SER2SOCKD: Add new component ser2sock daemon.
  - [X] README
  - [X] CMD
  - [X] INIT
  - [X] TASK
  - [X] QA/TESTING
- [X] API: Refactor api. Simplify subscriber storage class. Add new subscriber callback type. Added ON_RAW_RX_DATA so subscribers can get a raw AD2* stream needed by ser2sockd to avoid writing in ser2sock client and uart client routines.
- [X] CORE: Extract ser2sock fragments and move into ser2sock component.
## [1.0.5] - 2020-12-15
- [X] CORE: Tidy: Improve Kconfig menuconfig.
- [X] API: System specific nibble was a bit. Fixed.
- [X] API: ON_LOW_BATTERY() toggle ON/OFF with no battery on on test panel. This one keeps poping up...
- [X] CORE: Add support for Ethernet and WiFi [S]tatic or [D]ynamic '''MODE''' selection in '''netmode''' command. Static mode supports args IP,MASK,GW,DNS1,DNS2. Currently is ON/OFF settings are only used in Static mode. May later allow for override settings for a mix of DHCP+static.
- [X] CORE: Add Ethernet support for ESP32-POE-ISO board. https://raw.githubusercontent.com/OLIMEX/ESP32-POE-ISO/master/HARDWARE/ESP32-PoE-ISO-Rev.D/ESP32-PoE-ISO_Rev_D.pdf Requires patching https://github.com/vtunr/esp-idf/commit/0f8ea938d826a5aacd2f27db20c5e4edeb3e2ba9 see contrib/esp_eth_phy_lan8720.c.patch
- [X] CORE: Updated README build notes.
- [X] CORE: Refactor app_main cleanup and reorder.
- [X] CORE: Refactor app_main to allow breaking out before network loads just in case hardware is not working and it crashes.
- [X] CORE: Refactor network and event code on device_control.h(HAL). Must build Build with espidf v4.0.1+
- [X] CORE: Added 'V' verbose option to ```logmode``` command.
- [X] TWILIO: Split MODULE_init into MODULE_register_cmds MODULE_init so commands can be initialized even if not running.
- [X] STSDK: Split MODULE_init into MODULE_register_cmds MODULE_init so commands can be initialized even if not running.
- [X] CORE: Added ON_LOW_BATTERY test callback.
- [X] TWILIO: Missed log refactor after serialize refactor. No logic changes.
- [X] TWILIO: Fix ~250 byte memory leak in Twilio json code.. No logic changes.
- [X] CORE: tidy.
## [1.0.4] - 2020-11-08
- [X] CORE: Tidy QA testing.
- [X] TWILIO: Improve docs and ```twsas``` command report format as valid commands with comments.
- [X] TWILIO: Need to add ability to trigger on common states FIRE, ALARM, DISARM, ARM, CHIME etc in new twsas command.
- [X] TWILIO: Add CLI commands to allow the user to construct N number of virtual events to send messages for using the new AD2EventSearch API.
- [X] CORE: Need helper routines for doing simple template operations on strings. For now we can specify any static output message for a search alert results so it is not as critical. ex. 'twsas 1 o This is my own message for this alert'.
- [X] TWILIO: FIXME prevent multiple concurrent connections. Serialize.
- [X] TWILIO: Removed test alerts CHIME, ARM, FIRE, ALARM, LRR.
- [X] API: Added a pointer and integer arg to the search class for callback state passing.
- [X] CORE: Improve arg ad2_copy_nth_arg adding ability to parse last arg skipping spaces till EOL. Easy to grab large strings as final arguemnt in any command.
- [X] TWILIO: Commends and remove confusing constants with DEFINES.
- [X] TWILIO: Tested Email, Call and TEXT messages.
- [X] TWILIO: Refactor for 3 sub commands [M]essage [C]Call(Twiml) [E]Mail(SendGrid).
- [X] TWILIO: Add EVENT Search commands to create notifications on user defined logic.
- [X] CORE: refactor NV storage. I need to keep all slot values in a single file for easy export/import. I had them scattered all over. TODO: Still a little more and then a bunch of cleanup needed.
- [X] TWILIO: Added sendgrid pem file.
- [X] API: add some lookups for humans for enums.
- [X] CORE: Add on boot a dump of NV storage usage.
- [X] STSDK: Alarm bell ON when fire alarm is on.
- [X] CORE: Misc tidy cleanup var rename.
- [X] STSDK: Remove Alarm capability. It was not what I expected. Replaced with contact :c(.
- [X] STSDK: ARM Stay/Away and other indicators now working.
   - Issue reported on [ST forums](https://community.smartthings.com/t/securitysystem-capability-arm-fail-using-stsdk-but-disarm-works/205526). Crickets... Update. With help from ST dev team the problem went POOF! Took a bit but it looks good now. I how have all my feedback working.
- [X] API: FIX: Improve FIRE and ALARM tracking issue with state toggle.
- [X] TWILIO: TODO: Add class based command line configurable notifications to Twilio. Allow to enable/disable event messages for different event types.

## [1.0.3 P2 WIP] - 2020-10-31 (no release)
- [X] API: New class AD2EventSearch and support functions to subscribe. I spent a few days on this. A bit of a unicorn hunt. I could write discrete API and CLI code for every possible message type from the AlarmDecoder protocol but this seems excessive. The task is the same on every message so a single CLI and API to create custom tracking of all messages using REGEX and simple state logic was what I ended up with. This new class when constructed becomes a virtual contact with OPEN/CLOSE/FAULT states. These states are updated based upon user supplied regex patterns and simple user provided logic hints.
- [X] API: RFX message expand hex to bin for easy parsing.
- [X] API: Added message type tracking during parse for post processing.
- [X] API: new string parse function. hex_to_binsz. Rename confusing name bit_string to bin_to_binsz.
- [X] API: Fix: found bugs when testing EXIT NOW with ST App.
- [X] API: Added ON_EXIT_CHANGE event to api.
- [X] STSDK: Added EXIT NOW state contact capability and momentary to activate EXIT mode on DSC/Ademco.
- [X] CORE: Added ad2_exit_now call to send exit now command to the panel based upon panel type.
- [X] STSDK: Wired READY TO ARM contact capability and state tracking.
- [X] API: Added BYPASS CHANGE event
- [X] STSDK: Wired BYPASS to the bypass component contact capability.
- [X] STSDK: remove battery fault contact from device profile in ST dev portal.

## [1.0.3 P1] - 2020-10-20
- [X] CORE: Add command feedback to commands that require a restart to take effect.
- [X] STSDK: Remove restart for enable. Add warnings about restarting.
- [X] CORE: Fix: build error JSON.h issue using esp4.x.
- [X] STSDK: Fix: Not enabling by default.
- [X] TWILIO: Fix: mbedtls_x509_crt_parse is broken when building esp 4.x with return code -0x2180. The PEM parsing routine expects the last byte to be null but under 4.x build it is `-`. Using EMBED_TXTFILES automatically puts a \0 at the end of the block of memory and it is included in the size of the buffer. This is the setting that is used in the 3.x build component mk file.

## [1.0.3] - 2020-10-19
- [X] STSDK: Improve: Connect Panic Alarm and Medical Alarm buttons each requires 3 taps with 5 second timeout.
- [X] STSDK: Improve: Connect DISARM button.
- [X] STSDK: Improve: Connect ARM STAY, ARM AWAY buttons.
- [X] API: Improve: POWER_CHANGE, ALARM BELL CHANGE, BATTERY CHANGE. Rename ALARM to ALARM_BELL. Home->Stay
- [X] STSDK: Improve: Add battery capability. Tested. Batter low on panel sends 0% battery. Battery OK sends 100%.
- [X] STSDK: Improve: Add Power Source capability. Tested. Shows 'mains' or 'battery' depending on alarm state.
- [X] STSDK: Improve: Connected AD2* events to capabilities:
- [X] STSDK: Improve: Cleanup add comments to capabilities declarations.
- [X] STSDK: Improve: Connect Switch A/B to hal_
- [X] CORE: Improve: Add Alarm Panel Action buttons in main component to replace securitySystem that seems to be broken on the ST cloud. Good test for dynamic update of a capability. The buttons will reflect the current panel state.
- [X] CORE: Improve: Command <strike>reboot</strike> -> restart. Much better, almost did a reboot on my host :)
- [X] CORE: Tidy: Command help and moved to \r\n on all HOST communication same as AD2*
- [X] CORE: New: Command: logmode {debug|info|none}. Set logging mode to get more | less details.
- [X] CORE: Improve: Moved esp host UART init to hal_.
- [X] CORE: Improve: New formatting utils to make it easy to build messages to send and route all host com through a one api to allow for management <strike>printf</strike>.
- [X] CORE: Improve: Refactor init sequence and error checking to get the network up before connecting to the AD2*.
- [X] STSDK: Improve: Set default 'stenable' to Y. At first boot it starts and initializes the st nvme partition allowing stserial etc to be called. It is still necessary to add the stserial etc before it will show as an AC on WiFi scanners. From the factory with Keys it will just show up at first boot.
- [X] CORE: New: DSC/Ademco support.
- [X] CORE: New: More utils. Make it easy to send '\x01\x01\x01' using <S1> macro as is done in the current AD2 webapp. upper lower case.
- [X] CORE: Less C more C++.
- [X] CORE: Fix: Switching to espidf 4.x build.
- [X] CORE: Fix deprecated stsdk api calls.
- [X] CORE: Fix: building with espidf 4.x Currently testing only on 3.x branch but will switch to 4.x as soon as some warnings are sorted out.

## [1.0.2] - 2020-10-11
- [X] CORE: New: Command: ```'netmode <[W,E,N]> <ARGS>'```. If SmartThings is disabled allow control of network settings.
- [X] CORE: Improve: Add util function ```ad2_query_key_value``` for N/V parsing. Will be used to store settings in NV in a easy to use text only way.
- [X] CORE: Improve: Filled in empty wifi init function and added ```hal_event_handler```.
- [X] CORE: Fix: Build with espidf idf.py tool
- [X] CORE: Tidy: Rename wifi and eth init functions to hal_ prefix.
- [X] CORE: Tidy: Clean up command syntax and help to share in README.md
- [X] API: Fix: ON_FIRE processing in API.
- [X] API: Improve: getAD2PState override to AlarmDecoder parse class.
- [X] Twilio: Improve: Add rate limiting to send queue.
- [X] Twilio: Improve: Add ALARM and ALARM_RESTORE.
- [X] STSDK: New: Command ```'stenable <[Y|N]>'``` Disable / Enable for STSDK module and let network be managed by main.
- [X] STSDK: New: Prevent FALSE alarms like original ST app. Internal track FIRE panic momentary press events to track and count. If it is pressed 3 times in N seconds trigger a fire. TODO update user as to the "Trigger" level via a color ICON?
- [X] STSDK: Improve: Only report on messages to the default partition for multi partition support.
- [X] STSDK: Fix: Finish wiring smokeDetector capability and test FIRE alarm to show if smoke/clear events.
- [X] STSDK: Tidy: Function name fix ```connection_start_task```.

## [1.0.1 p1] - 2020-09-27
- [X] - Added support code for 'C' mode connection.
- [X] - Refactor 'ad2source' command to allow selecting GPIO UARTS TX/RX pins.
- [X] - New command 'ad2term' to connect directly to AD2* and stop processing.
- [X] - Adding a ad2_* string util for splitting strings. Still needs const char * override.

## [1.0.1] - 2020-09-25
- [X] Code formatting and Travis CI setup on github. What a mess :)... "astyle --style=otbs"

## [1.0.0B] - 2020-09-18

Refactor to take all dev work and clean it up into components that can be turned off / on as needed for a given project.
Changes and tests.
- [X] Add TODO: Add Networking needs to be added if STSDK disabled.
- [X] REFACTOR: Move files into component folders and start fixing stuff.
- [X] REFACTOR: Refactor API to allow multiple subscribers.
- [X] REFACTOR: Ability to disable Twilio
- [X] REFACTOR: Abiiity to disable STSDK
- [X] REFACTOR: Switching everything to C++
- [X] TWILIO: Reconnect AD2 events
- [X] TWILIO: Add more event and beging message formatting work.
- [X] STSDK: Reconnect AD2 events to STSDK
- [X] TEST: OTA over ST
- [X] TEST: Disarm over ST
- [X] TEST: Chime toggle of ST
- [X] OTA: Add version report
- [X] OTA: Add update command
- [X] TEST: CLI OTA update
- [X] OTA: Add subscriber callback for ON_FIRMWARE_VERSION
- [X] REFACTOR: Started adding event id string formats for human readable.

## [1.0.0] - 2020-09-18

Initial project release for testing.

Test restults.
 - [X] OTA updates
    - TODO: Add CLI trigger
 - [X] Toggle CHIME show CHIME state
 - [X] Disarm
 - [X] Onboarding adopting into SmartThings cloud.
 - [X] CLI command line interface for settings.
 - [X] Monitor alarm state from new SmartThings app.
