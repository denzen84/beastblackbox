# BEAST black box utility
## Overview
BEAST black box utility is command line tool that decodes ModeS and ModeA/C information stored in binary BEAST format in the file. It's useful to decode BEAST logs from your ADSB receiver/station. BEAST format is very compact and contains source ADSB messages from aircrafts with some additional information. To create BEAST logs in this way there is no need in special utilities - it can be easily created with standart UNIX tools, such as _netcat_. Utility is based on the source code of dump1090 and inherits all benefits of this tool. In other words, it is dump1090 code that reads binary BEAST data from the file instead of network or RTL-dongle as it does original dump1090.
## Main features
1. Decode information from binary BEAST format in two ways: dump1090-style view or SBS text format
2. Extract binary BEAST messages according to the filter to another file
3. Filter by ICAO address
4. Decode MLAT timestamps in two ways: relative time and realtime (for second option it needs to have realtime information in UNIX time format for the first file record).
5. Experimental flight track export of the specified ICAO to KML file. Useful to see track in Google Maps or Google Earth.
## Command line keys and options
```
--filename <file>        Source file to proceed
--extract <file>         Extract BEAST data to new file (if no ICAO filter specified it just copies the source)
--export-kml <file>      Export coordinates and height to KML (works only with --filter-icao)
--mlat-time <type>       Decode MLAT timestamps in specified manner. Types are: none (default), beast, dump1090
--init-time-unix <sec>   Start time (UNIX epoch, format: ss.ms) to calculate realtime using MLAT timestamps
--localtime              Decode time as local time (default: UTC)
--sbs-output             Show messages in SBS format (default: dump1090 style)
--filter-icao <addr>     Show only messages from the given ICAO
--max-messages <count>   Limit messages count from the start of the file (default: all)
--show-progress          Show progress during file operation
--quiet                  Do not output decoded messages to stdout (useful for --extract and --export-kml)

Additional BEAST options:
--modeac                 Enable decoding of SSR modes 3/A & 3/C
--gnss                   Show altitudes as HAE/GNSS (with H suffix) when available
--no-crc-check           Disable messages with broken CRC (discouraged)
--no-fix                 Disable single-bits error correction using CRC
--fix                    Enable single-bits error correction using CRC
--aggressive             More CPU for more messages (two bits fixes, ...)
--help                   Show this help
```
## How to log binary BEAST traffic
To save binary BEAST traffic from dump1090 to the file in Linux-based systems, the most simple way is to use _netcat_ utility as follow:

```nc 127.0.0.1 30005 > radar-ulss7-beast-bin.log```

But this method has one disadvantage. It will lost information about realtime and all messages timestamps could be only decoded as relative time. To avoid this it could be useful to use script:
```
#!/bin/bash
foldname=`date +%s.%N`
radar="ULSS7"

nc 127.0.0.1 30005 > $foldname-$radar-beast-bin.log &

exit 0
```

This script will save UTC Unix timestamp in the filename and will give opportunity to get realtime stamps using _--init-unix-time_ command line option.

## About MLAT timestamps and log timings
As mentioned above, binary Beast format doesn't contain real time information at full. According to Beast format description at [http://wiki.modesbeast.com](http://wiki.modesbeast.com/Radarcape:Firmware_Versions), MLAT timestamp consists of seconds count from the start of the day (upper 18 bits) and nanoseconds (first 30 bits).

>The GPS timestamp is completely handled in the FPGA (hardware) and does not require any interactions on the Linux side. This is >essential to meet the required accuracy. The local clock in the FPGA (64MHz or 96MHz) is stretched or compressed to meet 1e9 counts in >between two pulses by a linear algorithm, in order to avoid bigger jumps in the timestamp. Rollover from 999999999 to 0 occurs >synchronously to the 1PPS leading edge. In parallel, the Second-of-Day information is read from the TSIP serial data stream and also >aligned to the 1PPS pulse. Both parts are then mapped into the 48bits that are available for the timestamp and transmitted with each >Mode-S or Mode-A/C message.

> * SecondsOfDay are using the upper 18 bits of the timestamp
> * Nanoseconds are using the lower 30 bits. The value there directly converts into a 1ns based value and does not need to be converted by sample rate
Original and compatible devices like Radarscrape, Mode-S Beast, Flighradar24 feeder are used this time encoding standart.

In opposite, dump1090 uses simply 48-bit counter with absolutely no information about realtime.

BEAST black box utility implements both types of timing. It can be switched by key _--mlat-time_ with options _dump1090_ or _beast_. By default no timing method specified and utility gets current user localtime.

## Compiling and building
It's only tested on OrangePi boards based on H3 (32 bit ARM) and H5 (64 bit ARM) under Armbian 5.34+ (Debian Jessie and Debian Stretch). In this regard, there are no obstacles that cause problems with building on all Debian-based systems like Raspbian for Raspberry Pi. 

```
cd ~
git clone https://github.com/denzen84/beastblackbox.git
cd beastblackbox
make
```

As mentioned above, code is fully compatible with dump1090 and it can be compiled for all paltforms that available for dump1090.

## Usage examples
###### Example 1

