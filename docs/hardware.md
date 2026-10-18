# Hardware and recovery

Initial target: ZS3L, EFR32MG21A020F768IM32, 768 KiB main flash and 64 KiB RAM,
in the ZY-M100-v1.3 radar using PID `ztc6ggyl`. The original radar MCU remains
unchanged. USART0 uses module TX PA5 and RX PA6 at 115200, 8N1, no flow control.
Other hardware needs an explicitly verified board/protocol profile.

ZS3L bottom view, antenna at the top: the bottom row from left to right is
GND2 (22), RST2 (21), GND1 (20), CLK (19), DIO (18), 3V3 (17). UART RXD is
module pin 15; TXD is pin 16. Use 3.3 V logic and a common ground.

Before a first conversion, read the exact silicon and factory IEEE, then make
two matching reads of main flash, userdata and DEVINFO. Store them privately
outside this repository and record hashes. Main flash may contain network
keys and product credentials. These reads do not include protected secure-engine
storage or OTP and must not be described as a complete chip clone.

Keep SWD available until the application, bootloader and OTA recovery path have
been tested. Flash only the intended main-flash ranges, verify readback, and
preserve factory IEEE, calibration and userdata. Do not mass erase, modify debug
lock, provision OTP, or replace secure-engine firmware as a routine flash step.
Stock and custom network storage may be incompatible; first conversion can
require targeted NVM initialization and Zigbee re-pairing with the same IEEE.

Existing vendor OTA exposure does not establish that stock firmware accepts
an unsigned custom radio image. The first conversion is wired. Custom OTA must
be verified against the installed custom bootloader before advertising support.
