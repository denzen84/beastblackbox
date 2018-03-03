# Beast black box utility
## Overview
BEAST black box utility is command line tool that useful to decode ModeS and ModeA/C information stored in binary BEAST format in the file. Utility based on the source code of dump1090 and inherits all benefits of this tool. In other words, it is dump1090 code that reads binary BEAST data from the file instead of network or RTL-dongle as it does original dump1090.
## Main features
1. Decode information from binary BEAST format in two ways: dump1090-style view or SBS text format
2. Extract binary BEAST messages according to the filter to another file
3. Filter by ICAO address
4. Decode MLAT timestamps in two ways: relative time and realtime (for second option it needs to have realtime information in UNIX time format for the first file record).
## Command line keys and options
```
--filename <file>        Source file to proceed
--extract <file>         Extract BEAST data to new file (if no filter specified it just copies source)
--init-time-unix <sec>   Start time (UNIX format) to calculate message realtime using MLAT timestamps
--localtime              Decode time as local time (default is UTC)
--filter-icao <addr>     Show only messages from the given ICAO
--max-messages <count>   Limit messages count (from the start of the file)
--sbs-output             Show messages in SBS format
--show-progress          Show progress during file operation

Additional BEAST options:
--modeac                 Enable decoding of SSR modes 3/A & 3/C
--no-crc-check           Disable messages with broken CRC (discouraged)
--no-fix                 Disable single-bits error correction using CRC
--fix                    Enable single-bits error correction using CRC
--aggressive             More CPU for more messages (two bits fixes, ...)
--metric                 Use metric units (meters, km/h, ...)

--help                   Show this help
```
## Useful advices to log binary BEAST traffic
To save binary BEAST traffic from dump1090 to the file in Linux-based systems, the most simple way is to use _netcat_ utility as below:

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

```./beastblackbox --filename radar-ulss7-beast-bin-utc--1520012558.147403028.log --max-messages 1000 --sbs-output```

The same as Example 1, but outputs information in SBS format:

```MSG,4,1,1,406B05,1,1970/01/01,00:00:01.048,2018/03/03,20:16:35.898,,,497,77,,,64,,,,,0```

###### Example 3

```./beastblackbox --filename radar-ulss7-beast-bin-utc--1520012558.147403028.log --sbs-output --filter-icao 4249c6```

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

```./beastblackbox --filename radar-ulss7-beast-bin-utc--1520012558.147403028.log --sbs-output --filter-icao 4249c6 --init-time-unix 1520012558.147403028```

The same as Example 3, but realtime stamps are calculated relative to Unix time 1520012558.147403028 (Fri, 02 Mar 2018 17:42:38.147403028 GMT) instead of default time Unix time 0.0 (Thu, 01 Jan 1970 00:00:00 GMT):

```
MSG,3,1,1,4249C6,1,2018/03/02,17:42:38.185,2018/03/03,20:28:25.944,,17050,,,,,,,,,,0
MSG,4,1,1,4249C6,1,2018/03/02,17:42:38.185,2018/03/03,20:28:25.944,,,307,259,,,-1600,,,,,0
MSG,1,1,1,4249C6,1,2018/03/02,17:42:38.340,2018/03/03,20:28:25.944,UTA469  ,,,,,,,,,,,0
MSG,8,1,1,4249C6,1,2018/03/02,17:42:38.390,2018/03/03,20:28:25.945,,,,,,,,,,,,0
```

###### Example 5

```./beastblackbox --filename radar-ulss7-beast-bin-utc--1520012558.147403028.log --sbs-output --filter-icao 4249c6 --init-time-unix 1520012558.147403028 --localtime```

The same as Example 4, but realtime stamps are calculated in user locale instead of UTC.

###### Example 6

```./beastblackbox --filename radar-ulss7-beast-bin-utc--1520012558.147403028.log --sbs-output --filter-icao 4249c6 --extract 4a49c6-uta469-beast.log```

The same as Example 3, but saves all binary BEAST messages from ICAO 4249c6 to new file _uta469-beast.log_.
