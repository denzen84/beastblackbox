# Beast black box utiliy
## Overview
BEAST black box utility is command line tool that useful to decode ModeS and ModeA/C information stored in binary BEAST format in the file. Utility based on the source code of dump1090 and inherits all benefits of this tool. In other words, it is dump1090 code that reads binary BEAST data from the file instead of network or RTL-dongle as it does original dump1090.
## Main features
1. Decode information from binary BEAST format in two ways: dump1090-style view or SBS text format
2. Extract binary BEAST packets according to the filter to another file
3. Filter by ICAO address
4. Decode MLAT timestamps in two ways: relative time and realtime (for second option it need realtime information in UNIX time format for the first file record).
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
```

__Example 1:__

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


__Example 2:__

```./beastblackbox --filename radar-ulss7-beast-bin-utc--1520012558.147403028.log --max-messages 1000 --sbs-output```
The same as Example 1, buttputs information in SBS format:

```MSG,4,1,1,406B05,1,1970/01/01,00:00:01.048,2018/03/03,20:16:35.898,,,497,77,,,64,,,,,0```
