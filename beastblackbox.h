/*
 * beastblackbox.h
 *
 *  Created on: 23 mar 2018
 *  Author: Denis G Dugushkin
 */

// Partially copies source dump1090.h

// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// dump1090.h: main program header
//
// Copyright (c) 2014-2016 Oliver Jowett <oliver@mutability.co.uk>
//
// This file is free software: you may copy, redistribute and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 2 of the License, or (at your
// option) any later version.
//
// This file is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// This file incorporates work covered by the following copyright and
// permission notice:
//
//   Copyright (C) 2012 by Salvatore Sanfilippo <antirez@gmail.com>
//
//   All rights reserved.
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions are
//   met:
//
//    *  Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//    *  Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef BEASTBLACKBOX_H_
#define BEASTBLACKBOX_H_

// ============================= Include files ==========================

#ifndef _WIN32
    #include <stdio.h>
    #include <string.h>
    #include <stdlib.h>
    #include <stdint.h>
    #include <errno.h>
    #include <unistd.h>
    #include <math.h>
    #include <sys/time.h>
    #include <signal.h>
    #include <fcntl.h>
    #include <ctype.h>
    #include <sys/stat.h>
    #include <sys/ioctl.h>
    #include <time.h>
    #include <limits.h>
#else
    #include "winstubs.h" //Put everything Windows specific in here
#endif

#include "compat/compat.h"


// ============================= #defines ===============================
#define BUF_SIZE 4096
#define MAX_MSG_LEN 64

#define MODES_LONG_MSG_BYTES     14
#define MODES_SHORT_MSG_BYTES    7

#define MODES_LONG_MSG_BITS     (MODES_LONG_MSG_BYTES    * 8)
#define MODES_SHORT_MSG_BITS    (MODES_SHORT_MSG_BYTES   * 8)

#define MODEAC_MSG_BYTES         2
#define MODEAC_MSG_FLAG          (1<<0)
#define MODEAC_MSG_MODES_HIT     (1<<1)
#define MODEAC_MSG_MODEA_HIT     (1<<2)
#define MODEAC_MSG_MODEC_HIT     (1<<3)
#define MODEAC_MSG_MODEA_ONLY    (1<<4)
#define MODEAC_MSG_MODEC_OLD     (1<<5)

#define BEAST_DROP_UPPER_34_BITS 0x000000003FFFFFFF
#define MODES_USER_LATLON_VALID (1<<0)
#define INVALID_ALTITUDE (-9999)

/* Where did a bit of data arrive from? In order of increasing priority */
typedef enum {
    SOURCE_INVALID,        /* data is not valid */
    SOURCE_MLAT,           /* derived from mlat */
    SOURCE_MODE_S,         /* data from a Mode S message, no full CRC */
    SOURCE_MODE_S_CHECKED, /* data from a Mode S message with full CRC */
    SOURCE_TISB,           /* data from a TIS-B extended squitter message */
    SOURCE_ADSB,           /* data from a ADS-B extended squitter message */
} datasource_t;

/* What sort of address is this and who sent it?
 * (Earlier values are higher priority)
 */
typedef enum {
    ADDR_ADSB_ICAO,       /* Mode S or ADS-B, ICAO address, transponder sourced */
    ADDR_ADSB_ICAO_NT,    /* ADS-B, ICAO address, non-transponder */
    ADDR_ADSR_ICAO,       /* ADS-R, ICAO address */
    ADDR_TISB_ICAO,       /* TIS-B, ICAO address */

    ADDR_ADSB_OTHER,      /* ADS-B, other address format */
    ADDR_ADSR_OTHER,      /* ADS-R, other address format */
    ADDR_TISB_TRACKFILE,  /* TIS-B, Mode A code + track file number */
    ADDR_TISB_OTHER,      /* TIS-B, other address format */

    ADDR_UNKNOWN          /* unknown address format */
} addrtype_t;

typedef enum {
    UNIT_FEET,
    UNIT_METERS
} altitude_unit_t;

typedef enum {
    ALTITUDE_BARO,
    ALTITUDE_GNSS
} altitude_source_t;

typedef enum {
    AG_INVALID,
    AG_GROUND,
    AG_AIRBORNE,
    AG_UNCERTAIN
} airground_t;

typedef enum {
    SPEED_GROUNDSPEED,
    SPEED_IAS,
    SPEED_TAS
} speed_source_t;

typedef enum {
    HEADING_TRUE,
    HEADING_MAGNETIC
} heading_source_t;

typedef enum {
    SIL_PER_SAMPLE, SIL_PER_HOUR
} sil_type_t;

typedef enum {
    CPR_SURFACE, CPR_AIRBORNE, CPR_COARSE
} cpr_type_t;

// Added for BEAST black box
typedef enum {
    MLAT_NONE, MLAT_BEAST, MLAT_DUMP1090
} mlat_time_t;