```./beastblackbox --filename radar-ulss7-beast-bin-utc--1520012558.147403028.log --max-messages 1000```

Decodes first 1000 messages of the file _radar-ulss7-beast-bin-utc--1520012558.147403028.log_ and outputs information in dump1090-style.

Example output is:

```
*8f4001599909ba0e3804c5ad7195;
CRC: 000000
RSSI: -15.1 dBFS
Time: 1848344928531.42us, relative: +0.010s prev message, +1.072s log start
DF:17 AA:400159 CA:7 ME:9909BA0E3804C5
 Extended Squitter Airborne velocity over ground, subsonic (19/1)
  ICAO Address:  400159 (Mode S / ADS-B)
  Air/Ground:    airborne?
  GNSS delta:    -1700 ft
  Heading:       76
  Speed:         455 kt groundspeed
  Vertical rate: 0 ft/min GNSS
```


###### Example 2

```./beastblackbox --filename radar-ulss7-beast-bin-utc--1520012558.147403028.log --max-messages 1000 --mlat-time dump1090 --sbs-output```

The same as Example 1, but outputs information in SBS format:

```MSG,4,1,1,406B05,1,1970/01/01,00:00:01.048,2018/03/03,20:16:35.898,,,497,77,,,64,,,,,0```

###### Example 3

```./beastblackbox --filename radar-ulss7-beast-bin-utc--1520012558.147403028.log --mlat-time dump1090 --sbs-output --filter-icao 4249c6```

The same as Example 2, but outputs information in SBS format only for ICAO 4249c6:

```
MSG,3,1,1,4249C6,1,1970/01/01,00:00:00.037,2018/03/03,20:19:24.873,,17050,,,,,,,,,,0
MSG,4,1,1,4249C6,1,1970/01/01,00:00:00.038,2018/03/03,20:19:24.873,,,307,259,,,-1600,,,,,0
MSG,1,1,1,4249C6,1,1970/01/01,00:00:00.193,2018/03/03,20:19:24.873,UTA469  ,,,,,,,,,,,0
MSG,8,1,1,4249C6,1,1970/01/01,00:00:00.243,2018/03/03,20:19:24.873,,,,,,,,,,,,0
MSG,5,1,1,4249C6,1,1970/01/01,00:00:00.380,2018/03/03,20:19:24.873,,17050,,,,,,,0,,0,
MSG,3,1,1,4249C6,1,1970/01/01,00:00:00.457,2018/03/03,20:19:24.873,,17050,,,,,,,,,,0
MSG,4,1,1,4249C6,1,1970/01/01,00:00:00.458,2018/03/03,20:19:24.873,,,307,259,,,-1600,,,,,0
MSG,7,1,1,4249C6,1,1970/01/01,00:00:00.700,2018/03/03,20:19:24.873,,17025,,,,,,,,,,
MSG,3,1,1,4249C6,1,1970/01/01,00:00:00.887,2018/03/03,20:19:24.873,,17025,,,,,,,,,,0
MSG,4,1,1,4249C6,1,1970/01/01,00:00:00.888,2018/03/03,20:19:24.873,,,307,259,,,-1600,,,,,0
MSG,5,1,1,4249C6,1,1970/01/01,00:00:00.970,2018/03/03,20:19:24.873,UTA469  ,17025,,,,,,,0,,0,

Total processed 277 messages
```

###### Example 4

```./beastblackbox --filename radar-ulss7-beast-bin-utc--1520012558.147403028.log --sbs-output --filter-icao 4249c6 --init-time-unix 1520012558.147403028 --mlat-time dump1090```

The same as Example 3, but realtime stamps are calculated relative to Unix time 1520012558.147403028 (Fri, 02 Mar 2018 17:42:38.147403028 GMT) instead of default time Unix time 0.0 (Thu, 01 Jan 1970 00:00:00 GMT):

```
MSG,3,1,1,4249C6,1,2018/03/02,17:42:38.185,2018/03/03,20:28:25.944,,17050,,,,,,,,,,0
MSG,4,1,1,4249C6,1,2018/03/02,17:42:38.185,2018/03/03,20:28:25.944,,,307,259,,,-1600,,,,,0
MSG,1,1,1,4249C6,1,2018/03/02,17:42:38.340,2018/03/03,20:28:25.944,UTA469  ,,,,,,,,,,,0
MSG,8,1,1,4249C6,1,2018/03/02,17:42:38.390,2018/03/03,20:28:25.945,,,,,,,,,,,,0
```

###### Example 5

```./beastblackbox --filename radar-ulss7-beast-bin-utc--1520012558.147403028.log --sbs-output --filter-icao 4249c6 --init-time-unix 1520012558.147403028 --mlat-time dump1090 --localtime```

The same as Example 4, but realtime stamps are calculated in user locale instead of UTC.

###### Example 6

```./beastblackbox --filename radar-ulss7-beast-bin-utc--1520012558.147403028.log --sbs-output --filter-icao 4249c6 --extract 4a49c6-uta469-beast.log```

The same as Example 3, but saves all binary BEAST messages from ICAO 4249c6 to new file _uta469-beast.log_.
