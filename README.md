# Beast black box utiliy
## Overview
BEAST black box utility is command line tool that useful to decode ModeS and ModeA/C information stored in binary BEAST format in the file. Utility based on the source code of dump1090 and inherits all benefits of this tool. In other words, it is dump1090 code that reads binary BEAST data from the file instead of network or RTL-dongle as it does original dump1090.
## Main features
1. Decode information from binary BEAST format in two ways: dump1090-style view or SBS text format
2. Extract binary BEAST packets according to the filter to another file
3. Filter by ICAO address
4. Decode MLAT timestamps in two ways: relative time and realtime (for second option it need realtime information in UNIX time format for the first file record).
## Command line keys and options
--filename <file>        Source file to proceed
--extract <file>         Extract BEAST data to new file (if no filter specified it just copies source)
--init-time-unix <sec>   Start time (UNIX format) to calculate message realtime using MLAT timestamps
--localtime              Decode time as local time (default is UTC)
--filter-icao <addr>     Show only messages from the given ICAO
--max-messages <count>   Limit messages count (from the start of the file)
--sbs-output             Show messages in SBS format
--show-progress          Show progress during file operation