typedef void (*mlatprocessor_t)(struct timespec *msgTime, uint64_t mlatTimestamp);

#define MODES_NON_ICAO_ADDRESS       (1<<24) // Set on addresses to indicate they are not ICAO addresses
#define MODES_NOTUSED(V) ((void) V)


// Include subheaders after all the #defines are in place
#include "util.h"
#include "crc.h"
#include "cpr.h"
#include "icao_filter.h"
#include "kmlexport.h"

//======================== structure declarations =========================

// Program global state
struct {                             // Internal state
    int   exit;						 // Flag when user press Ctrl+C

    // File
    char *filename;                  // Input BEAST filename
	char *filename_extract;          // Output BEAST filename, for --extract option
	char *filename_kml;              // Output KML filename, for --export-kml option

	int input_bb;                    // File descriptor for input BEAST file
	int output_bb;					 // File descriptor for output BEAST file
	FILE *output_kml;				 // File descriptor for KML file

	// BEAST
    int   nfix_crc;                  // Number of crc bit error(s) to correct
    int   check_crc;                 // Only display messages with good CRC
    int   mode_ac;                   // Enable decoding of SSR Modes A & C
    int   debug;                     // Debugging mode
    uint32_t show_only;              // Only show messages from this ICAO
    int   use_gnss;                  // Use GNSS altitudes with H suffix ("HAE", though it isn't always) when available


	// Options
    int     show_progress;           // Show progress during file operation
    int     sbs_output;				 // SBS text output
    int     quiet;                     // Suppress stdout
    long long unsigned max_messages; // Max output messages

    // MLAT timestamps
    mlat_time_t mlat_decoder;		 // Type of MLAT processor
    mlatprocessor_t MLATtimefunc;    // Processor function for time calculations in specified manner (none, beast, dump1090)
	uint64_t firsttimestampMsg;	     // Timestamp of the first message (12MHz clock)
	uint64_t previoustimestampMsg;   // Timestamp of the last message (12MHz clock)
	struct timespec baseTime;        // Base time (UNIX format) to calculate relative time for messages using MLAT timestamps
	int useLocaltime;                // Trigger UTC/local user time

	// Counters
	long long unsigned msg_processed;
	long long unsigned msg_extracted;
	int err_not_known_ICAO;			 // Messages that might be valid, but we couldn't validate the CRC against a known ICAO
	int err_bad_crc;				 //bad message or unrepairable CRC error


    // State tracking
    struct aircraft *aircrafts;
} Modes;

// The struct we use to store information about a decoded message.
struct modesMessage {
    // Generic fields
    unsigned char msg[MODES_LONG_MSG_BYTES];      // Binary message.
    unsigned char verbatim[MODES_LONG_MSG_BYTES]; // Binary message, as originally received before correction
    int           msgbits;                        // Number of bits in message
    int           msgtype;                        // Downlink format #
    uint32_t      crc;                            // Message CRC
    int           correctedbits;                  // No. of bits corrected
    uint32_t      addr;                           // Address Announced
    addrtype_t    addrtype;                       // address format / source
    uint64_t      timestampMsg;                   // Timestamp of the message (12MHz clock)
    struct timespec sysTimestampMsg;              // Timestamp of the message (system time)
    int           remote;                         // If set this message is from a remote station
    double        signalLevel;                    // RSSI, in the range [0..1], as a fraction of full-scale power
    int           score;                          // Scoring from scoreModesMessage, if used

    datasource_t  source;                         // Characterizes the overall message source

    // Raw data, just extracted directly from the message
    // The names reflect the field names in Annex 4
    unsigned IID; // extracted from CRC of DF11s
    unsigned AA;
    unsigned AC;
    unsigned CA;
    unsigned CC;
    unsigned CF;
    unsigned DR;
    unsigned FS;
    unsigned ID;
    unsigned KE;
    unsigned ND;
    unsigned RI;
    unsigned SL;
    unsigned UM;
    unsigned VS;
    unsigned char MB[7];
    unsigned char MD[10];
    unsigned char ME[7];
    unsigned char MV[7];

    // Decoded data
    unsigned altitude_valid : 1;
    unsigned heading_valid : 1;
    unsigned speed_valid : 1;
    unsigned vert_rate_valid : 1;
    unsigned squawk_valid : 1;
    unsigned callsign_valid : 1;
    unsigned ew_velocity_valid : 1;
    unsigned ns_velocity_valid : 1;
    unsigned cpr_valid : 1;
    unsigned cpr_odd : 1;
    unsigned cpr_decoded : 1;
    unsigned cpr_relative : 1;
    unsigned category_valid : 1;
    unsigned gnss_delta_valid : 1;
    unsigned from_mlat : 1;
    unsigned from_tisb : 1;
    unsigned spi_valid : 1;
    unsigned spi : 1;
    unsigned alert_valid : 1;
    unsigned alert : 1;

    unsigned metype; // DF17/18 ME type
    unsigned mesub;  // DF17/18 ME subtype

    // valid if altitude_valid:
    int               altitude;         // Altitude in either feet or meters
    altitude_unit_t   altitude_unit;    // the unit used for altitude
    altitude_source_t altitude_source;  // whether the altitude is a barometric altude or a GNSS height
    // valid if gnss_delta_valid:
    int               gnss_delta;       // difference between GNSS and baro alt
    // valid if heading_valid:
    unsigned          heading;          // Reported by aircraft, or computed from from EW and NS velocity
    heading_source_t  heading_source;   // what "heading" is measuring (true or magnetic heading)
    // valid if speed_valid:
    unsigned          speed;            // in kts, reported by aircraft, or computed from from EW and NS velocity
    speed_source_t    speed_source;     // what "speed" is measuring (groundspeed / IAS / TAS)
    // valid if vert_rate_valid:
    int               vert_rate;        // vertical rate in feet/minute
    altitude_source_t vert_rate_source; // the altitude source used for vert_rate
    // valid if squawk_valid:
    unsigned          squawk;           // 13 bits identity (Squawk), encoded as 4 hex digits
    // valid if callsign_valid
    char              callsign[9];      // 8 chars flight number
    // valid if category_valid
    unsigned category;          // A0 - D7 encoded as a single hex byte
    // valid if cpr_valid
    cpr_type_t cpr_type;        // The encoding type used (surface, airborne, coarse TIS-B)
    unsigned cpr_lat;           // Non decoded latitude.
    unsigned cpr_lon;           // Non decoded longitude.
    unsigned cpr_nucp;          // NUCp/NIC value implied by message type

    airground_t airground;      // air/ground state

    // valid if cpr_decoded:
    double decoded_lat;
    double decoded_lon;

    // Operational Status
    struct {
        unsigned valid : 1;
        unsigned version : 3;

        unsigned om_acas_ra : 1;
        unsigned om_ident : 1;
        unsigned om_atc : 1;
        unsigned om_saf : 1;
        unsigned om_sda : 2;

        unsigned cc_acas : 1;
        unsigned cc_cdti : 1;
        unsigned cc_1090_in : 1;
        unsigned cc_arv : 1;
        unsigned cc_ts : 1;
        unsigned cc_tc : 2;
        unsigned cc_uat_in : 1;
        unsigned cc_poa : 1;
        unsigned cc_b2_low : 1;
        unsigned cc_nac_v : 3;
        unsigned cc_nic_supp_c : 1;
        unsigned cc_lw_valid : 1;

        unsigned nic_supp_a : 1;
        unsigned nac_p : 4;
        unsigned gva : 2;
        unsigned sil : 2;
        unsigned nic_baro : 1;

        sil_type_t sil_type;
        enum { ANGLE_HEADING, ANGLE_TRACK } track_angle;
        heading_source_t hrd;

        unsigned cc_lw;
        unsigned cc_antenna_offset;
    } opstatus;

    // Target State & Status (ADS-B V2 only)
    struct {
        unsigned valid : 1;
        unsigned altitude_valid : 1;
        unsigned baro_valid : 1;
        unsigned heading_valid : 1;
        unsigned mode_valid : 1;
        unsigned mode_autopilot : 1;
        unsigned mode_vnav : 1;
        unsigned mode_alt_hold : 1;
        unsigned mode_approach : 1;
        unsigned acas_operational : 1;
        unsigned nac_p : 4;
        unsigned nic_baro : 1;
        unsigned sil : 2;

        sil_type_t sil_type;
        enum { TSS_ALTITUDE_MCP, TSS_ALTITUDE_FMS } altitude_type;
        unsigned altitude;
        float baro;
        unsigned heading;
    } tss;
};

// This one needs modesMessage:
#include "track.h"

// ======================== function declarations =========================

#ifdef __cplusplus
extern "C" {
#endif

//
// Functions exported from mode_ac.c
//
int  detectModeA       (uint16_t *m, struct modesMessage *mm);
void decodeModeAMessage(struct modesMessage *mm, int ModeA);
int  ModeAToModeC      (unsigned int ModeA);

//
// Functions exported from mode_s.c
//
int modesMessageLenByType(int type);
int scoreModesMessage(unsigned char *msg, int validbits);
int decodeModesMessage (struct modesMessage *mm, unsigned char *msg);
void displayModesMessage(struct modesMessage *mm);
void useModesMessage    (struct modesMessage *mm);
//
// Functions exported from interactive.c
//
//void  interactiveShowData(void);

#ifdef __cplusplus
}
#endif

#endif /* BEASTBLACKBOX_H_ */
